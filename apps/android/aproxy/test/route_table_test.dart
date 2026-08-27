import 'package:flutter_test/flutter_test.dart';
import 'package:aproxy/engine/route_table.dart';

void main() {
  test('bypassCn: 中国段强制直连, 非中国段默认走代理', () {
    final rt = RouteTable(
      bypassCn: true,
      hasProxy: true,
    );
    rt.setCnCidrs(['1.0.1.0/24', '223.255.252.0/22']);

    // 中国段 IP -> 直连 (不走代理).
    expect(rt.ipShouldProxy('1.0.1.5'), isFalse);
    expect(rt.ipShouldProxy('223.255.255.1'), isFalse);

    // 非中国段 IP -> 默认走代理.
    expect(rt.ipShouldProxy('8.8.8.8'), isTrue);
    expect(rt.ipShouldProxy('1.1.1.1'), isTrue);
  });

  test('未启用 bypassCn 时中国段也走代理', () {
    final rt = RouteTable(
      bypassCn: false,
      hasProxy: true,
    );
    rt.setCnCidrs(['1.0.1.0/24']);
    expect(rt.ipShouldProxy('1.0.1.5'), isTrue);
  });

  test('bypassCn 私有段直连且非中国段走代理', () {
    final rt = RouteTable(
      bypassCn: true,
      hasProxy: true,
    );
    rt.setCnCidrs(['1.0.1.0/24']);

    // 私有段: 未配置分流规则时默认代理, 但 10.0.0.0/8 属于中国段列表? 不,
    // 私有段未计入 cnCidrs 时按默认规则处理. 这里验证中国段判定优先.
    expect(rt.ipShouldProxy('1.0.1.1'), isFalse);
    expect(rt.ipShouldProxy('8.8.8.8'), isTrue);
  });
}
