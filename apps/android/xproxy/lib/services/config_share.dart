import 'dart:convert';
import 'dart:io' show GZipCodec, ZLibCodec, ZLibOption;

import '../models/vpn_config.dart';
import 'config_share_dict.dart';

/// 二维码分享内容的类型标识与版本, 用于识别是否为本应用的配置码.
const String _shareType = 'xproxy-config';
const int _shareVersion = 2;

/// 当前压缩格式前缀: deflate + 预共享字典.
const String _sharePrefixV2 = 'xproxy2:';

/// 旧版格式前缀 (gzip, 无字典), 解码时兼容.
const String _sharePrefixV1 = 'xproxy1:';

/// 最高级别 deflate + 预共享字典: 与字典无关的公共片段几乎零成本,
/// 是当前体积最小的方案 (由 dart:io 内置 zlib 提供, 无需额外依赖).
final ZLibCodec _deflate = ZLibCodec(
  level: ZLibOption.maxLevel,
  dictionary: configShareDictionary,
);

/// 旧版 gzip 解码器 (仅用于兼容 xproxy1: 二维码).
final GZipCodec _gzip = GZipCodec(
  level: ZLibOption.maxLevel,
  windowBits: ZLibOption.maxWindowBits,
  memLevel: ZLibOption.maxMemLevel,
);

/// 分流列表在二维码中的最大条数. 超出时只保留前 N 条; 列表来自 URL 拉取
/// 时则只带 URL, 扫码方按 URL 重新拉取.
const int shareProxyListLimit = 10;

/// 生成配置二维码的内容.
///
/// json 正文经 deflate(预共享字典) 压缩后再做 base64url, 缩小二维码数据量;
/// 分流列表按 [shareProxyListLimit] 收敛, 避免数据过大导致二维码无法识别.
String encodeConfigShare(VpnConfig config) {
  final map = Map<String, dynamic>.from(config.toJson());
  map['proxyDomains'] = _shareList(config.proxyDomains, config.proxyDomainsUrl);
  map['proxyCidr'] = _shareList(config.proxyCidr, config.proxyCidrUrl);
  final json = jsonEncode({
    'type': _shareType,
    'version': _shareVersion,
    'config': map,
  });
  return '$_sharePrefixV2${base64Url.encode(_deflate.encode(utf8.encode(json)))}';
}

List<String> _shareList(List<String> items, String url) {
  if (url.trim().isNotEmpty) return const [];
  if (items.length <= shareProxyListLimit) return items;
  return items.take(shareProxyListLimit).toList();
}

/// 解析扫码得到的配置内容; 内容非法时抛出 [FormatException].
///
/// 返回的配置沿用二维码里的 id, 是否重新生成 id 由调用方决定.
VpnConfig decodeConfigShare(String raw) {
  final decoded = _decodePayload(raw);
  if (decoded is! Map<String, dynamic> || decoded['type'] != _shareType) {
    throw const FormatException('二维码不是 xproxy 配置');
  }
  final config = decoded['config'];
  if (config is! Map<String, dynamic>) {
    throw const FormatException('二维码缺少配置内容');
  }
  return VpnConfig.fromJson(config);
}

/// 解出正文 JSON: 支持 v2 字典压缩, 兼容 v1 gzip 与未压缩的明文 JSON.
Object? _decodePayload(String raw) {
  final text = raw.trim();
  if (text.startsWith(_sharePrefixV2)) {
    return _unpack(text, _sharePrefixV2.length, _deflate);
  }
  if (text.startsWith(_sharePrefixV1)) {
    return _unpack(text, _sharePrefixV1.length, _gzip);
  }
  if (text.startsWith('{')) return jsonDecode(text);
  throw const FormatException('二维码不是 xproxy 配置');
}

Object? _unpack(
  String text,
  int prefixLength,
  Codec<List<int>, List<int>> codec,
) {
  try {
    final packed = base64Url.decode(text.substring(prefixLength));
    return jsonDecode(utf8.decode(codec.decode(packed)));
  } on FormatException {
    throw const FormatException('二维码内容无法解析');
  }
}
