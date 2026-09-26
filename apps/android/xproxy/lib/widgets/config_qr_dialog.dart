import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:qr_flutter/qr_flutter.dart';

import '../models/vpn_config.dart';
import '../services/config_share.dart';

/// 弹出显示配置二维码的对话框.
Future<void> showConfigQrDialog(BuildContext context, VpnConfig config) {
  return showDialog<void>(
    context: context,
    builder: (_) => _ConfigQrDialog(config: config),
  );
}

class _ConfigQrDialog extends StatelessWidget {
  const _ConfigQrDialog({required this.config});

  final VpnConfig config;

  @override
  Widget build(BuildContext context) {
    // 二维码尽量占满弹窗宽度, 同时避免高度不足时溢出 (留出标题/按钮/名称).
    final media = MediaQuery.sizeOf(context);
    final side =
        math
            .min(media.width - 64, media.height - 230)
            .clamp(160.0, 420.0)
            .toDouble();
    return AlertDialog(
      title: const Text('分享配置'),
      insetPadding: const EdgeInsets.all(16),
      contentPadding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          // 固定尺寸: QrImageView 内部使用 LayoutBuilder, 不加约束时
          // AlertDialog 的固有尺寸测量会报错.
          SizedBox(
            width: side,
            height: side,
            child: QrImageView(
              data: encodeConfigShare(config),
              size: side,
              version: QrVersions.auto,
              errorCorrectionLevel: QrErrorCorrectLevel.M,
              backgroundColor: Colors.white,
              // 不留静默边距, 二维码铺满整个区域.
              padding: EdgeInsets.zero,
              errorStateBuilder: (_, _) => const Text('配置内容过大, 无法生成二维码'),
            ),
          ),
          const SizedBox(height: 12),
          Text(config.name, style: Theme.of(context).textTheme.titleSmall),
        ],
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.of(context).pop(),
          child: const Text('关闭'),
        ),
      ],
    );
  }
}
