import 'dart:convert';

import 'package:shared_preferences/shared_preferences.dart';

import '../models/vpn_config.dart';

/// 配置持久化: 以 json 数组形式存于 SharedPreferences.
class StorageService {
  static const String configsKey = 'xproxy_configs_v1';

  /// 配置解析失败时另存的原始内容 (避免下一次保存把损坏数据彻底覆盖).
  static const String corruptKey = 'xproxy_configs_v1_corrupt';

  Future<List<VpnConfig>> loadConfigs() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(configsKey);
    if (raw == null || raw.isEmpty) return [];
    try {
      final list = jsonDecode(raw) as List<dynamic>;
      return list
          .whereType<Map<String, dynamic>>()
          .map(VpnConfig.fromJson)
          .toList();
    } catch (_) {
      // 解析失败时备份原始内容并保留旧备份: 界面据此提示用户, 数据仍可人工恢复.
      if ((prefs.getString(corruptKey) ?? '').isEmpty) {
        await prefs.setString(corruptKey, raw);
      }
      return [];
    }
  }

  /// 是否存在解析失败的配置备份 (供界面提示).
  Future<bool> hasCorruptBackup() async {
    final prefs = await SharedPreferences.getInstance();
    return (prefs.getString(corruptKey) ?? '').isNotEmpty;
  }

  Future<void> saveConfigs(List<VpnConfig> configs) async {
    final prefs = await SharedPreferences.getInstance();
    final raw = jsonEncode(configs.map((c) => c.toJson()).toList());
    await prefs.setString(configsKey, raw);
  }

  /// 正在运行的配置 (界面关闭/进程存活的场景下用于恢复控制通道).
  static const String runIdKey = 'xproxy_running_id';
  static const String runPortKey = 'xproxy_launcher_port';
  static const String runTokenKey = 'xproxy_launcher_token';

  Future<void> saveRunState(
    String configId,
    int launcherPort,
    String launcherToken,
  ) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(runIdKey, configId);
    await prefs.setInt(runPortKey, launcherPort);
    await prefs.setString(runTokenKey, launcherToken);
  }

  /// 恢复运行状态: (配置 id, 控制通道端口, 控制通道 token).
  ///
  /// token 必须与运行中的 proxy 使用的地址一致, 否则重连会被控制通道拒绝.
  Future<(String, int, String)?> loadRunState() async {
    final prefs = await SharedPreferences.getInstance();
    final id = prefs.getString(runIdKey);
    final port = prefs.getInt(runPortKey);
    final token = prefs.getString(runTokenKey) ?? '';
    if (id == null ||
        id.isEmpty ||
        port == null ||
        port <= 0 ||
        token.isEmpty) {
      return null;
    }
    return (id, port, token);
  }

  Future<void> clearRunState() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove(runIdKey);
    await prefs.remove(runPortKey);
    await prefs.remove(runTokenKey);
  }
}
