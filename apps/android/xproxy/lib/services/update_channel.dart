import 'package:flutter/services.dart';

/// APK 的版本与签名指纹.
class ApkInfo {
  const ApkInfo({
    required this.versionCode,
    required this.versionName,
    required this.signerSha256,
    this.sha1 = '',
  });

  final int versionCode;
  final String versionName;

  /// 签名证书 SHA-256 (小写十六进制, 空表示读取失败).
  final String signerSha256;

  /// 文件整包 SHA-1 (小写十六进制, 空表示未计算/读取失败).
  final String sha1;

  /// 显示用版本号, 如 `1.0.0 (1216)`.
  String get display =>
      versionName.isEmpty
          ? 'build $versionCode'
          : '$versionName (build $versionCode)';
}

/// 更新包安装相关的原生能力 (落地目录/版本与签名读取/调起系统安装器).
class UpdateChannel {
  static const MethodChannel _channel = MethodChannel(
    'com.jackarain.xproxy/update',
  );

  /// 更新包落地目录 (应用私有外部目录, 无需额外存储权限).
  static Future<String> updateDir() async {
    final dir = await _channel.invokeMethod<String>('update_dir');
    if (dir == null || dir.isEmpty) {
      throw StateError('无法获取更新包目录');
    }
    return dir;
  }

  /// 当前已安装 APK 的整包 SHA-1, 用于与发布目录里的校验值比对.
  static Future<String> installedApkHash() async {
    final hash = await _channel.invokeMethod<String>('installed_apk_hash');
    if (hash == null || hash.isEmpty) {
      throw StateError('无法读取已安装 APK 的校验值');
    }
    return hash.toLowerCase();
  }

  /// 当前已安装应用的版本与签名指纹.
  static Future<ApkInfo> currentVersion() async {
    final map = await _channel.invokeMethod<Map<dynamic, dynamic>>(
      'current_version',
    );
    return _parseInfo(map!);
  }

  /// 读取已下载 APK 的版本、签名指纹与整包 SHA-1 (不安装).
  static Future<ApkInfo> inspectApk(String apkPath) async {
    final map = await _channel.invokeMethod<Map<dynamic, dynamic>>(
      'inspect_apk',
      {'apk': apkPath},
    );
    return _parseInfo(map!);
  }

  /// 调起系统安装器.
  ///
  /// 返回 `started` 表示安装器已调起; `need_permission` 表示需要先授予
  /// 「安装未知应用」权限 (系统设置页已打开), 授权后可重试.
  static Future<String> installApk(String apkPath) async {
    final status = await _channel.invokeMethod<String>('install', {
      'apk': apkPath,
    });
    return status ?? '';
  }

  static ApkInfo _parseInfo(Map<dynamic, dynamic> map) {
    return ApkInfo(
      versionCode: (map['versionCode'] as num?)?.toInt() ?? 0,
      versionName: map['versionName'] as String? ?? '',
      signerSha256: (map['signerSha256'] as String? ?? '').toLowerCase(),
      sha1: (map['sha1'] as String? ?? '').toLowerCase(),
    );
  }
}
