import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:aproxy/engine/forwarder.dart';
import 'package:aproxy/engine/logging.dart';
import 'package:aproxy/engine/upstream.dart';

/// 模拟转发器 + 上游 SOCKS5 代理:
/// 转发器帧 (int32 len + host:port -> int32(1) ack),
/// 随后按 SOCKS5 完成 greeting 与 CONNECT 应答。
class _MockProxy {
  _MockProxy() {
    _server = ServerSocket.bind('127.0.0.1', 0, v6Only: false);
  }

  late final Future<ServerSocket> _server;
  final List<Socket> _conns = [];
  int _connCount = 0;
  int get connCount => _connCount;
  Completer<void>? _accepted;

  Future<int> get port async => (await _server).port;

  void startAccepting() {
    _server.then((ss) {
      ss.listen((s) async {
        _connCount++;
        _conns.add(s);
        _accepted?.complete();
        try {
          await _handle(s);
        } catch (_) {
        } finally {
          try {
            s.close();
          } catch (_) {}
        }
      });
    });
  }

  Future<void> _handle(Socket s) async {
    final r = _ConnReader(s);
    final len = await r.readInt();
    if (len <= 0 || len > 512) return;
    await r.read(len); // host:port
    s.add(_i32(1)); // ack
    // SOCKS5 greeting (2 字节) -> 接受无认证.
    await r.read(2);
    s.add([0x05, 0x00]);
    // SOCKS5 CONNECT -> 成功应答.
    final head = await r.read(4);
    if (head.length < 4) return;
    final atyp = head[3];
    var extra = 2;
    if (atyp == 0x01) {
      extra += 4;
    } else if (atyp == 0x04) {
      extra += 16;
    } else if (atyp == 0x03) {
      final l = (await r.read(1))[0];
      extra += l + 1;
    }
    await r.read(extra);
    s.add([0x05, 0x00, 0x00, 0x01, 127, 0, 0, 1, 0, 80]);
  }

  Future<void> close() async {
    for (final s in _conns) {
      try {
        s.destroy();
      } catch (_) {}
    }
    try {
      await _server.then((ss) => ss.close());
    } catch (_) {}
  }
}

class _ConnReader {
  _ConnReader(this._s) {
    _s.listen(_onData, onDone: _onDone, onError: (_) => _onDone());
  }

  final Socket _s;
  final List<int> _buf = [];
  final List<Completer<void>> _waiters = [];
  bool _done = false;

  void _onData(List<int> d) {
    _buf.addAll(d);
    _wake();
  }

  void _onDone() {
    _done = true;
    _wake();
  }

  void _wake() {
    for (final w in _waiters) {
      if (!w.isCompleted) w.complete();
    }
    _waiters.clear();
  }

  Future<void> _wait() {
    final c = Completer<void>();
    _waiters.add(c);
    return c.future;
  }

  Future<Uint8List> read(int n) async {
    while (_buf.length < n) {
      if (_done) throw StateError('连接被关闭');
      await _wait();
    }
    final d = Uint8List.fromList(_buf.sublist(0, n));
    _buf.removeRange(0, n);
    return d;
  }

  Future<int> readInt() async =>
      ByteData.sublistView(await read(4)).getInt32(0, Endian.big);
}

Uint8List _i32(int v) {
  final b = ByteData(4);
  b.setInt32(0, v, Endian.big);
  return b.buffer.asUint8List();
}

/// 轮询引擎日志, 直到 [needle] 出现或超时.
Future<bool> _waitLog(EngineLog log, String needle, Duration timeout) async {
  final lines = <String>[];
  final deadline = DateTime.now().add(timeout);
  while (DateTime.now().isBefore(deadline)) {
    final d = log.drain();
    for (final l in d.lines) {
      lines.add(l['message'] as String? ?? '');
    }
    if (lines.any((l) => l.contains(needle))) return true;
    await Future<void>.delayed(const Duration(milliseconds: 100));
  }
  return lines.any((l) => l.contains(needle));
}

void main() {
  test('连接池: 预建 SOCKS5 连接, 代理需求复用, 取走后补充', () async {
    final mock = _MockProxy();
    addTearDown(mock.close);
    mock.startAccepting();
    final port = await mock.port;

    final log = EngineLog();
    final client = UpstreamClient(
      forwarder: Forwarder(forwardPort: port, log: log),
      proxyPass: 'socks5://127.0.0.1:$port',
      poolSize: 3,
      disableCheckCert: true,
      log: log,
    );
    addTearDown(client.close);

    client.startPool();
    expect(await _waitLog(log, '预建完成 3/3', const Duration(seconds: 8)), isTrue,
        reason: '池应预建满 poolSize 条连接');
    expect(mock.connCount, 3);

    // 取用: 应命中池 (不再新建), 复用连接完成 CONNECT.
    final c1 = await client.connect('example.com', 443, useProxy: true);
    addTearDown(() => c1.channel.close());
    final c2 = await client.connect('example.com', 443, useProxy: true);
    addTearDown(() => c2.channel.close());
    expect(await _waitLog(log, '复用预建连接', const Duration(seconds: 2)), isTrue,
        reason: '代理需求应复用池中连接');

    // 取走后立即补充: 连接数应超过初始预建数.
    final deadline = DateTime.now().add(const Duration(seconds: 8));
    while (mock.connCount < 5 && DateTime.now().isBefore(deadline)) {
      await Future<void>.delayed(const Duration(milliseconds: 100));
    }
    expect(mock.connCount, greaterThanOrEqualTo(5));
  });

  test('连接池: poolSize=0 禁用, 每次新建', () async {
    final mock = _MockProxy();
    addTearDown(mock.close);
    mock.startAccepting();
    final port = await mock.port;

    final log = EngineLog();
    final client = UpstreamClient(
      forwarder: Forwarder(forwardPort: port, log: log),
      proxyPass: 'socks5://127.0.0.1:$port',
      poolSize: 0,
      disableCheckCert: true,
      log: log,
    );
    addTearDown(client.close);

    client.startPool();
    await Future<void>.delayed(const Duration(milliseconds: 300));
    expect(mock.connCount, 0, reason: '禁用连接池时不应预建');

    final c = await client.connect('example.com', 443, useProxy: true);
    addTearDown(() => c.channel.close());
    expect(mock.connCount, 1, reason: '连接需求应新建');
  });
}
