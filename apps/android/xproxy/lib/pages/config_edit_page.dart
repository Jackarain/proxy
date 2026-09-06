import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';

import '../models/vpn_config.dart';

class ConfigEditPage extends StatefulWidget {
  const ConfigEditPage({super.key, required this.config, required this.isNew});

  final VpnConfig config;
  final bool isNew;

  @override
  State<ConfigEditPage> createState() => _ConfigEditPageState();
}

class _ConfigEditPageState extends State<ConfigEditPage> {
  late final VpnConfig c = widget.config;
  late final TextEditingController _name = TextEditingController(text: c.name);
  late final TextEditingController _proxyPass = TextEditingController(
    text: c.proxyPass,
  );
  late final TextEditingController _sni = TextEditingController(text: c.sni);
  late final TextEditingController _proxyDomains = TextEditingController(
    text: c.proxyDomains.join('\n'),
  );
  late final TextEditingController _proxyCidr = TextEditingController(
    text: c.proxyCidr.join('\n'),
  );
  late final TextEditingController _dns = TextEditingController(
    text: c.dns.join('\n'),
  );
  late final TextEditingController _dnsForeign = TextEditingController(
    text: c.joinForeignDns().join('\n'),
  );
  late final TextEditingController _testUrl = TextEditingController(
    text: c.testUrl,
  );
  late bool _disableCheckCert = c.disableCheckCert;
  late bool _bypassCn = c.bypassCn;
  late bool _dnsCache = c.dnsCache;
  late bool _noIpv6 = c.noIpv6;
  late bool _globalProxy = c.globalProxy;
  late String _proxyDomainsUrl = c.proxyDomainsUrl;
  late String _proxyCidrUrl = c.proxyCidrUrl;
  late final TextEditingController _proxyPassPoolSize = TextEditingController(
    text: c.proxyPassPoolSize.toString(),
  );

  @override
  void dispose() {
    for (final t in [
      _name,
      _proxyPass,
      _sni,
      _proxyDomains,
      _proxyCidr,
      _dns,
      _dnsForeign,
      _testUrl,
      _proxyPassPoolSize,
    ]) {
      t.dispose();
    }
    super.dispose();
  }

  List<String> _lines(String v) =>
      v
          .split(RegExp(r'[\r\n,;]+'))
          .map((s) => s.trim())
          .where((s) => s.isNotEmpty)
          .toList();

  void _save() {
    _finishSave();
  }

  void _finishSave() {
    final (foreignIps, foreignDoh) = VpnConfig.splitForeignDns(
      _lines(_dnsForeign.text),
    );
    final vpn =
        c
          ..name = _name.text.trim().isEmpty ? '未命名' : _name.text.trim()
          ..proxyPass = _proxyPass.text.trim()
          ..sni = _sni.text.trim()
          ..proxyDomains = _lines(_proxyDomains.text)
          ..proxyCidr = _lines(_proxyCidr.text)
          ..globalProxy = _globalProxy
          ..proxyDomainsUrl = _proxyDomainsUrl
          ..proxyCidrUrl = _proxyCidrUrl
          ..dns = _lines(_dns.text)
          ..dnsForeign = foreignIps
          ..dnsForeignDoh = foreignDoh
          ..dnsCache = _dnsCache
          ..noIpv6 = _noIpv6
          ..proxyPassPoolSize =
              int.tryParse(_proxyPassPoolSize.text.trim()) ?? 0
          ..testUrl = _testUrl.text.trim()
          ..disableCheckCert = _disableCheckCert
          ..bypassCn = _bypassCn;
    final errors = vpn.validate();
    if (errors.isNotEmpty) {
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text(errors.join('\n'))));
      return;
    }
    Navigator.of(context).pop(vpn);
  }

  /// 弹出「从 URL 拉取列表」对话框: 回填上次使用的 URL, 确认后拉取
  /// 远程列表并写入编辑框. 返回 false 表示用户取消.
  Future<bool> _promptFetchList({
    required String title,
    required String lastUrl,
    required ValueChanged<String> onUrlChanged,
    required TextEditingController target,
  }) async {
    final urlController = TextEditingController(text: lastUrl);
    final url = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(title),
        content: TextField(
          controller: urlController,
          autofocus: true,
          keyboardType: TextInputType.url,
          decoration: const InputDecoration(
            labelText: '列表 URL',
            hintText: 'https://example.com/list.txt',
            border: OutlineInputBorder(borderRadius: BorderRadius.zero),
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(),
            child: const Text('取消'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(ctx).pop(urlController.text.trim()),
            child: const Text('确定'),
          ),
        ],
      ),
    );
    if (url == null || url.isEmpty) return false;
    if (!url.startsWith('http://') && !url.startsWith('https://')) {
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(const SnackBar(content: Text('列表 URL 需以 http(s):// 开头')));
      }
      return false;
    }
    try {
      final items = await _fetchList(url);
      if (!mounted) return false;
      onUrlChanged(url);
      target.text = items.join('\n');
      if (items.isEmpty) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(const SnackBar(content: Text('列表内容为空')));
      } else {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('已拉取 ${items.length} 条')));
      }
      return true;
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('拉取失败: $e')));
      }
      return false;
    }
  }

  Future<List<String>> _fetchList(String url) async {
    final client =
        HttpClient()..connectionTimeout = const Duration(seconds: 15);
    try {
      final req = await client.getUrl(Uri.parse(url));
      final resp = await req.close();
      if (resp.statusCode != HttpStatus.ok) {
        throw HttpException('HTTP ${resp.statusCode}');
      }
      final text = await resp.transform(utf8.decoder).join();
      final items = <String>[];
      for (final line in text.split(RegExp(r'[\r\n]+'))) {
        final t = line.trim();
        if (t.isEmpty || t.startsWith('#')) continue;
        items.add(t);
      }
      return items;
    } finally {
      client.close();
    }
  }

  @override
  Widget build(BuildContext context) {
    return DefaultTabController(
      length: 3,
      child: Scaffold(
        appBar: AppBar(
          title: Text(widget.isNew ? '添加配置' : '编辑配置'),
          actions: [
            TextButton(onPressed: _save, child: const Text('保存')),
          ],
          bottom: const TabBar(
            tabs: [Tab(text: '基本'), Tab(text: 'DNS 配置'), Tab(text: '分流')],
          ),
        ),
        body: TabBarView(
          children: [
            _buildBasicTab(context),
            _buildDnsTab(context),
            _buildProxyTab(context),
          ],
        ),
      ),
    );
  }

  Widget _buildBasicTab(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        TextField(
          controller: _name,
          decoration: const InputDecoration(
            labelText: '名称',
            border: OutlineInputBorder(borderRadius: BorderRadius.zero),
          ),
        ),
        const SizedBox(height: 12),
        TextField(
          controller: _proxyPass,
          decoration: const InputDecoration(
            labelText: '上游代理 proxy_pass',
            hintText: '',
            border: OutlineInputBorder(borderRadius: BorderRadius.zero),
          ),
        ),
        const SizedBox(height: 12),
        TextField(
          controller: _sni,
          decoration: const InputDecoration(
            labelText: 'SNI',
            helperText: '与代理建立 TLS 连接时的 SNI, 留空用主机名',
            border: OutlineInputBorder(borderRadius: BorderRadius.zero),
          ),
        ),
        const SizedBox(height: 12),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const Text('关闭上游证书校验'),
          value: _disableCheckCert,
          onChanged: (v) => setState(() => _disableCheckCert = v),
        ),
        const SizedBox(height: 12),
        TextField(
          controller: _proxyPassPoolSize,
          keyboardType: TextInputType.number,
          decoration: const InputDecoration(
            labelText: '代理连接池大小',
            hintText: '预连到上游的 TCP/TLS 连接数, 0 表示禁用',
            border: OutlineInputBorder(borderRadius: BorderRadius.zero),
          ),
        ),
        const SizedBox(height: 12),
        TextField(
          controller: _testUrl,
          decoration: const InputDecoration(
            labelText: '测试连接 URL',
            hintText: 'https://www.google.com',
            border: OutlineInputBorder(borderRadius: BorderRadius.zero),
          ),
        ),
      ],
    );
  }

  Widget _buildDnsTab(BuildContext context) {
    final theme = Theme.of(context);
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        _buildDnsIntro(theme),
        const SizedBox(height: 20),
        _buildDnsField(
          theme: theme,
          title: '国内 DNS',
          description: '每行一个 IP，不支持 DoH',
          hint: '223.6.6.6 / 119.29.29.29',
          controller: _dns,
        ),
        const SizedBox(height: 20),
        _buildDnsField(
          theme: theme,
          title: '国外 DNS / DoH',
          description: '每行一个，支持普通 DNS 服务器和 DoH',
          hint: 'https://dns.google/dns-query',
          controller: _dnsForeign,
        ),
        const Divider(height: 32),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const Text('DNS 缓存'),
          subtitle: const Text('缓存解析结果，重复查询直接回包'),
          value: _dnsCache,
          onChanged: (v) => setState(() => _dnsCache = v),
        ),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const Text('禁用 IPv6 解析'),
          subtitle: const Text('AAAA 查询直接返回空应答'),
          value: _noIpv6,
          onChanged: (v) => setState(() => _noIpv6 = v),
        ),
      ],
    );
  }

  /// DNS 分流规则说明卡片：统一展示国内/国外 DNS 的职责。
  Widget _buildDnsIntro(ThemeData theme) {
    final onSecondary = theme.colorScheme.onSecondaryContainer;
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: theme.colorScheme.secondaryContainer,
        borderRadius: BorderRadius.circular(8),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(Icons.info_outline, size: 16, color: onSecondary),
              const SizedBox(width: 6),
              Text(
                'DNS 分流规则',
                style: theme.textTheme.titleSmall?.copyWith(color: onSecondary),
              ),
            ],
          ),
          const SizedBox(height: 8),
          Text(
            '国内 DNS 用于未命中分流规则的域名直连解析；'
            '国外 DNS/DoH 用于命中分流规则的域名经代理转发解析。'
            '若分流列表为空，则表示为全局代理模式，所有域名均使用国外 DNS/DoH 解析。',
            style: theme.textTheme.bodySmall?.copyWith(color: onSecondary),
          ),
        ],
      ),
    );
  }

  /// DNS 服务器输入项：标题 + 说明 + 输入框（说明统一展示在输入框上方）。
  Widget _buildDnsField({
    required ThemeData theme,
    required String title,
    required String description,
    required String hint,
    required TextEditingController controller,
  }) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(title, style: theme.textTheme.titleMedium),
        const SizedBox(height: 4),
        Text(
          description,
          style: theme.textTheme.bodySmall?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
          ),
        ),
        const SizedBox(height: 8),
        TextField(
          controller: controller,
          maxLines: 2,
          decoration: InputDecoration(
            hintText: hint,
            border: const OutlineInputBorder(borderRadius: BorderRadius.zero),
          ),
        ),
      ],
    );
  }

  Widget _buildProxyTab(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const Text('全局代理'),
          subtitle: const Text('开启后所有流量均走代理，忽略下方分流规则'),
          value: _globalProxy,
          onChanged: (v) => setState(() => _globalProxy = v),
        ),
        const Divider(height: 24),
        _buildProxyListField(
          title: '代理域名',
          description: '每行一个，命中这些域名的流量走代理',
          hint: 'google.com\nyoutube.com',
          controller: _proxyDomains,
          enabled: !_globalProxy,
          fetchLabel: '从 URL 拉取域名列表',
          onFetch: () => _promptFetchList(
            title: '拉取代理域名列表',
            lastUrl: _proxyDomainsUrl,
            onUrlChanged: (u) => setState(() => _proxyDomainsUrl = u),
            target: _proxyDomains,
          ),
        ),
        const SizedBox(height: 16),
        _buildProxyListField(
          title: '代理 CIDR',
          description: '每行一个，命中这些 IP 段的流量走代理',
          hint: '1.1.1.0/24\n2606:4700::/32',
          controller: _proxyCidr,
          enabled: !_globalProxy,
          fetchLabel: '从 URL 拉取 CIDR 列表',
          onFetch: () => _promptFetchList(
            title: '拉取代理 CIDR 列表',
            lastUrl: _proxyCidrUrl,
            onUrlChanged: (u) => setState(() => _proxyCidrUrl = u),
            target: _proxyCidr,
          ),
        ),
        const Divider(height: 24),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const Text('绕过中国大陆'),
          subtitle: const Text('拉取中国 IP 段, 非中国段接入 VPN'),
          value: _bypassCn,
          onChanged: (v) => setState(() => _bypassCn = v),
        ),
      ],
    );
  }

  /// 分流列表输入项: 标题 + 说明 + 多行输入框 + 「从 URL 拉取」按钮.
  Widget _buildProxyListField({
    required String title,
    required String description,
    required String hint,
    required TextEditingController controller,
    required bool enabled,
    required String fetchLabel,
    required VoidCallback onFetch,
  }) {
    final theme = Theme.of(context);
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(title, style: theme.textTheme.titleMedium),
        const SizedBox(height: 4),
        Text(
          description,
          style: theme.textTheme.bodySmall?.copyWith(
            color: theme.colorScheme.onSurfaceVariant,
          ),
        ),
        const SizedBox(height: 8),
        TextField(
          controller: controller,
          maxLines: 4,
          enabled: enabled,
          decoration: InputDecoration(
            hintText: hint,
            border: const OutlineInputBorder(
              borderRadius: BorderRadius.zero,
            ),
          ),
        ),
        const SizedBox(height: 8),
        Align(
          alignment: Alignment.centerLeft,
          child: OutlinedButton.icon(
            onPressed: enabled ? onFetch : null,
            icon: const Icon(Icons.download_outlined, size: 16),
            label: Text(fetchLabel),
            style: OutlinedButton.styleFrom(
              visualDensity: VisualDensity.compact,
              shape: const RoundedRectangleBorder(
                borderRadius: BorderRadius.zero,
              ),
            ),
          ),
        ),
      ],
    );
  }
}
