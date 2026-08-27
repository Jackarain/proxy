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
    this.tunMtu = 1500,
    List<String>? proxyDomains,
    List<String>? proxyCidr,
    this.disableCheckCert = true,
    this.udpTimeout = 300,
    this.proxyPassPoolSize = 20,
    this.tunAddress = '',
    this.tunPrefix = 0,
    List<String>? dns,
    List<String>? dnsForeign,
    this.dnsForeignDoh = '',
    this.dnsCache = true,
    this.dnsCacheSize = 200,
    this.dnsCacheTtl = 60,
    this.noIpv6 = true,
    this.testUrl = 'https://www.google.com',
    this.bypassCn = false,
  }) : proxyDomains = proxyDomains ?? [],
       proxyCidr = proxyCidr ?? [],
       dns = (dns == null || dns.isEmpty) ? ['223.6.6.6', '119.29.29.29'] : dns,
       dnsForeign = dnsForeign ?? [];

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

  /// proxy_pass 预选连接池大小, 经 proxy_pass_pool_size 传给 native;
  /// 启动后每 5 秒建立 1 条到上游的 TCP(+TLS) 连接, 取走/断开后立即补充;
  /// 0 表示禁用连接池. 默认 20.
  int proxyPassPoolSize;

  // ---- Android VpnService ----
  String tunAddress;
  int tunPrefix;

  /// 国内 DNS 列表（仅 IP），经 addDnsServer 注入，且未命中
  /// proxy_domains 的查询由 native 直连这些服务器解析.
  List<String> dns;

  /// 国外 DNS 列表（仅 IP），命中 proxy_domains 的查询经
  /// proxy_pass 代理转发到这些服务器解析. addDnsServer 始终
  /// 固定注入 8.8.8.8/1.1.1.1 与本配置无关，留空时 native
  /// 不替换查询目标，保持注入的固定地址.
  List<String> dnsForeign;

  /// 国外 DoH URL（可选，如 https://dns.google/dns-query），命中
  /// proxy_domains 的查询以 DoH 方式解析.
  String dnsForeignDoh;

  /// 是否启用 DNS 查询结果缓存（native 按域名缓存解析结果，命中直接回包）.
  bool dnsCache;

  /// DNS 缓存条数上限 (LRU 淘汰, 最近使用的优先保留).
  int dnsCacheSize;

  /// DNS 缓存超时 (分钟); 命中缓存时刷新超时重新计时.
  int dnsCacheTtl;

  /// 是否禁用 IPv6 解析：AAAA 查询直接返回空应答，不转发上游
  /// （上游 DoH 不支持 IPv6 时可避免每次查询的等待延迟）.
  bool noIpv6;

  /// 国外 DNS/DoH 输入行拆分: IP 归入国外 DNS 列表, http(s):// 开头
  /// 视为 DoH URL（多个 DoH 时取最后一个）.
  static (List<String>, String) splitForeignDns(List<String> lines) {
    final ips = <String>[];
    String doh = '';
    for (final line in lines) {
      final s = line.trim();
      if (s.isEmpty) continue;
      if (s.startsWith('http://') || s.startsWith('https://')) {
        doh = s;
      } else {
        ips.add(s);
      }
    }
    return (ips, doh);
  }

  /// 国外 DNS/DoH 展示合并行（DoH 追加在末尾）.
  List<String> joinForeignDns() =>
      [...dnsForeign, if (dnsForeignDoh.isNotEmpty) dnsForeignDoh];

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
      errors.add('代理地址需以 http(s):// 或 socks(4/5/5s):// 开头');
    }
    if (tunMtu <= 0 || tunMtu > 65535) {
      errors.add('MTU 需在 1-65535 之间');
    }
    if (udpTimeout <= 0) {
      errors.add('UDP 超时需大于 0');
    }
    if (proxyPassPoolSize < 0) {
      errors.add('连接池大小不能为负数');
    }
    if (dnsCacheSize < 0) {
      errors.add('DNS 缓存条数不能为负数');
    }
    if (dnsCacheTtl <= 0) {
      errors.add('DNS 缓存超时需大于 0');
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
        url.startsWith('socks5://') ||
        url.startsWith('socks5s://');
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
      if (proxyPassPoolSize > 0) 'proxy_pass_pool_size': proxyPassPoolSize,
      'dns_domestic': dns,
      'dns_foreign': dnsForeign,
      if (dnsForeignDoh.trim().isNotEmpty) 'dns_doh': dnsForeignDoh.trim(),
      if (dnsCache) 'dns_cache_size': dnsCacheSize,
      if (dnsCache) 'dns_cache_ttl': dnsCacheTtl * 60,
      if (noIpv6) 'dns_no_ipv6': true,
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
    'proxyPassPoolSize': proxyPassPoolSize,
    'tunAddress': tunAddress,
    'tunPrefix': tunPrefix,
    'dns': dns,
    'dnsForeign': dnsForeign,
    'dnsForeignDoh': dnsForeignDoh,
    'dnsCache': dnsCache,
    'dnsCacheSize': dnsCacheSize,
    'dnsCacheTtl': dnsCacheTtl,
    'noIpv6': noIpv6,
    'testUrl': testUrl,
    'bypassCn': bypassCn,
  };

  factory VpnConfig.fromJson(Map<String, dynamic> json) => VpnConfig(
    id: json['id'] as String? ?? VpnConfig.newId(),
    name: json['name'] as String? ?? '未命名',
    proxyPass: json['proxyPass'] as String? ?? '',
    tunMtu: json['tunMtu'] as int? ?? 1500,
    proxyDomains: _strList(json['proxyDomains']),
    proxyCidr: _strList(json['proxyCidr']),
    disableCheckCert: json['disableCheckCert'] as bool? ?? true,
    udpTimeout: json['udpTimeout'] as int? ?? 300,
    proxyPassPoolSize: json['proxyPassPoolSize'] as int? ?? 20,
    tunAddress: json['tunAddress'] as String? ?? '',
    tunPrefix: json['tunPrefix'] as int? ?? 0,
    dns: _strList(json['dns']),
    dnsForeign: _strList(json['dnsForeign']),
    dnsForeignDoh: json['dnsForeignDoh'] as String? ?? '',
    dnsCache: json['dnsCache'] as bool? ?? true,
    dnsCacheSize: json['dnsCacheSize'] as int? ?? 200,
    dnsCacheTtl: json['dnsCacheTtl'] as int? ?? 60,
    noIpv6: json['noIpv6'] as bool? ?? true,
    testUrl: json['testUrl'] as String? ?? 'https://www.google.com',
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
