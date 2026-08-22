import 'dart:async';

import 'package:flutter/services.dart';

/// Flutter 与 Android 原生层 (VpnService/JNI 桥) 的通道.
class VpnChannel {
  static const MethodChannel _channel = MethodChannel(
    'com.jackarain.xproxy/vpn',
  );
  static const EventChannel _events = EventChannel(
    'com.jackarain.xproxy/events',
  );

  /// 请求 VPN 授权 (阻塞直到用户在系统弹窗中作出选择).
  static Future<bool> prepare() async {
    final ok = await _channel.invokeMethod<bool>('prepare');
    return ok ?? false;
  }

  /// 启动 VpnService 并调用 xproxy.start(configJson).
  static Future<void> start(String configJson, int launcherPort) async {
    final ok = await _channel.invokeMethod<bool>('start', {
      'config': configJson,
      'launcherPort': launcherPort,
    });
    if (ok != true) {
      throw StateError('native start failed');
    }
  }

  /// 停止 VpnService 并调用 xproxy.stop().
  static Future<void> stop() async {
    await _channel.invokeMethod('stop');
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

  /// 放行原生对外 socket (经控制通道 protect 请求触发).
  static Future<bool> protect(int fd) async {
    final ok = await _channel.invokeMethod<bool>('protect', {'fd': fd});
    return ok ?? false;
  }

  /// 以用户配置的地址建立 VpnService tun, 返回注入 libproxy 的 fd.
  static Future<int> establishTun({
    required String address,
    required int prefix,
    required int mtu,
    required List<String> routes,
    required List<String> dns,
    required String session,
  }) async {
    final fd = await _channel.invokeMethod<int>('establish_tun', {
      'address': address,
      'prefix': prefix,
      'mtu': mtu,
      'routes': routes,
      'dns': dns,
      'session': session,
    });
    if (fd == null || fd < 0) {
      throw StateError('establish_tun 失败');
    }
    return fd;
  }

  /// 原生事件: {"type":"log"|"vpn_state", ...}.
  static Stream<Map<String, dynamic>> events() {
    return _events.receiveBroadcastStream().map(
      (e) => Map<String, dynamic>.from(e as Map),
    );
  }
}
