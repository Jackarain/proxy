import 'package:flutter/services.dart';

/// Flutter 与 Android 原生层 (VpnService 数据面/受保护转发器) 的通道.
///
/// 引擎 (tun2socks/转发/DNS) 全部运行在纯 Dart 进程内; Kotlin 仅负责
/// VpnService TUN 建立、TUN 原始 IP 包搬运, 以及可 protect 的出站 socket.
class VpnChannel {
  static const MethodChannel _channel = MethodChannel(
    'com.jackarain.aproxy/vpn',
  );
  static const EventChannel _events = EventChannel(
    'com.jackarain.aproxy/events',
  );

  /// 请求 VPN 授权 (阻塞直到用户在系统弹窗中作出选择).
  static Future<bool> prepare() async {
    final ok = await _channel.invokeMethod<bool>('prepare');
    return ok ?? false;
  }

  /// 启动 VpnService: 建立 TUN (全隧道), 启动数据面桥接与受保护转发器.
  static Future<void> start(String configJson, int launcherPort) async {
    final ok = await _channel.invokeMethod<bool>('start', {
      'config': configJson,
      'launcherPort': launcherPort,
    });
    if (ok != true) {
      throw StateError('native start failed');
    }
  }

  /// 不停服务重建 VPN (TUN 参数变更): 原生端在单个任务内 停旧->启新.
  static Future<void> restart(String configJson, int launcherPort) async {
    final ok = await _channel.invokeMethod<bool>('restart', {
      'config': configJson,
      'launcherPort': launcherPort,
    });
    if (ok != true) {
      throw StateError('native restart failed');
    }
  }

  /// 停止 VpnService (同时引擎侧由调用方负责停止 DartEngine).
  static Future<void> stop() async {
    await _channel.invokeMethod('stop');
  }

  /// 查询 VpnService 是否真正在运行 (数据面已建立, 端口可用).
  static Future<bool> isServiceActive() async {
    final ok = await _channel.invokeMethod<bool>('is_active');
    return ok ?? false;
  }

  /// 等待并获取引擎连接所需的端口 (TUN 数据面桥接 / 受保护转发器)。
  static Future<EnginePorts> connectEngine() async {
    final m = await _channel.invokeMethod<Map<dynamic, dynamic>>(
      'connect_engine',
    ).timeout(const Duration(seconds: 20), onTimeout: () {
      throw StateError('等待 VpnService 数据面超时');
    });
    if (m == null) {
      throw StateError('connect_engine 失败');
    }
    return EnginePorts(
      tunBridgePort: (m['tunBridgePort'] as num?)?.toInt() ?? 0,
      forwardPort: (m['forwardPort'] as num?)?.toInt() ?? 0,
      address: m['address'] as String? ?? '',
    );
  }

  /// 返回 Dart 引擎版本标识 (替代原 C++ 编译常量).
  static Future<String> buildVersion() async {
    final v = await _channel.invokeMethod<String>('build_version');
    return v ?? '';
  }

  /// 原生事件: {"type":"log"|"vpn_state", ...}.
  static Stream<Map<String, dynamic>> events() {
    return _events.receiveBroadcastStream().map(
      (e) => Map<String, dynamic>.from(e as Map),
    );
  }
}

/// 引擎连接所需的原生端口信息.
class EnginePorts {
  const EnginePorts({
    required this.tunBridgePort,
    required this.forwardPort,
    required this.address,
  });

  final int tunBridgePort;
  final int forwardPort;
  final String address;
}
