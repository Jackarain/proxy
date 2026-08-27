/// 分流路由表: 判断目标 IP 应走上游代理还是直连 (绕过)。
///
/// 语义与 libproxy 一致:
/// - 配置了 proxy_pass 且无任何分流规则 (proxy_cidr/proxy_domains 均空)
///   时, 默认全部流量走代理。
/// - proxy_cidr 命中 => 走代理。
/// - DNS 查询解析出的域名命中 proxy_domains => 其解析 IP 走代理 (由
///   DNS 引擎调用 [markDomainMode] 填充 [markResolvedIp])。
/// - 其余情况 => 直连。
/// - bypassCn 时, 中国大陆/私有段地址强制直连。
class RouteTable {
  RouteTable({
    List<String> proxyDomains = const [],
    List<String> proxyCidr = const [],
    bool bypassCn = false,
    bool hasProxy = false,
  }) {
    setDomainsAndCidr(proxyDomains, proxyCidr);
    _bypassCn = bypassCn;
    _hasProxy = hasProxy;
  }

  bool _bypassCn = false;
  bool _hasProxy = false;
  final List<(int, int)> _cidrs = [];
  final List<String> _domains = [];
  List<(int, int)> _cnMerged = [];
  final Map<int, bool> _ipMode = {}; // ip -> proxied?

  void setDomainsAndCidr(List<String> domains, List<String> cidrs) {
    _domains.clear();
    for (final d in domains) {
      final t = d.trim().toLowerCase();
      if (t.isNotEmpty) _domains.add(t);
    }
    _cidrs.clear();
    for (final c in cidrs) {
      final r = cidrToRange(c);
      if (r != null) _cidrs.add(r);
    }
    _cidrs.sort((a, b) => a.$1.compareTo(b.$1));
  }

  void setHasProxy(bool v) => _hasProxy = v;
  void setBypassCn(bool v) => _bypassCn = v;

  /// 提供中国大陆 IP 段 (CIDR 字符串), 用于 bypassCn 分流。
  void setCnCidrs(List<String> cidrs) {
    final ranges = <(int, int)>[];
    for (final c in cidrs) {
      final r = cidrToRange(c);
      if (r != null) ranges.add(r);
    }
    ranges.sort((a, b) => a.$1.compareTo(b.$1));
    _cnMerged = ranges;
  }

  /// 记录 DNS 解析结果: 域名 [domain] 解析出的 IP 列表中, 若该域名命中
  /// proxy_domains 则标记为 proxied。
  void markDomainResolved(String domain, List<String> ips) {
    // 未配置域名分流时不覆盖默认规则 (默认全部经代理), 否则所有经
    // DNS 解析出的 IP 都会被标为"直连", 导致国外流量绕过代理超时.
    if (_domains.isEmpty) return;
    final proxied = _domainMatches(domain);
    for (final ip in ips) {
      final v4 = InternetAddressInt.parseV4(ip);
      if (v4 != null) _ipMode[v4] = proxied;
    }
  }

  /// 域名后缀匹配是否命中 proxy_domains。
  bool _domainMatches(String host) {
    final h = host.trim().toLowerCase();
    for (final d in _domains) {
      if (h == d) return true;
      if (h.length > d.length &&
          h.endsWith(d) &&
          h[h.length - d.length - 1] == '.') {
        return true;
      }
    }
    return false;
  }

  /// 判断目标 IP 是否该走代理。
  bool ipShouldProxy(String ip) {
    final v4 = InternetAddressInt.parseV4(ip);
    if (v4 == null) return _defaultProxy();
    // bypassCn: 中国大陆/私有段强制直连.
    if (_bypassCn && _containsRange(_cnMerged, v4)) return false;
    // 域名解析标记优先.
    final cached = _ipMode[v4];
    if (cached != null) return cached;
    if (_containsRange(_cidrs, v4)) return true;
    return _defaultProxy();
  }

  bool _defaultProxy() =>
      _hasProxy && _cidrs.isEmpty && _domains.isEmpty;

  static bool _containsRange(List<(int, int)> ranges, int ip) {
    int lo = 0, hi = ranges.length - 1;
    while (lo <= hi) {
      final mid = (lo + hi) >> 1;
      final r = ranges[mid];
      if (ip < r.$1) {
        hi = mid - 1;
      } else if (ip > r.$2) {
        lo = mid + 1;
      } else {
        return true;
      }
    }
    return false;
  }

  /// "a.b.c.d/prefix" -> (起始, 结束).
  static (int, int)? cidrToRange(String cidr) {
    final slash = cidr.indexOf('/');
    if (slash <= 0) return null;
    final ip = InternetAddressInt.parseV4(cidr.substring(0, slash).trim());
    final prefix = int.tryParse(cidr.substring(slash + 1).trim());
    if (ip == null || prefix == null || prefix < 0 || prefix > 32) {
      return null;
    }
    final mask = prefix == 0 ? 0 : (0xffffffff << (32 - prefix)) & 0xffffffff;
    final start = ip & mask;
    return (start, start | (0xffffffff & ~mask));
  }
}

/// IPv4 字符串 <-> int 的轻量工具。
class InternetAddressInt {
  static int? parseV4(String s) {
    final parts = s.split('.');
    if (parts.length != 4) return null;
    var v = 0;
    for (final p in parts) {
      final o = int.tryParse(p);
      if (o == null || o < 0 || o > 255) return null;
      v = (v << 8) | o;
    }
    return v;
  }

  static String toString4(int v) =>
      '${(v >> 24) & 0xff}.${(v >> 16) & 0xff}.${(v >> 8) & 0xff}.${v & 0xff}';
}
