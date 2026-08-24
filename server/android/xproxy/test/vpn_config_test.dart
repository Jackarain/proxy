import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:xproxy/models/vpn_config.dart';
import 'package:xproxy/services/storage_service.dart';

void main() {
  group('VpnConfig', () {
    test('json 往返保持一致', () {
      final c = VpnConfig(
        id: 'abc',
        name: '测试',
        proxyPass: 'https://user:pass@host:443',
        tunMtu: 1400,
        proxyDomains: ['google.com', 'youtube.com'],
        proxyCidr: ['1.1.1.0/24'],
        disableCheckCert: true,
        tunAddress: '10.0.0.2',
        tunPrefix: 24,
        dns: ['8.8.8.8'],
        dnsForeign: ['1.1.1.1'],
        dnsForeignDoh: 'https://dns.google/dns-query',
        noIpv6: false,
        testUrl: 'https://google.com',
        bypassCn: true,
      );
      final restored = VpnConfig.fromJson(c.toJson());
      expect(restored.id, c.id);
      expect(restored.name, c.name);
      expect(restored.proxyPass, c.proxyPass);
      expect(restored.tunMtu, 1400);
      expect(restored.proxyDomains, ['google.com', 'youtube.com']);
      expect(restored.proxyCidr, ['1.1.1.0/24']);
      expect(restored.disableCheckCert, true);
      expect(restored.dns, ['8.8.8.8']);
      expect(restored.dnsForeign, ['1.1.1.1']);
      expect(restored.dnsForeignDoh, 'https://dns.google/dns-query');
      expect(restored.noIpv6, false);
      expect(restored.bypassCn, true);
    });

    test('国内/国外 DNS 默认值与旧配置迁移', () {
      final def = VpnConfig(id: '1', name: 'c');
      expect(def.dns, ['223.6.6.6', '119.29.29.29']);
      expect(def.dnsForeign, isEmpty);
      expect(def.dnsForeignDoh, isEmpty);

      // 旧配置无新字段时回退默认值.
      final legacy = VpnConfig.fromJson({'id': '2', 'name': 'c'});
      expect(legacy.dns, ['223.6.6.6', '119.29.29.29']);
      expect(legacy.dnsForeign, isEmpty);

      // 旧配置已有 dns 时原样迁移为国内 DNS.
      final migrated = VpnConfig.fromJson({
        'id': '3',
        'name': 'c',
        'dns': ['114.114.114.114'],
      });
      expect(migrated.dns, ['114.114.114.114']);
    });

    test('toProxyJson 含 proxy 原生键与 launcher_url', () {
      final c = VpnConfig(
        id: 'abc',
        name: '测试',
        proxyPass: 'https://user:pass@host:443',
        tunMtu: 1400,
        proxyDomains: ['google.com'],
        proxyCidr: ['1.1.1.0/24'],
        dns: ['223.6.6.6'],
        dnsForeign: ['8.8.8.8'],
        dnsForeignDoh: 'https://dns.google/dns-query',
      );
      final map =
          jsonDecode(c.toProxyJson(launcherPort: 12345)) as Map<String, dynamic>;
      expect(map['proxy_pass'], 'https://user:pass@host:443');
      expect(map['tun'], true);
      expect(map['tun_wait_fd'], true);
      expect(map['tun_mtu'], 1400);
      expect(map['proxy_domains'], ['google.com']);
      expect(map['proxy_cidr'], ['1.1.1.0/24']);
      expect(map['dns_domestic'], ['223.6.6.6']);
      expect(map['dns_foreign'], ['8.8.8.8']);
      expect(map['dns_doh'], 'https://dns.google/dns-query');
      expect(map['dns_cache_size'], 4096);
      expect(map['dns_cache_ttl'], 300);
      expect(map['dns_no_ipv6'], true);
      expect(map['launcher_url'], 'ws://127.0.0.1:12345');
    });

    test('禁用 IPv6 解析默认启用并随 json 迁移', () {
      final def = VpnConfig(id: '1', name: 'c');
      expect(def.noIpv6, isTrue);
      final restored = VpnConfig.fromJson(def.toJson());
      expect(restored.noIpv6, isTrue);
      // 旧配置无该字段时回退默认启用.
      final legacy = VpnConfig.fromJson({'id': '2', 'name': 'c'});
      expect(legacy.noIpv6, isTrue);
    });

    test('关闭禁用 IPv6 解析时不下发 dns_no_ipv6', () {
      final c = VpnConfig(
        id: 'abc',
        name: '测试',
        proxyPass: 'https://user:pass@host:443',
        noIpv6: false,
      );
      final map = jsonDecode(c.toProxyJson()) as Map<String, dynamic>;
      expect(map.containsKey('dns_no_ipv6'), isFalse);
    });

    test('关闭 DNS 缓存时不下发缓存参数', () {
      final c = VpnConfig(
        id: 'abc',
        name: '测试',
        proxyPass: 'https://user:pass@host:443',
        dnsCache: false,
      );
      final map =
          jsonDecode(c.toProxyJson()) as Map<String, dynamic>;
      expect(map.containsKey('dns_cache_size'), isFalse);
      expect(map.containsKey('dns_cache_ttl'), isFalse);
    });

    test('DNS 缓存默认启用', () {
      final def = VpnConfig(id: '1', name: 'c');
      expect(def.dnsCache, isTrue);
      final restored = VpnConfig.fromJson(def.toJson());
      expect(restored.dnsCache, isTrue);
    });

    test('deriveTun 默认与配置取值', () {
      final def = VpnConfig(id: '1', name: 'c');
      expect(def.deriveTun(), ('10.0.0.2', 24));

      final custom = VpnConfig(
        id: '2',
        name: 'c',
        tunAddress: '172.16.0.2',
        tunPrefix: 16,
      );
      expect(custom.deriveTun(), ('172.16.0.2', 16));
    });

    test('校验: proxy_pass 必填且 scheme 合法', () {
      final empty = VpnConfig(id: '1', name: 'a');
      expect(empty.validate(), isNotEmpty);

      final bad = VpnConfig(
        id: '2',
        name: 'b',
        proxyPass: 'not-a-url',
      );
      expect(bad.validate(), isNotEmpty);

      final good = VpnConfig(
        id: '3',
        name: 'c',
        proxyPass: 'https://user:pass@host:443',
      );
      expect(good.validate(), isEmpty);

      final socks5s = VpnConfig(
        id: '4',
        name: 'd',
        proxyPass: 'socks5s://user:pass@host:1080',
      );
      expect(socks5s.validate(), isEmpty);
    });

    test('校验: DNS 仅支持 IP 且 DoH 需为 URL', () {
      final badIp = VpnConfig(
        id: '1',
        name: 'a',
        proxyPass: 'https://user:pass@host:443',
        dns: ['not-an-ip'],
      );
      expect(badIp.validate(), isNotEmpty);

      final badForeign = VpnConfig(
        id: '2',
        name: 'b',
        proxyPass: 'https://user:pass@host:443',
        dnsForeign: ['example.com'],
      );
      expect(badForeign.validate(), isNotEmpty);

      final badDoh = VpnConfig(
        id: '3',
        name: 'c',
        proxyPass: 'https://user:pass@host:443',
        dnsForeignDoh: 'dns.google/dns-query',
      );
      expect(badDoh.validate(), isNotEmpty);

      final good = VpnConfig(
        id: '4',
        name: 'd',
        proxyPass: 'https://user:pass@host:443',
        dns: ['223.6.6.6'],
        dnsForeign: ['8.8.8.8'],
        dnsForeignDoh: 'https://dns.google/dns-query',
      );
      expect(good.validate(), isEmpty);
    });

    test('字符串列表解析支持换行/逗号/分号', () {
      final c = VpnConfig.fromJson({
        'proxyDomains': 'google.com\nyoutube.com,example.com;test.com',
      });
      expect(c.proxyDomains, ['google.com', 'youtube.com', 'example.com', 'test.com']);
    });

    test('国外 DNS/DoH 单输入框拆分与合并', () {
      final (ips, doh) = VpnConfig.splitForeignDns(
        ['8.8.8.8', '1.1.1.1', 'https://dns.google/dns-query', ''],
      );
      expect(ips, ['8.8.8.8', '1.1.1.1']);
      expect(doh, 'https://dns.google/dns-query');

      final c = VpnConfig(
        id: '1',
        name: 'c',
        dnsForeign: ['8.8.8.8', '1.1.1.1'],
        dnsForeignDoh: 'https://dns.google/dns-query',
      );
      expect(
        c.joinForeignDns(),
        ['8.8.8.8', '1.1.1.1', 'https://dns.google/dns-query'],
      );

      final noDoh = VpnConfig(id: '2', name: 'c', dnsForeign: ['8.8.8.8']);
      expect(noDoh.joinForeignDns(), ['8.8.8.8']);
    });
  });

  group('StorageService', () {
    test('配置持久化往返', () async {
      SharedPreferences.setMockInitialValues({});
      final storage = StorageService();
      final list = [
        VpnConfig(id: '1', name: 'a', proxyPass: 'https://x:1'),
        VpnConfig(id: '2', name: 'b', proxyPass: 'socks5://y:2'),
      ];
      await storage.saveConfigs(list);
      final loaded = await storage.loadConfigs();
      expect(loaded.length, 2);
      expect(loaded[0].name, 'a');
      expect(loaded[1].proxyPass, 'socks5://y:2');
    });

    test('运行状态保存/清理', () async {
      SharedPreferences.setMockInitialValues({});
      final storage = StorageService();
      expect(await storage.loadRunState(), isNull);
      await storage.saveRunState('cfg1', 9999);
      final state = await storage.loadRunState();
      expect(state, ('cfg1', 9999));
      await storage.clearRunState();
      expect(await storage.loadRunState(), isNull);
    });
  });
}
