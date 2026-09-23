import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:shared_preferences/shared_preferences.dart';

/// 发布目录的列表地址: `hash=1` 时每项带上整文件 SHA-1.
final Uri kUpdateIndexUrl = Uri.parse(
  'https://www.jackarain.org/download/?q=json&hash=1',
);

/// 安装包下载地址.
final Uri kUpdateApkUrl = Uri.parse(
  'https://www.jackarain.org/download/app-release.apk',
);

/// 发布目录里安装包的文件名.
const String kUpdateApkName = 'app-release.apk';

/// 启动后自动检查更新的最小间隔.
const Duration kUpdateCheckInterval = Duration(hours: 24);

/// 列表响应允许的最大长度, 避免异常响应撑爆内存.
const int kUpdateIndexMaxBytes = 256 * 1024;

/// 更新检查/下载中可预期的失败 (网络异常、服务端异常状态、响应格式不符等).
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

/// 发布目录里的安装包信息.
class RemoteApk {
  const RemoteApk({
    required this.hash,
    required this.size,
    this.lastWriteTime = '',
  });

  /// 整文件 SHA-1 (小写十六进制).
  final String hash;

  /// 文件长度, 未知为 -1.
  final int size;

  /// 服务器上的最后写入时间, 仅用于展示.
  final String lastWriteTime;
}

/// 远端发布信息的查询与安装包下载.
class UpdateService {
  UpdateService({Uri? indexUrl, Uri? apkUrl, Duration? timeout})
    : indexUrl = indexUrl ?? kUpdateIndexUrl,
      apkUrl = apkUrl ?? kUpdateApkUrl,
      timeout = timeout ?? const Duration(seconds: 20);

  final Uri indexUrl;
  final Uri apkUrl;
  final Duration timeout;

  static const String _userAgent = 'xproxy-android';

  /// 查询发布目录: 取 [kUpdateApkName] 的 SHA-1 与大小.
  ///
  /// 判断有无更新只依赖这个 SHA-1 与本地已安装包的 SHA-1 是否相同,
  /// 因此不需要下载安装包本身.
  Future<RemoteApk> fetch() async {
    final client = HttpClient()..connectionTimeout = timeout;
    try {
      final req = await client.getUrl(indexUrl).timeout(timeout);
      req.headers.set(HttpHeaders.acceptEncodingHeader, 'identity');
      req.headers.set(HttpHeaders.userAgentHeader, _userAgent);
      final resp = await req.close().timeout(timeout);
      if (resp.statusCode != HttpStatus.ok) {
        await _discard(resp);
        throw UpdateException(
          resp.statusCode == HttpStatus.notFound
              ? '更新列表不可用 (HTTP 404), 请稍后再试'
              : '更新列表返回 HTTP ${resp.statusCode}',
        );
      }
      final body = await _readBody(resp, kUpdateIndexMaxBytes);
      return _parseIndex(utf8.decode(body, allowMalformed: true));
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

  static RemoteApk _parseIndex(String body) {
    Object? decoded;
    try {
      decoded = jsonDecode(body);
    } on FormatException {
      throw UpdateException('更新列表不是合法的 JSON');
    }
    if (decoded is! List) throw UpdateException('更新列表格式不符合预期');
    for (final item in decoded) {
      if (item is! Map) continue;
      if (item['filename'] != kUpdateApkName) continue;
      final hash = (item['hash'] as String? ?? '').trim().toLowerCase();
      if (!_isSha1(hash)) throw UpdateException('发布信息缺少有效的 SHA-1 校验值');
      return RemoteApk(
        hash: hash,
        size: (item['filesize'] as num?)?.toInt() ?? -1,
        lastWriteTime: item['last_write_time'] as String? ?? '',
      );
    }
    throw UpdateException('更新列表里没有 $kUpdateApkName');
  }

  static final RegExp _sha1Pattern = RegExp(r'^[0-9a-f]{40}$');

  static bool _isSha1(String value) => _sha1Pattern.hasMatch(value);

  static Future<List<int>> _readBody(HttpClientResponse resp, int max) async {
    final body = <int>[];
    await for (final chunk in resp) {
      body.addAll(chunk);
      if (body.length > max) throw UpdateException('更新列表响应过大');
    }
    return body;
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
              ? '安装包不存在 (HTTP 404)'
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
  static const String _handledHashKey = 'xproxy_update_apk_hash';
  static const String _lastCheckKey = 'xproxy_update_last_check';

  /// 已发起安装但结果未知的安装包 SHA-1.
  static const String _installingKey = 'xproxy_update_installing';

  /// 已处理过的远端 SHA-1 (已安装或用户选择跳过该版本).
  Future<String> handledHash() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString(_handledHashKey) ?? '';
  }

  Future<void> saveHandledHash(String value) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_handledHashKey, value);
  }

  /// 是否已发起过安装但没有核对结果.
  Future<bool> hasInstalling() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.containsKey(_installingKey);
  }

  /// 记录已发起的安装: 安装会终止当前进程, 只能在下一次启动核对结果.
  Future<void> markInstalling(String hash) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_installingKey, hash);
  }

  /// 核对上次发起的安装: 已安装包的 SHA-1 与记录相同说明装成功, 记为已处理;
  /// 否则视为用户取消/安装失败, 丢弃记录以便重新提示.
  Future<void> resolveInstalling(String installedHash) async {
    final prefs = await SharedPreferences.getInstance();
    final pending = prefs.getString(_installingKey);
    if (pending == null) return;
    if (pending.isNotEmpty && pending == installedHash) {
      await prefs.setString(_handledHashKey, pending);
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

/// 人类可读的字节数, 如 `127.3 MB`.
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
