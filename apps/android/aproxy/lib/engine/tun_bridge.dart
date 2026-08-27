import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'logging.dart';

/// TUN 数据面桥接客户端 (连接 Kotlin 桥接服务)。
///
/// 协议: 双向传输均为 `int32(大端长度) + 原始 IP 包` 帧。
/// [onPacket] 收到来自 TUN 的 IP 包；[sendPacket] 把引擎生成的 IP 包写出。
class TunBridge {
  TunBridge({
    required this.port,
    required this.log,
    required this.onPacket,
  });

  final int port;
  final EngineLog log;
  final void Function(Uint8List packet) onPacket;

  Socket? _sock;
  bool _closed = true;
  final List<int> _buf = [];

  void Function()? onDisconnect;

  bool get connected => !_closed && _sock != null;

  /// 连接到桥接服务并开始接收数据包。断开时触发 [onDisconnect]。
  Future<void> connect() async {
    final sock = await Socket.connect('127.0.0.1', port);
    sock.setOption(SocketOption.tcpNoDelay, true);
    _sock = sock;
    _closed = false;
    sock.listen(
      _onChunk,
      onDone: _handleDisconnect,
      onError: (_) => _handleDisconnect(),
      cancelOnError: true,
    );
    log.log('TUN 数据面已连接 (port=$port)', level: 1);
  }

  void _onChunk(List<int> chunk) {
    _buf.addAll(chunk);
    while (_buf.length >= 4) {
      final len = (_buf[0] << 24) | (_buf[1] << 16) | (_buf[2] << 8) | _buf[3];
      if (len < 0 || _buf.length - 4 < len) break;
      final data = Uint8List.fromList(_buf.sublist(4, 4 + len));
      _buf.removeRange(0, 4 + len);
      onPacket(data);
    }
  }

  void _handleDisconnect() {
    if (_closed) return;
    _closed = true;
    log.log('TUN 数据面断开', level: 2);
    final d = onDisconnect;
    if (d != null) d();
  }

  void sendPacket(List<int> packet) {
    final s = _sock;
    if (s == null || _closed) return;
    try {
      final header = ByteData(4);
      header.setInt32(0, packet.length, Endian.big);
      s.add(header.buffer.asUint8List());
      s.add(packet);
    } catch (_) {}
  }

  void close() {
    _closed = true;
    try {
      _sock?.destroy();
    } catch (_) {}
    _sock = null;
  }
}
