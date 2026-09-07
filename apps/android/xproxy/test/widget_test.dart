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
}
