import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'forwarder.dart';
import 'logging.dart';
import 'upstream.dart';

/// 轻量 DNS 消息解析与构造 (支持 A / AAAA / CNAME 的最小子集)。
class Dns {
  /// 从报文解析 qname。
  static String? parseQueryName(Uint8List pkt) {
    if (pkt.length < 12) return null;
    final qdcount = (pkt[4] << 8) | pkt[5];
    if (qdcount < 1) return null;
    final name = _readName(pkt, 12);
    if (name.name == null || name.name!.isEmpty) return null;
    return name.name;
  }

  /// 解析 qtype (1=A, 28=AAAA, 255=ANY).
  static int? parseQueryType(Uint8List pkt) {
    if (pkt.length < 14) return null;
    final len = _nameLen(pkt, 12);
    if (len < 0) return null;
    // qname 之后紧跟 qtype(2B), 再是 qclass(2B); 这里取 qtype.
    final qtypePos = 12 + len;
    if (qtypePos + 2 > pkt.length) return null;
    return (pkt[qtypePos] << 8) | pkt[qtypePos + 1];
  }

  /// 构造一条 DNS 响应报文 (NOERROR/NXDOMAIN, answer 区可带记录)。
  static Uint8List buildResponse({
    required Uint8List query,
    required int flags,
    List<DnsAnswer> answers = const [],
  }) {
    // 复制原查询问题区.
    final qdcount = (query[4] << 8) | query[5];
    final nameLen = _nameLen(query, 12);
    final questionLen = nameLen + 4;
    final header = 12;
    final out = Uint8List(header + questionLen + _answersLen(answers));
    out[0] = query[0];
    out[1] = query[1];
    out[2] = (flags >> 8) & 0xff;
    out[3] = flags & 0xff;
    out[4] = (qdcount >> 8) & 0xff;
    out[5] = qdcount & 0xff;
    out[6] = 0;
    out[7] = 0; // ancount (下方填)
    out[8] = 0;
    out[9] = 0;
    out[10] = 0;
    out[11] = 0;
    // 复制问题区.
    for (var i = 0; i < questionLen; i++) {
      out[header + i] = query[12 + i];
    }
    out[6] = (answers.length >> 8) & 0xff;
    out[7] = answers.length & 0xff;
    // 答案区.
    var off = header + questionLen;
    for (final a in answers) {
      final blen = a.rdata.length;
      out[off++] = 0xc0;
      out[off++] = 0x0c; // 指针回问题名.
      out[off++] = (a.type >> 8) & 0xff;
      out[off++] = a.type & 0xff;
      out[off++] = 0;
      out[off++] = 1; // class IN
      out[off++] = (a.ttl >> 24) & 0xff;
      out[off++] = (a.ttl >> 16) & 0xff;
      out[off++] = (a.ttl >> 8) & 0xff;
      out[off++] = a.ttl & 0xff;
      out[off++] = (blen >> 8) & 0xff;
      out[off++] = blen & 0xff;
      for (var i = 0; i < blen; i++) {
        out[off++] = a.rdata[i];
      }
    }
    return out;
  }

  static Uint8List buildNoErrorEmpty(Uint8List query) =>
      buildResponse(query: query, flags: 0x8180);

  static Uint8List buildNoIpv6(Uint8List query) =>
      buildResponse(query: query, flags: 0x8180);

  static int _answersLen(List<DnsAnswer> answers) {
    var n = 0;
    for (final a in answers) {
      n += 2 + 2 + 2 + 4 + 2 + a.rdata.length;
    }
    return n;
  }

  static ({String? name, int consumed}) _readName(Uint8List pkt, int off) {
    final sb = StringBuffer();
    var pos = off;
    final limit = pkt.length;
    if (pos >= limit) return (name: null, consumed: 0);
    var jump = false;
    var jumps = 0;
    while (true) {
      if (pos >= limit) return (name: sb.toString(), consumed: pos - off + (jump ? 2 : 0));
      final len = pkt[pos];
      if (len == 0) {
        if (!jump) pos++;
        break;
      }
      if ((len & 0xc0) == 0xc0) {
        if (pos + 1 >= limit) return (name: sb.toString(), consumed: pos - off);
        final ptr = ((len & 0x3f) << 8) | pkt[pos + 1];
        if (!jump) pos += 2;
        if (jumps++ < 8) pos = ptr;
        jump = true;
        continue;
      }
      if (pos + 1 + len > limit) return (name: sb.toString(), consumed: pos - off);
      if (sb.isNotEmpty) sb.write('.');
      for (var i = 0; i < len; i++) {
        sb.writeCharCode(pkt[pos + 1 + i]);
      }
      pos += 1 + len;
      if (jump) break;
    }
    return (name: sb.toString(), consumed: pos - off);
  }

  static int _nameLen(Uint8List pkt, int off) {
    var pos = off;
    final limit = pkt.length;
    var total = 0;
    if (pos >= limit) return -1;
    while (true) {
      final len = pkt[pos];
      if (len == 0) return total + 1;
      if ((len & 0xc0) == 0xc0) return total + 2;
      if (pos + 1 + len > limit) return -1;
      total += 1 + len;
      pos += 1 + len;
    }
  }

  /// 解析答案中的 A/AAAA 记录 (rdata 用于做 UDP 应答)。
  static List<Uint8List> parseAnswers(Uint8List pkt, int wantedType) {
    final out = <Uint8List>[];
    if (pkt.length < 12) return out;
    final qdcount = (pkt[4] << 8) | pkt[5];
    final ancount = (pkt[6] << 8) | pkt[7];
    var off = 12;
    for (var q = 0; q < qdcount; q++) {
      final len = _nameLen(pkt, off);
      if (len < 0) return out;
      off += len + 4;
    }
    for (var a = 0; a < ancount; a++) {
      // 跳过 name (可能是指针).
      final l0 = pkt[off];
      if ((l0 & 0xc0) == 0xc0) {
        off += 2;
      } else {
        final len = _nameLen(pkt, off);
        if (len < 0) return out;
        off += len;
      }
      if (off + 10 > pkt.length) return out;
      final type = (pkt[off] << 8) | pkt[off + 1];
      final rdlen = (pkt[off + 8] << 8) | pkt[off + 9];
      if (off + 10 + rdlen > pkt.length) return out;
      if (type == wantedType && (type == 1 || type == 28)) {
        final rd = Uint8List.fromList(pkt.sublist(off + 10, off + 10 + rdlen));
        out.add(rd);
      }
      off += 10 + rdlen;
    }
    return out;
  }
}

/// DNS 应答记录.
class DnsAnswer {
  DnsAnswer({required this.type, required this.ttl, required this.rdata});
  final int type; // 1=A, 28=AAAA
  final int ttl;
  final Uint8List rdata;
}

/// 打开到 (代理的) DNS 服务器 53 端口 TCP 通道的函数。
/// [viaProxy] 为 true 时经上游代理连通, 否则直连。
typedef DnsChannelOpener = Future<ProtectedChannel> Function(
    String server, int qtype, bool viaProxy);

/// DNS 解析配置 (来自 VpnConfig).
class DnsConfig {
  DnsConfig({
    required this.proxyPass,
    this.foreignDns = const [],
    this.foreignDoh = '',
    this.domesticDefault = '223.6.6.6',
    this.proxyDomains = const [],
    this.noIpv6 = true,
    this.cacheSize = 200,
    this.cacheTtlMin = 60,
    List<String> domesticDns = const [],
  }) : domesticDns = (domesticDns.isNotEmpty)
            ? domesticDns
            : const ['223.6.6.6', '119.29.29.29'];

  final String proxyPass;
  final List<String> domesticDns;
  final List<String> foreignDns;
  final String foreignDoh;
  final String domesticDefault;

  /// 国外域名后缀规则; 空表示未配置域名分流 (全部当国外).
  final List<String> proxyDomains;

  /// 禁用 IPv6 解析: AAAA 直接返回空, 不转发上游.
  final bool noIpv6;

  /// 缓存条数上限 (LRU 淘汰); 0 表示禁用缓存.
  final int cacheSize;

  /// 缓存超时 (分钟); 命中缓存时刷新重新计时.
  final int cacheTtlMin;
}

/// 一条 DNS 缓存条目.
class _DnsCacheEntry {
  _DnsCacheEntry(this.answers, this.expireAt);
  final List<DnsAnswer> answers;
  int expireAt;
}

/// DNS 解析器, 实现完整的 DNS 分流设计:
///
/// - 固定向 TUN 注入 8.8.8.8/1.1.1.1, 一切查询进入 TUN, 这里按 qname 分流。
/// - 自查询 (解析 proxy_pass 自身域名) 强制直连国内 DNS, 避免解析循环。
/// - 国外域名 (命中 proxy_domains 或未配置域名分流时都当国外):
///   * 配置了国外 DNS:
///     - 该 DNS 是 proxy_pass 本身 (proxy_pass 支持 DoH) → 直接向
///       proxy_pass 发 DoH 查询;
///     - 否则 → 经 proxy_pass 代理转发普通 DNS 请求。
///   * 未配置国外 DNS: 若 proxy_pass 是 https → 默认把 proxy_pass 当作
///     DoH 服务; 否则经 proxy_pass 代理转发 DNS 到 8.8.8.8。
/// - 国内域名 → 国内 DNS 直连。
class DnsResolver {
  DnsResolver({
    required this.config,
    required this.opener,
    required this.upstream,
    required this.forwarder,
    required this.disableCheckCert,
    required this.log,
  });

  final DnsConfig config;
  final DnsChannelOpener opener; // 经 proxy/直连打开到普通 DNS:53 的通道
  final UpstreamClient upstream; // 用于 DoH (proxy_pass 作为 DoH 时)
  final Forwarder forwarder;
  final bool disableCheckCert;
  final EngineLog log;

  final Map<String, _DnsCacheEntry> _cache = {};
  // 单飞: 同一域名同一类型的并发解析共享同一次上游查询, 避免 DNS 风暴
  // (启动时数十个 DoH 同时解析 proxy_pass 域名会打满转发器连接池).
  final Map<String, Future<List<DnsAnswer>>> _inflight = {};

  String? get _proxyHost {
    final p = parseProxyPass(config.proxyPass);
    return p?.host;
  }

  bool get _proxyIsHttps {
    final p = parseProxyPass(config.proxyPass);
    return p != null &&
        (p.type == UpstreamType.https || p.type == UpstreamType.socks5s);
  }

  /// 解析 [domain]; 返回 A/AAAA 记录 (rdata 原始字节)。
  Future<List<DnsAnswer>> resolve(String domain, int qtype) async {
    final name = domain.trim().toLowerCase();

    // 禁用 IPv6 解析: AAAA 直接返回空, 不转发.
    if (qtype == 28 && config.noIpv6) return const [];

    // 命中缓存直接返回.
    final cacheKey = '$name#$qtype';
    final now = DateTime.now().millisecondsSinceEpoch;
    final cached = _cache[cacheKey];
    if (cached != null && cached.expireAt > now) {
      // 命中: 刷新超时重新计时, 并移到最近使用位置 (LRU).
      cached.expireAt = now + config.cacheTtlMin * 60000;
      _cache.remove(cacheKey);
      _cache[cacheKey] = cached;
      return cached.answers;
    }

    // 单飞: 已有同键解析在进行, 直接复用其结果.
    final inflight = _inflight[cacheKey];
    if (inflight != null) return inflight;
    final f = _resolveUncached(name, qtype, cacheKey);
    _inflight[cacheKey] = f;
    try {
      return await f;
    } finally {
      _inflight.remove(cacheKey);
    }
  }

  Future<List<DnsAnswer>> _resolveUncached(
      String name, int qtype, String cacheKey) async {
    // 自查询: 解析 proxy_pass 自身域名 → 强制直连国内 DNS (避免解析循环).
    final ph = _proxyHost;
    if (ph != null && name == ph.toLowerCase()) {
      log.log('【DNS】自查询 $name → 强制国内直连', level: 1);
      return _cacheDnsResult(cacheKey, _resolveDomestic(name, qtype));
    }

    // 分流: 国外域名按 proxy_domains 判定 (未配置时全部当国外).
    final foreign = _foreignByRule(name);
    if (!foreign) {
      // 国内域名 → 国内 DNS 直连 (不经代理).
      log.log('【DNS】$name 国内 → 国内DNS直连', level: 1);
      return _cacheDnsResult(cacheKey, _resolveDomestic(name, qtype));
    }

    // 国外域名.
    log.log('【DNS】$name 国外 → ${_describeForeignPath(name)}', level: 1);
    return _cacheDnsResult(cacheKey, _resolveForeign(name, qtype));
  }

  /// 预热 proxy_pass 自身域名的解析 (供 DoH/代理连接复用缓存).
  Future<void> warmUpProxyHost() async {
    final ph = _proxyHost;
    if (ph == null) return;
    try {
      await resolve(ph, 1);
      log.log('【DNS】proxy_pass 域名预热完成: $ph', level: 1);
    } catch (e) {
      log.log('【DNS】proxy_pass 域名预热失败: $ph $e', level: 2);
    }
  }

  /// 解析完成后写入缓存 (超时 [DnsConfig.cacheTtlMin]), 失败不缓存.
  Future<List<DnsAnswer>> _cacheDnsResult(String key, Future<List<DnsAnswer>> f) async {
    final answers = await f;
    if (answers.isNotEmpty) {
      if (config.cacheSize <= 0) return answers;
      _cache[key] = _DnsCacheEntry(
        answers,
        DateTime.now().millisecondsSinceEpoch + config.cacheTtlMin * 60000,
      );
      if (_cache.length > config.cacheSize) {
        // LRU 淘汰最久未访问的.
        _cache.remove(_cache.keys.first);
      }
    }
    return answers;
  }

  /// 描述国外域名的解析路径 (诊断用).
  String _describeForeignPath(String name) {
    if (config.foreignDoh.trim().isNotEmpty) return '经DoH(${config.foreignDoh})';
    if (config.foreignDns.isNotEmpty) {
      final ph = _proxyHost;
      for (final s in config.foreignDns) {
        if (ph != null && s.trim() == ph) return '经代理DoH';
      }
      return '经代理转发DNS到${config.foreignDns.join(",")}';
    }
    if (_proxyIsHttps) return '默认用proxy_pass作DoH';
    return '经代理转发DNS到8.8.8.8';
  }

  /// 国内域名 / self_query: 用配置的国内 DNS 列表直连解析, 逐台失败切换。
  /// 国内 DNS(如 223.6.6.6/119.29.29.29)以 UDP 协议为主, 必须走 UDP。
  Future<List<DnsAnswer>> _resolveDomestic(String name, int qtype) async {
    Object? last;
    for (final server in config.domesticDns) {
      if (server.trim().isEmpty) continue;
      try {
        return await _resolveViaUdp(name, qtype, server.trim());
      } catch (e) {
        last = e;
      }
    }
    // 国内 UDP DNS 全部失败 (被污染/不可达) 时, 用国内 DoH 直连兜底.
    // 国内 DNS 请求一律不经代理; 直连国内 DoH 符合分流设计.
    try {
      return await _resolveDomesticDoh(name, qtype);
    } catch (e) {
      last = e;
    }
    throw StateError('国内 DNS 全部失败: $last');
  }

  /// 国内 DoH 兜底端点: Aliyun DoH (dns.alidns.com) JSON API.
  /// anycast IP 稳定, 用 SNI 做证书校验; 直连不经代理.
  static const _domesticDohEndpoints = <(String, List<String>)>[
    ('dns.alidns.com', ['223.5.5.5', '223.6.6.6']),
  ];

  Future<List<DnsAnswer>> _resolveDomesticDoh(String name, int qtype) async {
    Object? last;
    for (final (host, ips) in _domesticDohEndpoints) {
      for (final ip in ips) {
        try {
          final answers = await _resolveDohJson(name, qtype, host, ip);
          log.log('【DNS】$name 国内DoH($host@$ip) 应答 ${answers.length}条', level: 1);
          return answers;
        } catch (e) {
          last = e;
          log.log('【DNS】国内DoH兜底 $host@$ip 失败: $e', level: 2);
        }
      }
    }
    throw StateError('国内 DoH 兜底全部失败: $last');
  }

  /// 经受保护转发器直连国内 DoH 端点, 用 JSON API 查询 (dns.alidns.com/resolve).
  Future<List<DnsAnswer>> _resolveDohJson(
      String name, int qtype, String host, String ip) async {
    final typeName = qtype == 28 ? 'AAAA' : 'A';
    final path = '/resolve?name=${Uri.encodeQueryComponent(name)}&type=$typeName';
    final ch = await forwarder.connect(
      ip,
      443,
      tlsHost: host,
      disableCheckCert: disableCheckCert,
    );
    try {
      final req =
          'GET $path HTTP/1.1\r\n'
          'Host: $host\r\n'
          'Accept: application/dns-json\r\n'
          'Connection: close\r\n'
          '\r\n';
      ch.write(req.codeUnits);
      final head = await ch.readUntilBlankLine().timeout(const Duration(seconds: 10));
      final text = String.fromCharCodes(head);
      final statusLine = text.split('\r\n').firstOrNull ?? '';
      final parts = statusLine.split(' ');
      final code = parts.length >= 2 ? int.tryParse(parts[1]) : null;
      if (code == null || code < 200 || code >= 300) {
        throw StateError('国内DoH HTTP $statusLine');
      }
      var contentLength = 0;
      for (final line in text.split('\r\n').skip(1)) {
        final lower = line.toLowerCase();
        if (lower.startsWith('content-length:')) {
          contentLength = int.tryParse(line.split(':').last.trim()) ?? 0;
        }
      }
      final body = await ch.read(contentLength).timeout(const Duration(seconds: 10));
      final answers = _parseDohJson(body);
      if (answers.isEmpty) throw StateError('国内DoH JSON 无应答');
      return answers;
    } finally {
      ch.close();
    }
  }

  /// 解析 Aliyun DoH JSON 应答: {"Status":0,"Answer":[{"type":1,"data":"1.2.3.4"}]}.
  List<DnsAnswer> _parseDohJson(Uint8List body) {
    final answers = <DnsAnswer>[];
    final decoded = jsonDecode(String.fromCharCodes(body));
    if (decoded is! Map || decoded['Answer'] is! List) return answers;
    for (final a in decoded['Answer'] as List) {
      if (a is! Map) continue;
      final t = a['type'];
      final data = a['data']?.toString();
      if (data == null) continue;
      final ip = (t == 1 || t == 28) ? InternetAddress.tryParse(data) : null;
      if (ip == null) continue;
      answers.add(DnsAnswer(type: t, ttl: 300, rdata: Uint8List.fromList(ip.rawAddress)));
    }
    return answers;
  }

  /// 经受保护转发器发 UDP DNS 查询到目标 [server]:53 并解析应答。
  Future<List<DnsAnswer>> _resolveViaUdp(
      String name, int qtype, String server) async {
    log.log('【DNS】UDP查询 域名=$name 服务器=$server', level: 1);
    final query = _buildQuery(name, qtype);
    final resp = await forwarder.requestUdp(server, 53, query);
    final answers = _extract(resp, qtype);
    log.log('【DNS】$name UDP($server) 应答 ${answers.length}条', level: 1);
    return answers;
  }

  /// 是否按"国外域名"处理: 命中 proxy_domains, 或未配置域名分流时全部当国外。
  bool _foreignByRule(String name) {
    if (config.proxyPass.trim().isEmpty) return false;
    final domains = config.proxyDomains;
    if (domains.isEmpty) return true;
    for (final d in domains) {
      final dd = d.trim().toLowerCase();
      if (dd.isEmpty) continue;
      if (name == dd) return true;
      if (name.length > dd.length &&
          name.endsWith(dd) &&
          name[name.length - dd.length - 1] == '.') {
        return true;
      }
    }
    return false;
  }

  // 国外域名解析: 按配置选择 DoH 或经代理转发.
  Future<List<DnsAnswer>> _resolveForeign(String name, int qtype) async {
    // 1) 显式配置了国外 DoH URL → 直接向该 DoH 端点发 DoH 查询.
    final doh = config.foreignDoh.trim();
    if (doh.isNotEmpty) {
      log.log('【DNS】$name 走国外DoH: $doh', level: 1);
      try {
        return await _resolveDoh(name, qtype, doh, viaProxy: true);
      } catch (e) {
        log.log('【DNS】DoH 失败 $doh: $e, 回退代理 DNS', level: 2);
      }
    }

    // 2) 配置了国外 DNS.
    if (config.foreignDns.isNotEmpty) {
      final servers = config.foreignDns;
      // 若国外 DNS 本身就是 proxy_pass (proxy_pass 通常支持 DoH), 直接 DoH.
      final ph = _proxyHost;
      for (final s in servers) {
        if (ph != null && s.trim() == ph) {
          log.log('【DNS】$name 国外DNS即proxy_pass → 走代理DoH', level: 1);
          try {
            return await _resolveDoh(name, qtype, _dohUrlForProxy(), viaProxy: true);
          } catch (e) {
            log.log('【DNS】proxy_pass 作 DoH 失败: $e', level: 2);
          }
        }
      }
      // 否则经 proxy_pass 代理转发普通 DNS 请求到该国外 DNS.
      log.log('【DNS】$name 经代理转发DNS: ${servers.join(",")}', level: 1);
      final lastError = <Object?>[];
      for (final s in servers) {
        try {
          final r = await _resolveViaTcp(name, qtype, s, true);
          log.log('【DNS】$name 经代理DNS($s) 成功, ${r.length}条', level: 1);
          return r;
        } catch (e) {
          lastError.add(e);
        }
      }
      log.log('【DNS】代理 DNS 全失败: $lastError', level: 2);
    }

    // 3) 未配置国外 DNS: 若 proxy_pass 是 https → 默认用 proxy_pass 作 DoH.
    if (_proxyIsHttps) {
      try {
        return await _resolveDoh(name, qtype, _dohUrlForProxy(), viaProxy: true);
      } catch (e) {
        log.log('【DNS】proxy_pass 默认 DoH 失败: $e', level: 2);
      }
    }

    // 4) 兜底: 经 proxy_pass 代理转发 DNS 到 8.8.8.8.
    log.log('【DNS】$name 兜底经代理转发DNS到8.8.8.8', level: 1);
    return _resolveViaTcp(name, qtype, '8.8.8.8', true);
  }

  /// 生成 proxy_pass 对应的 DoH 端点 URL (基于 https://host:port)。
  String _dohUrlForProxy() {
    final p = parseProxyPass(config.proxyPass);
    if (p == null) return 'https://8.8.8.8/dns-query';
    return 'https://${p.host}:${p.port}/dns-query';
  }

  /// 经 [opener] 建立到服务器 [server]:53 的通道并做 DNS-over-TCP 查询。
  Future<List<DnsAnswer>> _resolveViaTcp(
      String name, int qtype, String server, bool viaProxy) async {
    log.log('【DNS】TCP查询 域名=$name 服务器=$server 方式=${viaProxy ? '经代理' : '直连'}', level: 1);
    final ch = await opener(server, qtype, viaProxy);
    try {
      final query = _buildQuery(name, qtype);
      final lenHdr = ByteData(2);
      lenHdr.setInt16(0, query.length, Endian.big);
      ch.write([...lenHdr.buffer.asUint8List(), ...query]);
      final lenBytes = await ch.read(2);
      if (lenBytes.length < 2) throw StateError('DNS TCP 无响应');
      final len = (lenBytes[0] << 8) | lenBytes[1];
      final body = await ch.read(len);
      return _extract(body, qtype);
    } finally {
      ch.close();
    }
  }

  /// DoH 查询 (GET https://endpoint/dns-query?dns=base64url)。
  /// [viaProxy] 为 true 时, DoH 端点本身经 proxy_pass 的 TLS 通道建立。
  Future<List<DnsAnswer>> _resolveDoh(
      String name, int qtype, String url, {required bool viaProxy}) async {
    final uri = Uri.parse(url);
    final host = uri.host;
    final port = uri.port != 0 ? uri.port : 443;
    final path = uri.path.isEmpty ? '/dns-query' : uri.path;

    // 建立到该 DoH 主机的 TLS 通道 (经受保护转发器)。
    // [viaProxy] 为 true 时经前向器先连 proxy_pass, 再在 proxy 侧 TLS 到端点.
    final conn = await upstream.connectToDohHost(host, port, uri.hasScheme ? uri.scheme : 'https');
    final ch = conn.channel;

    final query = _buildQuery(name, qtype);
    final b64 = base64UrlEncode(query).replaceAll('=', '');
    // 认证: DoH 端点若带有凭据则必须携带, 否则 401.
    // 优先取 DoH URL userinfo 里的凭据; 为空则回退 proxy_pass 的凭据
    // (当 proxy_pass 被用作 DoH 服务时, 依赖 proxy_pass 的认证).
    var authHeader = '';
    if (uri.userInfo.isNotEmpty) {
      final parts = uri.userInfo.split(':');
      final au = Uri.decodeComponent(parts[0]);
      final ap = parts.length > 1 ? Uri.decodeComponent(parts[1]) : '';
      authHeader = 'Authorization: Basic ${_basicAuth(au, ap)}\r\n';
      log.log('【DNS】DoH使用URL内凭据认证', level: 1);
    } else {
      final p = parseProxyPass(config.proxyPass);
      if (p != null && (p.user.isNotEmpty || p.pass.isNotEmpty)) {
        authHeader = 'Authorization: Basic ${_basicAuth(p.user, p.pass)}\r\n';
        log.log('【DNS】DoH使用proxy_pass凭据登录', level: 1);
      }
    }
    final req =
        'GET $path?dns=$b64 HTTP/1.1\r\n'
        'Host: $host:$port\r\n'
        'Accept: application/dns-message\r\n'
        'Connection: close\r\n'
        '$authHeader'
        '\r\n';
    try {
      ch.write(req.codeUnits);
      // 读取 HTTP 响应头.
      final head = await ch
          .readUntilBlankLine()
          .timeout(const Duration(seconds: 10));
      final text = String.fromCharCodes(head);
      final statusLine = text.split('\r\n').firstOrNull ?? '';
      final parts = statusLine.split(' ');
      final code = parts.length >= 2 ? int.tryParse(parts[1]) : null;
      if (code == null || code < 200 || code >= 300) {
        throw StateError('DoH HTTP $statusLine');
      }
      // 解析 Content-Length.
      var contentLength = 0;
      for (final line in text.split('\r\n').skip(1)) {
        final lower = line.toLowerCase();
        if (lower.startsWith('content-length:')) {
          contentLength = int.tryParse(line.split(':').last.trim()) ?? 0;
        }
      }
      final body = await ch
          .read(contentLength)
          .timeout(const Duration(seconds: 10));
      ch.close();
      final answers = _extract(body, qtype);
      log.log('【DNS】$name DoH($host) 应答 ${answers.length}条', level: 1);
      return answers;
    } finally {
      ch.close();
    }
  }

  List<DnsAnswer> _extract(Uint8List body, int qtype) {
    final answers = <DnsAnswer>[];
    if (qtype == 28) {
      for (final a in Dns.parseAnswers(body, 28)) {
        answers.add(DnsAnswer(type: 28, ttl: 300, rdata: a));
      }
    }
    final v4 = Dns.parseAnswers(body, 1);
    for (final a in v4) {
      answers.add(DnsAnswer(type: 1, ttl: 300, rdata: a));
    }
    return answers;
  }

  static Uint8List _buildQuery(String domain, int qtype) {
    final out = <int>[
      0x12, 0x34,
      0x01, 0x00,
      0x00, 0x01,
      0, 0, 0, 0, 0, 0,
    ];
    for (final label in domain.split('.')) {
      out.add(label.length);
      for (final c in label.codeUnits) {
        out.add(c);
      }
    }
    out.add(0);
    out.add((qtype >> 8) & 0xff);
    out.add(qtype & 0xff);
    out.add(0);
    out.add(1); // IN
    return Uint8List.fromList(out);
  }

  /// 生成 HTTP Basic 认证串 (base64(user:pass))。
  static String _basicAuth(String user, String pass) {
    final raw = '$user:$pass';
    return base64Encode(utf8.encode(raw));
  }
}
