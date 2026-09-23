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

void main() {
  late Directory temp;

  setUp(() async {
    temp = await Directory.systemTemp.createTemp('xproxy_update_test');
  });

  tearDown(() async {
    if (await temp.exists()) await temp.delete(recursive: true);
  });

  group('probe', () {
    test('跟随重定向并按 Range 只取第一个字节', () async {
      const total = 32485237;
      final ranges = <String?>[];
      var written = 0;
      late HttpServer server;
      server = await _serve((req) async {
        if (req.uri.path == '/artifact.zip') {
          ranges.add(req.headers.value(HttpHeaders.rangeHeader));
          req.response.statusCode = HttpStatus.partialContent;
          req.response.headers.set(HttpHeaders.etagHeader, '"v1"');
          req.response.headers.set(
            HttpHeaders.contentRangeHeader,
            'bytes 0-0/$total',
          );
          req.response.add(const [0]);
          written += 1;
        } else {
          req.response.statusCode = HttpStatus.found;
          req.response.headers.set(
            HttpHeaders.locationHeader,
            'http://127.0.0.1:${server.port}/artifact.zip',
          );
        }
        await req.response.close();
      });
      addTearDown(() => server.close(force: true));

      final artifact =
          await UpdateService(url: _urlOf(server, '/update.zip')).probe();

      expect(artifact.fingerprint, '"v1"');
      expect(artifact.size, total);
      // 重定向后仍要带 Range: 否则会退化成拉取整个包.
      expect(ranges, ['bytes=0-0']);
      expect(written, 1);
    });

    test('服务端不支持 Range 时读完响应头即断开', () async {
      const total = 16 * 1024 * 1024;
      var written = 0;
      final server = await _serve((req) async {
        req.response.statusCode = HttpStatus.ok;
        req.response.headers.set(HttpHeaders.etagHeader, '"big"');
        req.response.headers.contentLength = total;
        final chunk = List<int>.filled(64 * 1024, 0);
        while (written < total) {
          req.response.add(chunk);
          await req.response.flush();
          written += chunk.length;
        }
        await req.response.close();
      });
      addTearDown(() => server.close(force: true));

      final artifact =
          await UpdateService(url: _urlOf(server, '/big.zip')).probe();

      expect(artifact.fingerprint, '"big"');
      expect(artifact.size, total);
      // 关键: 不能为了比对指纹把整个包读完.
      expect(written, lessThan(total));
    });

    test('404 报告为可重试的失败', () async {
      final server = await _serve((req) async {
        req.response.statusCode = HttpStatus.notFound;
        await req.response.close();
      });
      addTearDown(() => server.close(force: true));

      expect(
        UpdateService(url: _urlOf(server, '/missing.zip')).probe(),
        throwsA(isA<UpdateException>()),
      );
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

      final target = File('${temp.path}/update.zip');
      final progress = <int>[];
      final received = await UpdateDownload(
        url: _urlOf(server, '/update.zip'),
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

      final target = File('${temp.path}/cancel.zip');
      final download = UpdateDownload(
        url: _urlOf(server, '/slow.zip'),
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

      final target = File('${temp.path}/missing.zip');
      await expectLater(
        UpdateDownload(
          url: _urlOf(server, '/missing.zip'),
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
      await state.markInstalling('"v2"', 1216);
      expect(await state.hasInstalling(), isTrue);

      // 版本没变(用户取消了安装器): 不记为已处理, 下次仍会提示.
      await state.resolveInstalling(1215);
      expect(await state.hasInstalling(), isFalse);
      expect(await state.fingerprint(), '');

      // 再次安装并确认装成: 记为已处理, 不再重复提示.
      await state.markInstalling('"v2"', 1216);
      await state.resolveInstalling(1216);
      expect(await state.hasInstalling(), isFalse);
      expect(await state.fingerprint(), '"v2"');
    });
  });

  group('format', () {
    test('字节数与速度', () {
      expect(formatBytes(0), '0 B');
      expect(formatBytes(1023), '1023 B');
      expect(formatBytes(1536), '1.5 KB');
      expect(formatBytes(32485237), '31.0 MB');
      expect(formatSpeed(0), '');
      expect(formatSpeed(1258291), '1.2 MB/s');
    });
  });
}
