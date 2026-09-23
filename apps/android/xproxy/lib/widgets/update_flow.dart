import 'dart:async';
import 'dart:io';

import 'package:flutter/material.dart';

import '../services/app_session.dart';
import '../services/update_channel.dart';
import '../services/update_service.dart';

/// 是否有检查/更新流程正在进行 (自动与手动共用).
bool _running = false;

/// 启动后调用: 按 [kUpdateCheckInterval] 节流在后台检查更新, 失败静默.
Future<void> autoCheckUpdate(BuildContext context) async {
  try {
    final state = UpdateState();
    // 上次的安装结果只能在新进程里核对(安装会终止应用).
    await _resolveInstalling(state);
    final last = await state.lastCheck();
    if (last != null &&
        DateTime.now().difference(last) < kUpdateCheckInterval) {
      return;
    }
    await state.saveLastCheck(DateTime.now());
    if (!context.mounted) return;
    // VPN 运行中不自动检查: 此时应用自身流量也被 TUN 接管, 下载不受控.
    if (AppSession.instance.running) return;
    await _locked(
      context,
      manual: false,
      body: () => _checkAndPrompt(context, state, manual: false),
    );
  } catch (_) {
    // 后台检查失败不打扰用户, 下次启动重试.
  }
}

/// 用户主动触发的检查: 失败与「已是最新」都会给出提示.
Future<void> checkUpdateNow(BuildContext context) async {
  final state = UpdateState();
  try {
    await _resolveInstalling(state);
    await state.saveLastCheck(DateTime.now());
    if (!context.mounted) return;
    await _locked(
      context,
      manual: true,
      body: () => _checkAndPrompt(context, state, manual: true),
    );
  } catch (e) {
    if (context.mounted) _toast(context, '检查更新失败: $e');
  }
}

/// 远端包是否与本机已安装的是同一份: 是则记下指纹, 省掉整包下载.
Future<bool> _matchInstalledBuild(UpdateState state, String fingerprint) async {
  try {
    final path = await UpdateChannel.installedApkPath();
    if (await apkFileFingerprint(path) != fingerprint) return false;
    await state.saveFingerprint(fingerprint);
    return true;
  } catch (_) {
    // 读不到已安装包信息时退化为完整下载后再判定.
    return false;
  }
}

/// 核对上次发起的安装是否成功: 安装会终止应用, 结果只能等下次启动确认.
Future<void> _resolveInstalling(UpdateState state) async {
  if (!await state.hasInstalling()) return;
  try {
    final current = await UpdateChannel.currentVersion();
    await state.resolveInstalling(current.versionCode);
  } catch (_) {
    // 读不到本地版本时保留记录, 下次启动再核对.
  }
}

/// 串行化检查/更新流程: 自动检查与手动点击重叠时直接跳过, 避免弹出多个对话框.
Future<void> _locked(
  BuildContext context, {
  required bool manual,
  required Future<void> Function() body,
}) async {
  if (_running) {
    if (manual && context.mounted) _toast(context, '正在检查更新, 请稍候');
    return;
  }
  _running = true;
  try {
    await body();
  } finally {
    _running = false;
  }
}

/// 探测远端 -> 下载 -> 校验 -> 调起安装.
Future<void> _checkAndPrompt(
  BuildContext context,
  UpdateState state, {
  required bool manual,
}) async {
  final service = UpdateService();
  final probed = await service.probe();
  if (probed.fingerprint == await state.fingerprint()) {
    if (manual && context.mounted) _toast(context, '当前已是最新版本');
    return;
  }
  // 远端包与本机已安装的是同一份(如刚装上或首次检查): 不必下载即可确认是最新.
  if (await _matchInstalledBuild(state, probed.fingerprint)) {
    if (manual && context.mounted) _toast(context, '当前已是最新版本');
    return;
  }
  if (!context.mounted) return;
  final choice = await _promptDownload(context, probed);
  if (choice == _PromptResult.skip) {
    // 记下指纹: 该版本不再提示, 直到远端包发生变化.
    await state.saveFingerprint(probed.fingerprint);
    return;
  }
  if (choice != _PromptResult.download) return;

  final File apk;
  try {
    apk = File('${await UpdateChannel.updateDir()}/app-release.apk');
  } catch (e) {
    if (context.mounted) _toast(context, '无法准备下载目录: $e');
    return;
  }
  if (!context.mounted) return;
  final result = await showDialog<_DownloadResult>(
    context: context,
    barrierDismissible: false,
    builder:
        (_) => _DownloadDialog(
          url: service.url,
          target: apk,
          expected: probed.size,
        ),
  );
  if (result == null || result.outcome == _DownloadOutcome.cancelled) return;
  if (result.outcome == _DownloadOutcome.failed) {
    if (context.mounted) _toast(context, result.error);
    return;
  }

  final ApkInfo info;
  final ApkInfo current;
  try {
    // 读下载包自身的版本: 远端包变了不代表版本更新, 以 versionCode 为准.
    info = await UpdateChannel.inspectApk(apk.path);
    current = await UpdateChannel.currentVersion();
  } catch (e) {
    if (context.mounted) _toast(context, '更新包不可用: $e');
    return;
  }
  if (info.versionCode <= current.versionCode) {
    await state.saveFingerprint(probed.fingerprint);
    if (context.mounted) {
      _toast(context, '当前已是最新版本 (${info.display})');
    }
    return;
  }
  if (current.signerSha256.isNotEmpty &&
      info.signerSha256 != current.signerSha256) {
    // 签名不同时系统必然拒绝覆盖安装, 提前说明避免用户反复重试.
    await state.saveFingerprint(probed.fingerprint);
    if (!context.mounted) return;
    await _alert(
      context,
      '无法安装更新',
      '更新包与当前应用签名不一致, 系统会拒绝覆盖安装.\n'
          '若当前版本是本地 debug 签名构建的, 需要先卸载再安装正式包.',
    );
    return;
  }
  if (!context.mounted) return;
  if (await _confirmInstall(context, info) != true) return;

  // 安装会终止当前进程, 先记下待核对的更新: 装成则记为已处理, 用户取消则
  // 下次启动仍会提示, 不会被静默跳过.
  await state.markInstalling(probed.fingerprint, info.versionCode);
  if (AppSession.instance.running) {
    // 安装会替换应用并终止进程, 先停 VPN 让 native 侧正常收尾.
    try {
      await AppSession.instance.stopRun();
    } catch (_) {
      // 停止失败不影响安装: 进程被系统终止时 VPN 同样会断开.
    }
  }
  if (!context.mounted) return;
  await _install(context, apk.path);
}

/// 调起系统安装器, 缺「安装未知应用」权限时引导授权后重试.
Future<void> _install(BuildContext context, String apkPath) async {
  while (true) {
    final String status;
    try {
      status = await UpdateChannel.installApk(apkPath);
    } catch (e) {
      if (context.mounted) _toast(context, '安装失败: $e');
      return;
    }
    if (status != 'need_permission') {
      if (status != 'started' && context.mounted) {
        _toast(context, '无法调起安装器 ($status)');
      }
      return;
    }
    if (!context.mounted) return;
    if (await _confirmPermission(context) != true) return;
    if (!context.mounted) return;
  }
}

enum _PromptResult { download, later, skip }

enum _DownloadOutcome { done, cancelled, failed }

class _DownloadResult {
  const _DownloadResult(this.outcome, [this.error = '']);

  final _DownloadOutcome outcome;
  final String error;
}

Future<_PromptResult> _promptDownload(
  BuildContext context,
  RemoteApk probed,
) async {
  final size = probed.size > 0 ? ' (约 ${formatBytes(probed.size)})' : '';
  final result = await showDialog<_PromptResult>(
    context: context,
    builder:
        (context) => AlertDialog(
          title: const Text('发现新版本'),
          content: Text('检测到新的安装包$size, 下载后可直接安装.'),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context, _PromptResult.skip),
              child: const Text('跳过此版本'),
            ),
            TextButton(
              onPressed: () => Navigator.pop(context, _PromptResult.later),
              child: const Text('稍后'),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(context, _PromptResult.download),
              child: const Text('下载更新'),
            ),
          ],
        ),
  );
  return result ?? _PromptResult.later;
}

Future<bool?> _confirmInstall(BuildContext context, ApkInfo info) {
  return showDialog<bool>(
    context: context,
    builder:
        (context) => AlertDialog(
          title: const Text('下载完成'),
          content: Text(
            '新版本 ${info.display}, 是否立即安装?\n安装会重启应用, 正在运行的 VPN 会中断.',
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context, false),
              child: const Text('稍后'),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(context, true),
              child: const Text('立即安装'),
            ),
          ],
        ),
  );
}

Future<bool?> _confirmPermission(BuildContext context) {
  return showDialog<bool>(
    context: context,
    builder:
        (context) => AlertDialog(
          title: const Text('需要授权'),
          content: const Text(
            '安装更新需要「允许安装未知应用」权限, 系统设置页已打开.\n'
            '授权后返回本应用, 点击「继续安装」.',
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context, false),
              child: const Text('取消'),
            ),
            FilledButton(
              onPressed: () => Navigator.pop(context, true),
              child: const Text('继续安装'),
            ),
          ],
        ),
  );
}

Future<void> _alert(BuildContext context, String title, String message) {
  return showDialog<void>(
    context: context,
    builder:
        (context) => AlertDialog(
          title: Text(title),
          content: Text(message),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context),
              child: const Text('知道了'),
            ),
          ],
        ),
  );
}

void _toast(BuildContext context, String message) {
  ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(message)));
}

/// 下载进度对话框: 显示进度条、已下载字节数与速度, 支持取消.
class _DownloadDialog extends StatefulWidget {
  const _DownloadDialog({
    required this.url,
    required this.target,
    required this.expected,
  });

  final Uri url;
  final File target;

  /// 探测阶段得知的完整包长度 (未知为 -1).
  final int expected;

  @override
  State<_DownloadDialog> createState() => _DownloadDialogState();
}

class _DownloadDialogState extends State<_DownloadDialog> {
  late final UpdateDownload _download;
  final Stopwatch _watch = Stopwatch();
  Timer? _ticker;
  int _received = 0;
  late int _total = widget.expected;
  double _speed = 0;
  double _lastBytes = 0;
  double _lastMs = 0;
  bool _finished = false;

  @override
  void initState() {
    super.initState();
    _download = UpdateDownload(url: widget.url, target: widget.target);
    _watch.start();
    _ticker = Timer.periodic(const Duration(milliseconds: 500), (_) => _tick());
    unawaited(_start());
  }

  @override
  void dispose() {
    _ticker?.cancel();
    super.dispose();
  }

  Future<void> _start() async {
    try {
      await _download.start(
        onProgress: (received, total) {
          if (!mounted) return;
          setState(() {
            _received = received;
            if (total > 0) _total = total;
          });
        },
      );
      _finish(const _DownloadResult(_DownloadOutcome.done));
    } on UpdateCancelled {
      _finish(const _DownloadResult(_DownloadOutcome.cancelled));
    } catch (e) {
      _finish(_DownloadResult(_DownloadOutcome.failed, '$e'));
    }
  }

  /// 定期按时间窗口估算瞬时速度, 并做平滑避免数字跳动.
  void _tick() {
    if (!mounted) return;
    final ms = _watch.elapsedMilliseconds.toDouble();
    if (ms <= _lastMs) return;
    final instant = (_received - _lastBytes) * 1000 / (ms - _lastMs);
    _lastBytes = _received.toDouble();
    _lastMs = ms;
    if (instant <= 0) return;
    setState(() {
      _speed = _speed == 0 ? instant : _speed * 0.6 + instant * 0.4;
    });
  }

  void _finish(_DownloadResult result) {
    if (_finished) return;
    _finished = true;
    _ticker?.cancel();
    if (mounted) Navigator.of(context).pop(result);
  }

  @override
  Widget build(BuildContext context) {
    final total = _total > 0 ? _total : 0;
    final progress =
        total > 0 ? (_received / total).clamp(0.0, 1.0).toDouble() : null;
    final speed = formatSpeed(_speed);
    return PopScope(
      canPop: false,
      child: AlertDialog(
        title: const Text('下载更新包'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            LinearProgressIndicator(value: progress),
            const SizedBox(height: 12),
            Text(
              progress == null
                  ? '已下载 ${formatBytes(_received)}'
                  : '${formatBytes(_received)} / ${formatBytes(total)} '
                      '(${(progress * 100).toStringAsFixed(0)}%)',
            ),
            if (speed.isNotEmpty)
              Padding(
                padding: const EdgeInsets.only(top: 4),
                child: Text(
                  speed,
                  style: Theme.of(context).textTheme.labelSmall?.copyWith(
                    color: Theme.of(context).colorScheme.onSurfaceVariant,
                  ),
                ),
              ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => _download.cancel(),
            child: const Text('取消'),
          ),
        ],
      ),
    );
  }
}
