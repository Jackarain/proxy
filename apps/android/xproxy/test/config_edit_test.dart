import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:xproxy/models/vpn_config.dart';
import 'package:xproxy/pages/config_edit_page.dart';

Future<void> _pumpEdit(WidgetTester tester, VpnConfig config) async {
  await tester.pumpWidget(
    MaterialApp(home: ConfigEditPage(config: config, isNew: true)),
  );
  await tester.pumpAndSettle();
}

/// 只匹配纵向列表: TabBarView 内每个 Tab 的 ListView (TextField 的
/// Scrollable 不在此列).
Finder get _list =>
    find
        .byWidgetPredicate(
          (w) => w is Scrollable && w.axisDirection == AxisDirection.down,
        )
        .first;

TextField _field(WidgetTester tester, String label) {
  return tester.widget<TextField>(
    find.byWidgetPredicate(
      (w) => w is TextField && w.decoration?.labelText == label,
    ),
  );
}

void main() {
  testWidgets('基本 Tab 按分组卡片展示', (tester) async {
    await _pumpEdit(tester, VpnConfig(id: '1', name: '办公室'));

    expect(find.text('名称'), findsOneWidget);
    expect(find.text('上游代理'), findsOneWidget);
    expect(find.text('TLS'), findsOneWidget);
    expect(find.text('关闭上游证书校验'), findsOneWidget);
    await tester.scrollUntilVisible(find.text('测试连接'), 200, scrollable: _list);
    expect(find.text('测试连接 URL'), findsOneWidget);
  });

  testWidgets('DNS Tab 分组展示服务器与选项', (tester) async {
    await _pumpEdit(tester, VpnConfig(id: '2', name: 'DNS'));

    await tester.tap(find.widgetWithText(Tab, 'DNS 配置'));
    await tester.pumpAndSettle();

    expect(find.text('DNS 服务器'), findsOneWidget);
    expect(find.text('国内 DNS (每行一个 IP, 不支持 DoH)'), findsOneWidget);
    expect(find.text('国外 DNS / DoH (每行一个)'), findsOneWidget);
    await tester.scrollUntilVisible(
      find.text('DNS 选项'),
      200,
      scrollable: _list,
    );
    expect(find.text('禁用 IPv6 解析'), findsOneWidget);
  });

  testWidgets('分流 Tab 全局代理开启后代理域名/CIDR 输入被禁用', (tester) async {
    await _pumpEdit(tester, VpnConfig(id: '3', name: '全局', globalProxy: true));

    await tester.tap(find.widgetWithText(Tab, '分流'));
    await tester.pumpAndSettle();

    expect(find.text('分流规则'), findsOneWidget);
    await tester.scrollUntilVisible(
      find.text('代理 CIDR (每行一个)'),
      200,
      scrollable: _list,
    );
    expect(_field(tester, '代理域名 (每行一个)').enabled, isFalse);
    expect(_field(tester, '代理 CIDR (每行一个)').enabled, isFalse);
  });

  testWidgets('分流列表支持从 URL 拉取', (tester) async {
    await _pumpEdit(tester, VpnConfig(id: '4', name: '分流测试'));

    await tester.tap(find.widgetWithText(Tab, '分流'));
    await tester.pumpAndSettle();

    await tester.scrollUntilVisible(
      find.text('从 URL 拉取 CIDR 列表'),
      200,
      scrollable: _list,
    );
    expect(find.text('从 URL 拉取域名列表'), findsOneWidget);
    expect(find.text('从 URL 拉取 CIDR 列表'), findsOneWidget);
  });

  testWidgets('分应用分流: 仅选中应用走代理时要求已选应用', (tester) async {
    await _pumpEdit(
      tester,
      VpnConfig(id: '5', name: '分应用', proxyPass: 'https://1.2.3.4:443'),
    );

    await tester.tap(find.widgetWithText(Tab, '分流'));
    await tester.pumpAndSettle();
    await tester.scrollUntilVisible(
      find.text('已选应用'),
      200,
      scrollable: _list,
    );
    expect(find.text('未选择'), findsOneWidget);

    await tester.ensureVisible(find.text('仅选中应用走代理'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('仅选中应用走代理'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('保存'));
    await tester.pumpAndSettle();

    expect(find.textContaining('至少需要选择一个应用'), findsOneWidget);
  });

  testWidgets('分应用分流: 保存保留模式与已选应用', (tester) async {
    final config = VpnConfig(
      id: '6',
      name: '分应用',
      proxyPass: 'https://1.2.3.4:443',
      appSplitMode: AppSplitMode.exclude,
      appSplitPackages: ['com.android.chrome', 'org.mozilla.firefox'],
    );
    VpnConfig? saved;
    await tester.pumpWidget(
      MaterialApp(
        home: Builder(
          builder:
              (ctx) => TextButton(
                onPressed: () async {
                  saved = await Navigator.of(ctx).push<VpnConfig>(
                    MaterialPageRoute(
                      builder:
                          (_) => ConfigEditPage(config: config, isNew: false),
                    ),
                  );
                },
                child: const Text('打开'),
              ),
        ),
      ),
    );
    await tester.tap(find.text('打开'));
    await tester.pumpAndSettle();

    await tester.tap(find.widgetWithText(Tab, '分流'));
    await tester.pumpAndSettle();
    await tester.scrollUntilVisible(
      find.text('已选 2 个应用'),
      200,
      scrollable: _list,
    );
    expect(find.text('已选 2 个应用'), findsOneWidget);

    await tester.tap(find.text('保存'));
    await tester.pumpAndSettle();

    expect(saved?.appSplitMode, AppSplitMode.exclude);
    expect(saved?.appSplitPackages, [
      'com.android.chrome',
      'org.mozilla.firefox',
    ]);
  });
}
