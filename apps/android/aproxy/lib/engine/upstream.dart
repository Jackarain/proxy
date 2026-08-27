import 'dart:async';
import 'dart:convert';

import 'forwarder.dart';
import 'logging.dart';

/// 上游代理类型。
enum UpstreamType { http, https, socks4, socks5, socks5s }

/// 解析 proxy_pass 得到上游类型、host、port 与可选认证凭据。
({UpstreamType type, String host, int port, String user, String pass})?
parseProxyPass(String raw) {
  var s = raw.trim();
  UpstreamType type;
  var host0 = s;
  if (s.startsWith('socks5s://')) {
    type = UpstreamType.socks5s;
    host0 = s.substring('socks5s://'.length);
  } else if (s.startsWith('socks5://')) {
    type = UpstreamType.socks5;
    host0 = s.substring('socks5://'.length);
  } else if (s.startsWith('socks4://')) {
    type = UpstreamType.socks4;
    host0 = s.substring('socks4://'.length);
  } else if (s.startsWith('https://')) {
    type = UpstreamType.https;
    host0 = s.substring('https://'.length);
  } else if (s.startsWith('http://')) {
    type = UpstreamType.http;
    host0 = s.substring('http://'.length);
  } else {
    type = UpstreamType.http;
  }

  // 提取 user:pass@host:port (URL userinfo).
  String user = '';
  String pass = '';
  final at = host0.lastIndexOf('@');
  if (at >= 0) {
    final cred = host0.substring(0, at);
    host0 = host0.substring(at + 1);
    final ci = cred.lastIndexOf(':');
    if (ci >= 0) {
      user = Uri.decodeComponent(cred.substring(0, ci));
      pass = Uri.decodeComponent(cred.substring(ci + 1));
    } else {
      user = Uri.decodeComponent(cred);
    }
  }

  // 去掉 path/query 部分 (如 "vps.p2sp.net:443/some?x=1")。
  final slashQ = host0.indexOf(RegExp(r'[/?#]'));
  if (slashQ >= 0) host0 = host0.substring(0, slashQ);

  final idx = host0.lastIndexOf(':');
  if (idx <= 0) {
    final defaultPort = switch (type) {
      UpstreamType.http => 80,
      UpstreamType.https || UpstreamType.socks5s => 443,
      _ => 1080,
    };
    return (type: type, host: host0, port: defaultPort, user: user, pass: pass);
  }
  // IPv6 形如 [x:x:x::x]:port, 处理括号.
  String host;
  final portStr = host0.substring(idx + 1).trim();
  if (host0.startsWith('[')) {
    final close = host0.indexOf(']');
    if (close < 0) return null;
    host = host0.substring(1, close);
  } else {
    host = host0.substring(0, idx).trim();
  }
  final port = int.tryParse(portStr);
  if (host.isEmpty || port == null || port <= 0 || port > 65535) return null;
  return (type: type, host: host, port: port, user: user, pass: pass);
}

/// 上游代理连接 (握手已完成, 可用于负载流)。
class UpstreamConn {
  UpstreamConn(this.channel);
  final ProtectedChannel channel;
}

/// 上游代理客户端。
class UpstreamClient {
  UpstreamClient({
    required this.forwarder,
    required String proxyPass,
    this.poolSize = 0,
    required this.disableCheckCert,
    required this.log,
  }) {
    _proxyPass = proxyPass;
  }

  final Forwarder forwarder;
  final int poolSize;
  final bool disableCheckCert;
  final EngineLog log;

  String _proxyPass = '';
  String get proxyPass => _proxyPass;
  set proxyPass(String v) {
    if (_proxyPass == v) return;
    _proxyPass = v;
    _drainPool();
    _fillOne();
  }

  /// 预建连接池: 元素为已建立到上游代理 TCP(+TLS) 的连接
  /// (proxy 握手在取用时完成)。取走后不回收,
  /// 池数量低于 [poolSize] 时后台预建补充。
  final List<UpstreamConn> _pool = [];
  int _filling = 0;
  static const int _maxConcurrentFill = 2;
  bool _closed = false;
  Timer? _refillTimer;

  /// 把主机名解析为 IP(v4) 的回调, 由引擎 DnsResolver 提供。
  /// 用于把 proxy_pass 的域名解析成 IP 后再交给转发器, 避免 Kotlin 侧
  /// 重复解析域名(其 DNS 会回环进 TUN 导致 UnknownHostException)。
  Future<String?> Function(String host)? resolveHostIp;

  /// 连接一个 DoH/HTTPS 端点 (经受保护转发器, 可选 TLS)。
  /// 用于把 proxy_pass 当作 DoH 服务时, 直接向 proxy_pass 的 https 端点
  /// 发起 TLS 连接并发送 DoH 请求。
  Future<UpstreamConn> connectToDohHost(
      String host, int port, String scheme) async {
    final tls = scheme == 'https' || scheme == 'wss';
    // DoH 端点即代理自身时, 直接复用池中预建的 TLS 连接 (免 TCP/TLS 握手).
    final p = parseProxyPass(_proxyPass);
    if (_poolSizeEnabled && tls && p != null &&
        (p.type == UpstreamType.https || p.type == UpstreamType.socks5s) &&
        p.port == port && p.host == host) {
      final pooled = _takePooled();
      if (pooled != null) {
        log.log('【连接池】DoH 复用预建连接 $host:$port', level: 1);
        return pooled;
      }
    }
    // 与 connect() 一致: 域名先经引擎解析成 IP 再交给转发器, 避免 Kotlin
    // 侧解析域名时 DNS 回环进 TUN 导致 UnknownHostException.
    var connectHost = host;
    if (_isHostname(host) && resolveHostIp != null) {
      final ip = await resolveHostIp!(host);
      if (ip != null && ip.isNotEmpty && _isIpLiteral(ip)) {
        log.log('【上游】DoH主机域名=$host 解析为IP=$ip', level: 1);
        connectHost = ip;
      }
    }
    final ch = await forwarder.connect(
      connectHost,
      port,
      tlsHost: tls ? host : null,
      disableCheckCert: disableCheckCert,
    );
    return UpstreamConn(ch);
  }

  /// 上游 scheme 是否走 TLS。
  bool get needsTls {
    final p = parseProxyPass(_proxyPass);
    if (p == null) return false;
    return p.type == UpstreamType.https || p.type == UpstreamType.socks5s;
  }

  bool get _poolSizeEnabled => poolSize > 0 && !_closed;

  /// 启动连接池: 立即预建 1 条, 之后每秒补充 1 条直到池满。
  void startPool() {
    if (!_poolSizeEnabled) return;
    _refillTimer?.cancel();
    _refillTimer = Timer.periodic(
      const Duration(seconds: 1),
      (_) => _fillOne(),
    );
    _fillOne();
  }

  /// 关闭连接池并释放所有预建连接。
  void close() {
    _closed = true;
    _refillTimer?.cancel();
    _refillTimer = null;
    _drainPool();
  }

  /// 丢弃并关闭当前所有预建连接 (配置变更/关闭时)。
  void _drainPool() {
    for (final c in _pool) {
      try {
        c.channel.close();
      } catch (_) {}
    }
    _pool.clear();
  }

  /// 从池中取走一条连接; 池空返回 null。取走后立即触发补充。
  UpstreamConn? _takePooled() {
    if (!_poolSizeEnabled || _pool.isEmpty) return null;
    final c = _pool.removeLast();
    _fillOne();
    return c;
  }

  /// 补充预建连接: 触发时补建到池满, 最多 [_maxConcurrentFill] 条在途。
  void _fillOne() {
    if (!_poolSizeEnabled) return;
    while (_pool.length + _filling < poolSize && _filling < _maxConcurrentFill) {
      _filling++;
      unawaited(_doFill());
    }
  }

  Future<void> _doFill() async {
    try {
      final conn = await _createProxyConnection();
      if (_closed) {
        conn.channel.close();
        return;
      }
      if (_pool.length < poolSize) {
        _pool.add(conn);
        log.log('【连接池】预建完成 ${_pool.length}/$poolSize', level: 1);
      } else {
        conn.channel.close();
      }
    } catch (e) {
      log.log('【连接池】预建失败: $e', level: 2);
    } finally {
      _filling--;
      _fillOne();
    }
  }

  /// 创建一条到上游代理的预建连接 (仅 TCP(+TLS) 已建立,
  /// proxy 握手在取用时完成)。
  Future<UpstreamConn> _createProxyConnection() async {
    final p = parseProxyPass(_proxyPass);
    if (p == null) throw StateError('无效 proxy_pass: $_proxyPass');
    final tls = p.type == UpstreamType.https ||
        p.type == UpstreamType.socks5s;
    // 代理主机若是域名, 先经 DnsResolver 解析成 IP 再交给转发器。
    var proxyHost = p.host;
    if (_isHostname(proxyHost) && resolveHostIp != null) {
      final ip = await resolveHostIp!(proxyHost);
      if (ip != null && ip.isNotEmpty && _isIpLiteral(ip)) {
        proxyHost = ip;
      }
    }
    final ch = await forwarder.connect(
      proxyHost,
      p.port,
      tlsHost: tls ? p.host : null,
      disableCheckCert: disableCheckCert,
    );
    return UpstreamConn(ch);
  }

  /// 在已建立的代理连接上对目标执行 CONNECT 握手。
  Future<void> _proxyConnect(
    UpstreamConn conn,
    ({UpstreamType type, String host, int port, String user, String pass}) p,
    String host,
    int port,
  ) async {
    final ch = conn.channel;
    switch (p.type) {
      case UpstreamType.socks4:
        await _socks4Handshake(ch, p, host, port);
      case UpstreamType.socks5:
      case UpstreamType.socks5s:
        await _socks5Greet(ch, p);
        await _socks5Connect(ch, p, host, port);
      case UpstreamType.http:
      case UpstreamType.https:
        await _httpConnect(ch, p, host, port);
    }
  }

  /// 建立到目标 [dstHost]:[dstPort] 的连接。
  ///
  /// [useProxy] 为 true 且配置了 proxy_pass 时经上游代理 CONNECT;
  /// 否则直连 (经受保护转发器绕过 TUN)。
  Future<UpstreamConn> connect(String dstHost, int dstPort,
      {required bool useProxy}) async {
    final pass = _proxyPass.trim();
    if (pass.isEmpty || !useProxy) {
      // 直连 (绕过 TUN): 目标即上游.
      final ch = await forwarder.connect(dstHost, dstPort);
      return UpstreamConn(ch);
    }

    // 优先复用池中预建连接; 池空或握手失败时新建.
    final parsed = parseProxyPass(pass);
    if (parsed == null) throw StateError('无效 proxy_pass: $pass');
    final pooled = _takePooled();
    if (pooled != null) {
      try {
        await _proxyConnect(pooled, parsed, dstHost, dstPort);
        log.log('【连接池】复用预建连接 目标=$dstHost:$dstPort', level: 1);
        return pooled;
      } catch (e) {
        log.log('【连接池】复用连接握手失败, 新建: $e', level: 2);
        pooled.channel.close();
      }
    }
    final fresh = await _createProxyConnection();
    await _proxyConnect(fresh, parsed, dstHost, dstPort);
    return fresh;
  }

  /// SOCKS5 方法协商 (不依赖目标, 可在连接池预建阶段完成).
  Future<void> _socks5Greet(
    ProtectedChannel ch,
    ({UpstreamType type, String host, int port, String user, String pass}) p,
  ) async {
    final ub = p.user.codeUnits;
    final pb = p.pass.codeUnits;
    // 无凭据时只提供空认证; 有凭据时提供 username/password 认证 (0x02).
    final methods = <int>[0x00];
    if (ub.isNotEmpty && pb.isNotEmpty) methods.add(0x02);
    ch.write([0x05, methods.length, ...methods]);
    final rep = await ch.read(2);
    if (rep.length < 2 || rep[0] != 0x05) {
      throw StateError('SOCKS5 握手失败');
    }
    if (rep[1] == 0x02) {
      if (ub.length > 255 || pb.length > 255) {
        throw StateError('SOCKS5 用户名/密码过长');
      }
      ch.write([
        0x01, ub.length, ...ub, pb.length, ...pb,
      ]);
      final auth = await ch.read(2);
      if (auth.length < 2 || auth[1] != 0x00) {
        throw StateError('SOCKS5 认证失败');
      }
    } else if (rep[1] != 0x00) {
      throw StateError('SOCKS5 无可用认证方法 (method=${rep[1]})');
    }
  }

  /// SOCKS5 目标 CONNECT.
  Future<void> _socks5Connect(
    ProtectedChannel ch,
    ({UpstreamType type, String host, int port, String user, String pass}) p,
    String host,
    int port,
  ) async {
    final hb = host.codeUnits;
    final req = <int>[0x05, 0x01, 0x00, 0x03, hb.length, ...hb,
      (port >> 8) & 0xff, port & 0xff];
    ch.write(req);
    final head = await ch.read(4);
    if (head.length < 4 || head[1] != 0x00) {
      throw StateError('SOCKS5 CONNECT 失败 code=${head.isNotEmpty ? head[1] : -1}');
    }
    final atyp = head[3];
    var extra = 2; // 端口
    if (atyp == 0x01) extra += 4;
    if (atyp == 0x04) extra += 16;
    if (atyp == 0x03) {
      final l = await ch.read(1);
      extra += (l.isNotEmpty ? l[0] : 0) + 1;
    }
    if (extra > 0) await ch.read(extra);
  }

  Future<void> _socks4Handshake(
    ProtectedChannel ch,
    ({UpstreamType type, String host, int port, String user, String pass}) p,
    String host,
    int port,
  ) async {
    // SOCKS4a: 用 0.0.0.1 + 域名.
    ch.write([0x04, 0x01, (port >> 8) & 0xff, port & 0xff, 0, 0, 0, 1, 0]);
    // 可选 userid.
    final uid = p.user.codeUnits;
    for (final b in uid) {
      ch.write([b]);
    }
    ch.write([0]);
    for (final b in host.codeUnits) {
      ch.write([b]);
    }
    ch.write([0]);
    final rep = await ch.read(8);
    if (rep.length < 8 || rep[1] != 0x5a) {
      throw StateError('SOCKS4 连接失败 CD=${rep.length > 1 ? rep[1] : -1}');
    }
  }

  Future<void> _httpConnect(
    ProtectedChannel ch,
    ({UpstreamType type, String host, int port, String user, String pass}) p,
    String host,
    int port,
  ) async {
    final auth = (p.user.isNotEmpty || p.pass.isNotEmpty)
        ? 'Proxy-Authorization: Basic ${_basicAuth(p.user, p.pass)}\r\n'
        : '';
    final req = 'CONNECT $host:$port HTTP/1.1\r\n'
        'Host: $host:$port\r\n'
        '$auth'
        '\r\n';
    ch.write(req.codeUnits);
    final head = await ch.readUntilBlankLine();
    final text = String.fromCharCodes(head);
    final firstLine = text.split('\r\n').firstOrNull ?? '';
    final code = _parseStatusCode(firstLine);
    if (code == null || code < 200 || code >= 300) {
      throw StateError('CONNECT 失败: $firstLine');
    }
  }

  static String _basicAuth(String user, String pass) {
    final raw = '$user:$pass';
    final bytes = (String.fromCharCodes(raw.codeUnits));
    return base64Encode(List<int>.from(bytes.codeUnits));
  }

  int? _parseStatusCode(String statusLine) {
    final parts = statusLine.split(' ');
    if (parts.length < 2) return null;
    return int.tryParse(parts[1]);
  }

  /// 是否为域名 (非 IP 字面量)。
  bool _isHostname(String host) => !_isIpLiteral(host);

  /// 是否为 IPv4/IPv6 字面量。
  bool _isIpLiteral(String host) {
    if (_ipv4Pattern.hasMatch(host)) return true;
    return host.contains(':') && host.contains(RegExp(r'[0-9a-fA-F]'));
  }

  static final RegExp _ipv4Pattern =
      RegExp(r'^(\d{1,3}\.){3}\d{1,3}$');
}

extension _FirstOrNull on List<String> {
  String? get firstOrNull => isEmpty ? null : first;
}
