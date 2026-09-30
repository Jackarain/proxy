import { util } from '@kit.ArkTS';
import { VpnConfig, VpnConfigJson } from '../model/VpnConfig';
import { configShareDictionary } from './ConfigShareDict';
import { zlibCompress } from './Deflate';
import { gzipDecompress, utf8Decode, zlibDecompress } from './Inflate';

/** 二维码分享内容的类型标识与版本 (与 Android 端一致). */
const SHARE_TYPE: string = 'xproxy-config';
const SHARE_VERSION: number = 2;

/** 当前压缩格式前缀: deflate + 预共享字典 (与 Android 端一致). */
const SHARE_PREFIX_V2: string = 'xproxy2:';
/** 旧版格式前缀 (gzip, 无字典), 解码时兼容. */
const SHARE_PREFIX_V1: string = 'xproxy1:';

/** 分流列表在二维码中的最大条数 (超出时只保留前 N 条). */
export const SHARE_PROXY_LIST_LIMIT: number = 10;

/**
 * 生成配置分享内容.
 *
 * 正文 JSON 经 deflate (预共享字典) 压缩后做 base64url, 与 Android 端
 * `encodeConfigShare` 输出同格式, 双向可解析.
 */
export function encodeConfigShare(config: VpnConfig): string {
  const json: VpnConfigJson = JSON.parse(config.toJson()) as VpnConfigJson;
  json.proxyDomains = shareList(config.proxyDomains, config.proxyDomainsUrl);
  json.proxyCidr = shareList(config.proxyCidr, config.proxyCidrUrl);
  const payload: SharePayload = {
    type: SHARE_TYPE,
    version: SHARE_VERSION,
    config: json
  };
  const text: string = JSON.stringify(payload);
  const packed: Uint8Array = zlibCompress(utf8Encode(text), configShareDictionary());
  return SHARE_PREFIX_V2 + base64UrlEncode(packed);
}

function shareList(items: string[], url: string): string[] {
  if (url.trim().length > 0) {
    return [];
  }
  return items.length <= SHARE_PROXY_LIST_LIMIT ? items : items.slice(0, SHARE_PROXY_LIST_LIMIT);
}

/**
 * 解析分享内容; 内容非法时抛出异常.
 *
 * 支持 `xproxy2:` (deflate + 预共享字典), `xproxy1:` (gzip) 与明文 JSON.
 */
export function decodeConfigShare(raw: string): VpnConfig {
  const text: string = raw.trim();
  let payload: SharePayload | undefined = undefined;
  if (text.startsWith(SHARE_PREFIX_V2)) {
    const packed: Uint8Array = base64UrlDecode(text.substring(SHARE_PREFIX_V2.length));
    payload = parsePayload(utf8Decode(zlibDecompress(packed, configShareDictionary())));
  } else if (text.startsWith(SHARE_PREFIX_V1)) {
    const packed: Uint8Array = base64UrlDecode(text.substring(SHARE_PREFIX_V1.length));
    payload = parsePayload(utf8Decode(gzipDecompress(packed)));
  } else if (text.startsWith('{')) {
    payload = parsePayload(text);
  } else {
    throw new Error('分享内容不是 xproxy 配置');
  }
  if (payload.type !== SHARE_TYPE || payload.config === undefined) {
    throw new Error('分享内容不是 xproxy 配置');
  }
  return VpnConfig.fromRecord(payload.config);
}

function parsePayload(text: string): SharePayload {
  try {
    return JSON.parse(text) as SharePayload;
  } catch (err) {
    throw new Error('分享内容无法解析');
  }
}

/** base64url 解码 (兼容缺失的填充). */
function base64UrlDecode(text: string): Uint8Array {
  let value: string = text;
  const padding: number = value.length % 4;
  if (padding === 2) {
    value += '==';
  } else if (padding === 3) {
    value += '=';
  }
  try {
    return new util.Base64Helper().decodeSync(value, util.Type.BASIC_URL_SAFE);
  } catch (err) {
    throw new Error('分享内容无法解析 (base64 解码失败)');
  }
}

/** base64url 编码; Android 端解码要求带填充, 这里显式补齐. */
function base64UrlEncode(data: Uint8Array): string {
  const text: string = new util.Base64Helper().encodeToStringSync(data, util.Type.BASIC_URL_SAFE);
  const padding: number = text.length % 4;
  if (padding === 2) {
    return text + '==';
  }
  if (padding === 3) {
    return text + '=';
  }
  return text;
}

function utf8Encode(text: string): Uint8Array {
  return new util.TextEncoder().encodeInto(text);
}

/** 分享载荷. */
interface SharePayload {
  type: string;
  version: number;
  config: VpnConfigJson;
}
