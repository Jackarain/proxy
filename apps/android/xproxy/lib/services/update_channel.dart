import 'package:flutter/services.dart';

/// APK 的版本与签名指纹.
class ApkInfo {
  const ApkInfo({
    required this.versionCode,
    required this.versionName,
    required this.signerSha256,
  });

  final int versionCode;
  final String versionName;

  /// 签名证书 SHA-256 (小写十六进制, 空表示读取失败).
  final String signerSha256;

  /// 显示用版本号, 如 `1.0.0 (1216)`.
  String get display =>
      versionName.isEmpty
          ? 'build $versionCode'
          : '$versionName (build $versionCode)';
}

/// 解压后的更新包: 其中 APK 的落地路径与版本信息.
class UpdateArchive {
  const UpdateArchive({required this.apkPath, required this.info});

  final String apkPath;
  final ApkInfo info;
}

/// 更新包安装相关的原生能力 (落地目录/解压校验/调起系统安装器).
class UpdateChannel {
  static const MethodChannel _channel = MethodChannel(
    'com.jackarain.xproxy/update',
  );

  /// 更新包落地目录 (应用私有外部目录, 无需额外存储权限).
  static Future<String> downloadDir() async {
    final dir = await _channel.invokeMethod<String>('download_dir');
    if (dir == null || dir.isEmpty) {
      throw StateError('无法获取更新包目录');
    }
    return dir;
  }

  /// 当前已安装应用的版本与签名指纹.
  static Future<ApkInfo> currentVersion() async {
    final map = await _channel.invokeMethod<Map<dynamic, dynamic>>(
      'current_version',
    );
    return _parseInfo(map!);
  }

  /// 解压更新包并读取其中 APK 的版本与签名指纹 (不安装).
  ///
  /// 解压成功后压缩包即被删除: 后续安装直接使用解压出的 APK.
  static Future<UpdateArchive> inspectArchive(String zipPath) async {
    final map = await _channel.invokeMethod<Map<dynamic, dynamic>>(
      'inspect_zip',
      {'zip': zipPath},
    );
    final data = Map<String, dynamic>.from(map!);
    return UpdateArchive(
      apkPath: data['apkPath'] as String? ?? '',
      info: _parseInfo(data),
    );
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
    );
  }
}
