import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:xproxy/main.dart';

void main() {
  testWidgets('app builds', (tester) async {
    SharedPreferences.setMockInitialValues({});
    await tester.pumpWidget(const XproxyApp());
    await tester.pumpAndSettle();
    expect(find.text('xProxy'), findsOneWidget);
    expect(find.text('暂无配置, 点击右下角添加'), findsOneWidget);
    expect(find.text('添加配置'), findsOneWidget);
  });

  testWidgets('添加配置菜单为矩形且含手动/扫码入口', (tester) async {
    SharedPreferences.setMockInitialValues({});
    await tester.pumpWidget(const XproxyApp());
    await tester.pumpAndSettle();

    await tester.tap(find.text('添加配置'));
    await tester.pumpAndSettle();

    expect(find.text('手动添加'), findsOneWidget);
    expect(find.text('扫码添加'), findsOneWidget);
    // 实际形状由 bottomSheetTheme 应用在内部 Material 上.
    final material = tester.widget<Material>(
      find
          .descendant(
            of: find.byType(BottomSheet),
            matching: find.byType(Material),
          )
          .first,
    );
    final shape = material.shape as RoundedRectangleBorder?;
    expect(shape?.borderRadius, BorderRadius.zero);
  });
}
