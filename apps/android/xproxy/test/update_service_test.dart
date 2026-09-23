import 'dart:async';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:xproxy/services/update_service.dart';

/// 起一个本地更新服务, 处理器异常不向测试框架抛出 (客户端主动断开属预期).
Future<HttpServer> _serve(
  Future<void> Function(HttpRequest req) handler,
) async {
  final server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
  server.listen((req) {
    unawaited(() async {
      try {
        await handler(req);
      } catch (_) {}
    }());
  });
  return server;
}

Uri _urlOf(HttpServer server, String path) =>
    Uri.parse('http://127.0.0.1:${server.port}$path');

/// 生成一段可区分的内容 (头部字节不同即指纹不同).
List<int> _content(int length, int seed) => List<int>.generate(
  length,
  (i) => (i * 31 + seed * 7) & 0xff,
  growable: false,
);

/// 远端指纹只覆盖头部字节, 期望值同样只取头部.
String _headFingerprint(List<int> content) =>
    apkFingerprint(content.sublist(0, kApkFingerprintBytes));

void main() {
  late Directory temp;

  setUp(() async {
    temp = await Directory.systemTemp.createTemp('xproxy_update_test');
  });

  tearDown(() async {
    if (await temp.exists()) await temp.delete(recursive: true);
  });

  group('probe', () {
    test('按 Range 只取头部字节并给出内容指纹', () async {
      const total = 78928413;
      final content = _content(total, 1);
      final ranges = <String?>[];
      var written = 0;
      final server = await _serve((req) async {
        ranges.add(req.headers.value(HttpHeaders.rangeHeader));
        req.response.statusCode = HttpStatus.partialContent;
        req.response.headers.set(
          HttpHeaders.contentRangeHeader,
          'bytes 0-${kApkFingerprintBytes - 1}/$total',
        );
        final head = content.sublist(0, kApkFingerprintBytes);
        req.response.add(head);
        written += head.length;
        await req.response.close();
      });
      addTearDown(() => server.close(force: true));

      final probed =
          await UpdateService(url: _urlOf(server, '/app.apk')).probe();

      expect(probed.fingerprint, _headFingerprint(content));
      expect(probed.size, total);
      expect(ranges, ['bytes=0-${kApkFingerprintBytes - 1}']);
      // 只读了头部: 没有为了拿指纹而拉整个包.
      expect(written, kApkFingerprintBytes);
    });

    test('服务端忽略 Range 时读满头部即断开', () async {
      const total = 16 * 1024 * 1024;
      final content = _content(total, 2);
      var written = 0;
      final server = await _serve((req) async {
        req.response.statusCode = HttpStatus.ok;
        req.response.headers.contentLength = total;
        while (written < total) {
          final end = written + 64 * 1024;
          req.response.add(content.sublist(written, end));
          await req.response.flush();
          written = end;
        }
        await req.response.close();
      });
      addTearDown(() => server.close(force: true));

      final probed =
          await UpdateService(url: _urlOf(server, '/big.apk')).probe();

      expect(probed.fingerprint, _headFingerprint(content));
      expect(probed.size, total);
      expect(written, lessThan(total));
    });

    test('内容不同则指纹不同', () async {
      Future<String> fingerprintOf(List<int> content) async {
        final server = await _serve((req) async {
          req.response.headers.contentLength = content.length;
          req.response.add(content);
          await req.response.close();
        });
        addTearDown(() => server.close(force: true));
        return (await UpdateService(url: _urlOf(server, '/a.apk')).probe())
            .fingerprint;
      }

      final a = await fingerprintOf(_content(200 * 1024, 3));
      final b = await fingerprintOf(_content(200 * 1024, 4));
      final again = await fingerprintOf(_content(200 * 1024, 3));
      expect(a, again);
      expect(a, isNot(b));
    });

    test('404 报告为可重试的失败', () async {
      final server = await _serve((req) async {
        req.response.statusCode = HttpStatus.notFound;
        await req.response.close();
      });
      addTearDown(() => server.close(force: true));

      expect(
        UpdateService(url: _urlOf(server, '/missing.apk')).probe(),
        throwsA(isA<UpdateException>()),
      );
    });
  });

  group('local apk', () {
    test('本地包头部指纹与远端同一份内容一致', () async {
      const total = 300 * 1024;
      final content = _content(total, 5);
      final server = await _serve((req) async {
        req.response.headers.contentLength = total;
        req.response.add(content);
        await req.response.close();
      });
      addTearDown(() => server.close(force: true));

      final probed = await UpdateService(url: _urlOf(server, '/a.apk')).probe();

      final same = File('${temp.path}/same.apk');
      await same.writeAsBytes(content);
      final other = File('${temp.path}/other.apk');
      await other.writeAsBytes(_content(total, 6));

      expect(await apkFileFingerprint(same.path), probed.fingerprint);
      expect(await apkFileFingerprint(other.path), isNot(probed.fingerprint));
      expect(await apkFileFingerprint('${temp.path}/none.apk'), isNull);
    });
  });

  group('download', () {
    test('写出文件并按数据块上报进度', () async {
      const chunkSize = 64 * 1024;
      const chunks = 8;
      const total = chunkSize * chunks;
      final server = await _serve((req) async {
        req.response.headers.contentLength = total;
        final chunk = List<int>.filled(chunkSize, 7);
        for (var i = 0; i < chunks; i++) {
          req.response.add(chunk);
          await req.response.flush();
        }
        await req.response.close();
      });
      addTearDown(() => server.close(force: true));

      final target = File('${temp.path}/app-release.apk');
      final progress = <int>[];
      final received = await UpdateDownload(
        url: _urlOf(server, '/app.apk'),
        target: target,
      ).start(onProgress: (r, _) => progress.add(r));

      expect(received, total);
      expect(await target.length(), total);
      expect(progress.first, 0);
      expect(progress.last, total);
      for (var i = 1; i < progress.length; i++) {
        expect(progress[i], greaterThanOrEqualTo(progress[i - 1]));
      }
    });

    test('取消下载会中断连接并删除半成品', () async {
      const chunkSize = 64 * 1024;
      const total = chunkSize * 64;
      final server = await _serve((req) async {
        req.response.headers.contentLength = total;
        final chunk = List<int>.filled(chunkSize, 1);
        for (var i = 0; i < 64; i++) {
          req.response.add(chunk);
          await req.response.flush();
          await Future<void>.delayed(const Duration(milliseconds: 50));
        }
        await req.response.close();
      });
      addTearDown(() => server.close(force: true));

      final target = File('${temp.path}/cancel.apk');
      final download = UpdateDownload(
        url: _urlOf(server, '/slow.apk'),
        target: target,
      );
      var cancelling = false;
      final future = download.start(
        onProgress: (received, _) {
          if (received > 0 && !cancelling) {
            cancelling = true;
            download.cancel();
          }
        },
      );

      await expectLater(future, throwsA(isA<UpdateCancelled>()));
      expect(download.cancelled, isTrue);
      expect(await target.exists(), isFalse);
    });

    test('404 报告为失败且不留下文件', () async {
      final server = await _serve((req) async {
        req.response.statusCode = HttpStatus.notFound;
        await req.response.close();
      });
      addTearDown(() => server.close(force: true));

      final target = File('${temp.path}/missing.apk');
      await expectLater(
        UpdateDownload(
          url: _urlOf(server, '/missing.apk'),
          target: target,
        ).start(),
        throwsA(isA<UpdateException>()),
      );
      expect(await target.exists(), isFalse);
    });
  });

  group('update state', () {
    test('记录已处理指纹与检查时间', () async {
      SharedPreferences.setMockInitialValues({});
      final state = UpdateState();

      expect(await state.fingerprint(), '');
      expect(await state.lastCheck(), isNull);

      await state.saveFingerprint('"v1"');
      final now = DateTime(2026, 9, 24, 10, 30);
      await state.saveLastCheck(now);

      expect(await state.fingerprint(), '"v1"');
      expect(await state.lastCheck(), now);
    });

    test('核对上次发起的安装结果', () async {
      SharedPreferences.setMockInitialValues({});
      final state = UpdateState();

      expect(await state.hasInstalling(), isFalse);
      await state.markInstalling('deadbeef', 1216);
      expect(await state.hasInstalling(), isTrue);

      // 版本没变(用户取消了安装器): 不记为已处理, 下次仍会提示.
      await state.resolveInstalling(1215);
      expect(await state.hasInstalling(), isFalse);
      expect(await state.fingerprint(), '');

      // 再次安装并确认装成: 记为已处理, 不再重复提示.
      await state.markInstalling('deadbeef', 1216);
      await state.resolveInstalling(1216);
      expect(await state.hasInstalling(), isFalse);
      expect(await state.fingerprint(), 'deadbeef');
    });
  });

  group('format', () {
    test('字节数与速度', () {
      expect(formatBytes(0), '0 B');
      expect(formatBytes(1023), '1023 B');
      expect(formatBytes(1536), '1.5 KB');
      expect(formatBytes(78928413), '75.3 MB');
      expect(formatSpeed(0), '');
      expect(formatSpeed(1258291), '1.2 MB/s');
    });
  });
}
