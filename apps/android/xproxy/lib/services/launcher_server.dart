import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter/foundation.dart';

import 'cn_ip_list.dart';
import 'vpn_channel.dart';

/// 本地 JSON-RPC over WebSocket 控制服务端.
///
/// 作为 libproxy launcher 的控制端: proxy 启动后会主动连接
/// ws://127.0.0.1:port, 注册实例并持续上报 status/log;
/// 本端可向其发起 get_status / set_config / shutdown 等 RPC 请求,
/// 并响应 proxy 的 protect 请求 (放行对外 socket).
class LauncherServer {
  HttpServer? _server;
  WebSocket? _socket;
  int _nextId = 1;
  final Map<int, Completer<Map<String, dynamic>>> _pending = {};
  // 流控制器随 start() 创建; close() 后实例不可再用, 需新建.
  late StreamController<Map<String, dynamic>> _statusCtrl;
  late StreamController<Map<String, dynamic>> _logCtrl;
  late StreamController<Map<String, dynamic>> _registerCtrl;
  late StreamController<bool> _connCtrl;
  bool _started = false;
  bool _closed = false;

  /// 监听端口 (start 后有效).
  int get port => _server?.port ?? 0;

  /// 启动时的 VpnConfig.toJson 快照 (建立 tun 用).
  Map<String, dynamic>? _vpnConfig;

  /// 设置 VPN 配置快照, 连接建立后据此建立 tun (地址/路由/DNS/会话名).
  void setVpnConfig(Map<String, dynamic> config) => _vpnConfig = config;

  /// 是否已连接 (proxy 控制通道在线).
  bool get connected => _socket != null;

  /// proxy 上报的状态 (status 通知).
  Stream<Map<String, dynamic>> get statusStream => _statusCtrl.stream;

  /// proxy 上报的日志 (log 通知).
  Stream<Map<String, dynamic>> get logStream => _logCtrl.stream;

  /// proxy 实例注册信息 (register 通知).
  Stream<Map<String, dynamic>> get registerStream => _registerCtrl.stream;

  /// 连接状态变化 (true=已连接).
  Stream<bool> get connectionStream => _connCtrl.stream;

  /// 启动本地控制端. [port] 用于进程重启后恢复原端口以便 proxy 重连;
  /// 绑定失败时抛出异常 (调用方决定是否回退).
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
    debugPrint('launcher ws request: ${request.uri} headers=${request.headers}');
    if (WebSocketTransformer.isUpgradeRequest(request)) {
      try {
        final ws = await WebSocketTransformer.upgrade(request);
        debugPrint('launcher ws upgraded');
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

  void _replyError(Object? id, int code, String message) {
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

  /// 回复 proxy 的请求.
  void _reply(Object? id, Map<String, dynamic> result) {
    final ws = _socket;
    if (ws == null) return;
    try {
      ws.add(
        utf8.encode(
          jsonEncode({
            'jsonrpc': '2.0',
            'id': id,
            'result': result,
          }),
        ),
      );
    } catch (_) {}
  }

  /// 处理 proxy -> app 请求.
  Future<void> _handleWsRequest(
    String method,
    Map<String, dynamic> params,
    Object? id,
  ) async {
    try {
      switch (method) {
        case 'protect':
          // 放行 libproxy 对外 socket, 避免流量回环进 tun.
          final fd = (params['fd'] as num?)?.toInt() ?? -1;
          _reply(id, {'ok': await VpnChannel.protect(fd)});
        default:
          _replyError(id, -32601, 'method not found');
      }
    } catch (e) {
      _replyError(id, -32602, e.toString());
    }
  }

  // 是否已建立 tun 并注入 (仅首次连接时建立, 重连不重复注入).
  bool _tunEstablished = false;

  /// 重建 VPN (restart) 后清除 tun 注入标记: 新 proxy 实例重新连接时
  /// 需要按其最新配置重建 VpnService tun 并注入, 否则新实例无 tun 无法转发.
  void resetTunState() {
    _tunEstablished = false;
    _logLocal('TUN 状态已重置, 等待新实例建立');
  }

  /// 控制通道连接建立后: 以用户配置的地址建立 VpnService tun, 再注入
  /// libproxy (经 set_tun_fd). tun2socks 无需服务端分配地址, 直接使用
  /// VpnConfig 中的 tunAddress/dns 配置.
  Future<void> _handleEstablishTun() async {
    if (_tunEstablished) return;
    final cfg = _vpnConfig ?? const <String, dynamic>{};
    final (address, prefix) = _tunAddress(cfg);
    final mtu = (cfg['tunMtu'] as num?)?.toInt() ?? 1500;
    List<String> routes;
    if (cfg['bypassCn'] == true) {
      // 绕过中国大陆: 仅非中国段接入 VPN, 中国段走系统物理网络直连.
      final cn = await CnIpList.update();
      // 路由计算在独立 isolate 执行, 避免大量 CIDR 区间运算阻塞 UI.
      routes = await compute(CnIpList.vpnRoutes, cn);
    } else {
      routes = const [];
    }
    // 未配置路由时默认全隧道 (IPv4 + IPv6).
    if (routes.isEmpty) routes = ['0.0.0.0/0', '::/0'];
    // 固定注入 8.8.8.8/1.1.1.1 保证所有 DNS 查询进入 TUN,
    // 进入 TUN 后由 native 按 qname 分流转发.
    final dns = <String>['8.8.8.8', '1.1.1.1'];
    try {
      final fd = await VpnChannel.establishTun(
        address: address,
        prefix: prefix,
        mtu: mtu,
        routes: routes,
        dns: dns,
        session: cfg['name'] as String? ?? 'proxy',
      );
      var injected = false;
      try {
        final result = await call('set_tun_fd', {'fd': fd});
        if (result['ok'] == true) {
          injected = true;
          _tunEstablished = true;
        } else {
          _logLocal('set_tun_fd 失败: ${result['error']}');
        }
      } catch (e) {
        debugPrint('launcher set_tun_fd failed: $e');
        _logLocal('注入 TUN 失败: $e');
      } finally {
        // 注入失败/被停止流程中断时 fd 未被 native 接管, 必须关闭,
        // 否则 VpnService tun 设备残留.
        if (!injected) {
          try {
            await VpnChannel.closeTunFd(fd);
          } catch (_) {}
        }
      }
    } catch (e) {
      debugPrint('launcher establishTun failed: $e');
      _logLocal('建立 TUN 失败: $e');
    }
  }

  /// 从配置取 tun 地址与前缀, 未配置时退回默认 10.0.0.2/24.
  (String, int) _tunAddress(Map<String, dynamic> cfg) {
    final address = cfg['tunAddress'] as String? ?? '';
    final prefix = (cfg['tunPrefix'] as num?)?.toInt() ?? 0;
    if (address.trim().isNotEmpty && prefix > 0) {
      return (address.trim(), prefix);
    }
    return ('10.0.0.2', 24);
  }

  void _logLocal(String message) {
    try {
      _logCtrl.add({
        'lines': [
          {
            'time': DateTime.now().millisecondsSinceEpoch,
            'level': 3,
            'message': message,
          },
        ],
      });
    } catch (_) {}
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
        // proxy -> app 请求 (如 protect), 异步处理并回复.
        unawaited(_handleWsRequest(method, params, msg['id']));
        return;
      }
      // proxy -> app 通知.
      switch (method) {
        case 'status':
          _statusCtrl.add(params);
        case 'log':
          _logCtrl.add(params);
        case 'register':
          _registerCtrl.add(params);
          // proxy 连接建立后: 建立 VpnService tun 并注入 libproxy.
          unawaited(_handleEstablishTun());
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

  /// 向 avpn 发起一次 JSON-RPC 请求并等待响应.
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
    // 与原生端 ws binary(true) 对齐, 发送二进制帧.
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
      // 超时或完成后清理挂起表, 避免泄漏.
      _pending.remove(id);
    }
  }

  Future<void> close() async {
    // 幂等: endRun/启动失败清理等多条路径可能重复关闭, 直接返回.
    if (_closed) return;
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
