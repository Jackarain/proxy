import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';

import '../models/vpn_config.dart';
import '../services/app_session.dart';
import '../services/storage_service.dart';
import '../services/vpn_channel.dart';
import '../services/launcher_server.dart';
import 'config_edit_page.dart';
import 'running_page.dart';

class ConfigListPage extends StatefulWidget {
  const ConfigListPage({super.key});

  @override
  State<ConfigListPage> createState() => _ConfigListPageState();
}

class _ConfigListPageState extends State<ConfigListPage> {
  final StorageService _storage = StorageService();
  List<VpnConfig> _configs = [];
  bool _loading = true;
  bool _busy = false;
  String _gitHash = '';

  @override
  void initState() {
    super.initState();
    AppSession.instance.addListener(_onSession);
    _reload();
    _tryResumeSession();
    // 顶部显示 libxproxy 编译时记录的 git commit hash.
    VpnChannel.buildVersion().then((v) {
      if (mounted && v.isNotEmpty) setState(() => _gitHash = v);
    });
  }

  /// 界面重建 (Activity 重新打开) 时, 若 VPN 仍在同一进程运行,
  /// 恢复控制通道并标记运行状态. 存活判定基于控制通道 WebSocket 连接
  /// (proxy 重连机制会在数秒内重新连上), 不再使用 JNI status 接口.
  Future<void> _tryResumeSession() async {
    final state = await _storage.loadRunState();
    if (state == null) return;
    final (configId, port) = state;

    final session = AppSession.instance;
    session.beginRun(configId);
    // 同进程内 Activity 重建时可能已有控制通道, 直接复用.
    var server = session.server;
    if (server == null) {
      try {
        server = LauncherServer();
        await server.start(port: port);
        session.server = server;
      } catch (_) {
        // 原端口被占用等情况下控制通道暂不可用, 不影响 VPN 本身运行.
        if (mounted) setState(() {});
        return;
      }
    }

    // 等待 proxy 经控制通道连上; 未连上说明服务已不在运行, 清理状态.
    if (!await _waitLauncherConnected(server)) {
      await _storage.clearRunState();
      session.endRun();
      return;
    }

    // 恢复 vpnConfig 快照, 连接建立后据此建立 tun.
    final configs = await _storage.loadConfigs();
    for (final c in configs) {
      if (c.id == configId) {
        server.setVpnConfig(jsonDecode(jsonEncode(c.toJson())));
        break;
      }
    }
    server.connectionStream.listen((c) => session.setConnected(c));
    if (mounted) setState(() {});
  }

  /// 等待 proxy 控制通道连接 (最长 [timeout]), 判定服务是否存活.
  Future<bool> _waitLauncherConnected(
    LauncherServer server, {
    Duration timeout = const Duration(seconds: 5),
  }) async {
    if (server.connected) return true;
    final completer = Completer<bool>();
    final sub = server.connectionStream.listen((c) {
      if (c && !completer.isCompleted) completer.complete(true);
    });
    try {
      return await completer.future.timeout(timeout, onTimeout: () => false);
    } finally {
      await sub.cancel();
    }
  }

  @override
  void dispose() {
    AppSession.instance.removeListener(_onSession);
    super.dispose();
  }

  void _onSession() {
    if (mounted) setState(() {});
  }

  Future<void> _reload() async {
    final list = await _storage.loadConfigs();
    if (!mounted) return;
    setState(() {
      _configs = list;
      _loading = false;
    });
  }

  Future<void> _save() => _storage.saveConfigs(_configs);

  Future<void> _runConfig(VpnConfig config) async {
    if (_busy) return;
    if (AppSession.instance.running) {
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(const SnackBar(content: Text('请先停止当前连接')));
      }
      return;
    }
    setState(() => _busy = true);
    try {
      final ok = await VpnChannel.prepare();
      if (!ok) {
        if (mounted) {
          ScaffoldMessenger.of(
            context,
          ).showSnackBar(const SnackBar(content: Text('未获得 VPN 授权')));
        }
        return;
      }

      final session = AppSession.instance;
      var server = session.server;
      if (server == null) {
        server = LauncherServer();
        await server.start();
        session.server = server;
      }
      server.connectionStream.listen((c) => session.setConnected(c));

      final fullJson = jsonEncode(config.toJson());
      // 设置 vpnConfig 快照: 控制通道连接后据此建立 VpnService tun.
      server.setVpnConfig(config.toJson());
      await VpnChannel.start(fullJson, server.port);
      session.beginRun(config.id, configJson: fullJson);
      await _storage.saveRunState(config.id, server.port);
      if (!mounted) return;
      await Navigator.of(context).push(
        MaterialPageRoute(builder: (_) => RunningPage(configId: config.id)),
      );
      if (mounted) setState(() {});
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('启动失败: $e')));
      }
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _stopAll() async {
    if (_busy) return;
    setState(() => _busy = true);
    try {
      await AppSession.instance.stopRun();
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

  Future<void> _addConfig() async {
    final config = VpnConfig(id: VpnConfig.newId(), name: '新配置');
    final saved = await Navigator.of(context).push<VpnConfig>(
      MaterialPageRoute(
        builder: (_) => ConfigEditPage(config: config, isNew: true),
      ),
    );
    if (saved != null) {
      _configs.add(saved);
      await _save();
      _reload();
    }
  }

  Future<void> _editConfig(VpnConfig config) async {
    final saved = await Navigator.of(context).push<VpnConfig>(
      MaterialPageRoute(
        builder: (_) => ConfigEditPage(config: config.copy(), isNew: false),
      ),
    );
    if (saved != null) {
      final i = _configs.indexWhere((c) => c.id == saved.id);
      if (i >= 0) _configs[i] = saved;
      await _save();
      _reload();
      // 保存后自动应用到运行中的会话, 无需再手动点击应用.
      if (AppSession.instance.runningConfigId == saved.id) {
        try {
          final applied = await AppSession.instance.applyConfig(saved);
          if (mounted && applied != null) {
            ScaffoldMessenger.of(context).showSnackBar(
              SnackBar(
                content: Text(
                  applied == 'restarted' ? '配置已保存, VPN 已重建' : '配置已保存并热更新',
                ),
              ),
            );
          }
        } catch (e) {
          if (mounted) {
            ScaffoldMessenger.of(context).showSnackBar(
              SnackBar(content: Text('保存成功, 但应用失败: $e')),
            );
          }
        }
      }
    }
  }

  Future<void> _deleteConfig(VpnConfig config) async {
    if (AppSession.instance.runningConfigId == config.id) {
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(const SnackBar(content: Text('请先停止当前连接再删除')));
      }
      return;
    }
    final confirmed =
        await showDialog<bool>(
          context: context,
          builder:
              (ctx) => AlertDialog(
                title: const Text('删除配置'),
                content: Text('确定删除「${config.name}」吗?'),
                actions: [
                  TextButton(
                    onPressed: () => Navigator.of(ctx).pop(false),
                    child: const Text('取消'),
                  ),
                  FilledButton(
                    onPressed: () => Navigator.of(ctx).pop(true),
                    child: const Text('删除'),
                  ),
                ],
              ),
        ) ??
        false;
    if (!confirmed) return;
    _configs.removeWhere((c) => c.id == config.id);
    await _save();
    _reload();
  }

  Future<void> _duplicateConfig(VpnConfig config) async {
    final copy =
        config.copy()
          ..id = VpnConfig.newId()
          ..name = '${config.name} 副本';
    _configs.add(copy);
    await _save();
    _reload();
  }

  @override
  Widget build(BuildContext context) {
    final session = AppSession.instance;
    return Scaffold(
      appBar: AppBar(
        title: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Image.asset('assets/logo.png', width: 26, height: 26),
            const SizedBox(width: 8),
            const Text('xProxy'),
          ],
        ),
        actions: [
          if (_gitHash.isNotEmpty)
            Center(
              child: Padding(
                padding: const EdgeInsets.only(right: 8),
                child: Text(
                  _gitHash,
                  style: Theme.of(context).textTheme.labelSmall?.copyWith(
                    color: Theme.of(context).colorScheme.onSurfaceVariant,
                  ),
                ),
              ),
            ),
          if (session.running) ...[
            IconButton(
              tooltip: '运行控制台',
              onPressed:
                  _busy
                      ? null
                      : () {
                        final id = session.runningConfigId;
                        if (id != null) {
                          Navigator.of(context).push(
                            MaterialPageRoute(
                              builder: (_) => RunningPage(configId: id),
                            ),
                          );
                        }
                      },
              icon: const Icon(Icons.monitor_heart_outlined),
            ),
            TextButton.icon(
              onPressed: _busy ? null : _stopAll,
              icon: const Icon(Icons.stop_circle_outlined),
              label: const Text('停止'),
            ),
          ],
        ],
      ),
      body:
          _loading
              ? const Center(child: CircularProgressIndicator())
              : _configs.isEmpty
              ? const Center(child: Text('暂无配置, 点击右下角添加'))
              : RefreshIndicator(
                onRefresh: _reload,
                child: ListView.separated(
                  padding: const EdgeInsets.all(12),
                  itemCount: _configs.length,
                  separatorBuilder: (_, __) => const SizedBox(height: 8),
                  itemBuilder: (context, i) {
                    final config = _configs[i];
                    final running = session.runningConfigId == config.id;
                    return Card(
                      child: ListTile(
                        leading: CircleAvatar(
                          child: Icon(Icons.vpn_key_outlined),
                        ),
                        title: Row(
                          children: [
                            Expanded(
                              child: Text(
                                config.name,
                                style: const TextStyle(
                                  fontWeight: FontWeight.bold,
                                ),
                              ),
                            ),
                            if (running)
                              const Chip(
                                label: Text('运行中'),
                                visualDensity: VisualDensity.compact,
                                backgroundColor: Colors.green,
                                labelStyle: TextStyle(
                                  color: Colors.white,
                                  fontSize: 12,
                                ),
                              ),
                          ],
                        ),
                        subtitle: Text(
                          _subtitle(config),
                          maxLines: 2,
                          overflow: TextOverflow.ellipsis,
                        ),
                        trailing: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            IconButton(
                              tooltip: running ? '正在运行' : '运行此配置',
                              onPressed:
                                  running ? null : () => _runConfig(config),
                              icon: Icon(
                                running
                                    ? Icons.play_circle_filled
                                    : Icons.play_circle_outline,
                                color: running ? Colors.green : null,
                              ),
                            ),
                            PopupMenuButton<String>(
                              onSelected: (action) {
                                switch (action) {
                                  case 'edit':
                                    _editConfig(config);
                                  case 'duplicate':
                                    _duplicateConfig(config);
                                  case 'delete':
                                    _deleteConfig(config);
                                }
                              },
                              itemBuilder:
                                  (_) => const [
                                    PopupMenuItem(
                                      value: 'edit',
                                      child: Text('编辑'),
                                    ),
                                    PopupMenuItem(
                                      value: 'duplicate',
                                      child: Text('复制'),
                                    ),
                                    PopupMenuItem(
                                      value: 'delete',
                                      child: Text('删除'),
                                    ),
                                  ],
                            ),
                          ],
                        ),
                        onTap:
                            () =>
                                running
                                    ? Navigator.of(context).push(
                                      MaterialPageRoute(
                                        builder:
                                            (_) => RunningPage(
                                              configId: config.id,
                                            ),
                                      ),
                                    )
                                    : _editConfig(config),
                      ),
                    );
                  },
                ),
              ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: _busy ? null : _addConfig,
        icon: const Icon(Icons.add),
        label: const Text('添加配置'),
      ),
      bottomNavigationBar: _busy ? const LinearProgressIndicator() : null,
    );
  }

  String _subtitle(VpnConfig config) {
    final b = StringBuffer();
    b.write(
      '代理: ${config.proxyPass.isEmpty ? '未配置 proxy_pass' : config.proxyPass}',
    );
    if (config.proxyDomains.isNotEmpty || config.proxyCidr.isNotEmpty) {
      b.write(', 分流: ${config.proxyDomains.length + config.proxyCidr.length} 条');
    }
    return b.toString();
  }
}
