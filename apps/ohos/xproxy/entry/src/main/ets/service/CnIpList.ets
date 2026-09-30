import { http } from '@kit.NetworkKit';
import { preferences } from '@kit.ArkData';
import { common } from '@kit.AbilityKit';

const CACHE_KEY: string = 'cn_ip_list_cache';
const CACHE_TIME_KEY: string = 'cn_ip_list_cache_time';
const UPDATE_INTERVAL: number = 24 * 60 * 60 * 1000;
const MAX_BYTES: number = 8 * 1024 * 1024;

/** VPN 路由上限 (系统 vpnExtension 限制). */
export const MAX_VPN_ROUTES: number = 1024;

/** IPv4 区间 [start, end] (均为无符号 32 位值). */
interface IpRange {
  start: number;
  end: number;
}

/**
 * 中国大陆 IP 段 (CIDR) 列表服务.
 *
 * "绕过中国大陆" 以 VPN 路由的补集实现: 计算"非中国且非保留段"作为接入
 * VPN 的路由, 中国大陆与私有网段不进 VPN 路由表, 由系统走物理网络直连.
 */
export class CnIpList {
  static readonly sourceUrl: string = 'https://ispip.clang.cn/all_cn.txt';

  /** 必须直连的保留/私有网段 (不进入 VPN). */
  static readonly privateRanges: string[] = [
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
    '255.255.255.255/32'
  ];

  private static cached: string[] = [];

  /** 获取当前 CN 段列表: 优先返回缓存, 每日自动拉取更新. */
  static async update(context: common.Context, force: boolean): Promise<string[]> {
    const store = preferences.getPreferencesSync(context, { name: 'xproxy_store' });
    if (CnIpList.cached.length === 0) {
      const raw: string = store.getSync(CACHE_KEY, '') as string;
      if (raw.length > 0) {
        CnIpList.cached = raw.split('\n').filter((line: string) => line.length > 0);
      }
    }
    const last: number = store.getSync(CACHE_TIME_KEY, 0) as number;
    const expired: boolean = Date.now() - last > UPDATE_INTERVAL;
    if (!force && CnIpList.cached.length > 0 && !expired) {
      return CnIpList.cached;
    }
    try {
      const fresh: string[] = await CnIpList.fetchFromSource();
      if (fresh.length > 0) {
        const sorted: string[] = fresh.sort();
        CnIpList.cached = sorted;
        store.putSync(CACHE_KEY, sorted.join('\n'));
        store.putSync(CACHE_TIME_KEY, Date.now());
        store.flush();
        return sorted;
      }
    } catch (err) {
      // 拉取失败时回退缓存.
    }
    return CnIpList.cached;
  }

  static async fetchFromSource(): Promise<string[]> {
    const client = http.createHttp();
    try {
      const resp = await client.request(CnIpList.sourceUrl, {
        method: http.RequestMethod.GET,
        header: { 'Accept-Encoding': 'identity' },
        expectDataType: http.HttpDataType.STRING,
        connectTimeout: 15000,
        readTimeout: 30000,
        maxLimit: MAX_BYTES
      });
      if (resp.responseCode !== 200) {
        throw new Error(`HTTP ${resp.responseCode}`);
      }
      const text: string = typeof resp.result === 'string' ? resp.result : '';
      const cidrs: string[] = [];
      for (const line of text.split(/[\r\n]+/)) {
        const value: string = line.trim();
        if (value.indexOf('/') >= 0) {
          cidrs.push(value);
        }
      }
      return cidrs;
    } finally {
      client.destroy();
    }
  }

  /**
   * 计算 VPN 路由: 非中国且非保留网段 (接入 VPN), 其余直连.
   * 结果超过 [limit] 条时按"合并最小间隙"聚合, 保证系统可接受.
   */
  static vpnRoutes(cnCidrs: string[], limit: number): string[] {
    const direct: IpRange[] = [];
    for (const cidr of cnCidrs) {
      const range: IpRange | undefined = cidrToRange(cidr);
      if (range !== undefined) {
        direct.push(range);
      }
    }
    for (const cidr of CnIpList.privateRanges) {
      const range: IpRange | undefined = cidrToRange(cidr);
      if (range !== undefined) {
        direct.push(range);
      }
    }
    const merged: IpRange[] = mergeRanges(direct);
    const complement: IpRange[] = complementRanges(merged);
    const bounded: IpRange[] = aggregateToLimit(complement, limit);
    return rangesToCidrs(bounded);
  }
}

/** "a.b.c.d/prefix" -> 区间; 非法返回 undefined. */
function cidrToRange(cidr: string): IpRange | undefined {
  const slash: number = cidr.indexOf('/');
  if (slash <= 0) {
    return undefined;
  }
  const ip: number = ipToInt(cidr.substring(0, slash).trim());
  const prefix: number = Number.parseInt(cidr.substring(slash + 1).trim(), 10);
  if (!Number.isFinite(ip) || !Number.isFinite(prefix) || prefix < 0 || prefix > 32) {
    return undefined;
  }
  const mask: number = prefix === 0 ? 0 : ((0xffffffff << (32 - prefix)) >>> 0);
  const start: number = (ip & mask) >>> 0;
  const end: number = (start | ((~mask) >>> 0)) >>> 0;
  return { start: start, end: end };
}

function ipToInt(ip: string): number {
  const parts: string[] = ip.split('.');
  if (parts.length !== 4) {
    return Number.NaN;
  }
  let value: number = 0;
  for (const part of parts) {
    if (!/^\d{1,3}$/.test(part)) {
      return Number.NaN;
    }
    const octet: number = Number.parseInt(part, 10);
    if (octet > 255) {
      return Number.NaN;
    }
    value = ((value << 8) | octet) >>> 0;
  }
  return value;
}

function intToIp(value: number): string {
  return `${(value >>> 24) & 0xff}.${(value >>> 16) & 0xff}.${(value >>> 8) & 0xff}.${value & 0xff}`;
}

/** 区间合并与排序 (相邻或重叠区间合并). */
function mergeRanges(ranges: IpRange[]): IpRange[] {
  if (ranges.length === 0) {
    return [];
  }
  const sorted: IpRange[] = ranges.slice().sort((a: IpRange, b: IpRange) => a.start - b.start);
  const out: IpRange[] = [];
  for (const range of sorted) {
    const last: IpRange | undefined = out.length > 0 ? out[out.length - 1] : undefined;
    if (last === undefined || range.start > last.end + 1) {
      out.push({ start: range.start, end: range.end });
    } else if (range.end > last.end) {
      last.end = range.end;
    }
  }
  return out;
}

/** 从整个 IPv4 空间减去 [direct], 返回其余区间. */
function complementRanges(direct: IpRange[]): IpRange[] {
  const merged: IpRange[] = mergeRanges(direct);
  const out: IpRange[] = [];
  let cursor: number = 0;
  for (const range of merged) {
    if (range.start > cursor) {
      out.push({ start: cursor, end: range.start - 1 });
    }
    if (range.end + 1 > cursor) {
      cursor = range.end + 1;
    }
    if (cursor > 0xffffffff) {
      break;
    }
  }
  if (cursor <= 0xffffffff) {
    out.push({ start: cursor, end: 0xffffffff });
  }
  return out;
}

/**
 * 区间数量超过上限时按最小间隙合并相邻区间: 二分选取间隙阈值, 使合并后的
 * 区间数不超过 [limit]. 合并会少量放大 VPN 覆盖范围, 属可接受的降级.
 */
function aggregateToLimit(ranges: IpRange[], limit: number): IpRange[] {
  if (ranges.length <= limit || limit <= 0) {
    return ranges;
  }
  const gaps: number[] = [];
  for (let i = 1; i < ranges.length; i++) {
    gaps.push(ranges[i].start - ranges[i - 1].end - 1);
  }
  const unique: number[] = Array.from(new Set<number>(gaps)).sort((a: number, b: number) => a - b);
  let low: number = 0;
  let high: number = unique.length - 1;
  let threshold: number = unique[high];
  while (low <= high) {
    const mid: number = (low + high) >> 1;
    const candidate: number = unique[mid];
    let count: number = 1;
    for (const gap of gaps) {
      if (gap > candidate) {
        count++;
      }
    }
    if (count <= limit) {
      threshold = candidate;
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }
  const out: IpRange[] = [{ start: ranges[0].start, end: ranges[0].end }];
  for (let i = 1; i < ranges.length; i++) {
    const gap: number = ranges[i].start - ranges[i - 1].end - 1;
    if (gap > threshold) {
      out.push({ start: ranges[i].start, end: ranges[i].end });
    } else {
      out[out.length - 1].end = ranges[i].end;
    }
  }
  return out;
}

/** 区间列表转为 CIDR 列表 (贪心最大对齐块). */
function rangesToCidrs(ranges: IpRange[]): string[] {
  const out: string[] = [];
  for (const range of ranges) {
    let start: number = range.start;
    while (start <= range.end) {
      const size: number = largestAlignedBlock(start, range.end);
      const prefix: number = 32 - Math.round(Math.log2(size));
      out.push(`${intToIp(start)}/${prefix}`);
      start = start + size;
    }
  }
  return out;
}

/** 覆盖 [start, end] 且以 start 对齐的最大 2 的幂块大小. */
function largestAlignedBlock(start: number, end: number): number {
  for (let size = 0x80000000; size > 0; size = size >>> 1) {
    if ((start & (size - 1)) === 0 && start + size - 1 <= end) {
      return size;
    }
  }
  return 1;
}
