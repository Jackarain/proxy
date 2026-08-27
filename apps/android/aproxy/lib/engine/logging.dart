import 'package:flutter/foundation.dart';

/// 引擎日志队列。
///
/// 引擎各处调用 [log]() 入队；由 [drain] 定时取走并广播为
/// `{"lines": [...]}` 通知 (与控制通道 log 报文结构一致)，交给
/// 控制端 UI 展示。
class EngineLog {
  final List<Map<String, dynamic>> _queue = [];

  /// level: 0=trace/grey, 1=info, 2=warn(orange), 3=error(red).
  void log(String message, {int level = 1}) {
    _queue.add({
      'time': DateTime.now().millisecondsSinceEpoch,
      'level': level,
      'message': message,
    });
    // 同时输出到系统日志, 便于 adb 排障.
    debugPrint('[aproxy] $message', wrapWidth: 2048);
  }

  /// 取走并清空排队的日志行 (供控制通道作为 log 通知上报).
  ({bool has, List<Map<String, dynamic>> lines}) drain() {
    if (_queue.isEmpty) return (has: false, lines: const []);
    final lines = [..._queue];
    _queue.clear();
    return (has: true, lines: lines);
  }
}
