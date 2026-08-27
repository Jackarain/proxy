import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';

import '../models/vpn_config.dart';
import '../services/app_session.dart';
import '../services/cn_ip_list.dart';
import '../services/storage_service.dart';
import '../services/vpn_channel.dart';
import '../services/launcher_server.dart';
import '../engine/aengine.dart';
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
    // 顶部显示引擎版本标识.
    VpnChannel.buildVersion().then((v) {
      if (mounted && v.isNotEmpty) setState(() => _gitHash = v);
    });
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

  /// 恢复运行会话: Activity 重建/进程存活时重建控制通道并启动 Dart 引擎.
  Future<void> _tryResumeSession() async {
    final state = await _storage.loadRunState();
    if (state == null) return;
    final (configId, port) = state;

    // VpnService 可能已随进程结束而停止 (残留运行状态)。若未真正运行,
    // 清理状态并返回, 让用户重新点播放启动, 避免误报"启动失败".
    final active = await VpnChannel.isServiceActive();
    if (!active) {
      await _storage.clearRunState();
      if (mounted) setState(() {});
      return;
    }

    final session = AppSession.instance;
    session.beginRun(configId);
    var server = session.server;
    if (server == null) {
      try {
        server = LauncherServer();
        await server.start(port: port);
        session.server = server;
      } catch (_) {
        if (mounted) setState(() {});
        return;
      }
    }

    // 获取原生数据面端口并启动引擎 (若尚未运行).
    final engine = session.engine;
    if (engine == null) {
      final configs = await _storage.loadConfigs();
      VpnConfig? runningConfig;
      for (final c in configs) {
        if (c.id == configId) {
          runningConfig = c;
          break;
        }
      }
      if (runningConfig != null) {
        // 恢复运行: VpnService 已按原路由运行, 引擎只需数据面分流兜底.
        final cnCidrs = runningConfig.bypassCn
            ? await CnIpList.update()
            : const <String>[];
        await _startEngine(
          session,
          runningConfig,
          server,
          port,
          cnCidrs: cnCidrs,
        );
      }
    }
    server.connectionStream.listen((c) => session.setConnected(c));
    if (mounted) setState(() {});
  }

  /// 启动 Dart 引擎: 获取 Kotlin 数据面端口后连接并接管 VPN.
  Future<void> _startEngine(
    AppSession session,
    VpnConfig config,
    LauncherServer server,
    int port,
    {List<String> cnCidrs = const []}
  ) async {
    try {
      debugPrint('[aproxy] 等待 connectEngine 端口...');
      final ports = await VpnChannel.connectEngine();
      debugPrint(
          '[aproxy] connectEngine 得到桥接=${ports.tunBridgePort} '
          '转发=${ports.forwardPort} addr=${ports.address}');
      final engine = DartEngine.instance;
      await engine.start(
        configJson: jsonEncode(config.toJson()),
        launcherPort: port,
        tunBridgePort: ports.tunBridgePort,
        forwardPort: ports.forwardPort,
        cnCidrs: cnCidrs,
      );
      session.engine = engine;
      debugPrint('[aproxy] Dart 引擎已启动');
    } catch (e) {
      debugPrint('[aproxy] 引擎启动失败: $e');
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('引擎启动失败: $e')));
      }
    }
  }

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

      // bypassCn: 拉取中国段并附加非中国段 VPN 路由 (Kotlin 建 TUN 用).
      final prepared = await CnIpList.prepareStart(config);
      final cnCidrs = prepared.cnCidrs;
      final fullJson = jsonEncode(prepared.json);
      debugPrint('[aproxy] 启动 VpnService, launcherPort=${server.port}');
      // 1) 启动 VpnService (TUN + 数据面桥接 + 受保护转发器).
      await VpnChannel.start(fullJson, server.port);
      session.beginRun(config.id, configJson: fullJson);
      await _storage.saveRunState(config.id, server.port);
      debugPrint('[aproxy] VpnService 已请求启动, 等待数据面端口');

      // 2) 获取数据面端口并启动纯 Dart 引擎.
      await _startEngine(session, config, server, server.port, cnCidrs: cnCidrs);

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
      // 保存后自动应用到运行中的会话.
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
            const Text('aProxy'),
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
                  separatorBuilder: (_, _) => const SizedBox(height: 8),
                  itemBuilder: (context, i) {
                    final config = _configs[i];
                    final running = session.runningConfigId == config.id;
                    return Card(
                      child: ListTile(
                        leading: CircleAvatar(child: Icon(Icons.vpn_key_outlined)),
                        title: Row(
                          children: [
                            Expanded(
                              child: Text(
                                config.name,
                                style: const TextStyle(fontWeight: FontWeight.bold),
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
                              onPressed: running ? null : () => _runConfig(config),
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
                                    PopupMenuItem(value: 'edit', child: Text('编辑')),
                                    PopupMenuItem(value: 'duplicate', child: Text('复制')),
                                    PopupMenuItem(value: 'delete', child: Text('删除')),
                                  ],
                            ),
                          ],
                        ),
                        onTap:
                            () =>
                                running
                                    ? Navigator.of(context).push(
                                        MaterialPageRoute(
                                          builder: (_) => RunningPage(
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
