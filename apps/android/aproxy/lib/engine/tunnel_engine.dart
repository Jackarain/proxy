import 'dart:typed_data';

import 'dns.dart';
import 'forwarder.dart';
import 'ip_packet.dart';
import 'logging.dart';
import 'route_table.dart';
import 'tcp_session.dart';
import 'upstream.dart';

/// 全局引擎统计。
class EngineStats {
  int rxBytes = 0; // 客户端 -> 上游
  int txBytes = 0; // 上游 -> 客户端
  int connTotal = 0;
}

/// 数据面引擎: 消费 TUN 原始 IP 报文, 执行分流、TCP 会话与 UDP/DNS 处理。
class TunnelEngine {
  TunnelEngine({
    required this.log,
    required this.routeTable,
    required this.upstream,
    required this.forwarder,
    required this.dnsConfig,
    required this.disableCheckCert,
  }) {
    _resolver = DnsResolver(
      config: dnsConfig,
      opener: _openDnsChannel,
      upstream: upstream,
      forwarder: forwarder,
      disableCheckCert: disableCheckCert,
      log: log,
    );
  }

  final EngineLog log;
  final RouteTable routeTable;
  final UpstreamClient upstream;
  final Forwarder forwarder;
  final DnsConfig dnsConfig;
  final bool disableCheckCert;

  final stats = EngineStats();
  final List<SessionConn> sessionConnections = [];

  void Function(List<int> packet)? sendPacketImpl;
  late DnsResolver _resolver;

  final Map<String, TcpSession> _tcp = {};

  /// 把引擎的域名解析能力注入上游 (用于解析 proxy_pass 域名)。
  void wireUpstreamResolver(UpstreamClient up) {
    up.resolveHostIp = (host) => resolveHostToIp(host);
  }

  /// 供上游解析 proxy_pass 域名用 (DnsResolver 的入口)。
  Future<String?> resolveHostToIp(String host) async {
    try {
      final answers = await _resolver.resolve(host, 1);
      for (final a in answers) {
        if (a.type == 1 && a.rdata.length == 4) {
          final ip = a.rdata.join('.');
          log.log('【解析】$host →IP=$ip', level: 1);
          return ip;
        }
      }
    } catch (e) {
      log.log('【解析】$host →IP 失败: $e', level: 2);
    }
    return null;
  }

  /// 打开到 DNS 服务器 53 端口 TCP 的通道。
  Future<ProtectedChannel> _openDnsChannel(String server, int qtype, bool viaProxy) async {
    final conn = await upstream.connect(server, 53, useProxy: viaProxy);
    return conn.channel;
  }

  /// 把引擎生成的 IP 报文写回 TUN.
  void sendPacket(List<int> packet) {
    stats.txBytes += packet.length;
    sendPacketImpl?.call(packet);
  }

  /// 构造并发送一个 TCP 段 (v4/v6 自适应; 非法地址静默丢弃).
  void sendTcp({
    required int version,
    required String srcIp,
    required String dstIp,
    required int srcPort,
    required int dstPort,
    required int seq,
    required int ack,
    required int flags,
    int window = 65535,
    List<int>? data,
  }) {
    final pkt = buildTcpPacket(
      version: version,
      srcIp: srcIp,
      dstIp: dstIp,
      srcPort: srcPort,
      dstPort: dstPort,
      seq: seq,
      ack: ack,
      flags: flags,
      window: window,
      data: data,
    ).wrap();
    if (pkt != null) sendPacket(pkt);
  }

  /// 构造并发送一个 UDP 段 (v4/v6 自适应).
  void sendUdp({
    required int version,
    required String srcIp,
    required String dstIp,
    required int srcPort,
    required int dstPort,
    List<int>? data,
  }) {
    final pkt = buildUdpPacket(
      version: version,
      srcIp: srcIp,
      dstIp: dstIp,
      srcPort: srcPort,
      dstPort: dstPort,
      data: data,
    ).wrap();
    if (pkt != null) sendPacket(pkt);
  }

  /// 处理一个来自 TUN 的 IP 报文.
  void handlePacket(Uint8List packet) {
    IpPacket? ip;
    if (packet.isNotEmpty && (packet[0] >> 4) == 4) {
      ip = tryParseV4(packet);
    } else {
      ip = tryParseV6(packet);
    }
    if (ip == null) return;
    stats.rxBytes += packet.length;
    final tcp = ip.tcp;
    final udp = ip.udp;
    if (tcp != null && ip.protocol == protoTcp) {
      _handleTcp(ip, tcp, ip.payload);
    } else if (udp != null && ip.protocol == protoUdp) {
      _handleUdp(ip, udp);
    }
  }

  // ---- TCP ----
  void _handleTcp(IpPacket ip, TcpHeader tcp, Uint8List segBytes) {
    final key = _connKey(ip.src, tcp.srcPort, ip.dst, tcp.dstPort);
    final session = _tcp[key];
    if (session == null) {
      // 仅响应携带 SYN 的连接请求; 其余丢弃.
      if (tcp.syn && !tcp.hasAck && !tcp.rst) {
        if (tcp.dstPort == 53) {
          // DNS over TCP: 暂不处理, 直接回 RST (由 UDP DNS 兜底).
          return;
        }
        final useProxy = routeTable.ipShouldProxy(ip.dst);
        final s = TcpSession(
          engine: this,
          srcIp: ip.src,
          srcPort: tcp.srcPort,
          dstIp: ip.dst,
          dstPort: tcp.dstPort,
          useProxy: useProxy,
          version: ip.version,
        );
        _tcp[key] = s;
        stats.connTotal++;
        s.handleClientPacket(ip, tcp, segBytes);
      }
      return;
    }
    session.handleClientPacket(ip, tcp, segBytes);
  }

  void removeTcp(TcpSession s) {
    _tcp.remove(_connKey(s.srcIp, s.srcPort, s.dstIp, s.dstPort));
    s.dispose();
    sessionConnections.removeWhere((c) => identical(c.session, s));
  }

  String _connKey(String a, int ap, String b, int bp) => '$a:$ap>$b:$bp';

  // ---- UDP (DNS 为主) ----
  void _handleUdp(IpPacket ip, UdpHeader udp) {
    if (udp.dstPort == 53) {
      _handleDns(ip, udp);
    } else {
      // 其它 UDP: MVP 暂不转发 (记录并丢弃).
    }
  }

  void _handleDns(IpPacket ip, UdpHeader udp) {
    final segBytes = ip.payload;
    final query = Uint8List.fromList(
      segBytes.length >= 8
          ? segBytes.sublist(8, segBytes.length)
          : const [],
    );
    if (query.length < 12) return;
    final qname = Dns.parseQueryName(query);
    final qtype = Dns.parseQueryType(query) ?? 1;
    if (qname == null) return;

    // 解析并应答 (DnsResolver 内部完成自查询/分流/DoH/代理转发).
    _resolver.resolve(qname, qtype).then((answers) {
      final resp = Dns.buildResponse(
        query: query,
        flags: 0x8180,
        answers: answers,
      );
      _sendDnsResponse(ip, udp, resp);
      // 记录解析结果供 TCP 分流使用.
      final ips = answers.map((a) => _rdataToIp(a.rdata)).toList();
      routeTable.markDomainResolved(qname, ips);
    }).catchError((Object e) {
      log.log('DNS 解析失败 $qname: $e', level: 3);
      try {
        _sendDnsResponse(ip, udp, Dns.buildNoErrorEmpty(query));
      } catch (_) {}
    });
  }

    String _rdataToIp(Uint8List rd) {
    if (rd.length == 4) {
      return '${rd[0]}.${rd[1]}.${rd[2]}.${rd[3]}';
    }
    if (rd.length == 16) {
      return '${rd[0]}.${rd[1]}.${rd[2]}.${rd[3]}'; // 简化 IPv6 不展开
    }
    return '';
  }

  void _sendDnsResponse(IpPacket ip, UdpHeader udp, Uint8List data) {
    sendUdp(
      version: ip.version,
      srcIp: ip.dst,
      dstIp: ip.src,
      srcPort: udp.dstPort,
      dstPort: udp.srcPort,
      data: data,
    );
  }

  /// 关闭所有会话.
  void shutdown() {
    for (final s in _tcp.values) {
      s.dispose();
    }
    _tcp.clear();
    sessionConnections.clear();
  }
}
