import 'package:flutter/material.dart';

import '../models/installed_app.dart';
import '../services/vpn_channel.dart';

/// 按应用分流: 选择走 VPN (或直连) 的应用.
///
/// 点「完成」返回选中的包名列表; 直接返回 (取消) 时返回 null, 调用方保留
/// 原有选择. 系统应用默认折叠, 已选中的系统应用始终可见.
class AppSelectPage extends StatefulWidget {
  const AppSelectPage({super.key, required this.selected});

  /// 打开页面时已选中的应用包名.
  final List<String> selected;

  @override
  State<AppSelectPage> createState() => _AppSelectPageState();
}

class _AppSelectPageState extends State<AppSelectPage> {
  final TextEditingController _search = TextEditingController();
  final Set<String> _selected = {};
  List<InstalledApp>? _apps;
  String? _error;
  bool _showSystem = false;

  @override
  void initState() {
    super.initState();
    _selected.addAll(widget.selected);
    _search.addListener(() => setState(() {}));
    _load();
  }

  @override
  void dispose() {
    _search.dispose();
    super.dispose();
  }

  Future<void> _load() async {
    setState(() {
      _apps = null;
      _error = null;
    });
    try {
      final apps = await VpnChannel.listApps();
      apps.sort(
        (a, b) => a.label.toLowerCase().compareTo(b.label.toLowerCase()),
      );
      if (!mounted) return;
      setState(() => _apps = apps);
    } catch (e) {
      if (!mounted) return;
      setState(() => _error = '读取应用列表失败: $e');
    }
  }

  /// 当前可见的应用: 按名称/包名搜索, 未开启「显示系统应用」时折叠系统
  /// 应用 (已选中的仍显示, 避免已选项被隐藏).
  List<InstalledApp> _visible(List<InstalledApp> apps) {
    final query = _search.text.trim().toLowerCase();
    return apps.where((app) {
      if (!_showSystem &&
          app.system &&
          !_selected.contains(app.packageName)) {
        return false;
      }
      if (query.isEmpty) return true;
      return app.label.toLowerCase().contains(query) ||
          app.packageName.toLowerCase().contains(query);
    }).toList();
  }

  void _toggle(String packageName, bool selected) {
    setState(() {
      if (selected) {
        _selected.add(packageName);
      } else {
        _selected.remove(packageName);
      }
    });
  }

  @override
  Widget build(BuildContext context) {
    final apps = _apps;
    return Scaffold(
      appBar: AppBar(
        title: Text('选择应用 (${_selected.length})'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(_selected.toList()),
            child: const Text('完成'),
          ),
        ],
      ),
      body: apps == null
          ? _buildPlaceholder()
          : Column(
              children: [
                Padding(
                  padding: const EdgeInsets.fromLTRB(12, 12, 12, 0),
                  child: TextField(
                    controller: _search,
                    decoration: const InputDecoration(
                      labelText: '搜索应用',
                      hintText: '名称或包名',
                      prefixIcon: Icon(Icons.search),
                      border: OutlineInputBorder(
                        borderRadius: BorderRadius.zero,
                      ),
                    ),
                  ),
                ),
                SwitchListTile(
                  contentPadding: const EdgeInsets.symmetric(horizontal: 12),
                  title: const Text('显示系统应用'),
                  value: _showSystem,
                  onChanged: (v) => setState(() => _showSystem = v),
                ),
                _buildActions(apps),
                const Divider(height: 1),
                Expanded(child: _buildList(apps)),
              ],
            ),
    );
  }

  Widget _buildPlaceholder() {
    final error = _error;
    if (error == null) {
      return const Center(child: CircularProgressIndicator());
    }
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Text(error, textAlign: TextAlign.center),
            const SizedBox(height: 12),
            FilledButton(onPressed: _load, child: const Text('重试')),
          ],
        ),
      ),
    );
  }

  /// 全选/清空只作用于当前可见列表 (搜索/系统应用开关的结果).
  Widget _buildActions(List<InstalledApp> apps) {
    final visible = _visible(apps);
    return Row(
      children: [
        TextButton(
          onPressed: visible.isEmpty
              ? null
              : () => setState(
                  () => _selected.addAll(visible.map((a) => a.packageName)),
                ),
          child: const Text('全选当前列表'),
        ),
        TextButton(
          onPressed: _selected.isEmpty
              ? null
              : () => setState(() => _selected.clear()),
          child: const Text('清空'),
        ),
      ],
    );
  }

  /// 列表为空时的提示: 区分「搜索无结果」「系统应用被折叠」「完全没有应用」,
  /// 后者常见于模拟器/新设备, 也可能是包可见性受限, 给出可操作的出口而不是死路.
  Widget _buildEmpty(List<InstalledApp> apps) {
    if (_search.text.trim().isNotEmpty) {
      return const Center(child: Text('没有匹配的应用'));
    }
    final hiddenSystem = apps.where((a) => a.system).length;
    if (hiddenSystem > 0 && !_showSystem) {
      return Center(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Text(
                '未发现第三方应用, 已折叠 $hiddenSystem 个系统应用',
                textAlign: TextAlign.center,
              ),
              const SizedBox(height: 12),
              FilledButton(
                onPressed: () => setState(() => _showSystem = true),
                child: const Text('显示系统应用'),
              ),
            ],
          ),
        ),
      );
    }
    if (apps.isEmpty) {
      return const Center(
        child: Padding(
          padding: EdgeInsets.all(24),
          child: Text(
            '未读取到任何应用, 可能是系统限制了应用列表访问. '
            '请确认已安装最新版本, 必要时重新安装本应用后再试.',
            textAlign: TextAlign.center,
          ),
        ),
      );
    }
    return const Center(child: Text('没有匹配的应用'));
  }

  Widget _buildList(List<InstalledApp> apps) {
    final visible = _visible(apps);
    if (visible.isEmpty) {
      return _buildEmpty(apps);
    }
    return ListView.builder(
      itemCount: visible.length,
      itemBuilder: (_, index) {
        final app = visible[index];
        return CheckboxListTile(
          value: _selected.contains(app.packageName),
          onChanged: (v) => _toggle(app.packageName, v ?? false),
          title: Text(app.label),
          subtitle: Text(app.packageName),
        );
      },
    );
  }
}
