import 'package:flutter_test/flutter_test.dart';
import 'package:xproxy/main.dart';

void main() {
  testWidgets('app builds', (tester) async {
    await tester.pumpWidget(const XproxyApp());
    expect(find.text('proxy 配置'), findsOneWidget);
  });
}
