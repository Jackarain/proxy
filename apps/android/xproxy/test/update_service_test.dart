import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:xproxy/services/update_service.dart';

/// 发布目录列表的真实响应形态.
const String _indexJson = '''
[
  {"last_write_time":"09-24-2026 02:53","filename":"app-release.apk","is_dir":false,
   "filesize":133496348,"hash":"5FEDC82B81A856B782EBCF9AF4ED62924FE1FA52"},
  {"last_write_time":"09-12-2026 23:43","filename":"proxy_server-android-release-apk.zip",
   "is_dir":false,"filesize":32481488,
   "hash":"9cca542338a0044aaf2f3abaada652cb7c895de0"}
]
''';

const String _apkSha1 = '5fedc82b81a856b782ebcf9af4ed62924fe1fa52';

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

Future<HttpServer> _serveIndex(String body) => _serve((req) async {
  req.response.headers.contentType = ContentType.json;
  req.response.write(body);
  await req.response.close();
});

UpdateService _serviceFor(HttpServer server) =>
    UpdateService(indexUrl: _urlOf(server, '/download/?q=json&hash=1'));

void main() {
  late Directory temp;

  setUp(() async {
    temp = await Directory.systemTemp.createTemp('xproxy_update_test');
  });

  tearDown(() async {
    if (await temp.exists()) await temp.delete(recursive: true);
  });

  group('fetch', () {
    test('从列表里取安装包的 SHA-1 与大小', () async {
      final server = await _serveIndex(_indexJson);
      addTearDown(() => server.close(force: true));

      final remote = await _serviceFor(server).fetch();

      expect(remote.hash, _apkSha1);
      expect(remote.size, 133496348);
      expect(remote.lastWriteTime, '09-24-2026 02:53');
    });

    test('列表里没有安装包时报错', () async {
      final server = await _serveIndex(
        jsonEncode([
          {'filename': 'other.apk', 'filesize': 1, 'hash': 'a' * 40},
        ]),
      );
      addTearDown(() => server.close(force: true));

      expect(
        _serviceFor(server).fetch(),
        throwsA(
          isA<UpdateException>().having(
            (e) => e.message,
            'message',
            contains('app-release.apk'),
          ),
        ),
      );
    });

    test('校验值缺失或格式不对时报错', () async {
      for (final hash in ['', 'abc', 'z' * 40, 'a' * 64]) {
        final server = await _serveIndex(
          jsonEncode([
            {'filename': 'app-release.apk', 'filesize': 1, 'hash': hash},
          ]),
        );
        addTearDown(() => server.close(force: true));
        expect(
          _serviceFor(server).fetch(),
          throwsA(isA<UpdateException>()),
          reason: 'hash=$hash',
        );
      }
    });

    test('响应不是 JSON 时报错', () async {
      final server = await _serveIndex('<html>nginx</html>');
      addTearDown(() => server.close(force: true));

      expect(_serviceFor(server).fetch(), throwsA(isA<UpdateException>()));
    });

    test('响应过大时报错', () async {
      final server = await _serveIndex('x' * (kUpdateIndexMaxBytes + 1024));
      addTearDown(() => server.close(force: true));

      expect(_serviceFor(server).fetch(), throwsA(isA<UpdateException>()));
    });

    test('HTTP 异常状态报错', () async {
      for (final status in [
        HttpStatus.notFound,
        HttpStatus.internalServerError,
      ]) {
        final server = await _serve((req) async {
          req.response.statusCode = status;
          await req.response.close();
        });
        addTearDown(() => server.close(force: true));
        expect(_serviceFor(server).fetch(), throwsA(isA<UpdateException>()));
      }
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

      final target = File('${temp.path}/$kUpdateApkName');
      final progress = <int>[];
      final received = await UpdateDownload(
        url: _urlOf(server, '/app-release.apk'),
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
    test('记录已处理的校验值与检查时间', () async {
      SharedPreferences.setMockInitialValues({});
      final state = UpdateState();

      expect(await state.handledHash(), '');
      expect(await state.lastCheck(), isNull);

      await state.saveHandledHash(_apkSha1);
      final now = DateTime(2026, 9, 24, 10, 30);
      await state.saveLastCheck(now);

      expect(await state.handledHash(), _apkSha1);
      expect(await state.lastCheck(), now);
    });

    test('核对上次发起的安装结果', () async {
      SharedPreferences.setMockInitialValues({});
      final state = UpdateState();

      expect(await state.hasInstalling(), isFalse);
      await state.markInstalling('new' * 13 + 'a');
      expect(await state.hasInstalling(), isTrue);

      // 装完后的包校验值仍是旧的(用户取消了安装器): 不记为已处理, 下次仍提示.
      await state.resolveInstalling('old' * 13 + 'a');
      expect(await state.hasInstalling(), isFalse);
      expect(await state.handledHash(), '');

      // 装成后已安装包就是待核对的那份: 记为已处理, 不再重复提示.
      await state.markInstalling(_apkSha1);
      await state.resolveInstalling(_apkSha1);
      expect(await state.hasInstalling(), isFalse);
      expect(await state.handledHash(), _apkSha1);
    });
  });

  group('format', () {
    test('字节数与速度', () {
      expect(formatBytes(0), '0 B');
      expect(formatBytes(1023), '1023 B');
      expect(formatBytes(1536), '1.5 KB');
      expect(formatBytes(133496348), '127 MB');
      expect(formatSpeed(0), '');
      expect(formatSpeed(1258291), '1.2 MB/s');
    });
  });
}
