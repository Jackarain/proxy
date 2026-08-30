import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:aproxy/engine/ip_packet.dart';
import 'package:aproxy/engine/tunnel_engine.dart';

void main() {
  group('TCP MSS 选项', () {
    test('tcpMssOption 构造', () {
      expect(tcpMssOption(1460), [2, 4, 0x05, 0xb4]);
      expect(tcpMssOption(1440), [2, 4, 0x05, 0xa0]);
    });

    test('parseTcpMss 解析', () {
      expect(parseTcpMss(null), isNull);
      expect(parseTcpMss(Uint8List(0)), isNull);
      expect(parseTcpMss(tcpMssOption(1460)), 1460);
      // NOP 填充
      expect(
          parseTcpMss(Uint8List.fromList([1, 2, 4, 0x05, 0xb4])), 1460);
      // 其它选项在前 (SACK-permitted + MSS)
      expect(
        parseTcpMss(Uint8List.fromList([
          4, 2, // SACK permitted
          2, 4, 0x05, 0xb4, // MSS 1460
        ])),
        1460,
      );
      // 截断的 MSS 选项 (len < 4) 不解析
      expect(parseTcpMss(Uint8List.fromList([2, 3, 0x05])), isNull);
    });

    test('SYN+ACK 携带 MSS 选项且头部长度正确', () {
      final pkt = buildTcpPacket(
        version: 4,
        srcIp: '10.0.0.2',
        dstIp: '10.0.0.1',
        srcPort: 443,
        dstPort: 12345,
        seq: 1,
        ack: 1,
        flags: 0x12, // SYN|ACK
        options: tcpMssOption(1460),
      ).wrap();
      expect(pkt, isNotNull);
      final tcp = tryParseV4(pkt!)!.tcp!;
      expect(tcp.dataOffset, 6); // 20 + 4 字节选项
      expect(tcp.dataStart, 24);
      expect(tcp.options, tcpMssOption(1460));
      expect(parseTcpMss(tcp.options), 1460);
    });

    test('IPv6 SYN+ACK 同样携带 MSS 选项', () {
      final pkt = buildTcpPacket(
        version: 6,
        srcIp: 'fd00::2',
        dstIp: 'fd00::1',
        srcPort: 443,
        dstPort: 12345,
        seq: 1,
        ack: 1,
        flags: 0x12,
        options: tcpMssOption(1440),
      ).wrap();
      expect(pkt, isNotNull);
      final tcp = tryParseV6(pkt!)!.tcp!;
      expect(tcp.dataOffset, 6);
      expect(parseTcpMss(tcp.options), 1440);
    });
  });

  group('下行 MSS 切片', () {
    test('不超过 mss 时原样返回', () {
      final data = Uint8List(1000);
      final parts = sliceByMss(data, 1460);
      expect(parts.length, 1);
      expect(parts.single, data);
    });

    test('超过 mss 时按块切分且内容完整', () {
      final data = Uint8List.fromList(List.generate(3000, (i) => i & 0xff));
      final parts = sliceByMss(data, 1460);
      expect(parts.map((p) => p.length).toList(), [1460, 1460, 80]);
      final joined = Uint8List(data.length);
      var off = 0;
      for (final p in parts) {
        joined.setRange(off, off + p.length, p);
        off += p.length;
      }
      expect(joined, data);
    });

    test('非法 mss 回退为整块', () {
      final data = Uint8List(100);
      expect(sliceByMss(data, 0).single, data);
      expect(sliceByMss(data, -1).single, data);
    });
  });
}
