import { newConfigId } from '../common/Util';

/** 一条 proxy 配置: 同时承载 proxy 原生配置字段与 VPN 专用字段. */
export class VpnConfig {
  id: string = '';
  name: string = '';

  // ---- proxy 原生配置 ----
  /** 上游代理地址, 如 https://user:pass@host:443. */
  proxyPass: string = '';
  /** 与 proxy_pass 建立 TLS 连接时使用的 SNI, 空表示用主机名. */
  sni: string = '';
  /** TUN 设备 MTU. */
  tunMtu: number = 1500;
  /** 代理域名列表 (后缀匹配). */
  proxyDomains: string[] = [];
  /** 代理 CIDR 列表. */
  proxyCidr: string[] = [];
  /** 全局代理: 忽略 proxyDomains/proxyCidr 分流规则. */
  globalProxy: boolean = false;
  /** 上次拉取代理域名列表的地址. */
  proxyDomainsUrl: string = '';
  /** 上次拉取代理 CIDR 列表的地址. */
  proxyCidrUrl: string = '';
  /** 关闭上游代理的证书校验 (自签证书场景). */
  disableCheckCert: boolean = true;
  /** UDP 流过期时间 (秒). */
  udpTimeout: number = 300;
  /** proxy_pass 预选连接池大小, 0 表示禁用. */
  proxyPassPoolSize: number = 20;

  // ---- VPN 隧道 ----
  tunAddress: string = '';
  tunPrefix: number = 0;
  /** 国内 DNS 列表 (仅 IP), 未命中 proxy_domains 的查询直连解析. */
  dns: string[] = ['223.6.6.6', '119.29.29.29'];
  /** 国外 DNS 列表 (仅 IP), 命中 proxy_domains 的查询经代理解析. */
  dnsForeign: string[] = [];
  /** 国外 DoH URL (可选). */
  dnsForeignDoh: string = '';
  /** DNS 查询结果缓存. */
  dnsCache: boolean = true;
  /** 禁用 IPv6 解析 (AAAA 查询直接返回空应答). */
  noIpv6: boolean = true;
  /** 测试连接的 URL, 用于运行页测量延迟. */
  testUrl: string = 'https://www.google.com';
  /** 绕过中国大陆: 仅非中国段接入 VPN. */
  bypassCn: boolean = false;

  static create(name: string): VpnConfig {
    const cfg = new VpnConfig();
    cfg.id = newConfigId();
    cfg.name = name;
    return cfg;
  }

  /** 本端 tun 地址与前缀, 未配置时默认 10.0.0.2/24. */
  deriveTun(): TunAddress {
    const address: string = this.tunAddress.trim();
    if (address.length > 0 && this.tunPrefix > 0) {
      return { address: address, prefix: this.tunPrefix };
    }
    return { address: '10.0.0.2', prefix: 24 };
  }

  /** 脱敏后的上游地址: 隐藏 userinfo 里的口令, 供界面展示. */
  maskedProxyPass(): string {
    const url: string = this.proxyPass.trim();
    const at: number = url.lastIndexOf('@');
    if (at <= 0) {
      return url;
    }
    const schemeEnd: number = url.indexOf('://');
    const userStart: number = schemeEnd >= 0 ? schemeEnd + 3 : 0;
    if (userStart >= at) {
      return url;
    }
    const userInfo: string = url.substring(userStart, at);
    const colon: number = userInfo.indexOf(':');
    if (colon < 0) {
      return url;
    }
    return `${url.substring(0, userStart)}${userInfo.substring(0, colon)}:***@${url.substring(at + 1)}`;
  }

  /** 国外 DNS/DoH 展示合并行 (DoH 追加在末尾). */
  joinForeignDns(): string[] {
    const list: string[] = this.dnsForeign.slice();
    if (this.dnsForeignDoh.length > 0) {
      list.push(this.dnsForeignDoh);
    }
    return list;
  }

  /** 校验配置, 返回错误描述列表; 为空表示可运行. */
  validate(): string[] {
    const errors: string[] = [];
    const pass: string = this.proxyPass.trim();
    if (pass.length === 0) {
      errors.push('请填写上游代理地址');
    } else if (!isValidProxyUrl(pass)) {
      errors.push('代理地址需以 http(s):// 或 socks(4/5/5s):// 开头');
    }
    if (this.tunMtu <= 0 || this.tunMtu > 65535) {
      errors.push('MTU 需在 1-65535 之间');
    }
    if (this.udpTimeout <= 0) {
      errors.push('UDP 超时需大于 0');
    }
    if (this.proxyPassPoolSize < 0) {
      errors.push('连接池大小不能为负数');
    }
    const sni: string = this.sni.trim();
    if (sni.length > 0 && /\s/.test(sni)) {
      errors.push('SNI 不能包含空白字符');
    }
    const badDomestic: string[] = this.dns.filter((d: string) => !isIpAddress(d.trim()));
    if (badDomestic.length > 0) {
      errors.push(`国内 DNS 仅支持 IP 地址: ${badDomestic.join(', ')}`);
    }
    const badForeign: string[] = this.dnsForeign.filter((d: string) => !isIpAddress(d.trim()));
    if (badForeign.length > 0) {
      errors.push(`国外 DNS 仅支持 IP 地址: ${badForeign.join(', ')}`);
    }
    const doh: string = this.dnsForeignDoh.trim();
    if (doh.length > 0 && !doh.startsWith('https://') && !doh.startsWith('http://')) {
      errors.push('国外 DoH 需以 http:// 或 https:// 开头');
    }
    const test: string = this.testUrl.trim();
    if (test.length === 0 || (!test.startsWith('https://') && !test.startsWith('http://'))) {
      errors.push('测试连接需以 http:// 或 https:// 开头');
    }
    return errors;
  }

  copy(): VpnConfig {
    return VpnConfig.fromJson(this.toJson());
  }

  /**
   * 传给 libxproxy 的启动配置 (键名与 xproxy 配置解析一致). 控制通道地址
   * 由 VPN 扩展在监听端口确定后补写 launcher_url.
   */
  toProxyJson(launcherUrl: string): string {
    const map: ProxyJson = {
      proxy_pass: this.proxyPass.trim(),
      tun: true,
      tun_mtu: this.tunMtu,
      tun_wait_fd: true,
      disable_check_cert: this.disableCheckCert,
      udp_timeout: this.udpTimeout,
      proxy_pass_pool_size: this.proxyPassPoolSize,
      dns_domestic: this.dns,
      dns_foreign: this.dnsForeign,
      dns_no_ipv6: this.noIpv6
    };
    if (this.sni.trim().length > 0) {
      map.ssl_sni = this.sni.trim();
    }
    if (!this.globalProxy && this.proxyDomains.length > 0) {
      map.proxy_domains = this.proxyDomains;
    }
    if (!this.globalProxy && this.proxyCidr.length > 0) {
      map.proxy_cidr = this.proxyCidr;
    }
    if (this.dnsForeignDoh.trim().length > 0) {
      map.dns_doh = this.dnsForeignDoh.trim();
    }
    if (this.dnsCache) {
      map.dns_cache_size = 4096;
      map.dns_cache_ttl = 300;
    }
    if (launcherUrl.length > 0) {
      map.launcher_url = launcherUrl;
    }
    return JSON.stringify(map);
  }

  /**
   * 运行期热更新参数 (经控制通道 set_config 下发). 仅包含 native 可热改的
   * 字段; TUN 与分流相关字段由运行页判断后整体重建 VPN.
   */
  toProxyOptions(): ProxyOptions {
    const options: ProxyOptions = {
      proxy_pass: this.proxyPass.trim(),
      proxy_domains: this.globalProxy ? [] : this.proxyDomains,
      proxy_cidr: this.globalProxy ? [] : this.proxyCidr,
      disable_check_cert: this.disableCheckCert,
      udp_timeout: this.udpTimeout,
      dns_cache_size: this.dnsCache ? 4096 : 0,
      dns_cache_ttl: this.dnsCache ? 300 : 0,
      dns_no_ipv6: this.noIpv6
    };
    return options;
  }

  toJson(): string {
    return JSON.stringify(this);
  }

  static fromJson(text: string): VpnConfig {
    const json = JSON.parse(text) as VpnConfigJson;
    return VpnConfig.fromRecord(json);
  }

  static fromRecord(json: VpnConfigJson): VpnConfig {
    const cfg = new VpnConfig();
    cfg.id = json.id !== undefined && json.id.length > 0 ? json.id : newConfigId();
    cfg.name = json.name !== undefined && json.name.length > 0 ? json.name : '未命名';
    cfg.proxyPass = json.proxyPass ?? '';
    cfg.sni = json.sni ?? '';
    cfg.tunMtu = json.tunMtu ?? 1500;
    cfg.proxyDomains = json.proxyDomains ?? [];
    cfg.proxyCidr = json.proxyCidr ?? [];
    cfg.globalProxy = json.globalProxy ?? false;
    cfg.proxyDomainsUrl = json.proxyDomainsUrl ?? '';
    cfg.proxyCidrUrl = json.proxyCidrUrl ?? '';
    cfg.disableCheckCert = json.disableCheckCert ?? true;
    cfg.udpTimeout = json.udpTimeout ?? 300;
    cfg.proxyPassPoolSize = json.proxyPassPoolSize ?? 20;
    cfg.tunAddress = json.tunAddress ?? '';
    cfg.tunPrefix = json.tunPrefix ?? 0;
    const dns: string[] = json.dns ?? [];
    cfg.dns = dns.length > 0 ? dns : ['223.6.6.6', '119.29.29.29'];
    cfg.dnsForeign = json.dnsForeign ?? [];
    cfg.dnsForeignDoh = json.dnsForeignDoh ?? '';
    cfg.dnsCache = json.dnsCache ?? true;
    cfg.noIpv6 = json.noIpv6 ?? true;
    cfg.testUrl = json.testUrl ?? 'https://www.google.com';
    cfg.bypassCn = json.bypassCn ?? false;
    return cfg;
  }
}

/** tun 地址与前缀. */
export interface TunAddress {
  address: string;
  prefix: number;
}

/** VpnConfig 的 JSON 形态 (字段均可缺省, 兼容历史数据). */
export interface VpnConfigJson {
  id?: string;
  name?: string;
  proxyPass?: string;
  sni?: string;
  tunMtu?: number;
  proxyDomains?: string[];
  proxyCidr?: string[];
  globalProxy?: boolean;
  proxyDomainsUrl?: string;
  proxyCidrUrl?: string;
  disableCheckCert?: boolean;
  udpTimeout?: number;
  proxyPassPoolSize?: number;
  tunAddress?: string;
  tunPrefix?: number;
  dns?: string[];
  dnsForeign?: string[];
  dnsForeignDoh?: string;
  dnsCache?: boolean;
  noIpv6?: boolean;
  testUrl?: string;
  bypassCn?: boolean;
}

/** libxproxy 启动配置. */
export interface ProxyJson {
  proxy_pass: string;
  tun: boolean;
  tun_mtu: number;
  tun_wait_fd: boolean;
  disable_check_cert: boolean;
  udp_timeout: number;
  proxy_pass_pool_size: number;
  dns_domestic: string[];
  dns_foreign: string[];
  dns_no_ipv6: boolean;
  ssl_sni?: string;
  proxy_domains?: string[];
  proxy_cidr?: string[];
  dns_doh?: string;
  dns_cache_size?: number;
  dns_cache_ttl?: number;
  launcher_url?: string;
}

/** 运行期热更新参数. */
export interface ProxyOptions {
  proxy_pass: string;
  proxy_domains: string[];
  proxy_cidr: string[];
  disable_check_cert: boolean;
  udp_timeout: number;
  dns_cache_size: number;
  dns_cache_ttl: number;
  dns_no_ipv6: boolean;
}

const PROXY_SCHEMES: string[] = ['http://', 'https://', 'socks4://', 'socks5://', 'socks5s://'];

function isValidProxyUrl(url: string): boolean {
  return PROXY_SCHEMES.some((scheme: string) => url.startsWith(scheme));
}

/** 是否为 IPv4/IPv6 字面量地址. */
export function isIpAddress(value: string): boolean {
  if (value.length === 0) {
    return false;
  }
  if (value.indexOf(':') >= 0) {
    return /^[0-9a-fA-F:]+$/.test(value);
  }
  const parts: string[] = value.split('.');
  if (parts.length !== 4) {
    return false;
  }
  return parts.every((p: string) => {
    if (!/^\d{1,3}$/.test(p)) {
      return false;
    }
    const n: number = Number.parseInt(p, 10);
    return n >= 0 && n <= 255;
  });
}

/** 拆分输入文本行: IP 归入列表, http(s):// 开头视为 DoH URL. */
export function splitForeignDns(lines: string[]): ForeignDns {
  const ips: string[] = [];
  let doh: string = '';
  for (const line of lines) {
    const s: string = line.trim();
    if (s.length === 0) {
      continue;
    }
    if (s.startsWith('http://') || s.startsWith('https://')) {
      doh = s;
    } else {
      ips.push(s);
    }
  }
  return { ips: ips, doh: doh };
}

/** 国外 DNS 拆分结果. */
export interface ForeignDns {
  ips: string[];
  doh: string;
}
