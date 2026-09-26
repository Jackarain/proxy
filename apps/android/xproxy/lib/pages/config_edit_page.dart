import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

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
      builder:
          (ctx) => AlertDialog(
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
                onPressed:
                    () => Navigator.of(ctx).pop(urlController.text.trim()),
                child: const Text('确定'),
              ),
            ],
          ),
    );
    urlController.dispose();
    if (url == null || url.isEmpty) return false;
    if (!url.startsWith('http://') && !url.startsWith('https://')) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('列表 URL 需以 http(s):// 开头')),
        );
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
    // 列表体积上限: 异常/恶意响应不至于把整段内容读进内存.
    const maxBytes = 512 * 1024;
    final client =
        HttpClient()..connectionTimeout = const Duration(seconds: 15);
    try {
      final req = await client.getUrl(Uri.parse(url));
      final resp = await req.close();
      if (resp.statusCode != HttpStatus.ok) {
        throw HttpException('HTTP ${resp.statusCode}');
      }
      final body = <int>[];
      await for (final chunk in resp) {
        body.addAll(chunk);
        if (body.length > maxBytes) throw HttpException('列表内容过大');
      }
      final text = utf8.decode(body, allowMalformed: true);
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
          actions: [TextButton(onPressed: _save, child: const Text('保存'))],
          bottom: const TabBar(
            tabs: [Tab(text: '基本'), Tab(text: 'DNS 配置'), Tab(text: '分流')],
          ),
        ),
        body: TabBarView(
          children: [_buildBasicTab(), _buildDnsTab(), _buildProxyTab()],
        ),
      ),
    );
  }

  Widget _buildBasicTab() {
    return ListView(
      padding: const EdgeInsets.fromLTRB(12, 12, 12, 24),
      children: [
        _section('基本', [
          _textField(_name, label: '名称', hint: '如 家庭代理 / 公司服务器'),
        ], subtitle: '配置在列表中的显示名称'),
        _section('上游代理', [
          _textField(
            _proxyPass,
            label: '上游代理 proxy_pass',
            hint: '如 https://1.2.3.4:443 或 socks5://1.2.3.4:1080',
          ),
          _textField(_sni, label: 'SNI', hint: '与代理建立 TLS 连接时的 SNI, 留空用主机名'),
          _switch(
            title: '关闭上游证书校验',
            subtitle: '跳过上游 TLS 证书校验, 仅自签证书场景使用',
            value: _disableCheckCert,
            onChanged: (v) => setState(() => _disableCheckCert = v),
          ),
          _numberField(_proxyPassPoolSize, label: '代理连接池大小'),
        ], subtitle: '预连到上游的 TCP/TLS 连接数, 0 表示禁用连接池'),
        _section('测试连接', [
          _textField(
            _testUrl,
            label: '测试连接 URL',
            hint: 'https://www.google.com',
            keyboardType: TextInputType.url,
          ),
        ], subtitle: '运行页据此测量代理延迟'),
      ],
    );
  }

  Widget _buildDnsTab() {
    return ListView(
      padding: const EdgeInsets.fromLTRB(12, 12, 12, 24),
      children: [
        _section('DNS 服务器', [
          _textField(
            _dns,
            label: '国内 DNS (每行一个 IP, 不支持 DoH)',
            hint: '223.6.6.6\n119.29.29.29',
            maxLines: 2,
          ),
          _textField(
            _dnsForeign,
            label: '国外 DNS / DoH (每行一个)',
            hint: 'https://dns.google/dns-query',
            maxLines: 2,
          ),
        ], subtitle: '未命中分流的域名走国内 DNS 直连解析, 命中分流的走国外 DNS/DoH'),
        _section('DNS 选项', [
          _switch(
            title: 'DNS 缓存',
            subtitle: '缓存解析结果, 重复查询直接回包',
            value: _dnsCache,
            onChanged: (v) => setState(() => _dnsCache = v),
          ),
          _switch(
            title: '禁用 IPv6 解析',
            subtitle: 'AAAA 查询直接返回空应答',
            value: _noIpv6,
            onChanged: (v) => setState(() => _noIpv6 = v),
          ),
        ], subtitle: '分流列表为空时按全局代理处理, 所有域名走国外 DNS/DoH'),
      ],
    );
  }

  Widget _buildProxyTab() {
    return ListView(
      padding: const EdgeInsets.fromLTRB(12, 12, 12, 24),
      children: [
        _section('分流规则', [
          _switch(
            title: '全局代理',
            subtitle: '开启后所有流量均走代理, 忽略下方分流规则',
            value: _globalProxy,
            onChanged: (v) => setState(() => _globalProxy = v),
          ),
          _listField(
            _proxyDomains,
            label: '代理域名 (每行一个)',
            hint: 'google.com\nyoutube.com',
            enabled: !_globalProxy,
            fetchLabel: '从 URL 拉取域名列表',
            onFetch: _fetchProxyDomains,
          ),
          _listField(
            _proxyCidr,
            label: '代理 CIDR (每行一个)',
            hint: '1.1.1.0/24\n2606:4700::/32',
            enabled: !_globalProxy,
            fetchLabel: '从 URL 拉取 CIDR 列表',
            onFetch: _fetchProxyCidr,
          ),
          _switch(
            title: '绕过中国大陆',
            subtitle: '拉取中国 IP 段, 非中国段接入 VPN',
            value: _bypassCn,
            onChanged: (v) => setState(() => _bypassCn = v),
          ),
        ], subtitle: '命中代理域名/CIDR 的流量走上游代理, 其余直连'),
      ],
    );
  }

  Future<bool> _fetchProxyDomains() => _promptFetchList(
    title: '拉取代理域名列表',
    lastUrl: _proxyDomainsUrl,
    onUrlChanged: (u) => setState(() => _proxyDomainsUrl = u),
    target: _proxyDomains,
  );

  Future<bool> _fetchProxyCidr() => _promptFetchList(
    title: '拉取代理 CIDR 列表',
    lastUrl: _proxyCidrUrl,
    onUrlChanged: (u) => setState(() => _proxyCidrUrl = u),
    target: _proxyCidr,
  );

  /// 分组卡片: 同类配置集中放置, 子项之间统一留白.
  Widget _section(String title, List<Widget> children, {String? subtitle}) {
    final theme = Theme.of(context);
    return Card(
      margin: const EdgeInsets.only(bottom: 12),
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(
              title,
              style: theme.textTheme.titleSmall?.copyWith(
                color: theme.colorScheme.primary,
                fontWeight: FontWeight.bold,
              ),
            ),
            if (subtitle != null) ...[
              const SizedBox(height: 2),
              Text(
                subtitle,
                style: theme.textTheme.bodySmall?.copyWith(
                  color: theme.colorScheme.onSurfaceVariant,
                ),
              ),
            ],
            const SizedBox(height: 12),
            for (var i = 0; i < children.length; i++) ...[
              if (i > 0) const SizedBox(height: 12),
              children[i],
            ],
          ],
        ),
      ),
    );
  }

  Widget _textField(
    TextEditingController controller, {
    required String label,
    String? hint,
    int maxLines = 1,
    TextInputType? keyboardType,
    List<TextInputFormatter>? formatters,
    ValueChanged<String>? onChanged,
  }) {
    return TextField(
      controller: controller,
      maxLines: maxLines,
      keyboardType: keyboardType,
      inputFormatters: formatters,
      onChanged: onChanged,
      decoration: InputDecoration(labelText: label, hintText: hint),
    );
  }

  Widget _numberField(
    TextEditingController controller, {
    required String label,
  }) {
    return _textField(
      controller,
      label: label,
      keyboardType: TextInputType.number,
      formatters: [FilteringTextInputFormatter.digitsOnly],
    );
  }

  Widget _switch({
    required String title,
    String? subtitle,
    required bool value,
    required ValueChanged<bool> onChanged,
  }) {
    return SwitchListTile(
      title: Text(title),
      subtitle: subtitle == null ? null : Text(subtitle),
      value: value,
      onChanged: onChanged,
      contentPadding: EdgeInsets.zero,
    );
  }

  /// 多行列表输入项: 输入框 + 「从 URL 拉取」按钮.
  Widget _listField(
    TextEditingController controller, {
    required String label,
    required String hint,
    required bool enabled,
    required String fetchLabel,
    required VoidCallback onFetch,
  }) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        TextField(
          controller: controller,
          maxLines: 4,
          enabled: enabled,
          decoration: InputDecoration(labelText: label, hintText: hint),
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
            ),
          ),
        ),
      ],
    );
  }
}
