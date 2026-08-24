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
  late final TextEditingController _tunMtu = TextEditingController(
    text: c.tunMtu.toString(),
  );
  late final TextEditingController _tunAddress = TextEditingController(
    text: c.tunAddress,
  );
  late final TextEditingController _tunPrefix = TextEditingController(
    text: c.tunPrefix.toString(),
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

  @override
  void dispose() {
    for (final t in [
      _name,
      _proxyPass,
      _tunMtu,
      _tunAddress,
      _tunPrefix,
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

  int _toInt(String v, int def) => int.tryParse(v.trim()) ?? def;

  List<String> _lines(String v) =>
      v
          .split(RegExp(r'[\r\n,;]+'))
          .map((s) => s.trim())
          .where((s) => s.isNotEmpty)
          .toList();

  void _save() {
    final (foreignIps, foreignDoh) = VpnConfig.splitForeignDns(
      _lines(_dnsForeign.text),
    );
    final vpn =
        c
          ..name = _name.text.trim().isEmpty ? '未命名' : _name.text.trim()
          ..proxyPass = _proxyPass.text.trim()
          ..tunMtu = _toInt(_tunMtu.text, 1400)
          ..tunAddress = _tunAddress.text.trim()
          ..tunPrefix = _toInt(_tunPrefix.text, 0)
          ..proxyDomains = _lines(_proxyDomains.text)
          ..proxyCidr = _lines(_proxyCidr.text)
          ..dns = _lines(_dns.text)
          ..dnsForeign = foreignIps
          ..dnsForeignDoh = foreignDoh
          ..dnsCache = _dnsCache
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

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.isNew ? '添加配置' : '编辑配置'),
        actions: [TextButton(onPressed: _save, child: const Text('保存'))],
      ),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _section('基本'),
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
              hintText: 'https://user:pass@host:443',
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

          _section('DNS'),
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

          _section('TUN 网络'),
          Row(
            children: [
              Expanded(
                child: TextField(
                  controller: _tunAddress,
                  decoration: const InputDecoration(
                    labelText: 'TUN 地址',
                    hintText: '10.0.0.2',
                    border: OutlineInputBorder(borderRadius: BorderRadius.zero),
                  ),
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: TextField(
                  controller: _tunPrefix,
                  keyboardType: TextInputType.number,
                  inputFormatters: [FilteringTextInputFormatter.digitsOnly],
                  decoration: const InputDecoration(
                    labelText: '前缀',
                    border: OutlineInputBorder(borderRadius: BorderRadius.zero),
                  ),
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          TextField(
            controller: _tunMtu,
            keyboardType: TextInputType.number,
            inputFormatters: [FilteringTextInputFormatter.digitsOnly],
            decoration: const InputDecoration(
              labelText: 'MTU',
              border: OutlineInputBorder(borderRadius: BorderRadius.zero),
            ),
          ),

          _section('分流规则'),
          TextField(
            controller: _proxyDomains,
            maxLines: 4,
            decoration: const InputDecoration(
              labelText: '代理域名 (每行一个, 后缀匹配)',
              hintText: 'google.com\nyoutube.com',
              border: OutlineInputBorder(borderRadius: BorderRadius.zero),
            ),
          ),
          const SizedBox(height: 12),
          TextField(
            controller: _proxyCidr,
            maxLines: 4,
            decoration: const InputDecoration(
              labelText: '代理 CIDR (每行一个)',
              hintText: '1.1.1.0/24',
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

          _section('测试'),
          TextField(
            controller: _testUrl,
            decoration: const InputDecoration(
              labelText: '测试连接 URL',
              hintText: 'https://google.com',
              border: OutlineInputBorder(borderRadius: BorderRadius.zero),
            ),
          ),
        ],
      ),
    );
  }

  Widget _section(String title) {
    return Padding(
      padding: const EdgeInsets.only(top: 16, bottom: 8),
      child: Text(
        title,
        style: Theme.of(context).textTheme.titleMedium?.copyWith(
          fontWeight: FontWeight.bold,
        ),
      ),
    );
  }
}
