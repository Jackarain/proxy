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
    return AlertDialog(
      title: const Text('分享配置'),
      content: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            color: Colors.white,
            padding: const EdgeInsets.all(12),
            // 固定尺寸: QrImageView 内部使用 LayoutBuilder, 不加约束时
            // AlertDialog 的固有尺寸测量会报错.
            child: SizedBox(
              width: 240,
              height: 240,
              child: QrImageView(
                data: encodeConfigShare(config),
                size: 240,
                version: QrVersions.auto,
                errorCorrectionLevel: QrErrorCorrectLevel.M,
                backgroundColor: Colors.white,
                errorStateBuilder: (_, _) => const Text('配置内容过大, 无法生成二维码'),
              ),
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
