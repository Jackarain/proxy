import 'dart:math';
import 'dart:typed_data';

import 'forwarder.dart';
import 'ip_packet.dart';
import 'tunnel_engine.dart';
import 'upstream.dart';

/// 一次 TCP 会话 (over TUN 数据面)。
///
/// 维护简单而可用的状态机: 处理 SYN/数据/FIN/RST; 顺序转发, 不做窗口
/// 与重传 (为真实的浏览/API 场景足够). 上游经 [UpstreamClient] 建立,
/// 数据通过 [ProtectedChannel.startPayload] 流入, 反方向由 TUN 包送入。
class TcpSession {
  TcpSession({
    required this.engine,
    required this.srcIp,
    required this.srcPort,
    required this.dstIp,
    required this.dstPort,
    required this.useProxy,
    required this.version,
  }) {
    final rnd = Random.secure();
    _serverIsn = (rnd.nextInt(0x7fffffff)) & 0xffffffff;
  }

  final TunnelEngine engine;
  final String srcIp;
  final int srcPort;
  final String dstIp;
  final int dstPort;
  final bool useProxy;
  final int version; // 4 或 6

  late int _clientIsn = 0;
  late int _serverIsn;
  late int _clientNextSeq; // 期望下一个来自客户端 data 的 seq (不含 payload).
  late int _serverNextSeq = (_serverIsn + 1) & 0xffffffff;
  int _clientMss = 536; // 对端通告 MSS (未携带时按 RFC 默认 536).

  ProtectedChannel? _upstream;
  bool _established = false;
  bool _closing = false;
  bool _sentFin = false;

  int bytesUp = 0; // 客户端 -> 上游
  int bytesDown = 0; // 上游 -> 客户端

  void log(String m, {int level = 1}) =>
      engine.log.log('tcp $srcIp:$srcPort->$dstIp:$dstPort $m', level: level);

  String get target => '$dstIp:$dstPort';

  /// 处理来自客户端的 TCP 报文。返回是否已消费 (需要回包时返回 true 由
  /// 引擎构造应答)。[segBytes] 为完整 TCP 段 (含头), pkt.payload 的别名。
  bool handleClientPacket(IpPacket pkt, TcpHeader tcp, Uint8List segBytes) {
    final dataLen = tcp.dataLength;

    // 1) SYN.
    if (tcp.syn && !tcp.hasAck) {
      _clientIsn = tcp.seq;
      _clientNextSeq = (_clientIsn + 1) & 0xffffffff;
      final mss = parseTcpMss(tcp.options);
      if (mss != null && mss > 0) _clientMss = mss;
      _connectUpstream();
      return true; // 应回 SYN+ACK.
    }

    if (!_established) {
      // 尚未建立完成前到达的数据丢弃 (除 RST).
      if (tcp.rst) return false;
      return false;
    }

    // RST.
    if (tcp.rst) {
      _teardown();
      return false;
    }

    // 只接受有序数据段.
    if (dataLen > 0) {
      final expected = _clientNextSeq;
      if (tcp.seq == expected) {
        final payload = (tcp.dataStart + tcp.dataLength <= segBytes.length)
            ? Uint8List.fromList(
                segBytes.sublist(tcp.dataStart, tcp.dataStart + tcp.dataLength))
            : Uint8List(0);
        if (payload.isNotEmpty) {
          _upstream?.write(payload);
          bytesUp += payload.length;
        }
        _clientNextSeq = (_clientNextSeq + dataLen) & 0xffffffff;
        _sendAck();
      } else {
        // 乱序: 丢弃 (MVP 不做重组/重传).
      }
    }

    // FIN.
    if (tcp.fin) {
      _sendFinAck();
    }

    return false;
  }

  /// 发出 ACK 给客户端。
  void _sendAck() {
    engine.sendTcp(
      version: version,
      srcIp: dstIp,
      dstIp: srcIp,
      srcPort: dstPort,
      dstPort: srcPort,
      seq: _serverNextSeq,
      ack: _clientNextSeq,
      flags: 0x10, // ACK
    );
  }

  void _sendFinAck() {
    engine.sendTcp(
      version: version,
      srcIp: dstIp,
      dstIp: srcIp,
      srcPort: dstPort,
      dstPort: srcPort,
      seq: _serverNextSeq,
      ack: (_clientNextSeq + 1) & 0xffffffff,
      flags: 0x11, // FIN|ACK
    );
    _sentFin = true;
    _closing = true;
    _scheduleRemove();
  }

  /// 回 SYN+ACK 给客户端。
  void replySynAck() {
    engine.sendTcp(
      version: version,
      srcIp: dstIp,
      dstIp: srcIp,
      srcPort: dstPort,
      dstPort: srcPort,
      seq: _serverIsn,
      ack: _clientNextSeq,
      flags: 0x12, // SYN|ACK
      options: tcpMssOption(engine.mssFor(version)),
    );
    log('SYN+ACK 已发');
  }

  /// 发送 RST 给客户端。
  void sendRst() {
    engine.sendTcp(
      version: version,
      srcIp: dstIp,
      dstIp: srcIp,
      srcPort: dstPort,
      dstPort: srcPort,
      seq: _serverNextSeq,
      ack: _clientNextSeq,
      flags: 0x04, // RST
    );
  }

  void _connectUpstream() {
    engine.upstream
        .connect(dstIp, dstPort, useProxy: useProxy)
        .then((conn) {
      if (_closing) {
        conn.channel.close();
        return;
      }
      _upstream = conn.channel;
      _established = true;
      _registerUpstream(conn);
      replySynAck();
      log('已建立TCP隧道: 目标=$target 方式=${useProxy ? '经代理' : '直连'}', level: 1);
    }).catchError((Object e) {
      log('TCP隧道建立失败: 目标=$target 方式=${useProxy ? '经代理' : '直连'} 原因=$e',
          level: 3);
      sendRst();
      engine.removeTcp(this);
    });
  }

  void _registerUpstream(UpstreamConn conn) {
    // 上报当前活跃连接给统计 (供 status 会话列表).
    engine.sessionConnections.add(SessionConn(
      proto: 'tcp',
      target: target,
      clientIp: srcIp,
      session: this,
    ));
    conn.channel.onClosed = () {
      if (!_sentFin) _sendFin();
    };
    conn.channel.startPayload(_onUpstreamData);
  }

  void _onUpstreamData(Uint8List data) {
    if (_closing || data.isEmpty) return;
    _pushToClient(data);
  }

  void _pushToClient(Uint8List data) {
    final cap = _clientMss < engine.maxTcpPayload(version)
        ? _clientMss
        : engine.maxTcpPayload(version);
    for (final chunk in sliceByMss(data, cap)) {
      engine.sendTcp(
        version: version,
        srcIp: dstIp,
        dstIp: srcIp,
        srcPort: dstPort,
        dstPort: srcPort,
        seq: _serverNextSeq,
        ack: _clientNextSeq,
        flags: 0x18, // PSH|ACK
        data: chunk,
      );
      _serverNextSeq = (_serverNextSeq + chunk.length) & 0xffffffff;
      bytesDown += chunk.length;
    }
  }

  void _sendFin() {
    engine.sendTcp(
      version: version,
      srcIp: dstIp,
      dstIp: srcIp,
      srcPort: dstPort,
      dstPort: srcPort,
      seq: _serverNextSeq,
      ack: _clientNextSeq,
      flags: 0x11, // FIN|ACK
    );
    _sentFin = true;
    _closing = true;
    _scheduleRemove();
  }

  void _scheduleRemove() {
    Future.delayed(const Duration(seconds: 5), () {
      if (_closing) engine.removeTcp(this);
    });
  }

  void _teardown() {
    try {
      _upstream?.close();
    } catch (_) {}
    _upstream = null;
    engine.removeTcp(this);
  }

  /// 客户端断开 (会话从表移除时收尾).
  void dispose() {
    try {
      _upstream?.close();
    } catch (_) {}
    _upstream = null;
  }
}

/// 会话连接明细 (状态上报用).
class SessionConn {
  SessionConn({
    required this.proto,
    required this.target,
    required this.clientIp,
    this.session,
  });
  final String proto;
  final String target;
  final String clientIp;
  final TcpSession? session;
  int get rxBytes => session?.bytesDown ?? 0;
  int get txBytes => session?.bytesUp ?? 0;
}
