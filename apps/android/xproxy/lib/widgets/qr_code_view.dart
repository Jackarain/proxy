import 'package:flutter/material.dart';
import 'package:qr/qr.dart';

/// 用 qr 包直接绘制的二维码控件.
///
/// 不预留静默边距, 二维码铺满整个控件; 数据超出二维码容量时显示
/// [errorText], 与原先 qr_flutter 的 errorStateBuilder 行为一致.
class QrCodeView extends StatelessWidget {
  const QrCodeView({
    super.key,
    required this.data,
    required this.size,
    this.moduleColor = Colors.black,
    this.backgroundColor = Colors.white,
    this.errorText = '配置内容过大, 无法生成二维码',
  });

  /// 二维码承载的原始文本.
  final String data;

  /// 控件边长 (二维码为正方形).
  final double size;

  /// 前景 (深色模块) 颜色.
  final Color moduleColor;

  /// 背景颜色.
  final Color backgroundColor;

  /// 内容过长无法生成时的占位文案.
  final String errorText;

  @override
  Widget build(BuildContext context) {
    final QrImage image;
    try {
      image = QrImage(
        QrCode(
          payload: QrPayload.fromString(data),
          errorCorrectLevel: QrErrorCorrectLevel.medium,
        ),
      );
    } on InputTooLongException {
      return SizedBox(
        width: size,
        height: size,
        child: Center(
          child: Text(
            errorText,
            textAlign: TextAlign.center,
            style: const TextStyle(color: Colors.black87),
          ),
        ),
      );
    }
    return SizedBox(
      width: size,
      height: size,
      child: CustomPaint(
        painter: _QrPainter(
          image: image,
          moduleColor: moduleColor,
          backgroundColor: backgroundColor,
        ),
      ),
    );
  }
}

class _QrPainter extends CustomPainter {
  const _QrPainter({
    required this.image,
    required this.moduleColor,
    required this.backgroundColor,
  });

  final QrImage image;
  final Color moduleColor;
  final Color backgroundColor;

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawRect(
      Offset.zero & size,
      Paint()..color = backgroundColor,
    );
    final dark = Paint()
      ..color = moduleColor
      ..isAntiAlias = false;
    final count = image.moduleCount;
    final module = size.shortestSide / count;
    for (var row = 0; row < count; row++) {
      final top = (row * module).floorToDouble();
      final bottom = ((row + 1) * module).ceilToDouble();
      for (var col = 0; col < count; col++) {
        if (!image.isDark(row, col)) continue;
        // 模块边界取整, 避免相邻模块之间出现背景色缝隙.
        canvas.drawRect(
          Rect.fromLTRB(
            (col * module).floorToDouble(),
            top,
            ((col + 1) * module).ceilToDouble(),
            bottom,
          ),
          dark,
        );
      }
    }
  }

  @override
  bool shouldRepaint(_QrPainter oldDelegate) =>
      oldDelegate.image != image ||
      oldDelegate.moduleColor != moduleColor ||
      oldDelegate.backgroundColor != backgroundColor;
}
