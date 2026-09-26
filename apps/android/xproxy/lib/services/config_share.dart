import 'dart:convert';

import '../models/vpn_config.dart';

/// 二维码分享内容的类型标识与版本, 用于识别是否为本应用的配置码.
const String _shareType = 'xproxy-config';
const int _shareVersion = 1;

/// 分流列表在二维码中的最大条数. 超出时只保留前 N 条; 列表来自 URL 拉取
/// 时则只带 URL, 扫码方按 URL 重新拉取.
const int shareProxyListLimit = 10;

/// 生成配置二维码的内容.
///
/// 配置本身可直接编码为 json; 分流列表按 [shareProxyListLimit] 收敛, 避免
/// 数据过大导致二维码无法识别.
String encodeConfigShare(VpnConfig config) {
  final map = Map<String, dynamic>.from(config.toJson());
  map['proxyDomains'] = _shareList(config.proxyDomains, config.proxyDomainsUrl);
  map['proxyCidr'] = _shareList(config.proxyCidr, config.proxyCidrUrl);
  return jsonEncode({
    'type': _shareType,
    'version': _shareVersion,
    'config': map,
  });
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
  Object? decoded;
  try {
    decoded = jsonDecode(raw);
  } on FormatException {
    throw const FormatException('二维码内容无法解析');
  }
  if (decoded is! Map<String, dynamic> || decoded['type'] != _shareType) {
    throw const FormatException('二维码不是 xproxy 配置');
  }
  final config = decoded['config'];
  if (config is! Map<String, dynamic>) {
    throw const FormatException('二维码缺少配置内容');
  }
  return VpnConfig.fromJson(config);
}
