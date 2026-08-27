import 'dart:async';
import 'dart:convert';
import 'dart:io';

/// 本地 JSON-RPC over WebSocket 控制服务端.
///
/// 作为纯 Dart 代理引擎的控制端: 引擎启动后主动连接 ws://127.0.0.1:port,
/// 注册实例并持续上报 status/log; Flutter 端可向其发起 get_status /
/// set_config / shutdown 等 RPC, 引擎处理并回复.
class LauncherServer {
  HttpServer? _server;
  WebSocket? _socket;
  int _nextId = 1;
  final Map<int, Completer<Map<String, dynamic>>> _pending = {};
  late StreamController<Map<String, dynamic>> _statusCtrl;
  late StreamController<Map<String, dynamic>> _logCtrl;
  late StreamController<Map<String, dynamic>> _registerCtrl;
  late StreamController<bool> _connCtrl;
  bool _started = false;
  bool _closed = false;

  /// 监听端口 (start 后有效).
  int get port => _server?.port ?? 0;

  /// 是否已连接 (引擎控制通道在线).
  bool get connected => _socket != null;

  Stream<Map<String, dynamic>> get statusStream => _statusCtrl.stream;
  Stream<Map<String, dynamic>> get logStream => _logCtrl.stream;
  Stream<Map<String, dynamic>> get registerStream => _registerCtrl.stream;
  Stream<bool> get connectionStream => _connCtrl.stream;

  /// 启动本地控制端. [port] 用于进程重启后恢复原端口以便引擎重连.
  Future<void> start({int? port}) async {
    if (_closed) throw StateError('launcher 已关闭, 请创建新实例');
    if (_server != null) throw StateError('launcher 已启动');
    _statusCtrl = StreamController<Map<String, dynamic>>.broadcast();
    _logCtrl = StreamController<Map<String, dynamic>>.broadcast();
    _registerCtrl = StreamController<Map<String, dynamic>>.broadcast();
    _connCtrl = StreamController<bool>.broadcast();
    _server = await HttpServer.bind(InternetAddress.loopbackIPv4, port ?? 0);
    _server!.listen(_handleRequest, onError: (_) {});
    _started = true;
  }

  Future<void> _handleRequest(HttpRequest request) async {
    if (WebSocketTransformer.isUpgradeRequest(request)) {
      try {
        final ws = await WebSocketTransformer.upgrade(request);
        _socket?.close();
        _socket = ws;
        _connCtrl.add(true);
        ws.listen(
          _onMessage,
          onDone: () {
            if (identical(_socket, ws)) {
              _socket = null;
              _connCtrl.add(false);
            }
          },
          onError: (_) {
            if (identical(_socket, ws)) {
              _socket = null;
              _connCtrl.add(false);
            }
          },
          cancelOnError: true,
        );
      } catch (_) {
        // 升级失败, 忽略.
      }
    } else {
      request.response.statusCode = HttpStatus.notFound;
      await request.response.close();
    }
  }

  void _onMessage(dynamic data) {
    String text;
    if (data is List<int>) {
      text = utf8.decode(data);
    } else if (data is String) {
      text = data;
    } else {
      return;
    }

    Map<String, dynamic> msg;
    try {
      msg = jsonDecode(text) as Map<String, dynamic>;
    } catch (_) {
      return;
    }

    if (msg.containsKey('method')) {
      final method = msg['method'] as String? ?? '';
      final params = msg['params'] as Map<String, dynamic>? ?? const {};
      if (msg.containsKey('id')) {
        // 引擎 -> app 请求; 仅支持 protect, 其余回 method not found.
        _replyWsError(msg['id'], -32601, 'method not found');
        return;
      }
      // 引擎 -> app 通知.
      switch (method) {
        case 'status':
          _statusCtrl.add(params);
        case 'log':
          _logCtrl.add(params);
        case 'register':
          _registerCtrl.add(params);
      }
    } else if (msg.containsKey('id')) {
      // 本端 RPC 请求的响应.
      final id = (msg['id'] as num?)?.toInt();
      if (id == null) return;
      final completer = _pending.remove(id);
      if (completer == null) return;
      if (msg.containsKey('result')) {
        completer.complete(msg['result'] as Map<String, dynamic>? ?? const {});
      } else {
        completer.completeError(
          StateError('rpc error: ${jsonEncode(msg['error'])}'),
        );
      }
    }
  }

  /// 回复引擎发来的请求 (失败统一回 method not found).
  void _replyWsError(Object? id, int code, String message) {
    final ws = _socket;
    if (ws == null) return;
    try {
      ws.add(
        utf8.encode(
          jsonEncode({
            'jsonrpc': '2.0',
            'id': id,
            'error': {'code': code, 'message': message},
          }),
        ),
      );
    } catch (_) {}
  }

  /// 向引擎发起一次 JSON-RPC 请求并等待响应.
  Future<Map<String, dynamic>> call(
    String method,
    Map<String, dynamic> params, {
    Duration timeout = const Duration(seconds: 10),
  }) async {
    final ws = _socket;
    if (ws == null) throw StateError('launcher 未连接');
    final id = _nextId++;
    final completer = Completer<Map<String, dynamic>>();
    _pending[id] = completer;
    ws.add(
      utf8.encode(
        jsonEncode({
          'jsonrpc': '2.0',
          'id': id,
          'method': method,
          'params': params,
        }),
      ),
    );
    try {
      return await completer.future.timeout(timeout);
    } finally {
      _pending.remove(id);
    }
  }

  Future<void> close() async {
    _closed = true;
    for (final c in _pending.values) {
      if (!c.isCompleted) {
        c.completeError(StateError('launcher closed'));
      }
    }
    _pending.clear();
    try {
      await _socket?.close();
    } catch (_) {}
    _socket = null;
    final server = _server;
    _server = null;
    if (server != null) {
      try {
        await server.close(force: true);
      } catch (_) {}
    }
    if (!_started) return;
    await _statusCtrl.close();
    await _logCtrl.close();
    await _registerCtrl.close();
    await _connCtrl.close();
  }
}
