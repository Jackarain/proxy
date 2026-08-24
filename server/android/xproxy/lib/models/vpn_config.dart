import 'dart:convert';
import 'dart:io';
import 'dart:math';

/// 一条 proxy 配置.
///
/// 同时包含 proxy 原生配置字段 (proxy_pass/proxy_domains/proxy_cidr 等,
/// 通过 json 传入 libxproxy.so) 与 Android VpnService 专用字段
/// (tunAddress/dns 等).
class VpnConfig {
  VpnConfig({
    required this.id,
    required this.name,
    this.proxyPass = '',
    this.tunMtu = 1400,
    List<String>? proxyDomains,
    List<String>? proxyCidr,
    this.disableCheckCert = true,
    this.udpTimeout = 300,
    this.tunAddress = '',
    this.tunPrefix = 0,
    List<String>? dns,
    List<String>? dnsForeign,
    this.dnsForeignDoh = '',
    this.testUrl = 'https://google.com',
    this.bypassCn = false,
  }) : proxyDomains = proxyDomains ?? [],
       proxyCidr = proxyCidr ?? [],
       dns = (dns == null || dns.isEmpty) ? ['223.6.6.6', '119.29.29.29'] : dns,
       dnsForeign = (dnsForeign == null || dnsForeign.isEmpty)
           ? ['8.8.8.8', '1.1.1.1']
           : dnsForeign;

  String id;
  String name;

  // ---- proxy 原生配置 ----
  /// 上游代理地址, 如 https://user:pass@host:443.
  String proxyPass;

  /// TUN 设备 MTU.
  int tunMtu;

  /// 代理域名列表 (后缀匹配): 命中走上游代理, 未命中本地直连.
  List<String> proxyDomains;

  /// 代理 CIDR 列表 (IPv4/IPv6): 命中走上游代理, 未命中直连.
  List<String> proxyCidr;

  /// 关闭上游代理的证书校验 (自签证书场景).
  bool disableCheckCert;

  /// UDP 流过期时间 (秒), 经 udp_timeout 传给 native; 默认 300.
  int udpTimeout;

  // ---- Android VpnService ----
  String tunAddress;
  int tunPrefix;

  /// 国内 DNS 列表（仅 IP），经 addDnsServer 注入，且未命中
  /// proxy_domains 的查询由 native 直连这些服务器解析.
  List<String> dns;

  /// 国外 DNS 列表（仅 IP），命中 proxy_domains 的查询经
  /// proxy_pass 代理转发到这些服务器解析.
  List<String> dnsForeign;

  /// 国外 DoH URL（可选，如 https://dns.google/dns-query），命中
  /// proxy_domains 的查询以 DoH 方式解析.
  String dnsForeignDoh;

  // ---- UI 工具 ----
  String testUrl; // 测试连接的 URL, 用于运行页测量 VPN 延迟.

  /// 绕过中国大陆: 拉取中国 IP 段, 仅将非中国段接入 VPN,
  /// 中国大陆流量走系统物理网络直连.
  bool bypassCn;

  /// 生成新的配置 id (时间戳 + 随机后缀).
  static String newId() {
    final rand = Random.secure();
    final ts = DateTime.now().microsecondsSinceEpoch.toRadixString(16);
    final suffix =
        List.generate(
          8,
          (_) => rand.nextInt(0x100).toRadixString(16).padLeft(2, '0'),
        ).join();
    return '$ts$suffix';
  }

  /// 本端 tun 地址与前缀, 未配置时默认 10.0.0.2/24.
  (String, int) deriveTun() {
    if (tunAddress.trim().isNotEmpty && tunPrefix > 0) {
      return (tunAddress.trim(), tunPrefix);
    }
    return ('10.0.0.2', 24);
  }

  /// 校验配置, 返回错误描述列表; 为空表示可运行.
  List<String> validate() {
    final errors = <String>[];
    final pass = proxyPass.trim();
    if (pass.isEmpty) {
      errors.add('请填写上游代理地址');
    } else if (!_isValidProxyUrl(pass)) {
      errors.add('代理地址需以 http(s):// 或 socks(4/5):// 开头');
    }
    if (tunMtu <= 0 || tunMtu > 65535) {
      errors.add('MTU 需在 1-65535 之间');
    }
    if (udpTimeout <= 0) {
      errors.add('UDP 超时需大于 0');
    }
    final badDomestic = dns
        .where((d) => InternetAddress.tryParse(d.trim()) == null)
        .toList();
    if (badDomestic.isNotEmpty) {
      errors.add('国内 DNS 仅支持 IP 地址: ${badDomestic.join(', ')}');
    }
    final badForeign = dnsForeign
        .where((d) => InternetAddress.tryParse(d.trim()) == null)
        .toList();
    if (badForeign.isNotEmpty) {
      errors.add('国外 DNS 仅支持 IP 地址: ${badForeign.join(', ')}');
    }
    final doh = dnsForeignDoh.trim();
    if (doh.isNotEmpty &&
        !doh.startsWith('https://') &&
        !doh.startsWith('http://')) {
      errors.add('国外 DoH 需以 http:// 或 https:// 开头');
    }
    final test = testUrl.trim();
    if (test.isEmpty ||
        (!test.startsWith('https://') && !test.startsWith('http://'))) {
      errors.add('测试连接需以 http:// 或 https:// 开头');
    }
    return errors;
  }

  static bool _isValidProxyUrl(String url) {
    return url.startsWith('http://') ||
        url.startsWith('https://') ||
        url.startsWith('socks4://') ||
        url.startsWith('socks5://');
  }

  VpnConfig copy() => VpnConfig.fromJson(toJson());

  /// 传给 libxproxy.so 的启动配置 json (键名与 xproxy 配置解析一致).
  String toProxyJson({int launcherPort = 0}) {
    final map = <String, dynamic>{
      'proxy_pass': proxyPass.trim(),
      'tun': true,
      'tun_mtu': tunMtu,
      'tun_wait_fd': true,
      if (proxyDomains.isNotEmpty) 'proxy_domains': proxyDomains,
      if (proxyCidr.isNotEmpty) 'proxy_cidr': proxyCidr,
      'disable_check_cert': disableCheckCert,
      'udp_timeout': udpTimeout,
      'dns_domestic': dns,
      'dns_foreign': dnsForeign,
      if (dnsForeignDoh.trim().isNotEmpty) 'dns_doh': dnsForeignDoh.trim(),
      if (launcherPort > 0) 'launcher_url': 'ws://127.0.0.1:$launcherPort',
    };
    return jsonEncode(map);
  }

  /// 运行期热更新参数 (经控制通道 set_config 下发, 键与 xproxy 配置一致).
  Map<String, dynamic> toProxyOptions() {
    final map = <String, dynamic>{
      'proxy_pass': proxyPass.trim(),
      'proxy_domains': proxyDomains,
      'proxy_cidr': proxyCidr,
      'disable_check_cert': disableCheckCert,
    };
    return map;
  }

  Map<String, dynamic> toJson() => {
    'id': id,
    'name': name,
    'proxyPass': proxyPass,
    'tunMtu': tunMtu,
    'proxyDomains': proxyDomains,
    'proxyCidr': proxyCidr,
    'disableCheckCert': disableCheckCert,
    'udpTimeout': udpTimeout,
    'tunAddress': tunAddress,
    'tunPrefix': tunPrefix,
    'dns': dns,
    'dnsForeign': dnsForeign,
    'dnsForeignDoh': dnsForeignDoh,
    'testUrl': testUrl,
    'bypassCn': bypassCn,
  };

  factory VpnConfig.fromJson(Map<String, dynamic> json) => VpnConfig(
    id: json['id'] as String? ?? VpnConfig.newId(),
    name: json['name'] as String? ?? '未命名',
    proxyPass: json['proxyPass'] as String? ?? '',
    tunMtu: json['tunMtu'] as int? ?? 1400,
    proxyDomains: _strList(json['proxyDomains']),
    proxyCidr: _strList(json['proxyCidr']),
    disableCheckCert: json['disableCheckCert'] as bool? ?? true,
    udpTimeout: json['udpTimeout'] as int? ?? 300,
    tunAddress: json['tunAddress'] as String? ?? '',
    tunPrefix: json['tunPrefix'] as int? ?? 0,
    dns: _strList(json['dns']),
    dnsForeign: _strList(json['dnsForeign']),
    dnsForeignDoh: json['dnsForeignDoh'] as String? ?? '',
    testUrl: json['testUrl'] as String? ?? 'https://google.com',
    bypassCn: json['bypassCn'] as bool? ?? false,
  );

  static List<String> _strList(dynamic v) {
    if (v is List) return v.whereType<String>().toList();
    if (v is String && v.trim().isNotEmpty) {
      return v.trim().split(RegExp(r'[\s,;]+'));
    }
    return [];
  }
}
