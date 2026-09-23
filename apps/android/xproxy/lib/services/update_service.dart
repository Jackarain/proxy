import 'dart:async';
import 'dart:io';

import 'package:shared_preferences/shared_preferences.dart';

/// APK 分发地址 (站点上的正式签名包).
final Uri kUpdateApkUrl = Uri.parse(
  'https://www.jackarain.org/download/app-release.apk',
);

/// 启动后自动检查更新的最小间隔.
const Duration kUpdateCheckInterval = Duration(hours: 24);

/// 指纹取 APK 头部字节数: 覆盖 zip 首部若干 entry(含清单/代码), 足以区分不同构建.
const int kApkFingerprintBytes = 64 * 1024;

/// 更新检查/下载中可预期的失败 (网络异常、服务端异常状态等).
class UpdateException implements Exception {
  UpdateException(this.message);

  final String message;

  @override
  String toString() => message;
}

/// 下载被用户取消.
class UpdateCancelled implements Exception {
  @override
  String toString() => '已取消下载';
}

/// 远端安装包描述: 由头部探测得到, 不需要拉取完整内容.
class RemoteApk {
  const RemoteApk({required this.fingerprint, required this.size});

  /// 内容指纹, 见 [apkFingerprint].
  final String fingerprint;

  /// 完整包长度, 未知为 -1.
  final int size;
}

/// APK 内容指纹: 取头部 [kApkFingerprintBytes] 字节做 FNV-1a 摘要.
///
/// 站点未提供 `ETag`/`Last-Modified`, 只能以内容比对; 头部已包含 zip 目录项与
/// 清单/代码, 同一份包必然同值, 重新构建必然变值. 非加密用途, 只用于判重.
String apkFingerprint(List<int> head) {
  var hash = 0xcbf29ce484222325;
  for (final byte in head) {
    // int 固定 64 位, 乘法自然回绕; 末次掩码保证输出为无符号十六进制.
    hash = (hash ^ byte) * 0x100000001b3;
  }
  return (hash & 0x7FFFFFFFFFFFFFFF).toRadixString(16).padLeft(16, '0');
}

/// 读取本地 APK 头部计算指纹 (与远端探测同一算法), 用于判断已安装包是否就是远端那份.
///
/// 读不到时返回 null, 调用方退化为下载后再判定版本.
Future<String?> apkFileFingerprint(String path) async {
  RandomAccessFile? file;
  try {
    file = await File(path).open();
    final head = await file.read(kApkFingerprintBytes);
    if (head.isEmpty) return null;
    return apkFingerprint(head);
  } catch (_) {
    return null;
  } finally {
    try {
      await file?.close();
    } catch (_) {}
  }
}

/// 远端安装包的探测与下载.
class UpdateService {
  UpdateService({Uri? url, Duration? timeout})
    : url = url ?? kUpdateApkUrl,
      timeout = timeout ?? const Duration(seconds: 20);

  final Uri url;
  final Duration timeout;

  static const String _userAgent = 'xproxy-android';

  /// 探测远端安装包: 只拉取头部 [kApkFingerprintBytes] 字节算指纹, 用于判断远端
  /// 是否就是本机已处理过/已安装的那份, 不需要完整下载.
  Future<RemoteApk> probe() async {
    final client = HttpClient()..connectionTimeout = timeout;
    try {
      final req = await client.getUrl(url).timeout(timeout);
      req.headers.set(HttpHeaders.acceptEncodingHeader, 'identity');
      req.headers.set(HttpHeaders.userAgentHeader, _userAgent);
      req.headers.set(
        HttpHeaders.rangeHeader,
        'bytes=0-${kApkFingerprintBytes - 1}',
      );
      final resp = await req.close().timeout(timeout);
      final status = resp.statusCode;
      if (status != HttpStatus.partialContent && status != HttpStatus.ok) {
        await _discard(resp);
        throw UpdateException(
          status == HttpStatus.notFound
              ? '更新地址暂不可用 (HTTP 404), 请稍后再试'
              : '更新地址返回 HTTP $status',
        );
      }
      final size = _totalFrom(
        resp.headers.value(HttpHeaders.contentRangeHeader),
        resp.contentLength,
      );
      final head = await _readHead(resp);
      if (head.isEmpty) throw UpdateException('更新包内容为空');
      return RemoteApk(fingerprint: apkFingerprint(head), size: size);
    } on UpdateException {
      rethrow;
    } on TimeoutException {
      throw UpdateException('连接更新地址超时');
    } on SocketException catch (e) {
      throw UpdateException('网络错误: ${e.message}');
    } on HttpException catch (e) {
      throw UpdateException('网络错误: ${e.message}');
    } finally {
      client.close(force: true);
    }
  }

  static int _totalFrom(String? contentRange, int contentLength) {
    if (contentRange != null) {
      final slash = contentRange.lastIndexOf('/');
      if (slash >= 0) {
        final total = int.tryParse(contentRange.substring(slash + 1).trim());
        if (total != null && total > 0) return total;
      }
    }
    return contentLength;
  }

  /// 读取至多 [kApkFingerprintBytes] 字节即断开连接 (服务端忽略 Range 时避免拉全量).
  static Future<List<int>> _readHead(HttpClientResponse resp) async {
    final head = <int>[];
    await for (final chunk in resp) {
      final remain = kApkFingerprintBytes - head.length;
      if (chunk.length <= remain) {
        head.addAll(chunk);
      } else {
        head.addAll(chunk.sublist(0, remain));
      }
      if (head.length >= kApkFingerprintBytes) break;
    }
    return head;
  }

  /// 只消费首个数据块后断开连接 (主动中断产生的异常无需上报).
  static Future<void> _discard(HttpClientResponse resp) async {
    try {
      await resp.take(1).drain<void>();
    } catch (_) {}
  }
}

/// 一次可中断的下载任务.
class UpdateDownload {
  UpdateDownload({required this.url, required this.target, Duration? timeout})
    : timeout = timeout ?? const Duration(seconds: 20);

  final Uri url;
  final File target;
  final Duration timeout;

  HttpClient? _client;
  bool _cancelled = false;

  bool get cancelled => _cancelled;

  /// 下载到 [target], [onProgress] 按收到的数据块回调 (received, total).
  ///
  /// 返回写入的字节数; 被 [cancel] 中断时抛出 [UpdateCancelled],
  /// 其它失败抛出 [UpdateException], 两种情况下都不保留半成品文件.
  Future<int> start({
    void Function(int received, int total)? onProgress,
  }) async {
    final client = HttpClient()..connectionTimeout = timeout;
    _client = client;
    try {
      final req = await client.getUrl(url).timeout(timeout);
      req.headers.set(HttpHeaders.acceptEncodingHeader, 'identity');
      req.headers.set(HttpHeaders.userAgentHeader, UpdateService._userAgent);
      final resp = await req.close().timeout(timeout);
      if (resp.statusCode != HttpStatus.ok) {
        await UpdateService._discard(resp);
        throw UpdateException(
          resp.statusCode == HttpStatus.notFound
              ? '更新包不存在 (HTTP 404)'
              : '下载失败: HTTP ${resp.statusCode}',
        );
      }
      final total = resp.contentLength;
      final sink = target.openWrite();
      var received = 0;
      onProgress?.call(0, total);
      try {
        await for (final chunk in resp) {
          if (_cancelled) throw UpdateCancelled();
          sink.add(chunk);
          received += chunk.length;
          onProgress?.call(received, total);
        }
      } finally {
        await sink.close();
      }
      if (total > 0 && received != total) {
        throw UpdateException('下载不完整 ($received/$total 字节)');
      }
      return received;
    } catch (e) {
      await _removeTarget();
      if (_cancelled) throw UpdateCancelled();
      if (e is UpdateException) rethrow;
      throw UpdateException('下载失败: $e');
    } finally {
      _client = null;
      client.close(force: true);
    }
  }

  /// 中断下载: 关闭底层连接, 正在进行的 [start] 会抛出 [UpdateCancelled].
  void cancel() {
    if (_cancelled) return;
    _cancelled = true;
    _client?.close(force: true);
  }

  Future<void> _removeTarget() async {
    try {
      if (await target.exists()) await target.delete();
    } catch (_) {}
  }
}

/// 更新检查的持久化状态.
class UpdateState {
  static const String _fingerprintKey = 'xproxy_update_fingerprint';
  static const String _lastCheckKey = 'xproxy_update_last_check';

  /// 已发起安装但结果未知的更新 (`versionCode|fingerprint`).
  static const String _installingKey = 'xproxy_update_installing';

  /// 已处理的远端指纹 (已安装或用户选择跳过该版本).
  Future<String> fingerprint() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString(_fingerprintKey) ?? '';
  }

  Future<void> saveFingerprint(String value) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_fingerprintKey, value);
  }

  /// 是否已发起过安装但没有核对结果.
  Future<bool> hasInstalling() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.containsKey(_installingKey);
  }

  /// 记录已发起的安装: 安装会终止当前进程, 只能在下一次启动核对结果.
  Future<void> markInstalling(String fingerprint, int versionCode) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_installingKey, '$versionCode|$fingerprint');
  }

  /// 核对上次发起的安装: 版本已达到(或超过)说明装成功, 记为已处理;
  /// 否则视为用户取消/安装失败, 丢弃记录以便重新提示.
  Future<void> resolveInstalling(int currentVersionCode) async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(_installingKey);
    if (raw == null) return;
    final sep = raw.indexOf('|');
    if (sep > 0) {
      final versionCode = int.tryParse(raw.substring(0, sep)) ?? 0;
      if (versionCode > 0 && currentVersionCode >= versionCode) {
        await prefs.setString(_fingerprintKey, raw.substring(sep + 1));
      }
    }
    await prefs.remove(_installingKey);
  }

  Future<DateTime?> lastCheck() async {
    final prefs = await SharedPreferences.getInstance();
    final ms = prefs.getInt(_lastCheckKey);
    return ms == null ? null : DateTime.fromMillisecondsSinceEpoch(ms);
  }

  Future<void> saveLastCheck(DateTime time) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setInt(_lastCheckKey, time.millisecondsSinceEpoch);
  }
}

/// 人类可读的字节数, 如 `31.0 MB`.
String formatBytes(int bytes) {
  if (bytes <= 0) return '0 B';
  const units = ['B', 'KB', 'MB', 'GB'];
  var value = bytes.toDouble();
  var unit = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value /= 1024;
    unit++;
  }
  if (unit == 0) return '$bytes B';
  return '${value.toStringAsFixed(value >= 100 ? 0 : 1)} ${units[unit]}';
}

/// 人类可读的下载速度, 如 `1.2 MB/s`.
String formatSpeed(double bytesPerSecond) {
  if (bytesPerSecond <= 0) return '';
  return '${formatBytes(bytesPerSecond.round())}/s';
}
