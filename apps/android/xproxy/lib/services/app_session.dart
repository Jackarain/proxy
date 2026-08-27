import 'dart:convert';

import 'package:flutter/foundation.dart';

import 'launcher_server.dart';
import 'storage_service.dart';
import 'vpn_channel.dart';
import '../models/vpn_config.dart';

/// 全局运行状态 (单例).
class AppSession extends ChangeNotifier {
  AppSession._();
  static final AppSession instance = AppSession._();

  /// 当前运行的配置 id, 为空表示未运行.
  String? runningConfigId;

  /// 本地控制通道服务 (跨重启保持).
  LauncherServer? server;

  /// 连接状态: true=proxy 控制通道在线.
  bool connected = false;

  /// 启动时下发的完整配置 json (含 VpnService 专用字段), 用于
  /// set_config 时判断是否需要重建 TUN.
  String? startedConfigJson;

  bool get running => runningConfigId != null;

  void beginRun(String configId, {String? configJson}) {
    runningConfigId = configId;
    if (configJson != null) startedConfigJson = configJson;
    notifyListeners();
  }

  /// 停止当前运行: 停原生服务、清理持久化运行状态、关闭控制通道.
  Future<void> stopRun() async {
    try {
      await VpnChannel.stop();
    } finally {
      await StorageService().clearRunState();
      final server = this.server;
      this.server = null;
      try {
        await server?.close();
      } catch (_) {
        // 关闭控制通道失败不影响停止流程.
      }
      endRun();
    }
  }

  void endRun() {
    runningConfigId = null;
    connected = false;
    startedConfigJson = null;
    notifyListeners();
  }

  void setConnected(bool value) {
    if (connected != value) {
      connected = value;
      notifyListeners();
    }
  }

  /// 将配置应用到运行中的会话:
  /// - TUN 字段变更时整体重建 VPN (VpnService 重新 establish);
  /// - 其余参数经控制通道 set_config 热更新.
  /// 返回 'restarted' / 'updated'; 配置未在运行时返回 null.
  Future<String?> applyConfig(VpnConfig config) async {
    if (!running || runningConfigId != config.id) return null;
    final server = this.server;
    if (server == null) throw StateError('控制通道未就绪');

    if (_tunFieldsChanged(config)) {
      final fullJson = jsonEncode(config.toJson());
      // 更新 vpnConfig 快照并清除 tun 注入标记: 新实例连接后按最新
      // TUN 配置重建设备, 否则会沿用旧快照/跳过注入导致无法转发.
      server.setVpnConfig(config.toJson());
      server.resetTunState();
      await VpnChannel.restart(fullJson, server.port);
      beginRun(config.id, configJson: fullJson);
      await StorageService().saveRunState(config.id, server.port);
      return 'restarted';
    }

    final result = await server.call(
      'set_config',
      {'options': config.toProxyOptions()},
    );
    // proxy 的 set_config 返回 {applied, needs_restart, errors}.
    final errors = result['errors'] as Map<String, dynamic>? ?? const {};
    if (errors.isNotEmpty) {
      throw StateError('set_config 失败: ${jsonEncode(errors)}');
    }
    final needsRestart = result['needs_restart'] as List? ?? const [];
    return needsRestart.isNotEmpty ? 'restarted' : 'updated';
  }

  /// TUN 相关字段是否与启动时不同.
  bool _tunFieldsChanged(VpnConfig config) {
    final started = startedConfigJson;
    if (started == null || started.isEmpty) return false;
    try {
      final old = VpnConfig.fromJson(
        jsonDecode(started) as Map<String, dynamic>,
      );
      return old.tunAddress != config.tunAddress ||
          old.tunPrefix != config.tunPrefix ||
          old.dns.join(',') != config.dns.join(',') ||
          old.dnsForeign.join(',') != config.dnsForeign.join(',') ||
          old.dnsForeignDoh != config.dnsForeignDoh ||
          old.tunMtu != config.tunMtu;
    } catch (_) {
      return false;
    }
  }
}
