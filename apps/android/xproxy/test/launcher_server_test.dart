import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:xproxy/services/launcher_server.dart';

void main() {
  test('launcher server 处理 jsonrpc 通知与请求', () async {
    final server = LauncherServer();
    await server.start();
    final port = server.port;
    addTearDown(server.close);

    final connected = Completer<bool>();
    final connSub = server.connectionStream.listen((v) {
      if (!connected.isCompleted) connected.complete(v);
    });
    addTearDown(connSub.cancel);

    final ws = await WebSocket.connect('ws://127.0.0.1:$port/${server.token}');
    addTearDown(ws.close);

    // 消息队列: 单订阅 WebSocket 流只能监听一次.
    final queue = <dynamic>[];
    final waiters = <Completer<dynamic>>[];
    ws.listen((m) {
      if (waiters.isNotEmpty) {
        waiters.removeAt(0).complete(m);
      } else {
        queue.add(m);
      }
    });
    Future<Map<String, dynamic>> next() async {
      final raw =
          queue.isNotEmpty
              ? queue.removeAt(0)
              : await (() {
                final c = Completer<dynamic>();
                waiters.add(c);
                return c.future;
              })();
      final text = raw is List<int> ? utf8.decode(raw) : raw as String;
      return jsonDecode(text) as Map<String, dynamic>;
    }

    expect(await connected.future, true, reason: '原生端应能连接');

    // 1. 原生端 -> app 通知 (status).
    final statusReceived = Completer<Map<String, dynamic>>();
    final statusSub = server.statusStream.listen(statusReceived.complete);
    addTearDown(statusSub.cancel);
    ws.add(
      utf8.encode(
        jsonEncode({
          'jsonrpc': '2.0',
          'method': 'status',
          'params': {'mode': 'client', 'active_connections': 1},
        }),
      ),
    );
    final status = await statusReceived.future;
    expect(status['mode'], 'client');
    expect(status['active_connections'], 1);

    // 2. app -> 原生端 RPC 请求 (二进制帧), 并处理响应.
    final callFuture = server.call('get_status', const {});
    final msg = await next();
    expect(msg['method'], 'get_status');
    expect(msg['jsonrpc'], '2.0');
    expect(msg['id'], isNotNull);

    ws.add(
      utf8.encode(
        jsonEncode({
          'jsonrpc': '2.0',
          'id': msg['id'],
          'result': {'ok': true, 'mode': 'client'},
        }),
      ),
    );
    final result = await callFuture;
    expect(result['ok'], true);
    expect(result['mode'], 'client');

    // 3. RPC 错误响应抛异常.
    final errFuture = server.call('shutdown', const {});
    final msg2 = await next();
    ws.add(
      utf8.encode(
        jsonEncode({
          'jsonrpc': '2.0',
          'id': msg2['id'],
          'error': {'code': -32601, 'message': 'method not found'},
        }),
      ),
    );
    await expectLater(errFuture, throwsA(isA<StateError>()));

    // 4. 原生端发来 JSON-RPC 请求 -> 应回 method not found 错误.
    ws.add(
      utf8.encode(
        jsonEncode({
          'jsonrpc': '2.0',
          'id': 42,
          'method': 'unknown',
          'params': <String, dynamic>{},
        }),
      ),
    );
    final reqReply = await next();
    expect(reqReply['id'], 42);
    expect(reqReply['error']['code'], -32601);

    // 5. 调用超时.
    final timedOut = server.call(
      'no_reply',
      const {},
      timeout: const Duration(milliseconds: 200),
    );
    await next(); // 消费请求帧, 不回响应.
    await expectLater(timedOut, throwsA(isA<TimeoutException>()));

    // 6. 连接断开通知.
    final disconnected = Completer<bool>();
    final dSub = server.connectionStream.listen((v) {
      if (!disconnected.isCompleted) disconnected.complete(v);
    }, onError: (_) {});
    addTearDown(dSub.cancel);
    await ws.close();
    expect(
      await disconnected.future.timeout(const Duration(seconds: 3)),
      false,
    );
  });

  test('未启动即关闭与重复关闭均安全', () async {
    final server = LauncherServer();
    await server.close();
    await server.close();
    // 关闭后不可再启动.
    await expectLater(server.start(), throwsA(isA<StateError>()));
  });

  test('关闭后挂起的调用立即失败', () async {
    final server = LauncherServer();
    await server.start();
    addTearDown(() async {
      try {
        await server.close();
      } catch (_) {}
    });
    final ws = await WebSocket.connect(
      'ws://127.0.0.1:${server.port}/${server.token}',
    );
    addTearDown(ws.close);
    await Future<void>.delayed(const Duration(milliseconds: 100));
    await server.close();
    await expectLater(server.call('get_status', const {}), throwsA(anything));
  });

  test('控制通道只接受带 token 路径的连接', () async {
    final server = LauncherServer();
    await server.start();
    addTearDown(server.close);

    expect(server.token, isNotEmpty);
    await expectLater(
      WebSocket.connect('ws://127.0.0.1:${server.port}'),
      throwsA(isA<WebSocketException>()),
    );
    await expectLater(
      WebSocket.connect('ws://127.0.0.1:${server.port}/wrong-token'),
      throwsA(isA<WebSocketException>()),
    );

    // 带正确 token 的连接可用.
    final ws = await WebSocket.connect(
      'ws://127.0.0.1:${server.port}/${server.token}',
    );
    await ws.close();
  });

  test('start 可复用指定 token (界面重建后 proxy 仍能重连)', () async {
    final server = LauncherServer();
    await server.start(token: 'abcdef0123456789');
    addTearDown(server.close);

    expect(server.token, 'abcdef0123456789');
    final ws = await WebSocket.connect(
      'ws://127.0.0.1:${server.port}/abcdef0123456789',
    );
    await ws.close();
  });
}
