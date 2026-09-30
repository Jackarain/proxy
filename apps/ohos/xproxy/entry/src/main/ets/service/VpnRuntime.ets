import { connection, vpnExtension } from '@kit.NetworkKit';
import { common } from '@kit.AbilityKit';
import { fileIo as fs } from '@kit.CoreFileKit';
import { start as proxyStart, stop as proxyStop } from 'libxproxy.so';
import { Logger } from '../common/Logger';
import { formatTime, randomToken } from '../common/Util';
import { VpnConfig, ProxyOptions } from '../model/VpnConfig';
import { CnIpList, MAX_VPN_ROUTES } from './CnIpList';
import { LauncherEvents, LauncherServer, ClientRole } from './LauncherServer';
import { RuntimeStates, RuntimeStore, RuntimeState } from './RuntimeStore';

const log = new Logger('vpn');

/** 等待 proxy 控制通道连接的超时 (毫秒). */
const REGISTER_TIMEOUT: number = 15000;
/** RPC 默认超时 (毫秒). */
const RPC_TIMEOUT: number = 10000;

interface PendingRpc {
  resolve: (value: Record<string, Object>) => void;
  reject: (err: Error) => void;
  timer: number;
}

interface JsonRpcMessage {
  jsonrpc?: string;
  id?: number;
  method?: string;
  params?: Record<string, Object>;
  result?: Record<string, Object>;
  error?: Record<string, Object>;
}

/**
 * VPN 扩展进程内的运行体: 持有 native proxy、tun 连接与控制通道服务.
 *
 * 启动顺序与 Android 侧一致: 先启动 libproxy (tun_wait_fd 模式, 无 tun),
 * 控制通道连接建立后按用户配置建立 VPN tun 并注入 native, 此后 native 的
 * protect 请求经本对象放行对外 socket.
 */
export class VpnRuntime implements LauncherEvents {
  private context: common.VpnExtensionContext;
  private dir: string;
  private config: VpnConfig;
  private connection: vpnExtension.VpnConnection | undefined = undefined;
  private server: LauncherServer;
  private tunFd: number = -1;
  private tunInjected: boolean = false;
  private nextRpcId: number = 1;
  private pending: Map<number, PendingRpc> = new Map<number, PendingRpc>();
  private lastStatus: string = '';
  private started: boolean = false;
  private stopWaiters: (() => void)[] = [];

  constructor(context: common.VpnExtensionContext, dir: string, config: VpnConfig) {
    this.context = context;
    this.dir = dir;
    this.config = config;
    this.server = new LauncherServer(this);
  }

  /** 启动控制通道与 native proxy. */
  async start(preferredPort: number, token: string): Promise<void> {
    RuntimeStore.ensureDir(this.dir);
    RuntimeStore.resetLog(this.dir);
    const port: number = await this.server.start(preferredPort, token);
    this.publishState(RuntimeStates.STARTING, '', port, token);

    const url: string = `ws://127.0.0.1:${port}/${token}`;
    const rc: number = proxyStart(this.config.toProxyJson(url));
    if (rc !== 0) {
      this.publishState(RuntimeStates.ERROR, `xproxy.start 失败: rc=${rc}`, port, token);
      throw new Error(`xproxy.start 失败: rc=${rc}`);
    }
    this.started = true;
    log.info(`native proxy started, launcher ${url}`);
    this.waitProxyConnected(port, token);
  }

  /** 等待 proxy 控制通道连接; 超时视为启动失败. */
  private waitProxyConnected(port: number, token: string): void {
    let waited: number = 0;
    const timer: number = setInterval(() => {
      if (!this.started) {
        clearInterval(timer);
        return;
      }
      if (this.server.isProxyConnected()) {
        clearInterval(timer);
        return;
      }
      waited += 500;
      if (waited >= REGISTER_TIMEOUT) {
        clearInterval(timer);
        this.publishState(RuntimeStates.ERROR, 'proxy 控制通道连接超时', port, token);
        this.stop();
      }
    }, 500);
  }

  /** 停止 proxy 并释放 tun 与控制通道. */
  async stop(): Promise<void> {
    if (!this.started && this.connection === undefined) {
      return;
    }
    this.started = false;
    this.tunInjected = false;
    try {
      proxyStop();
    } catch (err) {
      log.warn(`xproxy.stop 异常: ${err}`);
    }
    const connection = this.connection;
    this.connection = undefined;
    if (connection !== undefined) {
      try {
        await connection.destroy();
      } catch (err) {
        log.warn(`destroy vpn 异常: ${err}`);
      }
    }
    this.tunFd = -1;
    for (const pending of this.pending.values()) {
      clearTimeout(pending.timer);
      pending.reject(new Error('vpn stopped'));
    }
    this.pending.clear();
    await this.server.stop();
    this.publishState(RuntimeStates.STOPPED, '', this.server.getPort(), this.server.getToken());
    for (const waiter of this.stopWaiters) {
      waiter();
    }
    this.stopWaiters = [];
    log.info('vpn runtime stopped');
  }

  onStopped(callback: () => void): void {
    this.stopWaiters.push(callback);
  }

  /* --------------------------- LauncherEvents --------------------------- */

  onProxyConnected(): void {
    this.establishTun();
  }

  onProxyClosed(): void {
    if (this.started) {
      log.warn('proxy 控制通道断开');
    }
  }

  onProxyMessage(text: string): void {
    let msg: JsonRpcMessage;
    try {
      msg = JSON.parse(text) as JsonRpcMessage;
    } catch (err) {
      return;
    }
    if (msg.method !== undefined) {
      const params: Record<string, Object> = msg.params ?? {};
      if (msg.id !== undefined) {
        this.handleProxyRequest(msg.method, params, msg.id);
        return;
      }
      this.handleProxyNotify(msg.method, params);
      return;
    }
    if (msg.id !== undefined) {
      const pending = this.pending.get(msg.id);
      if (pending === undefined) {
        return;
      }
      this.pending.delete(msg.id);
      clearTimeout(pending.timer);
      if (msg.error !== undefined) {
        pending.reject(new Error(`rpc error: ${JSON.stringify(msg.error)}`));
      } else {
        pending.resolve(msg.result ?? {});
      }
    }
  }

  onUiMessage(text: string): void {
    let msg: JsonRpcMessage;
    try {
      msg = JSON.parse(text) as JsonRpcMessage;
    } catch (err) {
      return;
    }
    if (msg.method === undefined || msg.id === undefined) {
      return;
    }
    if (msg.method === 'ui_get_status') {
      const result: Record<string, Object> = {
        'ok': true,
        'status': this.lastStatus,
        'running': this.started
      };
      this.server.broadcast(this.reply(msg.id, result));
    } else if (msg.method === 'ui_set_config') {
      this.applyUiConfig(msg.id, msg.params ?? {});
    } else {
      this.server.broadcast(this.replyError(msg.id, -32601, `method not found: ${msg.method}`));
    }
  }

  /* ------------------------------ 内部逻辑 ------------------------------ */

  /** 控制通道连接建立后: 建 tun 并注入 native. */
  private async establishTun(): Promise<void> {
    if (this.tunInjected) {
      return;
    }
    const tun = this.config.deriveTun();
    let routes: string[] = [];
    if (this.config.bypassCn) {
      try {
        const cn = await CnIpList.update(this.context, false);
        routes = CnIpList.vpnRoutes(cn, MAX_VPN_ROUTES);
      } catch (err) {
        log.warn(`中国 IP 段获取失败: ${err}`);
      }
    }
    if (routes.length === 0) {
      routes = ['0.0.0.0/0', '::/0'];
    }
    const dns: string[] = ['8.8.8.8', '1.1.1.1'];
    try {
      const fd: number = await this.createTun(tun.address, tun.prefix, this.config.tunMtu, routes, dns);
      this.tunFd = fd;
      const result: Record<string, Object> = await this.call('set_tun_fd', { 'fd': fd });
      if (result['ok'] === true) {
        this.tunInjected = true;
        this.publishState(RuntimeStates.RUNNING, '', this.server.getPort(), this.server.getToken());
        log.info(`tun fd=${fd} injected`);
      } else {
        this.appendLocalLog(`set_tun_fd 失败: ${JSON.stringify(result)}`);
        this.closeTunFd(fd);
      }
    } catch (err) {
      this.appendLocalLog(`建立 TUN 失败: ${err}`);
      this.publishState(RuntimeStates.ERROR, `建立 TUN 失败: ${err}`, this.server.getPort(),
        this.server.getToken());
    }
  }

  /**
   * 建立 VPN 连接并返回 tun fd. 先按完整路由 (含 IPv6 默认路由) 创建; 设备
   * 不接受 IPv6 路由时退回仅 IPv4 路由, 避免整体建立失败.
   */
  private async createTun(address: string, prefix: number, mtu: number, routes: string[],
    dns: string[]): Promise<number> {
    const connection: vpnExtension.VpnConnection = vpnExtension.createVpnConnection(this.context);
    this.connection = connection;
    try {
      return await connection.create(buildVpnConfig(address, prefix, mtu, routes, dns, true));
    } catch (err) {
      log.warn(`含 IPv6 路由创建 VPN 失败, 退回仅 IPv4: ${err}`);
      return await connection.create(buildVpnConfig(address, prefix, mtu, routes, dns, false));
    }
  }

  /** 放行 native 对外 socket, 避免流量回环进 tun. */
  private async protectSocket(fd: number): Promise<boolean> {
    const connection = this.connection;
    if (connection === undefined || fd < 0) {
      return false;
    }
    try {
      await connection.protect(fd);
      return true;
    } catch (err) {
      log.warn(`protect fd=${fd} 失败: ${err}`);
      return false;
    }
  }

  private closeTunFd(fd: number): void {
    if (fd >= 0) {
      try {
        fsClose(fd);
      } catch (err) {
        log.warn(`close tun fd=${fd} 失败: ${err}`);
      }
    }
  }

  /** 处理 proxy 发来的请求 (带 id), 异步处理并回复. */
  private handleProxyRequest(method: string, params: Record<string, Object>, id: number): void {
    if (method === 'protect') {
      const fd: number = numberParam(params, 'fd', -1);
      this.protectSocket(fd).then((ok: boolean) => {
        this.server.sendToProxy(this.reply(id, { 'ok': ok }));
      }).catch((err: Error) => {
        this.server.sendToProxy(this.replyError(id, -32602, err.message));
      });
      return;
    }
    this.server.sendToProxy(this.replyError(id, -32601, `method not found: ${method}`));
  }

  /** 处理 proxy 发来的通知 (无 id). */
  private handleProxyNotify(method: string, params: Record<string, Object>): void {
    if (method === 'log') {
      this.forwardLog(params);
    } else if (method === 'status') {
      this.lastStatus = JSON.stringify(params);
      this.server.broadcast(notification('status', params));
    } else if (method === 'register') {
      this.server.broadcast(notification('register', params));
    }
  }

  private forwardLog(params: Record<string, Object>): void {
    const lines: string[] = [];
    const raw = params['lines'];
    if (Array.isArray(raw)) {
      for (const item of raw as Record<string, Object>[]) {
        const message = item['message'];
        const time = item['time'];
        const ms: number = typeof time === 'number' ? time : Date.now();
        if (typeof message === 'string') {
          lines.push(`[${formatTime(ms)}] ${message}`);
        }
      }
    }
    for (const line of lines) {
      RuntimeStore.appendLog(this.dir, line);
    }
    if (lines.length > 0) {
      this.server.broadcast(notification('log', { 'lines': lines }));
    }
  }

  private appendLocalLog(message: string): void {
    const line: string = `[${formatTime(Date.now())}] ${message}`;
    RuntimeStore.appendLog(this.dir, line);
    this.server.broadcast(notification('log', { 'lines': [line] }));
    log.info(message);
  }

  /** 运行期热更新 (界面下发). native 不支持热改的字段由界面侧走重启. */
  private applyUiConfig(id: number, params: Record<string, Object>): void {
    const options = params['options'] as ProxyOptions;
    if (options === undefined) {
      this.server.broadcast(this.replyError(id, -32602, 'missing options'));
      return;
    }
    this.call('set_config', { 'options': options }).then((result: Record<string, Object>) => {
      this.server.broadcast(this.reply(id, result));
    }).catch((err: Error) => {
      this.server.broadcast(this.replyError(id, -32602, err.message));
    });
  }

  /** 向 proxy 发起一次 JSON-RPC 请求并等待响应. */
  call(method: string, params: Record<string, Object>,
    timeout: number = RPC_TIMEOUT): Promise<Record<string, Object>> {
    return new Promise<Record<string, Object>>((resolve, reject) => {
      const id: number = this.nextRpcId++;
      const timer: number = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`rpc ${method} 超时`));
      }, timeout);
      this.pending.set(id, { resolve: resolve, reject: reject, timer: timer });
      const payload: Record<string, Object> = {
        'jsonrpc': '2.0',
        'id': id,
        'method': method,
        'params': params
      };
      if (!this.server.sendToProxy(JSON.stringify(payload))) {
        clearTimeout(timer);
        this.pending.delete(id);
        reject(new Error('控制通道未连接'));
      }
    });
  }

  private reply(id: number, result: Record<string, Object>): string {
    const payload: RpcReply = { jsonrpc: '2.0', id: id, result: result };
    return JSON.stringify(payload);
  }

  private replyError(id: number, code: number, message: string): string {
    const error: RpcError = { code: code, message: message };
    const payload: RpcReply = { jsonrpc: '2.0', id: id, error: error };
    return JSON.stringify(payload);
  }

  private publishState(state: string, message: string, port: number, token: string): void {
    const value: RuntimeState = {
      state: state,
      message: message,
      port: port,
      token: token,
      configId: this.config.id,
      updatedAt: Date.now()
    };
    RuntimeStore.writeState(this.dir, value);
  }
}

function notification(method: string, params: Record<string, Object>): string {
  const payload: RpcNotify = { jsonrpc: '2.0', method: method, params: params };
  return JSON.stringify(payload);
}

/** JSON-RPC 错误体. */
interface RpcError {
  code: number;
  message: string;
}

/** JSON-RPC 响应. */
interface RpcReply {
  jsonrpc: string;
  id: number;
  result?: Record<string, Object>;
  error?: RpcError;
}

/** JSON-RPC 通知. */
interface RpcNotify {
  jsonrpc: string;
  method: string;
  params: Record<string, Object>;
}

function numberParam(params: Record<string, Object>, key: string, fallback: number): number {
  const value = params[key];
  return typeof value === 'number' ? value : fallback;
}

/** 构造 vpnExtension.VpnConfig. [withIpv6] 为 false 时丢弃 IPv6 路由. */
function buildVpnConfig(address: string, prefix: number, mtu: number, routes: string[],
  dns: string[], withIpv6: boolean): vpnExtension.VpnConfig {
  const routeInfos: vpnExtension.RouteInfo[] = [];
  for (const cidr of routes) {
    const route: vpnExtension.RouteInfo | undefined = parseRoute(cidr, address);
    if (route === undefined) {
      continue;
    }
    if (!withIpv6 && isIpv6Route(cidr)) {
      continue;
    }
    routeInfos.push(route);
  }
  const ipv6: boolean = address.indexOf(':') >= 0;
  const config: vpnExtension.VpnConfig = {
    addresses: [
      {
        address: { address: address, family: ipv6 ? 2 : 1 },
        prefixLength: prefix
      }
    ],
    routes: routeInfos,
    dnsAddresses: dns,
    mtu: mtu,
    isIPv4Accepted: true,
    isIPv6Accepted: withIpv6,
    isInternal: false,
    isBlocking: true
  };
  return config;
}

function isIpv6Route(cidr: string): boolean {
  const slash: number = cidr.indexOf('/');
  const host: string = slash > 0 ? cidr.substring(0, slash) : cidr;
  return host.indexOf(':') >= 0;
}

/** 解析 "a.b.c.d/prefix" 或 "::/0" 为 VPN 路由条目. */
function parseRoute(cidr: string, tunAddress: string): vpnExtension.RouteInfo | undefined {
  const slash: number = cidr.indexOf('/');
  if (slash <= 0) {
    return undefined;
  }
  const host: string = cidr.substring(0, slash).trim();
  const prefix: number = Number.parseInt(cidr.substring(slash + 1).trim(), 10);
  if (host.length === 0 || !Number.isFinite(prefix) || prefix < 0) {
    return undefined;
  }
  const family: number = host.indexOf(':') >= 0 ? 2 : 1;
  const gatewayAddress: string = family === 2
    ? (tunAddress.indexOf(':') >= 0 ? tunAddress : '::')
    : (tunAddress.indexOf(':') >= 0 ? '0.0.0.0' : tunAddress);
  const gateway: connection.NetAddress = { address: gatewayAddress, family: family };
  const route: vpnExtension.RouteInfo = {
    interface: 'vpn-tun',
    destination: {
      address: { address: host, family: family } as connection.NetAddress,
      prefixLength: prefix
    },
    gateway: gateway,
    hasGateway: false,
    isDefaultRoute: prefix === 0
  };
  return route;
}

/** 关闭未成功注入 native 的 tun fd. */
function fsClose(fd: number): void {
  fs.closeSync(fd);
}
