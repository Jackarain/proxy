import 'package:flutter_test/flutter_test.dart';
import 'package:aproxy/main.dart';

void main() {
  testWidgets('app builds', (tester) async {
    await tester.pumpWidget(const AproxyApp());
    expect(find.text('proxy 配置'), findsOneWidget);
  });
}
