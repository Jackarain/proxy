import 'dart:async';
import 'dart:io';

import 'package:shared_preferences/shared_preferences.dart';

/// Android release 包的分发地址 (CI 每次构建产出的 artifact).
final Uri kUpdateArtifactUrl = Uri.parse(
  'https://nightly.link/Jackarain/proxy/workflows/Build/master/'
  'proxy_server-android-release-apk.zip',
);

/// 启动后自动检查更新的最小间隔.
const Duration kUpdateCheckInterval = Duration(hours: 24);

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

/// 远端构建包描述: 由 1 字节探测得到, 不需要拉取完整内容.
class UpdateArtifact {
  const UpdateArtifact({required this.fingerprint, required this.size});

  /// 内容指纹: ETag 优先, 缺失时退化为 Last-Modified + 长度.
  final String fingerprint;

  /// 完整包长度, 未知为 -1.
  final int size;
}

/// 远端更新包的探测与下载.
class UpdateService {
  UpdateService({Uri? url, Duration? timeout})
    : url = url ?? kUpdateArtifactUrl,
      timeout = timeout ?? const Duration(seconds: 20);

  final Uri url;
  final Duration timeout;

  static const String _userAgent = 'xproxy-android';

  /// 探测远端构建包: 只请求第一个字节, 由响应头判断内容是否变化.
  ///
  /// 服务端若不支持 Range 会返回整个包, 此时读完响应头即断开连接,
  /// 避免为了比对指纹而拉取数十兆数据.
  Future<UpdateArtifact> probe() async {
    final client = HttpClient()..connectionTimeout = timeout;
    try {
      final req = await client.getUrl(url).timeout(timeout);
      req.headers.set(HttpHeaders.acceptEncodingHeader, 'identity');
      req.headers.set(HttpHeaders.userAgentHeader, _userAgent);
      req.headers.set(HttpHeaders.rangeHeader, 'bytes=0-0');
      final resp = await req.close().timeout(timeout);
      final status = resp.statusCode;
      final etag = resp.headers.value(HttpHeaders.etagHeader);
      final lastModified = resp.headers.value(HttpHeaders.lastModifiedHeader);
      final size = _totalFrom(
        resp.headers.value(HttpHeaders.contentRangeHeader),
        resp.contentLength,
      );
      await _discard(resp);
      if (status == HttpStatus.notFound) {
        throw UpdateException('更新服务暂时不可用 (HTTP 404), 请稍后再试');
      }
      if (status != HttpStatus.partialContent && status != HttpStatus.ok) {
        throw UpdateException('更新服务返回 HTTP $status');
      }
      final fingerprint =
          (etag != null && etag.isNotEmpty)
              ? etag
              : '${lastModified ?? ''}:$size';
      if (etag == null && lastModified == null) {
        throw UpdateException('更新服务未返回可比对的内容标识');
      }
      return UpdateArtifact(fingerprint: fingerprint, size: size);
    } on UpdateException {
      rethrow;
    } on TimeoutException {
      throw UpdateException('连接更新服务超时');
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
