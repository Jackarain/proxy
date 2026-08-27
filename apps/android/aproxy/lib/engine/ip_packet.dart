library;

import 'dart:typed_data';

/// 纯 Dart 的 IPv4/IPv6 + TCP/UDP 报文解析与构造。
///
/// 该库只做最小的报文编解码，供 tun2socks 引擎在"上层 socket"与"原始
/// TUN 报文"之间转换。支持 IPv4 与 IPv6。
class IpPacket {
  IpPacket({
    required this.version,
    required this.protocol,
    required this.src,
    required this.dst,
    required this.payload,
    this.tcp,
    this.udp,
  });

  final int version; // 4 或 6
  final int protocol; // 6=TCP, 17=UDP
  final String src;
  final String dst;
  Uint8List payload;

  TcpHeader? tcp;
  UdpHeader? udp;

  bool get isV4 => version == 4;
  bool get isV6 => version == 6;
}

class TcpHeader {
  TcpHeader({
    required this.srcPort,
    required this.dstPort,
    required this.seq,
    required this.ack,
    required this.dataOffset,
    required this.flags,
    required this.window,
    this.options,
  });

  final int srcPort;
  final int dstPort;
  final int seq;
  final int ack;
  final int dataOffset;
  final int flags;
  final int window;
  Uint8List? options;

  bool get syn => (flags & 0x02) != 0;
  bool get hasAck => (flags & 0x10) != 0;
  bool get fin => (flags & 0x01) != 0;
  bool get rst => (flags & 0x04) != 0;
  bool get psh => (flags & 0x08) != 0;

  int dataStart = 0;
  int dataLength = 0;
}

class UdpHeader {
  UdpHeader({required this.srcPort, required this.dstPort, required this.length});
  final int srcPort;
  final int dstPort;
  final int length;
}

const int protoTcp = 6;
const int protoUdp = 17;

Uint8List _ip4FromString(String s) {
  final out = Uint8List(4);
  final parts = s.split('.');
  if (parts.length != 4) return Uint8List(0);
  for (var i = 0; i < 4; i++) {
    final v = int.tryParse(parts[i]);
    if (v == null || v < 0 || v > 255) return Uint8List(0);
    out[i] = v;
  }
  return out;
}

String _ip4ToString(Uint8List b) => '${b[0]}.${b[1]}.${b[2]}.${b[3]}';

Uint8List? _ip6FromString(String s) {
  final out = Uint8List(16);
  var head = s;
  var tail = '';
  final dc = s.indexOf('::');
  if (dc >= 0) {
    head = s.substring(0, dc);
    tail = s.substring(dc + 2);
  }
  final headParts = head.isEmpty ? <String>[] : head.split(':');
  final tailParts = tail.isEmpty ? <String>[] : tail.split(':');
  var fill = 8 - headParts.length - tailParts.length;
  if (fill < 0) return null;
  var idx = 0;
  for (final p in headParts) {
    final v = int.tryParse(p, radix: 16);
    if (v == null) return null;
    out[idx * 2] = (v >> 8) & 0xff;
    out[idx * 2 + 1] = v & 0xff;
    idx++;
  }
  idx += fill;
  for (final p in tailParts) {
    final v = int.tryParse(p, radix: 16);
    if (v == null) return null;
    out[idx * 2] = (v >> 8) & 0xff;
    out[idx * 2 + 1] = v & 0xff;
    idx++;
  }
  return out;
}

String _ip6ToString(Uint8List b) {
  var bestStart = -1;
  var bestLen = 0;
  var runStart = -1;
  for (var i = 0; i < 8; i++) {
    final isZero = b[i * 2] == 0 && b[i * 2 + 1] == 0;
    if (isZero && runStart < 0) runStart = i;
    if (!isZero) runStart = -1;
    if (runStart >= 0 && i - runStart + 1 > bestLen) {
      bestLen = i - runStart + 1;
      bestStart = runStart;
    }
  }
  if (bestLen >= 2 && bestStart >= 0) {
    final parts = <String>[];
    for (var i = 0; i < bestStart; i++) {
      parts.add(((b[i * 2] << 8) | b[i * 2 + 1]).toRadixString(16));
    }
    parts.add('');
    for (var i = bestStart + bestLen; i < 8; i++) {
      parts.add(((b[i * 2] << 8) | b[i * 2 + 1]).toRadixString(16));
    }
    return parts.join(':');
  }
  final parts = <String>[];
  for (var i = 0; i < 8; i++) {
    parts.add(((b[i * 2] << 8) | b[i * 2 + 1]).toRadixString(16));
  }
  return parts.join(':');
}

int _sum16(Uint8List bytes) {
  var sum = 0;
  for (var i = 0; i < bytes.length; i += 2) {
    final b0 = bytes[i];
    final b1 = i + 1 < bytes.length ? bytes[i + 1] : 0;
    sum += (b0 << 8) | b1;
  }
  while (sum > 0xffff) {
    sum = (sum & 0xffff) + (sum >> 16);
  }
  return sum & 0xffff;
}

/// 尝试解析 IPv4 报文。返回 null 表示无法解析。
IpPacket? tryParseV4(Uint8List data) {
  if (data.length < 20) return null;
  if ((data[0] >> 4) != 4) return null;
  final ihl = (data[0] & 0x0f) * 4;
  if (ihl < 20 || data.length < ihl) return null;
  final protocol = data[9];
  final src = _ip4ToString(Uint8List.sublistView(data, 12, 16));
  final dst = _ip4ToString(Uint8List.sublistView(data, 16, 20));
  final packet = IpPacket(
    version: 4,
    protocol: protocol,
    src: src,
    dst: dst,
    payload: Uint8List.sublistView(data, ihl),
  );
  _attachTransport(packet);
  return packet;
}

/// 尝试解析 IPv6 报文。
IpPacket? tryParseV6(Uint8List data) {
  if (data.length < 40) return null;
  if ((data[0] >> 4) != 6) return null;
  var offset = 40;
  var proto = data[6];
  var guard = 0;
  while ((proto == 0 || proto == 43 || proto == 44 || proto == 51 ||
      proto == 60) && guard < 8) {
    if (offset + 2 > data.length) return null;
    final extLen = data[offset + 1] * 8 + 8;
    if (offset + extLen > data.length) return null;
    proto = data[offset];
    offset += extLen;
    guard++;
  }
  final src = _ip6ToString(Uint8List.sublistView(data, 8, 24));
  final dst = _ip6ToString(Uint8List.sublistView(data, 24, 40));
  final packet = IpPacket(
    version: 6,
    protocol: proto,
    src: src,
    dst: dst,
    payload: Uint8List.sublistView(data, offset),
  );
  _attachTransport(packet);
  return packet;
}

void _attachTransport(IpPacket packet) {
  if (packet.protocol == protoTcp) {
    packet.tcp = tryParseTcp(packet.payload);
  } else if (packet.protocol == protoUdp) {
    packet.udp = tryParseUdp(packet.payload);
  }
}

TcpHeader? tryParseTcp(Uint8List p) {
  if (p.length < 20) return null;
  final srcPort = (p[0] << 8) | p[1];
  final dstPort = (p[2] << 8) | p[3];
  final seq = (p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7];
  final ack = (p[8] << 24) | (p[9] << 16) | (p[10] << 8) | p[11];
  final dataOffsetNib = p[12] >> 4;
  final flags = p[13] & 0x3f;
  final window = (p[14] << 8) | p[15];
  final t = TcpHeader(
    srcPort: srcPort & 0xffff,
    dstPort: dstPort & 0xffff,
    seq: seq,
    ack: ack,
    dataOffset: dataOffsetNib,
    flags: flags,
    window: window,
  );
  final start = dataOffsetNib * 4;
  if (start <= p.length) {
    t.dataStart = start;
    t.dataLength = p.length - start;
  } else {
    t.dataStart = p.length;
    t.dataLength = 0;
  }
  return t;
}

UdpHeader? tryParseUdp(Uint8List p) {
  if (p.length < 8) return null;
  return UdpHeader(
    srcPort: (p[0] << 8) | p[1],
    dstPort: (p[2] << 8) | p[3],
    length: (p[4] << 8) | p[5],
  );
}

/// 一个待封装的传输段; 调用 [wrap] 得到完整 IP 报文。
class IpPacketBuilder {
  IpPacketBuilder({
    required this.version,
    required this.srcIp,
    required this.dstIp,
    required this.protocol,
    required this.segment,
  });

  final int version;
  final String srcIp;
  final String dstIp;
  final int protocol;
  final Uint8List segment;

  /// 封装为完整 IP 报文; 非法 IP 返回 null。
  Uint8List? wrap() {
    if (version == 4) return _wrapV4();
    return _wrapV6();
  }

  Uint8List? _wrapV4() {
    final s = _ip4FromString(srcIp);
    final d = _ip4FromString(dstIp);
    if (s.isEmpty || d.isEmpty) return null;
    final total = 20 + segment.length;
    final out = Uint8List(total);
    out[0] = 0x45;
    out[2] = (total >> 8) & 0xff;
    out[3] = total & 0xff;
    out[6] = 0x40;
    out[8] = 64;
    out[9] = protocol;
    for (var i = 0; i < 4; i++) {
      out[12 + i] = s[i];
      out[16 + i] = d[i];
    }
    for (var i = 0; i < segment.length; i++) {
      out[20 + i] = segment[i];
    }
    // IP 头校验和取反 (RFC 791: one's complement); 直接写 _sum16 的结果
    // 会导致对端校验失败而丢弃全部回包 (SYN+ACK/DNS 应答都到不了客户端).
    final cs = ~_sum16(Uint8List.sublistView(out, 0, 20)) & 0xffff;
    out[10] = (cs >> 8) & 0xff;
    out[11] = cs & 0xff;
    return out;
  }

  Uint8List? _wrapV6() {
    final s = _ip6FromString(srcIp);
    final d = _ip6FromString(dstIp);
    if (s == null || d == null) return null;
    final total = 40 + segment.length;
    final out = Uint8List(total);
    out[0] = 0x60;
    out[4] = (segment.length >> 8) & 0xff;
    out[5] = segment.length & 0xff;
    out[6] = protocol;
    out[7] = 64;
    for (var i = 0; i < 16; i++) {
      out[8 + i] = s[i];
      out[24 + i] = d[i];
    }
    for (var i = 0; i < segment.length; i++) {
      out[40 + i] = segment[i];
    }
    return out;
  }
}

/// 构造 TCP 段并封装为 IP 报文 (自动按 v4/v6)。
IpPacketBuilder buildTcpPacket({
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
  final segment = _buildTcp(
    srcIp, dstIp, srcPort, dstPort, seq, ack, flags, window, data,
    isV6: version == 6,
  );
  return IpPacketBuilder(
    version: version,
    srcIp: srcIp,
    dstIp: dstIp,
    protocol: protoTcp,
    segment: segment,
  );
}

/// 构造 UDP 并封装为 IP 报文。
IpPacketBuilder buildUdpPacket({
  required int version,
  required String srcIp,
  required String dstIp,
  required int srcPort,
  required int dstPort,
  List<int>? data,
}) {
  final segment = _buildUdp(srcIp, dstIp, srcPort, dstPort, data, isV6: version == 6);
  return IpPacketBuilder(
    version: version,
    srcIp: srcIp,
    dstIp: dstIp,
    protocol: protoUdp,
    segment: segment,
  );
}

Uint8List _buildTcp(
  String srcIp,
  String dstIp,
  int srcPort,
  int dstPort,
  int seq,
  int ack,
  int flags,
  int window,
  List<int>? data,
  {required bool isV6}
) {
  final dataBytes = data ?? const [];
  final tcpLen = 20;
  final out = Uint8List(tcpLen + dataBytes.length);
  out[0] = (srcPort >> 8) & 0xff;
  out[1] = srcPort & 0xff;
  out[2] = (dstPort >> 8) & 0xff;
  out[3] = dstPort & 0xff;
  out[4] = (seq >> 24) & 0xff;
  out[5] = (seq >> 16) & 0xff;
  out[6] = (seq >> 8) & 0xff;
  out[7] = seq & 0xff;
  out[8] = (ack >> 24) & 0xff;
  out[9] = (ack >> 16) & 0xff;
  out[10] = (ack >> 8) & 0xff;
  out[11] = ack & 0xff;
  out[12] = (5 << 4) & 0xf0;
  out[13] = flags & 0x3f;
  out[14] = (window >> 8) & 0xff;
  out[15] = window & 0xff;
  for (var i = 0; i < dataBytes.length; i++) {
    out[tcpLen + i] = dataBytes[i];
  }
  final cs = isV6
      ? _checksum6(srcIp, dstIp, protoTcp, out)
      : _checksum4(srcIp, dstIp, protoTcp, out);
  out[16] = (cs >> 8) & 0xff;
  out[17] = cs & 0xff;
  return out;
}

Uint8List _buildUdp(
  String srcIp,
  String dstIp,
  int srcPort,
  int dstPort,
  List<int>? data,
  {required bool isV6}
) {
  final dataBytes = data ?? const [];
  final out = Uint8List(8 + dataBytes.length);
  out[0] = (srcPort >> 8) & 0xff;
  out[1] = srcPort & 0xff;
  out[2] = (dstPort >> 8) & 0xff;
  out[3] = dstPort & 0xff;
  out[4] = (out.length >> 8) & 0xff;
  out[5] = out.length & 0xff;
  for (var i = 0; i < dataBytes.length; i++) {
    out[8 + i] = dataBytes[i];
  }
  final cs = isV6
      ? _checksum6(srcIp, dstIp, protoUdp, out)
      : _checksum4(srcIp, dstIp, protoUdp, out);
  out[6] = (cs >> 8) & 0xff;
  out[7] = cs & 0xff;
  return out;
}

int _checksum4(String srcIp, String dstIp, int proto, Uint8List segment) {
  final s = _ip4FromString(srcIp);
  final d = _ip4FromString(dstIp);
  if (s.isEmpty || d.isEmpty) return 0;
  final out = Uint8List(12 + segment.length);
  for (var i = 0; i < 4; i++) {
    out[i] = s[i];
    out[4 + i] = d[i];
  }
  out[8] = 0;
  out[9] = proto;
  out[10] = (segment.length >> 8) & 0xff;
  out[11] = segment.length & 0xff;
  for (var i = 0; i < segment.length; i++) {
    out[12 + i] = segment[i];
  }
  return ~_sum16(out) & 0xffff;
}

int _checksum6(String srcIp, String dstIp, int proto, Uint8List segment) {
  final s = _ip6FromString(srcIp);
  final d = _ip6FromString(dstIp);
  if (s == null || d == null) return 0;
  final out = Uint8List(40 + segment.length);
  for (var i = 0; i < 16; i++) {
    out[i] = s[i];
    out[16 + i] = d[i];
  }
  out[34] = (segment.length >> 8) & 0xff;
  out[35] = segment.length & 0xff;
  out[39] = proto;
  for (var i = 0; i < segment.length; i++) {
    out[40 + i] = segment[i];
  }
  return ~_sum16(out) & 0xffff;
}
