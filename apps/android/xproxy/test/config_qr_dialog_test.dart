import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:xproxy/models/vpn_config.dart';
import 'package:xproxy/widgets/config_qr_dialog.dart';
import 'package:xproxy/widgets/qr_code_view.dart';

void main() {
  testWidgets('分享弹窗显示配置二维码', (tester) async {
    final config = VpnConfig(
      id: '1',
      name: '办公室',
      proxyPass: 'https://1.2.3.4:443',
    );
    await tester.pumpWidget(
      MaterialApp(
        home: Builder(
          builder:
              (ctx) => TextButton(
                onPressed: () => showConfigQrDialog(ctx, config),
                child: const Text('分享'),
              ),
        ),
      ),
    );

    await tester.tap(find.text('分享'));
    await tester.pumpAndSettle();

    expect(find.text('分享配置'), findsNothing);
    expect(find.text('办公室'), findsOneWidget);
    expect(find.byType(QrCodeView), findsOneWidget);
    final dialog = tester.widget<Dialog>(find.byType(Dialog));
    expect(dialog.backgroundColor, Colors.white);
    final qr = tester.widget<QrCodeView>(find.byType(QrCodeView));
    expect(qr.size, greaterThan(240));

    // 无关闭按钮, 点击弹窗任意位置关闭.
    expect(find.text('关闭'), findsNothing);
    await tester.tap(find.byType(QrCodeView));
    await tester.pumpAndSettle();
    expect(find.byType(QrCodeView), findsNothing);
  });
}
