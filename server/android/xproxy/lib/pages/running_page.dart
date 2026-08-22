import 'dart:convert';
import 'dart:io';

import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../models/vpn_config.dart';
import '../services/app_session.dart';
import '../services/launcher_server.dart';
import '../services/storage_service.dart';
import '../services/vpn_channel.dart';

class RunningPage extends StatefulWidget {
  const RunningPage({super.key, required this.configId});

  final String configId;

  @override
  State<RunningPage> createState() => _RunningPageState();
}

class _RunningPageState extends State<RunningPage>
    with SingleTickerProviderStateMixin {
  late final TabController _tabs = TabController(length: 2, vsync: this);
  final StorageService _storage = StorageService();
  final List<Map<String, dynamic>> _logs = [];
  String _stateMessage = '';
  Map<String, dynamic>? _status;
  bool _busy = false;
  bool _testing = false;
  String _testResult = '';
  bool _connected = false;
  StreamSubscription<Map<String, dynamic>>? _statusSub;
  StreamSubscription<Map<String, dynamic>>? _logSub;
  StreamSubscription<bool>? _connSub;
  StreamSubscription<Map<String, dynamic>>? _nativeEventsSub;

  LauncherServer? get _server => AppSession.instance.server;

  @override
  void initState() {
    super.initState();
    final server = _server;
    if (server != null) {
      _connSub = server.connectionStream.listen((c) {
        if (mounted) setState(() => _connected = c);
      });
      _statusSub = server.statusStream.listen((s) {
        if (mounted) setState(() => _status = s);
      });
      _logSub = server.logStream.listen(_onLog);
    }
    // 事件通道 (native vpn_state; 日志已全部经 WS 控制通道上报).
    _nativeEventsSub = VpnChannel.events().listen((e) {
      if (e['type'] == 'vpn_state') {
        final state = e['state'] as String? ?? '';
        if (state == 'error') {
          // native 启动失败/异常退出: 清理运行状态, 界面提示.
          _storage.clearRunState();
          AppSession.instance.endRun();
        }
        if (mounted) {
          setState(() {
            _stateMessage = e['message'] as String? ?? '';
          });
        }
      }
    }, onError: (Object _) {
      // 引擎分离等场景下事件流中断, 状态仍由 WS 控制通道维持.
    });
    _connected = server?.connected ?? false;
  }

  static const int _maxLogLines = 500;

  void _addLog(Map<String, dynamic> entry) {
    _logs.add(entry);
    if (_logs.length > _maxLogLines) {
      _logs.removeRange(0, _logs.length - _maxLogLines);
    }
  }

  void _onLog(Map<String, dynamic> log) {
    final lines = log['lines'];
    if (lines is List && lines.isNotEmpty) {
      setState(() {
        for (final line in lines) {
          if (line is Map) {
            _addLog(Map<String, dynamic>.from(line));
          } else if (line is String && line.trim().isNotEmpty) {
            // launcher_log 队列上报的整行文本 (已含时间戳与级别前缀).
            _addLog({'time': 0, 'level': 1, 'message': line});
          }
        }
      });
    }
  }

  @override
  void dispose() {
    _statusSub?.cancel();
    _logSub?.cancel();
    _connSub?.cancel();
    _nativeEventsSub?.cancel();
    _tabs.dispose();
    super.dispose();
  }

  Future<void> _stop() async {
    setState(() => _busy = true);
    try {
      await AppSession.instance.stopRun();
      _stateMessage = '';
      if (mounted) Navigator.of(context).pop();
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('停止失败: $e')));
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('运行控制台'),
        actions: [
          IconButton(
            onPressed: _logs.isEmpty ? null : _copyAllLogs,
            icon: const Icon(Icons.copy_all),
            tooltip: '复制全部日志',
          ),
          IconButton(
            onPressed: _logs.isEmpty ? null : _clearLogs,
            icon: const Icon(Icons.delete_sweep),
            tooltip: '清空日志',
          ),
          TextButton.icon(
            onPressed: _busy ? null : _stop,
            icon: const Icon(Icons.stop),
            label: const Text('停止'),
          ),
        ],
      ),
      body: Column(
        children: [
          _statusBanner(context),
          TabBar(
            controller: _tabs,
            tabs: const [Tab(text: '状态'), Tab(text: '日志')],
          ),
          Expanded(
            child: TabBarView(
              controller: _tabs,
              children: [_buildStatusTab(context), _buildLogTab()],
            ),
          ),
        ],
      ),
    );
  }

  Widget _statusBanner(BuildContext context) {
    final running = AppSession.instance.running;
    final Color color;
    final String text;
    if (!running) {
      color = Colors.orange;
      text = '未运行';
    } else if (_connected) {
      color = Colors.green;
      text = '控制通道已连接';
    } else {
      color = Colors.orange;
      text = '等待 proxy 连接控制通道...';
    }
    if (_stateMessage.isNotEmpty) {
      return Container(
        width: double.infinity,
        color: color,
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 6),
        child: Text(_stateMessage, style: const TextStyle(color: Colors.white)),
      );
    }
    return Container(
      width: double.infinity,
      color: color,
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 6),
      child: Row(
        children: [
          Icon(Icons.circle, size: 10, color: Colors.white),
          const SizedBox(width: 8),
          Text(text, style: const TextStyle(color: Colors.white)),
        ],
      ),
    );
  }

  Widget _buildStatusTab(BuildContext context) {
    final s = _status ?? const {};
    final rates = s['rates'] as Map<String, dynamic>? ?? const {};
    final global = s['global'] as Map<String, dynamic>? ?? const {};
    // proxy 状态报告的活跃连接分布在 users[*].connections 中.
    final users = s['users'] as List<dynamic>? ?? const [];
    final sessions = <Map<String, dynamic>>[];
    for (final u in users) {
      if (u is! Map<String, dynamic>) continue;
      final conns = u['connections'] as List<dynamic>? ?? const [];
      for (final c in conns) {
        if (c is Map<String, dynamic>) sessions.add(c);
      }
    }
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Row(
          children: [
            Expanded(child: _statCard('上传速率', _fmtRate(rates['rx_rate_bps']))),
            const SizedBox(width: 12),
            Expanded(child: _statCard('下载速率', _fmtRate(rates['tx_rate_bps']))),
          ],
        ),
        const SizedBox(height: 12),
        Row(
          children: [
            Expanded(child: _statCard('上行流量', _fmtBytes(global['rx_bytes']))),
            const SizedBox(width: 12),
            Expanded(child: _statCard('下行流量', _fmtBytes(global['tx_bytes']))),
          ],
        ),
        const SizedBox(height: 12),
        _infoTile('运行时长', _fmtUptime(s['uptime'])),
        _infoTile('活动连接', '${s['active_connections'] ?? 0}'),
        _infoTile('累计连接', '${s['conn_total'] ?? 0}'),
        if (sessions.isNotEmpty) ...[
          const SizedBox(height: 8),
          Text('会话', style: Theme.of(context).textTheme.titleSmall),
          for (final item in sessions)
              Card(
                child: ListTile(
                  dense: true,
                  title: Text(
    '${item['proto'] ?? '-'}  → ${item['target'] ?? '-'}',
                  ),
                  subtitle: Text(
    'client: ${item['client_ip'] ?? '-'}\n'
                    'up ${_fmtBytes(item['rx_bytes'])} '
                    'down ${_fmtBytes(item['tx_bytes'])} '
                    '(${_fmtRate(item['rx_rate_bps'])} / ${_fmtRate(item['tx_rate_bps'])})',
                  ),
                  isThreeLine: true,
                ),
              ),
        ],
        const SizedBox(height: 24),
        FilledButton.icon(
          onPressed: _testing ? null : _testVpn,
          icon: Icon(_testing ? Icons.hourglass_top : Icons.speed),
          label: Text(_testing ? '测试中...' : '测试连接'),
        ),
        if (_testResult.isNotEmpty) ...[
          const SizedBox(height: 8),
          Text(
            _testResult,
            style: TextStyle(
              fontSize: 13,
              color: _testResult.startsWith('延迟')
                  ? Colors.green
                  : Theme.of(context).colorScheme.error,
            ),
          ),
        ],
      ],
    );
  }

  /// 通过配置的测试 URL 发起下载 (流量走 VPN 隧道),
  /// 测量网络延迟 (首字节) 与下载速率.
  Future<void> _testVpn() async {
    final config = _runningConfig();
    if (config == null) {
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(const SnackBar(content: Text('无法获取当前配置')));
      }
      return;
    }
    final url = config.testUrl.trim().isEmpty
        ? 'https://google.com'
        : config.testUrl.trim();

    setState(() {
      _testing = true;
      _testResult = '';
    });
    final stopwatch = Stopwatch()..start();
    HttpClient? client;
    try {
      client = HttpClient()..connectionTimeout = const Duration(seconds: 5);
      final req = await client
          .openUrl('GET', Uri.parse(url))
          .timeout(const Duration(seconds: 10));
      final res = await req.close().timeout(const Duration(seconds: 10));
      // 首字节到达时间即延迟.
      final latencyMs = stopwatch.elapsedMilliseconds;
      // 继续读取响应体, 按字节数/耗时计算下载速率.
      var received = 0;
      await for (final chunk in res) {
        received += chunk.length;
      }
      final totalMs = stopwatch.elapsedMilliseconds;
      final kbPerSec = totalMs > 0 ? received * 1000.0 / totalMs / 1024.0 : 0.0;
      final speedText = kbPerSec >= 1024
          ? '${(kbPerSec / 1024).toStringAsFixed(2)} MB/s'
          : '${kbPerSec.toStringAsFixed(1)} KB/s';
      if (mounted) {
        setState(
          () => _testResult =
              '延迟: $latencyMs ms\n速率: $speedText (HTTP ${res.statusCode})',
        );
      }
    } catch (e) {
      if (mounted) {
        setState(() => _testResult = '测试失败: $e');
      }
    } finally {
      client?.close(force: true);
      if (mounted) setState(() => _testing = false);
    }
  }

  /// 当前运行的完整配置 (含 testUrl 等 UI 字段).
  VpnConfig? _runningConfig() {
    final json = AppSession.instance.startedConfigJson;
    if (json == null || json.isEmpty) return null;
    try {
      return VpnConfig.fromJson(jsonDecode(json) as Map<String, dynamic>);
    } catch (_) {
      return null;
    }
  }

  Widget _buildLogTab() {
    if (_logs.isEmpty) {
      return const Center(child: Text('暂无日志'));
    }
    return ListView.builder(
      padding: const EdgeInsets.all(12),
      itemCount: _logs.length,
      itemBuilder: (context, i) {
        final log = _logs[i];
        final level = log['level'] as int? ?? 1;
        final message = log['message'] as String? ?? '';
        final time = log['time'] as int? ?? 0;
        final color = switch (level) {
          0 => Colors.grey,
          2 => Colors.orange,
          3 => Colors.red,
          _ => Theme.of(context).colorScheme.onSurface,
        };
        return Padding(
          padding: const EdgeInsets.symmetric(vertical: 2),
          child: SelectableText(
            '[${_fmtTimeMs(time)}] $message',
            style: TextStyle(fontSize: 12, color: color),
          ),
        );
      },
    );
  }

  void _clearLogs() {
    setState(() => _logs.clear());
  }

  Future<void> _copyAllLogs() async {
    final text = _logs
        .map((log) => '[${_fmtTimeMs(log['time'] as int? ?? 0)}] '
            '${log['message'] as String? ?? ''}')
        .join('\n');
    await Clipboard.setData(ClipboardData(text: text));
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('已复制全部日志到剪贴板')),
    );
  }

  Widget _statCard(String label, String value) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(label, style: Theme.of(context).textTheme.bodySmall),
            const SizedBox(height: 4),
            Text(
              value,
              style: Theme.of(
                context,
              ).textTheme.titleLarge?.copyWith(fontWeight: FontWeight.bold),
            ),
          ],
        ),
      ),
    );
  }

  Widget _infoTile(String label, String value) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: [
          SizedBox(
            width: 90,
            child: Text(label, style: Theme.of(context).textTheme.bodySmall),
          ),
          Expanded(child: Text(value)),
        ],
      ),
    );
  }

  String _fmtRate(Object? v) {
    final n = v is num ? v.toDouble() : 0.0;
    if (n >= 1024 * 1024) return '${(n / 1024 / 1024).toStringAsFixed(2)} MB/s';
    if (n >= 1024) return '${(n / 1024).toStringAsFixed(1)} KB/s';
    return '${n.toStringAsFixed(0)} B/s';
  }

  String _fmtBytes(Object? v) {
    final n = v is num ? v.toDouble() : 0.0;
    if (n >= 1024 * 1024 * 1024) {
      return '${(n / 1024 / 1024 / 1024).toStringAsFixed(2)} GB';
    }
    if (n >= 1024 * 1024) return '${(n / 1024 / 1024).toStringAsFixed(1)} MB';
    if (n >= 1024) return '${(n / 1024).toStringAsFixed(0)} KB';
    return '${n.toStringAsFixed(0)} B';
  }

  String _fmtUptime(Object? v) {
    final n = v is int ? v : 0;
    final h = n ~/ 3600;
    final m = (n % 3600) ~/ 60;
    final s = n % 60;
    return '${h.toString().padLeft(2, '0')}:${m.toString().padLeft(2, '0')}:${s.toString().padLeft(2, '0')}';
  }


  String _fmtTimeMs(int ms) {
    if (ms <= 0) return '--:--:--';
    final t = DateTime.fromMillisecondsSinceEpoch(ms);
    final h = t.hour.toString().padLeft(2, '0');
    final m = t.minute.toString().padLeft(2, '0');
    final s = t.second.toString().padLeft(2, '0');
    return '$h:$m:$s';
  }
}
