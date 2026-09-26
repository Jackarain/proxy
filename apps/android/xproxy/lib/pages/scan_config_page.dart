import 'package:flutter/material.dart';
import 'package:image_picker/image_picker.dart';
import 'package:mobile_scanner/mobile_scanner.dart';

/// 扫码页: 相机实时扫描二维码, 也可从相册选图识别.
///
/// 识别成功后以二维码原始文本作为结果 pop 返回; 用户返回则为 null.
class ScanConfigPage extends StatefulWidget {
  const ScanConfigPage({super.key});

  @override
  State<ScanConfigPage> createState() => _ScanConfigPageState();
}

class _ScanConfigPageState extends State<ScanConfigPage> {
  final MobileScannerController _controller = MobileScannerController(
    formats: const [BarcodeFormat.qrCode],
    detectionSpeed: DetectionSpeed.noDuplicates,
  );
  bool _handled = false;

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  void _complete(String raw) {
    if (_handled || !mounted) return;
    _handled = true;
    Navigator.of(context).pop(raw);
  }

  void _onDetect(BarcodeCapture capture) {
    final raw = _firstValue(capture);
    if (raw != null) _complete(raw);
  }

  static String? _firstValue(BarcodeCapture capture) {
    for (final barcode in capture.barcodes) {
      final raw = barcode.rawValue;
      if (raw != null && raw.isNotEmpty) return raw;
    }
    return null;
  }

  Future<void> _pickFromGallery() async {
    final picked = await ImagePicker().pickImage(source: ImageSource.gallery);
    if (picked == null || !mounted) return;
    try {
      final capture = await _controller.analyzeImage(picked.path);
      final raw = capture == null ? null : _firstValue(capture);
      if (!mounted) return;
      if (raw == null) {
        _showMessage('未在图片中识别到二维码');
        return;
      }
      _complete(raw);
    } catch (e) {
      if (mounted) _showMessage('识别失败: $e');
    }
  }

  void _showMessage(String message) {
    ScaffoldMessenger.of(
      context,
    ).showSnackBar(SnackBar(content: Text(message)));
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('扫码添加配置'),
        actions: [
          IconButton(
            tooltip: '从相册选择',
            onPressed: _pickFromGallery,
            icon: const Icon(Icons.photo_library_outlined),
          ),
        ],
      ),
      body: Stack(
        fit: StackFit.expand,
        children: [
          MobileScanner(
            controller: _controller,
            onDetect: _onDetect,
            errorBuilder:
                (_, error) => Center(
                  child: Padding(
                    padding: const EdgeInsets.all(24),
                    child: Text(
                      '相机不可用: ${error.errorCode.name}\n请检查相机权限后重试',
                      textAlign: TextAlign.center,
                    ),
                  ),
                ),
          ),
          IgnorePointer(
            child: Center(
              child: Container(
                width: 240,
                height: 240,
                decoration: BoxDecoration(
                  border: Border.all(color: Colors.white70, width: 3),
                ),
              ),
            ),
          ),
          const Positioned(
            left: 0,
            right: 0,
            bottom: 32,
            child: Text(
              '将二维码放入框内, 或从相册选择图片',
              textAlign: TextAlign.center,
              style: TextStyle(color: Colors.white),
            ),
          ),
        ],
      ),
    );
  }
}
