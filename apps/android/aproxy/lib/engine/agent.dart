import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'logging.dart';
import 'tunnel_engine.dart';

/// 引擎控制通道代理 (作为 WS 客户端连接控制端 `ws://127.0.0.1:port`)。
///
/// wire 协议与 libproxy 对齐:
/// - 连接建立后上报 `register` 通知;
/// - 周期性上报 `status` 通知 (含全局/速率/会话明细);
/// - 日志经 `log` 通知上报;
/// - 处理控制端下发 RPC: set_config / get_status / shutdown / set_tun_fd。
class EngineAgent {
  EngineAgent({
    required this.launcherPort,
    required this.instanceId,
    required this.log,
    required this.tunnel,
    required this.version,
    required this.onShutdown,
    required this.onApplyConfig,
  });

  final int launcherPort;
  final String instanceId;
  final EngineLog log;
  final TunnelEngine tunnel;
  final String version;
  final void Function() onShutdown;
  final Future<void> Function(Map<String, dynamic> options) onApplyConfig;

  WebSocket? _ws;
  bool _stopped = false;
  Timer? _statusTimer;
  final int _startedAt = DateTime.now().millisecondsSinceEpoch;
  int _lastRx = 0;
  int _lastTx = 0;
  final Map<String, int> _lastConnRx = {};
  final Map<String, int> _lastConnTx = {};

  void Function()? onConnected;
  void Function()? onDisconnected;

  /// 连接控制端; 断开后自动重连 (数秒内).
  Future<void> connect() async {
    _stopped = false;
    while (!_stopped) {
      try {
        final ws = await WebSocket.connect('ws://127.0.0.1:$launcherPort');
        _ws = ws;
        _registerHandlers(ws);
        _sendRegister(ws);
        onConnected?.call();
        // 状态上报循环.
        _statusTimer = Timer.periodic(
          const Duration(seconds: 2),
          (_) {
            _sendStatus(ws);
            _sendLogs(ws);
          },
        );
        await ws.done;
        _statusTimer?.cancel();
        _statusTimer = null;
        onDisconnected?.call();
      } catch (e) {
        log.log('控制通道连接失败: $e, 稍后重连', level: 2);
      }
      if (_stopped) break;
      _closeWs();
      await Future<void>.delayed(const Duration(seconds: 2));
    }
  }

  void _registerHandlers(WebSocket ws) {
    ws.listen(_onMessage, onDone: () {}, onError: (_) {});
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
    if (!msg.containsKey('method')) return;
    final id = msg['id'];
    final method = msg['method'] as String? ?? '';
    final params = msg['params'] as Map<String, dynamic>? ?? const {};
    switch (method) {
      case 'set_config':
        _handleSetConfig(id, params);
      case 'get_status':
        _reply(id, snapshotStatus());
      case 'set_tun_fd':
        // TUN 由 Kotlin 在启动时建立, 无需注入 fd. 返回 ok.
        _reply(id, {'ok': true});
      case 'shutdown':
        _reply(id, {});
        Future.delayed(const Duration(milliseconds: 200), onShutdown);
    }
  }

  Future<void> _handleSetConfig(Object? id, Map<String, dynamic> params) async {
    try {
      final options = (params['options'] as Map<String, dynamic>?) ?? const {};
      await onApplyConfig(options);
      _reply(id, {
        'applied': true,
        'needs_restart': [],
        'errors': <String, dynamic>{},
      });
    } catch (e) {
      _reply(id, {
        'applied': false,
        'needs_restart': [],
        'errors': {'error': e.toString()},
      });
    }
  }

  void _sendRegister(WebSocket ws) {
    _notify(ws, 'register', {
      'instance_id': instanceId,
      'pid': pid(),
      'version': version,
      'started_at': _startedAt ~/ 1000,
    });
  }

  static int pid() {
    // 尽力获取进程 pid; 失败时返回 0.
    try {
      return int.parse(File('/proc/self/stat').readAsStringSync().split(' ')[0]);
    } catch (_) {
      return 0;
    }
  }

  void _sendStatus(WebSocket ws) => _notify(ws, 'status', snapshotStatus());

  /// 发送排队的日志 (与控制通道 log 通知结构一致).
  void _sendLogs(WebSocket ws) {
    final d = log.drain();
    if (!d.has) return;
    _notify(ws, 'log', {'lines': d.lines});
  }

  Map<String, dynamic> snapshotStatus() {
    final now = DateTime.now().millisecondsSinceEpoch;
    final uptime = (now - _startedAt) ~/ 1000;
    final s = tunnel.stats;
    final rxRate = s.rxBytes >= _lastRx ? (s.rxBytes - _lastRx) * 1000 ~/ 2000 : 0;
    final txRate = s.txBytes >= _lastTx ? (s.txBytes - _lastTx) * 1000 ~/ 2000 : 0;
    _lastRx = s.rxBytes;
    _lastTx = s.txBytes;

    final conns = <Map<String, dynamic>>[];
    for (final c in tunnel.sessionConnections) {
      final key = '${c.proto}:${c.clientIp}>${c.target}';
      final lastRx = _lastConnRx[key] ?? c.rxBytes;
      final lastTx = _lastConnTx[key] ?? c.txBytes;
      final rxRate = c.rxBytes >= lastRx ? (c.rxBytes - lastRx) * 1000 ~/ 2000 : 0;
      final txRate = c.txBytes >= lastTx ? (c.txBytes - lastTx) * 1000 ~/ 2000 : 0;
      _lastConnRx[key] = c.rxBytes;
      _lastConnTx[key] = c.txBytes;
      conns.add({
        'id': conns.length,
        'client_ip': c.clientIp,
        'target': c.target,
        'proto': c.proto,
        'elapsed': 0,
        'rx_bytes': c.rxBytes,
        'tx_bytes': c.txBytes,
        'rx_rate_bps': rxRate,
        'tx_rate_bps': txRate,
      });
    }
    final live = conns
        .map((c) => '${c['proto']}:${c['client_ip']}>${c['target']}')
        .toSet();
    _lastConnRx.removeWhere((k, _) => !live.contains(k));
    _lastConnTx.removeWhere((k, _) => !live.contains(k));

    return {
      'ts': now ~/ 1000,
      'uptime': uptime,
      'global': {
        'rx_bytes': s.rxBytes,
        'tx_bytes': s.txBytes,
      },
      'rates': {'rx_rate_bps': rxRate, 'tx_rate_bps': txRate},
      'active_connections': tunnel.sessionConnections.length,
      'conn_total': s.connTotal,
      'users': [
        {
          'name': '(匿名)',
          'connections': conns,
        },
      ],
    };
  }

  void _notify(WebSocket ws, String method, Map<String, dynamic> params) {
    final wsRef = _ws;
    if (wsRef == null) return;
    try {
      wsRef.add(
        utf8.encode(
          jsonEncode({
            'jsonrpc': '2.0',
            'method': method,
            'params': params,
          }),
        ),
      );
    } catch (_) {}
  }

  void _reply(Object? id, Map<String, dynamic> result) {
    final ws = _ws;
    if (ws == null) return;
    try {
      ws.add(
        utf8.encode(
          jsonEncode({'jsonrpc': '2.0', 'id': id, 'result': result}),
        ),
      );
    } catch (_) {}
  }

  void _closeWs() {
    try {
      _ws?.close();
    } catch (_) {}
    _ws = null;
    _statusTimer?.cancel();
    _statusTimer = null;
  }

  void stop() {
    _stopped = true;
    _closeWs();
  }
}
