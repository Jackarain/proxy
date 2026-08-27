import 'package:flutter_test/flutter_test.dart';
import 'package:aproxy/services/cn_ip_list.dart';

void main() {
  group('CnIpList 区间运算', () {
    test('cidrToRange 解析', () {
      expect(CnIpList.cidrToRange('0.0.0.0/0'), (0, 0xffffffff));
      expect(CnIpList.cidrToRange('192.168.1.0/24'),
          (0xc0a80100, 0xc0a801ff));
      expect(CnIpList.cidrToRange('1.2.3.4/32'), (0x01020304, 0x01020304));
      expect(CnIpList.cidrToRange('bad'), isNull);
      expect(CnIpList.cidrToRange('1.2.3.4/33'), isNull);
    });

    test('mergeRanges 合并重叠与相邻', () {
      expect(
        CnIpList.mergeRanges([(0, 10), (5, 20), (30, 40), (41, 50)]),
        [(0, 20), (30, 50)],
      );
    });

    test('complementRanges 求补集', () {
      expect(
        CnIpList.complementRanges([(0, 127)]),
        [(128, 0xffffffff)],
      );
      expect(
        CnIpList.complementRanges([(100, 200)]),
        [(0, 99), (201, 0xffffffff)],
      );
    });

    test('rangesToCidrs 区间转 CIDR', () {
      expect(CnIpList.rangesToCidrs([(0, 255)]), ['0.0.0.0/24']);
      expect(CnIpList.rangesToCidrs([(0, 0xffffffff)]), ['0.0.0.0/0']);
      expect(
        CnIpList.rangesToCidrs([(0xc0a80100, 0xc0a801ff)]),
        ['192.168.1.0/24'],
      );
    });

    test('vpnRoutes 补集覆盖全空间且不含中国段', () {
      const cn = ['1.0.1.0/24', '223.255.252.0/22'];
      final routes = CnIpList.vpnRoutes(cn);
      expect(routes, isNotEmpty);

      // 中国段不得出现在 VPN 路由中.
      for (final r in routes) {
        expect(r, isNot(contains('1.0.1.0/24')));
        expect(r, isNot(contains('223.255.252.0/22')));
      }

      // 私有段不得出现在 VPN 路由中.
      for (final p in CnIpList.privateRanges) {
        expect(routes, isNot(contains(p)));
      }

      // 补集 + 直连 = 全空间 (区间无重叠无遗漏).
      final direct = <(int, int)>[
        for (final c in cn)
          if (CnIpList.cidrToRange(c) != null) CnIpList.cidrToRange(c)!,
        for (final p in CnIpList.privateRanges)
          if (CnIpList.cidrToRange(p) != null) CnIpList.cidrToRange(p)!,
      ];
      final directMerged = CnIpList.mergeRanges(direct);
      final vpn = <(int, int)>[
        for (final c in routes)
          CnIpList.cidrToRange(c)!,
      ]..sort((a, b) => a.$1.compareTo(b.$1));

      var total = 0;
      var prev = -1;
      for (final r in [...directMerged, ...vpn]..sort(
        (a, b) => a.$1.compareTo(b.$1),
      )) {
        expect(r.$1, prev + 1, reason: '区间应连续无空洞');
        total += r.$2 - r.$1 + 1;
        prev = r.$2;
      }
      expect(total, 0x100000000);
    });
  });
}
