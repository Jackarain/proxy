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
  bool _saving = false;

  @override
  void dispose() {
    for (final t in [
      _name,
      _proxyPass,
      _proxyDomains,
      _proxyCidr,
      _dns,
      _dnsForeign,
      _testUrl,
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

  Future<void> _save() async {
    if (_saving) return;
    setState(() => _saving = true);
    try {
      final domains = await _expandUrlLines(_lines(_proxyDomains.text));
      final cidrs = await _expandUrlLines(_lines(_proxyCidr.text));
      _finishSave(domains, cidrs);
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('拉取分流列表失败: $e')));
      }
    } finally {
      if (mounted) setState(() => _saving = false);
    }
  }

  void _finishSave(List<String> domains, List<String> cidrs) {
    final (foreignIps, foreignDoh) = VpnConfig.splitForeignDns(
      _lines(_dnsForeign.text),
    );
    final vpn =
        c
          ..name = _name.text.trim().isEmpty ? '未命名' : _name.text.trim()
          ..proxyPass = _proxyPass.text.trim()
          ..proxyDomains = domains
          ..proxyCidr = cidrs
          ..dns = _lines(_dns.text)
          ..dnsForeign = foreignIps
          ..dnsForeignDoh = foreignDoh
          ..dnsCache = _dnsCache
          ..noIpv6 = _noIpv6
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

  /// 展开输入中的列表 URL: 逐行判断, http(s):// 开头的行拉取远程
  /// 列表内容（每行一项, 跳过空行与 # 注释行）, 其余行原样保留.
  Future<List<String>> _expandUrlLines(List<String> lines) async {
    final out = <String>[];
    for (final line in lines) {
      if (line.startsWith('http://') || line.startsWith('https://')) {
        out.addAll(await _fetchList(line));
      } else {
        out.add(line);
      }
    }
    return out;
  }

  Future<List<String>> _fetchList(String url) async {
    final client = HttpClient()
      ..connectionTimeout = const Duration(seconds: 15);
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
            TextButton(
              onPressed: _saving ? null : _save,
              child: Text(_saving ? '拉取中…' : '保存'),
            ),
          ],
          bottom: const TabBar(
            tabs: [
              Tab(text: '基本'),
              Tab(text: 'DNS 配置'),
              Tab(text: '分流'),
            ],
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
            hintText: '******host:443',
            border: OutlineInputBorder(borderRadius: BorderRadius.zero),
          ),
        ),
        const SizedBox(height: 12),
        SwitchListTile(
          contentPadding: EdgeInsets.zero,
          title: const Text('关闭上游证书校验'),
          subtitle: const Text('自签证书场景启用'),
          value: _disableCheckCert,
          onChanged: (v) => setState(() => _disableCheckCert = v),
        ),
        const SizedBox(height: 12),
        TextField(
          controller: _testUrl,
          decoration: const InputDecoration(
            labelText: '测试连接 URL',
            hintText: 'https://google.com',
            border: OutlineInputBorder(borderRadius: BorderRadius.zero),
          ),
        ),
      ],
    );
  }

  Widget _buildDnsTab(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Text(
          '国内 DNS 用于未命中分流规则的域名直连解析，国外 DNS/DoH '
          '用于命中分流规则的域名经代理转发解析',
          style: Theme.of(context).textTheme.bodySmall,
        ),
        const SizedBox(height: 12),
        TextField(
          controller: _dns,
          maxLines: 2,
          decoration: const InputDecoration(
            labelText: '国内 DNS (每行一个 IP)',
            hintText: '223.6.6.6\n119.29.29.29',
            helperText: '国内域名直连解析',
            border: OutlineInputBorder(borderRadius: BorderRadius.zero),
          ),
        ),
        const SizedBox(height: 12),
        TextField(
          controller: _dnsForeign,
          maxLines: 2,
          decoration: const InputDecoration(
            labelText: '国外 DNS / DoH (每行一个)',
            hintText: '8.8.8.8\nhttps://dns.google/dns-query',
            helperText: 'IP 为 DNS 服务器, http(s):// 开头为 DoH',
            border: OutlineInputBorder(borderRadius: BorderRadius.zero),
          ),
        ),
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
          subtitle: const Text('AAAA 查询直接返回空应答，避免上游 DoH 不支持 IPv6 时每次等待'),
          value: _noIpv6,
          onChanged: (v) => setState(() => _noIpv6 = v),
        ),
      ],
    );
  }

  Widget _buildProxyTab(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        TextField(
          controller: _proxyDomains,
          maxLines: 4,
          decoration: const InputDecoration(
            labelText: '代理域名 (每行一个, 支持列表 URL)',
            hintText: 'google.com\nyoutube.com\nhttps://example.com/proxy.txt',
            border: OutlineInputBorder(borderRadius: BorderRadius.zero),
          ),
        ),
        const SizedBox(height: 12),
        TextField(
          controller: _proxyCidr,
          maxLines: 4,
          decoration: const InputDecoration(
            labelText: '代理 CIDR (每行一个, 支持列表 URL)',
            hintText: '1.1.1.0/24\nhttps://example.com/cidr.txt',
            border: OutlineInputBorder(borderRadius: BorderRadius.zero),
          ),
        ),
        const SizedBox(height: 12),
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
}
