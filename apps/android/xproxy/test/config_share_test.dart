import 'package:flutter_test/flutter_test.dart';
import 'package:xproxy/models/vpn_config.dart';
import 'package:xproxy/services/config_share.dart';

void main() {
  test('分享内容保留配置字段并可重新解析', () {
    final config = VpnConfig(
      id: '1',
      name: '办公室',
      proxyPass: 'https://user:pass@1.2.3.4:443',
      sni: 'proxy.example.com',
      proxyDomains: ['a.com', 'b.com'],
      proxyCidr: ['1.1.1.0/24'],
      dnsForeign: ['8.8.8.8'],
      dnsForeignDoh: 'https://dns.google/dns-query',
      disableCheckCert: false,
      bypassCn: true,
    );

    final decoded = decodeConfigShare(encodeConfigShare(config));

    expect(decoded.name, '办公室');
    expect(decoded.proxyPass, 'https://user:pass@1.2.3.4:443');
    expect(decoded.sni, 'proxy.example.com');
    expect(decoded.proxyDomains, ['a.com', 'b.com']);
    expect(decoded.proxyCidr, ['1.1.1.0/24']);
    expect(decoded.dnsForeign, ['8.8.8.8']);
    expect(decoded.dnsForeignDoh, 'https://dns.google/dns-query');
    expect(decoded.disableCheckCert, isFalse);
    expect(decoded.bypassCn, isTrue);
  });

  test('非 URL 来源的分流列表超过 10 条时只保留前 10 条', () {
    final config = VpnConfig(
      id: '2',
      name: '大列表',
      proxyDomains: List.generate(30, (i) => 'd$i.com'),
      proxyCidr: List.generate(15, (i) => '10.0.$i.0/24'),
    );

    final decoded = decodeConfigShare(encodeConfigShare(config));

    expect(decoded.proxyDomains, List.generate(10, (i) => 'd$i.com'));
    expect(decoded.proxyCidr, List.generate(10, (i) => '10.0.$i.0/24'));
  });

  test('URL 来源的分流列表只保留 URL', () {
    final config = VpnConfig(
      id: '3',
      name: 'URL 列表',
      proxyDomains: ['a.com', 'b.com'],
      proxyDomainsUrl: 'https://example.com/domains.txt',
      proxyCidr: ['1.1.1.0/24'],
      proxyCidrUrl: 'https://example.com/cidr.txt',
    );

    final decoded = decodeConfigShare(encodeConfigShare(config));

    expect(decoded.proxyDomains, isEmpty);
    expect(decoded.proxyCidr, isEmpty);
    expect(decoded.proxyDomainsUrl, 'https://example.com/domains.txt');
    expect(decoded.proxyCidrUrl, 'https://example.com/cidr.txt');
  });

  test('非本应用二维码被拒绝', () {
    expect(
      () => decodeConfigShare('{"type":"other"}'),
      throwsA(isA<FormatException>()),
    );
    expect(
      () => decodeConfigShare('not a json'),
      throwsA(isA<FormatException>()),
    );
  });
}
