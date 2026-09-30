import { VpnConfig, VpnConfigJson } from '../model/VpnConfig';

/** 二维码分享内容的类型标识与版本. */
const SHARE_TYPE: string = 'xproxy-config';
const SHARE_VERSION: number = 1;

/** 分流列表在二维码中的最大条数 (超出时只保留前 N 条). */
export const SHARE_PROXY_LIST_LIMIT: number = 10;

/**
 * 生成配置分享文本.
 *
 * 正文为明文 JSON (与本应用解码端一致); 分流列表按上限收敛, 来自 URL 的
 * 列表只带 URL, 扫码方按 URL 重新拉取.
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
  return JSON.stringify(payload);
}

function shareList(items: string[], url: string): string[] {
  if (url.trim().length > 0) {
    return [];
  }
  return items.length <= SHARE_PROXY_LIST_LIMIT ? items : items.slice(0, SHARE_PROXY_LIST_LIMIT);
}

/** 解析分享文本; 内容非法时抛出异常. */
export function decodeConfigShare(raw: string): VpnConfig {
  const text: string = raw.trim();
  if (!text.startsWith('{')) {
    throw new Error('分享内容不是 xproxy 配置 (不支持压缩格式)');
  }
  let payload: SharePayload;
  try {
    payload = JSON.parse(text) as SharePayload;
  } catch (err) {
    throw new Error('分享内容无法解析');
  }
  if (payload.type !== SHARE_TYPE || payload.config === undefined) {
    throw new Error('分享内容不是 xproxy 配置');
  }
  return VpnConfig.fromRecord(payload.config);
}

/** 分享载荷. */
interface SharePayload {
  type: string;
  version: number;
  config: VpnConfigJson;
}
