import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'logging.dart';

/// 一条受保护出站连接 (经 Kotlin 转发器)。
///
/// 协议 (与 Kotlin handleForward 对齐):
///   -> int32(host:port 长度) + host:port (UTF8)
///   <- int32(1=成功)
///   建立后: Dart 写原始字节 -> 上游; 上游数据以 int32(长度)+数据 帧返回.
abstract class ProtectedChannel {
  /// 读取恰好 [n] 字节 (握手阶段)。EOF 前返回不足字节; 关闭返回空。
  Future<Uint8List> read(int n);

  /// 读取直到 `\r\n\r\n` (HTTP 头)。
  Future<Uint8List> readUntilBlankLine();

  /// 读取一帧: 先读 int32(数据长度) 再读 n 字节数据。
  /// 转发器把上游数据统一帧化为 int32(n)+data, 此方法还原为 data。
  Future<Uint8List> readFrame();

  /// 写出数据到上游。
  void write(List<int> data);

  /// 握手完成后调用: 开始把后续上游数据按帧派发给 [onData]。
  /// 缓冲中已读到的残余不是问题 (read 只消费握手所需量)。
  void startPayload(void Function(Uint8List data) onData);

  /// 到达数据流末尾回调。
  void Function()? onClosed;

  /// 主动关闭 (也触发 onClosed).
  void close();

  /// 是否仍可用。
  bool get isActive;
}

class _Channel extends ProtectedChannel {
  _Channel(this._sock) {
    // 写方向异步错误 (对端关闭后的 Broken pipe 等) 交给读方向 EOF 上报,
    // 这里仅避免其成为 Unhandled Exception.
    _sock.done.catchError((Object _) {});
  }

  final Socket _sock;
  final List<int> _buf = [];
  // deframe: 为 true 时, 把 Kotlin 转发器的 int32(len)+data 帧还原成 data 流.
  // 这样 read/readUntilBlankLine 都基于去帧后的干净数据工作 (SOCKS/HTTP/DNS 握手)。
  bool deframe = false;
  final List<int> _rawBuf = [];
  final List<Completer<void>> _waiters = [];
  bool _eof = false;
  bool _closed = false;
  bool _closedByMe = false;
  void Function(Uint8List)? _payloadHandler;
  bool _payloadMode = false;

  StreamSubscription? _sub;

  void bindListener() {
    _sub = _sock.listen(
      _onChunk,
      onDone: _handleEof,
      onError: (_) => _handleEof(),
      cancelOnError: true,
    );
  }

  void _onChunk(List<int> chunk) {
    if (deframe) {
      _rawBuf.addAll(chunk);
      _purgeRawBuf();
    } else {
      _buf.addAll(chunk);
    }
    _wakeWaiters();
    if (!deframe && _payloadMode && _payloadHandler != null) {
      _pumpPayload();
    }
  }

  // 从原始字节缓冲里按帧 int32(len)+data 提取数据拼入 _buf.
  void _purgeRawBuf() {
    while (_rawBuf.length >= 4) {
      final n = (_rawBuf[0] << 24) | (_rawBuf[1] << 16) |
          (_rawBuf[2] << 8) | (_rawBuf[3]);
      if (n <= 0 || n > 1 << 20) {
        // 坏帧: 丢弃一个字节尝试从下一处对齐.
        _rawBuf.removeAt(0);
        continue;
      }
      if (_rawBuf.length - 4 < n) break;
      _buf.addAll(_rawBuf.getRange(4, 4 + n));
      _rawBuf.removeRange(0, 4 + n);
    }
  }

  // 只唤醒等待者, 不满足读取; 具体读取逻辑在 read/readUntilBlank 重循环.
  void _wakeWaiters() {
    for (final w in _waiters) {
      if (!w.isCompleted) w.complete();
    }
    _waiters.clear();
  }

  void _pumpPayload() {
    if (_buf.isEmpty) return;
    final d = Uint8List.fromList(_buf);
    _buf.clear();
    _payloadHandler!(d);
  }

  void _handleEof() {
    if (_eof) return;
    _eof = true;
    _closed = true;
    try {
      _sub?.cancel();
    } catch (_) {}
    // 对端已关闭: 立即销毁底层 socket, 避免半关闭连接滞留
    // (CLOSE-WAIT) 占满转发器连接池. 后续 close() 幂等返回.
    try {
      _sock.destroy();
    } catch (_) {}
    _wakeWaiters();
    if (!_closedByMe) onClosed?.call();
    _closedByMe = false;
  }

  @override
  Future<Uint8List> read(int n) async {
    while (_buf.length < n) {
      if (_eof) {
        final d = Uint8List.fromList(_buf);
        _buf.clear();
        return d;
      }
      await _wait();
    }
    final d = Uint8List.fromList(_buf.sublist(0, n));
    _buf.removeRange(0, n);
    return d;
  }

  @override
  Future<Uint8List> readFrame() async {
    final lenBytes = await read(4);
    if (lenBytes.length < 4) return Uint8List(0);
    final n = (lenBytes[0] << 24) | (lenBytes[1] << 16) |
        (lenBytes[2] << 8) | lenBytes[3];
    if (n <= 0 || n > 1 << 20) return Uint8List(0);
    return read(n);
  }

  @override
  Future<Uint8List> readUntilBlankLine() async {
    final out = <int>[];
    while (true) {
      var found = -1;
      for (var i = 0; i + 3 < _buf.length; i++) {
        if (_buf[i] == 13 && _buf[i + 1] == 10 &&
            _buf[i + 2] == 13 && _buf[i + 3] == 10) {
          found = i;
          break;
        }
      }
      if (found >= 0) {
        out.addAll(_buf.sublist(0, found + 4));
        _buf.removeRange(0, found + 4);
        return Uint8List.fromList(out);
      }
      out.addAll(_buf);
      _buf.clear();
      if (_eof) return Uint8List.fromList(out);
      await _wait();
    }
  }

  Future<void> _wait() {
    final c = Completer<void>();
    _waiters.add(c);
    return c.future;
  }

  @override
  void write(List<int> data) {
    if (_closed) return;
    try {
      _sock.add(data);
    } catch (_) {}
  }

  @override
  void startPayload(void Function(Uint8List data) onData) {
    _payloadHandler = onData;
    _payloadMode = true;
    _pumpPayload();
  }

  @override
  void close() {
    if (_closed) return;
    _closed = true;
    _eof = true;
    _closedByMe = true;
    try {
      _sub?.cancel();
    } catch (_) {}
    try {
      _sock.destroy();
    } catch (_) {}
    _wakeWaiters();
    onClosed?.call();
  }

  @override
  bool get isActive => !_closed;
}

/// 受保护转发器客户端。
class Forwarder {
  Forwarder({required this.forwardPort, required this.log});

  final int forwardPort;
  final EngineLog log;

  /// 经受保护转发器发一个 UDP 数据报并等待单包应答 (用于国内 DNS)。
  ///
  /// 协议 (与 Kotlin handleForwardUdp 对齐):
  ///   -> int32(-1) + int32(hostLen)+host + int32(port) + int32(qLen)+query
  ///   <- int32(respLen)+resp   (respLen=0 表示失败)
  Future<Uint8List> requestUdp(String host, int port, List<int> datagram) async {
    log.log('【转发UDP】连接转发器 host=$host port=$port', level: 1);
    final raw = await Socket.connect('127.0.0.1', forwardPort);
    _swallowWriteErrors(raw);
    try {
      // 帧: int32(-1)+int32(hostLen)+host+int32(port)+int32(qLen)+query
      // 固定开销 16 字节: mode(4)+hostLen(4)+port(4)+qLen(4).
      final hostBytes = host.codeUnits;
      final full = ByteData(16 + hostBytes.length + datagram.length);
      var o = 0;
      full.setInt32(o, -1, Endian.big); o += 4;
      full.setInt32(o, hostBytes.length, Endian.big); o += 4;
      for (final b in hostBytes) { full.setUint8(o++, b); }
      full.setInt32(o, port, Endian.big); o += 4;
      full.setInt32(o, datagram.length, Endian.big); o += 4;
      for (final q in datagram) { full.setUint8(o++, q); }
      final sent = full.buffer.asUint8List();
      raw.add(sent);
      await raw.flush();

      final ch = _Channel(raw);
      ch.bindListener();
      final lenBytes = await ch.read(4);
      if (lenBytes.length < 4) {
        throw SocketException('UDP 转发器连接被关闭: $host:$port');
      }
      final respLen = (lenBytes[0] << 24) | (lenBytes[1] << 16) |
          (lenBytes[2] << 8) | lenBytes[3];
      if (respLen <= 0) {
        throw SocketException('UDP 转发器失败: $host:$port respLen=$respLen');
      }
      final resp = await ch.read(respLen);
      if (resp.length < respLen) {
        throw SocketException('UDP 应答不完整: $host:$port');
      }
      ch.close();
      return resp;
    } finally {
      raw.destroy();
    }
  }

  /// 建立到目标 [host:port] 的受保护出站连接。
  ///
  /// 设置 [tlsHost]/[disableCheckCert] 时, ack 后把该字节流 TLS 包裹
  /// (用于 https / socks5s 上游)。失败抛 [SocketException]。
  Future<ProtectedChannel> connect(
    String host,
    int port, {
    String? tlsHost,
    bool disableCheckCert = false,
  }) async {
    final raw = await Socket.connect('127.0.0.1', forwardPort);
    _swallowWriteErrors(raw);
    raw.setOption(SocketOption.tcpNoDelay, true);

    final target = '$host:$port';
    final targetBytes = target.codeUnits;
    final header = ByteData(4);
    header.setInt32(0, targetBytes.length, Endian.big);
    raw.add(header.buffer.asUint8List());
    raw.add(targetBytes);

    // 用轻量通道读取 ack.
    final ackCh = _Channel(raw);
    ackCh.bindListener();
    Uint8List ack;
    try {
      // 转发器忙 (连接池打满排队) 时 ack 可能迟迟不来, 超时后关闭释放.
      ack = await ackCh.read(4).timeout(const Duration(seconds: 15));
    } catch (_) {
      ackCh.close();
      throw SocketException('转发器连接超时: $host:$port');
    }
    if (ack.length < 4) {
      ackCh.close();
      throw SocketException('转发器连接被关闭: $host:$port');
    }
    final ok = (ack[0] << 24) | (ack[1] << 16) | (ack[2] << 8) | ack[3];
    if (ok != 1) {
      ackCh.close();
      throw SocketException('转发器连接失败: $host:$port');
    }

    // 注意: 绝不能在此取消 ackCh 的监听. Dart 的 Socket 在最后一个订阅被
    // cancel 时会 shutdown(receive), Linux/Android 上 SHUT_RD 会向对端发送
    // FIN, Kotlin 转发器随即读到 client EOF 并关闭上游, TLS 握手因此失败
    // ("Connection terminated during handshake"). 因此 ack 读取后保持订阅
    // 活跃: SecureSocket.secure 内部通过 _detachRaw 接管底层 raw socket.
    if (tlsHost != null && tlsHost.isNotEmpty) {
      final secure = await SecureSocket.secure(
        raw,
        host: tlsHost,
        onBadCertificate: disableCheckCert ? (_) => true : null,
        context: null,
      );
      secure.setOption(SocketOption.tcpNoDelay, true);
      final ch = _Channel(secure);
      ch.bindListener();
      return ch;
    }

    // 非 TLS 路径直接复用 ackCh: 其监听保持活跃且已缓冲后续数据.
    return ackCh;
  }

  /// 吞掉 socket 写方向的异步错误 (如对端关闭后的 Broken pipe), 避免变成
  /// Unhandled Exception; 连接失败统一由读方向 (EOF/ack 读取) 上报。
  static void _swallowWriteErrors(Socket s) {
    s.done.catchError((Object _) {});
  }
}

// Forwarder 依赖 dart:io SecureSocket —— 需要导入.
