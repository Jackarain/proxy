import 'dart:ui' as ui;

import 'package:flutter/material.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:qr/qr.dart';
import 'package:xproxy/widgets/qr_code_view.dart';

void main() {
  testWidgets('按数据绘制二维码', (tester) async {
    await tester.pumpWidget(
      const MaterialApp(
        home: Scaffold(
          body: Center(child: QrCodeView(data: 'xproxy://config', size: 200)),
        ),
      ),
    );

    expect(tester.takeException(), isNull);
    final view = tester.widget<QrCodeView>(find.byType(QrCodeView));
    expect(view.size, 200);
    expect(find.byType(CustomPaint), findsWidgets);
  });

  testWidgets('内容超出容量时显示占位文案', (tester) async {
    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: Center(child: QrCodeView(data: 'a' * 4000, size: 200)),
        ),
      ),
    );

    expect(tester.takeException(), isNull);
    expect(find.text('配置内容过大, 无法生成二维码'), findsOneWidget);
  });

  testWidgets('绘制结果与 qr 模块矩阵一致', (tester) async {
    const data = 'xproxy://config/1';
    const side = 250.0;
    await tester.pumpWidget(
      const MaterialApp(
        home: Scaffold(
          body: Center(
            child: RepaintBoundary(
              key: ValueKey('qr'),
              child: QrCodeView(data: data, size: side),
            ),
          ),
        ),
      ),
    );

    final boundary = tester.renderObject<RenderRepaintBoundary>(
      find.byKey(const ValueKey('qr')),
    );
    final image = await tester.runAsync(
      () => boundary.toImage(pixelRatio: 1),
    );
    final bytes = await tester.runAsync(
      () => image!.toByteData(format: ui.ImageByteFormat.rawRgba),
    );
    expect(image!.width, side.toInt());
    expect(image.height, side.toInt());
    final rgba = bytes!.buffer.asUint8List();

    final expected = QrImage(
      QrCode(payload: QrPayload.fromString(data)),
    );
    final count = expected.moduleCount;
    final module = side / count;
    for (var row = 0; row < count; row++) {
      for (var col = 0; col < count; col++) {
        final x = ((col + 0.5) * module).floor();
        final y = ((row + 0.5) * module).floor();
        final offset = (y * image.width + x) * 4;
        final dark =
            rgba[offset] < 128 &&
            rgba[offset + 1] < 128 &&
            rgba[offset + 2] < 128;
        expect(dark, expected.isDark(row, col), reason: 'row=$row col=$col');
      }
    }
  });
}
