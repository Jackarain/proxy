import 'dart:convert';
import 'dart:io';

import 'package:shared_preferences/shared_preferences.dart';

import '../models/vpn_config.dart';

/// 中国大陆 IP 段 (CIDR) 列表服务.
///
/// 数据源默认 https://ispip.clang.cn/all_cn.txt, 拉取结果缓存到本地,
/// 启动时自动更新, 每日最多拉取一次; 更新时与缓存合并 (新增项保留),
/// 下次建立 VPN 时应用完整列表.
///
/// "绕过中国大陆" 的实现: VpnService 的 addRoute 是"接入 VPN"的路由,
/// 因此这里计算"非中国且非保留段"作为 VPN 路由, 中国大陆与私有网段
/// 不进入 VPN 路由表, 由系统按物理网络直连.
class CnIpList {
  static const String sourceUrl = 'https://ispip.clang.cn/all_cn.txt';
  static const String _cacheKey = 'cn_ip_list_cache';
  static const String _cacheTimeKey = 'cn_ip_list_cache_time';
  static const Duration _updateInterval = Duration(days: 1);

  /// 必须直连的保留/私有网段 (不进入 VPN).
  static const List<String> privateRanges = [
    '0.0.0.0/8',
    '10.0.0.0/8',
    '100.64.0.0/10',
    '127.0.0.0/8',
    '169.254.0.0/16',
    '172.16.0.0/12',
    '192.0.0.0/24',
    '192.0.2.0/24',
    '192.168.0.0/16',
    '198.18.0.0/15',
    '198.51.100.0/24',
    '203.0.113.0/24',
    '224.0.0.0/4',
    '240.0.0.0/4',
    '255.255.255.255/32',
  ];

  static List<String>? _cached;

  /// 组装启动配置: bypassCn 时附加 tunRoutes (非中国段 VPN 路由) 并返回
  /// 中国段列表 (供引擎数据面分流兜底); 未启用时原样返回配置.
  static Future<({Map<String, dynamic> json, List<String> cnCidrs})>
      prepareStart(VpnConfig config) async {
    final map = config.toJson();
    if (!config.bypassCn) return (json: map, cnCidrs: const <String>[]);
    final cnCidrs = await update();
    if (cnCidrs.isNotEmpty) {
      map['tunRoutes'] = vpnRoutes(cnCidrs);
    }
    return (json: map, cnCidrs: cnCidrs);
  }

  /// 获取当前 CN 段列表: 优先返回缓存 (立即), 每日自动拉取更新.
  /// [force] 强制重新拉取. 拉取失败时回退缓存.
  static Future<List<String>> update({bool force = false}) async {
    final cached = _cached ??= await loadCache();
    if (!force && cached.isNotEmpty && !await _expired()) {
      return cached;
    }
    try {
      final fresh = await fetchFromSource();
      if (fresh.isNotEmpty) {
        final merged = <String>{...cached, ...fresh}.toList()..sort();
        _cached = merged;
        await saveCache(merged);
        return merged;
      }
    } catch (_) {
      // 拉取失败时回退缓存.
    }
    return cached;
  }

  /// 计算 VPN 路由: 非中国且非保留网段 (接入 VPN), 其余直连.
  /// 返回 CIDR 列表, 供 VpnService establish 时 addRoute.
  static List<String> vpnRoutes(List<String> cnCidrs) {
    final direct = <(int, int)>[];
    for (final c in cnCidrs) {
      final r = cidrToRange(c);
      if (r != null) direct.add(r);
    }
    for (final p in privateRanges) {
      final r = cidrToRange(p);
      if (r != null) direct.add(r);
    }
    return rangesToCidrs(complementRanges(direct));
  }

  static Future<List<String>> fetchFromSource() async {
    final client = HttpClient()
      ..connectionTimeout = const Duration(seconds: 15);
    try {
      final req = await client.getUrl(Uri.parse(sourceUrl));
      final resp = await req.close();
      if (resp.statusCode != HttpStatus.ok) {
        throw HttpException('HTTP ${resp.statusCode}');
      }
      final text = await resp.transform(utf8.decoder).join();
      final cidrs = <String>[];
      for (final line in text.split(RegExp(r'[\r\n]+'))) {
        final t = line.trim();
        if (t.contains('/')) cidrs.add(t);
      }
      return cidrs;
    } finally {
      client.close();
    }
  }

  static Future<List<String>> loadCache() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getString(_cacheKey);
    if (raw == null || raw.isEmpty) return [];
    return raw
        .split('\n')
        .where((e) => e.trim().isNotEmpty)
        .toList();
  }

  static Future<void> saveCache(List<String> cidrs) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_cacheKey, cidrs.join('\n'));
    await prefs.setInt(_cacheTimeKey, DateTime.now().millisecondsSinceEpoch);
  }

  static Future<bool> _expired() async {
    final prefs = await SharedPreferences.getInstance();
    final last = prefs.getInt(_cacheTimeKey) ?? 0;
    return DateTime.now().millisecondsSinceEpoch - last >
        _updateInterval.inMilliseconds;
  }

  // ---- IP 区间运算 ----

  /// "a.b.c.d/prefix" -> (网络起始, 网络结束); 非法返回 null.
  static (int, int)? cidrToRange(String cidr) {
    final slash = cidr.indexOf('/');
    if (slash <= 0) return null;
    final ip = _ipToInt(cidr.substring(0, slash).trim());
    final prefix = int.tryParse(cidr.substring(slash + 1).trim());
    if (ip == null || prefix == null || prefix < 0 || prefix > 32) {
      return null;
    }
    final mask = prefix == 0 ? 0 : (0xffffffff << (32 - prefix)) & 0xffffffff;
    final start = ip & mask;
    return (start, start | (0xffffffff & ~mask));
  }

  /// 合并重叠/相邻区间.
  static List<(int, int)> mergeRanges(List<(int, int)> ranges) {
    final sorted = [...ranges]..sort((a, b) => a.$1.compareTo(b.$1));
    final out = <(int, int)>[];
    for (final r in sorted) {
      if (out.isEmpty || r.$1 > out.last.$2 + 1) {
        out.add(r);
      } else if (r.$2 > out.last.$2) {
        out[out.length - 1] = (out.last.$1, r.$2);
      }
    }
    return out;
  }

  /// 从整个 IPv4 空间减去 [direct], 返回其余区间 (非中国非保留段).
  static List<(int, int)> complementRanges(List<(int, int)> direct) {
    final merged = mergeRanges(direct);
    final out = <(int, int)>[];
    int cur = 0;
    for (final r in merged) {
      if (r.$1 > cur) out.add((cur, r.$1 - 1));
      if (r.$2 + 1 > cur) cur = r.$2 + 1;
      if (cur > 0xffffffff) break;
    }
    if (cur <= 0xffffffff) out.add((cur, 0xffffffff));
    return out;
  }

  /// 区间列表转为 CIDR 列表 (贪心最大对齐块).
  static List<String> rangesToCidrs(List<(int, int)> ranges) {
    final out = <String>[];
    for (final (start, end) in ranges) {
      var s = start;
      while (s <= end) {
        final size = _largestAlignedBlock(s, end);
        final prefix = 32 - (size.bitLength - 1);
        out.add('${_intToIp(s)}/$prefix');
        s += size;
      }
    }
    return out;
  }

  /// 返回覆盖 [start, end] 且以 start 对齐的最大 2 的幂块大小.
  static int _largestAlignedBlock(int start, int end) {
    for (var size = 1 << 32; size > 0; size >>= 1) {
      if ((start & (size - 1)) == 0 && start + size - 1 <= end) {
        return size;
      }
    }
    return 1;
  }

  static int? _ipToInt(String ip) {
    final parts = ip.split('.');
    if (parts.length != 4) return null;
    var v = 0;
    for (final p in parts) {
      final o = int.tryParse(p);
      if (o == null || o < 0 || o > 255) return null;
      v = (v << 8) | o;
    }
    return v;
  }

  static String _intToIp(int v) =>
      '${(v >> 24) & 0xff}.${(v >> 16) & 0xff}.${(v >> 8) & 0xff}.${v & 0xff}';
}
