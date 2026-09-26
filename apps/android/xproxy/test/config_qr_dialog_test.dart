import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:qr_flutter/qr_flutter.dart';
import 'package:xproxy/models/vpn_config.dart';
import 'package:xproxy/widgets/config_qr_dialog.dart';

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
    expect(find.byType(QrImageView), findsOneWidget);
    final dialog = tester.widget<AlertDialog>(find.byType(AlertDialog));
    expect(dialog.backgroundColor, Colors.white);
    final qr = tester.widget<QrImageView>(find.byType(QrImageView));
    expect(qr.size, greaterThan(240));
    expect(qr.padding, EdgeInsets.zero);
  });
}
