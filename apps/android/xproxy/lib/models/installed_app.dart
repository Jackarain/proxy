/// 已安装应用 (按应用分流的可选项).
class InstalledApp {
  const InstalledApp({
    required this.packageName,
    required this.label,
    required this.system,
    required this.uid,
  });

  /// 应用包名.
  final String packageName;

  /// 桌面显示名称.
  final String label;

  /// 系统应用标记 (UI 默认折叠).
  final bool system;

  /// 应用 UID (VpnService 按 UID 过滤, 供排查用).
  final int uid;

  factory InstalledApp.fromMap(Map<Object?, Object?> map) => InstalledApp(
    packageName: map['package'] as String? ?? '',
    label: map['label'] as String? ?? '',
    system: map['system'] as bool? ?? false,
    uid: (map['uid'] as num?)?.toInt() ?? 0,
  );
}
