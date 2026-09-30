import { common, Want } from '@kit.AbilityKit';
import { util } from '@kit.ArkTS';
import { vpnExtension, webSocket } from '@kit.NetworkKit';
import { Logger } from '../common/Logger';
import { randomToken } from '../common/Util';
import { ProxyOptions, VpnConfig } from '../model/VpnConfig';
import { RuntimeState, RuntimeStates, RuntimeStore } from './RuntimeStore';
import { SavedRunState, StorageService } from './StorageService';

const log = new Logger('session');

const VPN_BUNDLE: string = 'com.jackarain.xproxy';
const VPN_ABILITY: string = 'VpnExtAbility';
const START_TIMEOUT: number = 20000;
const POLL_INTERVAL: number = 300;

interface PendingRpc {
  resolve: (value: Record<string, Object>) => void;
  reject: (err: Error) => void;
}

/**
 * 界面侧运行会话 (单例).
 *
 * 界面进程与 VPN 扩展进程之间不共享内存: 启动参数经 Want 传递, 运行状态经
 * 运行目录下的 state.json 读取, 状态/日志经控制通道 WebSocket (`/<token>/ui`)
 * 实时接收.
 */
export class AppSession {
  static readonly instance: AppSession = new AppSession();

  /** 当前运行的配置 id, 空表示未运行. */
  runningConfigId: string = '';
  /** 运行目录 (跨启动恢复控制通道用). */
  runDir: string = '';
  /** 控制通道地址. */
  port: number = 0;
  token: string = '';
  /** proxy 控制通道是否在线. */
  connected: boolean = false;
  /** 最近一次 status 通知的原始 JSON. */
  statusJson: string = '';
  /** native 日志行 (最多保留 500 行). */
  logs: string[] = [];
  /** 启动/运行期错误信息. */
  lastError: string = '';

  private socket: webSocket.WebSocket | undefined = undefined;
  private nextRpcId: number = 1;
  private pending: Map<number, PendingRpc> = new Map<number, PendingRpc>();
  private listeners: (() => void)[] = [];
  private reconnectTimer: number = -1;
  private stopping: boolean = false;

  subscribe(listener: () => void): void {
    this.listeners.push(listener);
  }

  unsubscribe(listener: () => void): void {
    const index: number = this.listeners.indexOf(listener);
    if (index >= 0) {
      this.listeners.splice(index, 1);
    }
  }

  private notify(): void {
    for (const listener of this.listeners.slice()) {
      listener();
    }
  }

  isRunning(): boolean {
    return this.runningConfigId.length > 0;
  }

  /* ------------------------------- 启动 ------------------------------- */

  async start(context: common.UIAbilityContext, config: VpnConfig): Promise<void> {
    this.stopSocket();
    this.lastError = '';
    const runId: string = randomToken().substring(0, 16);
    const dir: string = RuntimeStore.runDir(context.filesDir, runId);
    RuntimeStore.prune(context.filesDir, 4);
    RuntimeStore.writeConfig(dir, config);
    RuntimeStore.writeState(dir, {
      state: RuntimeStates.STARTING,
      message: '',
      port: 0,
      token: '',
      configId: config.id,
      updatedAt: Date.now()
    });
    const token: string = randomToken();
    const params: Record<string, Object> = {
      'runDir': dir,
      'port': 0,
      'token': token
    };
    const want: Want = {
      bundleName: VPN_BUNDLE,
      abilityName: VPN_ABILITY,
      parameters: params
    };
    await vpnExtension.startVpnExtensionAbility(want);
    const state: RuntimeState = await this.waitState(dir, START_TIMEOUT);
    if (state.port <= 0 || state.state === RuntimeStates.ERROR) {
      this.lastError = state.message.length > 0 ? state.message : 'VPN 启动失败';
      this.runningConfigId = '';
      this.notify();
      throw new Error(this.lastError);
    }
    this.runningConfigId = config.id;
    this.runDir = dir;
    this.port = state.port;
    this.token = state.token.length > 0 ? state.token : token;
    this.logs = RuntimeStore.readLog(dir, 200);
    new StorageService(context).saveRunState(config.id, dir, state.port, this.token);
    this.notify();
    this.connectSocket();
  }

  private async waitState(dir: string, timeout: number): Promise<RuntimeState> {
    const deadline: number = Date.now() + timeout;
    let latest: RuntimeState = {
      state: RuntimeStates.STARTING,
      message: '',
      port: 0,
      token: '',
      configId: '',
      updatedAt: 0
    };
    while (Date.now() < deadline) {
      const state: RuntimeState | undefined = RuntimeStore.readState(dir);
      if (state !== undefined) {
        latest = state;
        if (state.state === RuntimeStates.ERROR) {
          return state;
        }
        if (state.port > 0 && (state.state === RuntimeStates.RUNNING ||
          state.state === RuntimeStates.STARTING)) {
          return state;
        }
        if (state.state === RuntimeStates.STOPPED) {
          return state;
        }
      }
      await sleep(POLL_INTERVAL);
    }
    return latest;
  }

  /* ------------------------------- 停止 ------------------------------- */

  async stop(context: common.UIAbilityContext): Promise<void> {
    if (!this.isRunning()) {
      return;
    }
    this.stopping = true;
    this.stopSocket();
    try {
      await vpnExtension.stopVpnExtensionAbility({
        bundleName: VPN_BUNDLE,
        abilityName: VPN_ABILITY
      });
    } catch (err) {
      log.warn(`stopVpnExtensionAbility 异常: ${err}`);
    }
    const dir: string = this.runDir;
    await this.waitStopped(dir);
    new StorageService(context).clearRunState();
    this.runningConfigId = '';
    this.connected = false;
    this.statusJson = '';
    this.stopping = false;
    this.notify();
  }

  private async waitStopped(dir: string): Promise<void> {
    const deadline: number = Date.now() + 8000;
    while (Date.now() < deadline) {
      const state: RuntimeState | undefined = RuntimeStore.readState(dir);
      if (state !== undefined && state.state === RuntimeStates.STOPPED) {
        return;
      }
      await sleep(POLL_INTERVAL);
    }
  }

  /** 配置变更: native 无法热改的字段整体重启 VPN, 其余经控制通道热更新. */
  async applyConfig(context: common.UIAbilityContext, config: VpnConfig): Promise<string> {
    if (!this.isRunning() || this.runningConfigId !== config.id) {
      return '';
    }
    const started: VpnConfig | undefined = RuntimeStore.readConfig(this.runDir);
    if (started === undefined || needsRestart(started, config)) {
      await this.stop(context);
      await this.start(context, config);
      return 'restarted';
    }
    const result: Record<string, Object> = await this.call('ui_set_config', {
      'options': config.toProxyOptions() as ProxyOptions
    });
    const errors = result['errors'];
    if (errors !== undefined && Array.isArray(errors) && (errors as Object[]).length > 0) {
      throw new Error(`set_config 失败: ${JSON.stringify(errors)}`);
    }
    return 'updated';
  }

  /* --------------------------- 控制通道 (UI 侧) --------------------------- */

  private connectSocket(): void {
    if (this.port <= 0 || this.token.length === 0) {
      return;
    }
    const socket: webSocket.WebSocket = webSocket.createWebSocket();
    this.socket = socket;
    socket.on('open', (err: Error, value: Object) => {
      if (err !== undefined && err !== null) {
        this.scheduleReconnect();
        return;
      }
      this.connected = true;
      this.notify();
      const hello: UiRequest = { jsonrpc: '2.0', id: 0, method: 'ui_get_status', params: {} };
      socket.send(JSON.stringify(hello)).catch((err: Error) => {
        log.warn(`发送状态查询失败: ${err.message}`);
      });
    });
    socket.on('message', (err: Error, value: string | ArrayBuffer) => {
      if (typeof value === 'string') {
        this.handleMessage(value);
      } else {
        this.handleMessage(new util.TextDecoder().decodeToString(new Uint8Array(value)));
      }
    });
    socket.on('close', (err: Error, value: Object) => {
      this.connected = false;
      this.notify();
      if (!this.stopping && this.isRunning()) {
        this.scheduleReconnect();
      }
    });
    socket.on('error', (err: Error) => {
      this.connected = false;
      this.notify();
      if (!this.stopping && this.isRunning()) {
        this.scheduleReconnect();
      }
    });
    socket.connect(`ws://127.0.0.1:${this.port}/${this.token}/ui`)
      .catch((err: Error) => {
        log.warn(`控制通道连接失败: ${err.message}`);
        this.scheduleReconnect();
      });
  }

  private scheduleReconnect(): void {
    if (this.reconnectTimer !== -1) {
      return;
    }
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = -1;
      this.stopSocket();
      this.connectSocket();
    }, 1000);
  }

  private stopSocket(): void {
    if (this.reconnectTimer !== -1) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = -1;
    }
    const socket = this.socket;
    this.socket = undefined;
    this.connected = false;
    if (socket !== undefined) {
      socket.close().catch(() => {
      });
    }
    for (const pending of this.pending.values()) {
      pending.reject(new Error('控制通道已关闭'));
    }
    this.pending.clear();
  }

  private handleMessage(text: string): void {
    let msg: UiMessage;
    try {
      msg = JSON.parse(text) as UiMessage;
    } catch (err) {
      return;
    }
    if (msg.method === 'log') {
      const params = msg.params as LogParams | undefined;
      if (params !== undefined && params.lines !== undefined) {
        for (const line of params.lines) {
          this.logs.push(line);
        }
        if (this.logs.length > 500) {
          this.logs = this.logs.slice(this.logs.length - 500);
        }
        this.notify();
      }
      return;
    }
    if (msg.method === 'status') {
      this.statusJson = JSON.stringify(msg.params ?? {});
      this.notify();
      return;
    }
    if (msg.id !== undefined) {
      const pending = this.pending.get(msg.id);
      if (pending !== undefined) {
        this.pending.delete(msg.id);
        if (msg.error !== undefined) {
          pending.reject(new Error(JSON.stringify(msg.error)));
        } else {
          pending.resolve(msg.result ?? {});
        }
      }
    }
  }

  private call(method: string, params: Record<string, Object>): Promise<Record<string, Object>> {
    return new Promise<Record<string, Object>>((resolve, reject) => {
      const socket = this.socket;
      if (socket === undefined || !this.connected) {
        reject(new Error('控制通道未连接'));
        return;
      }
      const id: number = this.nextRpcId++;
      this.pending.set(id, { resolve: resolve, reject: reject });
      const request: UiRequest = { jsonrpc: '2.0', id: id, method: method, params: params };
      socket.send(JSON.stringify(request))
        .catch((err: Error) => {
          this.pending.delete(id);
          reject(err);
        });
    });
  }

  /* ------------------------------- 恢复 ------------------------------- */

  /** 进程重启后尝试恢复仍在运行的 VPN 控制通道. */
  async restore(context: common.UIAbilityContext): Promise<void> {
    const store = new StorageService(context);
    const raw: string = store.loadRunStateRaw();
    if (raw.length === 0) {
      return;
    }
    const saved: SavedRunState = JSON.parse(raw) as SavedRunState;
    if (saved.dir.length === 0 || saved.port <= 0) {
      store.clearRunState();
      return;
    }
    const state: RuntimeState | undefined = RuntimeStore.readState(saved.dir);
    if (state === undefined || state.state !== RuntimeStates.RUNNING) {
      store.clearRunState();
      return;
    }
    this.runningConfigId = state.configId;
    this.runDir = saved.dir;
    this.port = state.port;
    this.token = state.token;
    this.logs = RuntimeStore.readLog(saved.dir, 200);
    this.notify();
    this.connectSocket();
  }
}

/** 需要整体重建 VPN 的字段是否变化. */
function needsRestart(started: VpnConfig, config: VpnConfig): boolean {
  return started.tunAddress !== config.tunAddress ||
    started.tunPrefix !== config.tunPrefix ||
    started.sni !== config.sni ||
    started.tunMtu !== config.tunMtu ||
    started.bypassCn !== config.bypassCn ||
    started.globalProxy !== config.globalProxy ||
    started.proxyPassPoolSize !== config.proxyPassPoolSize ||
    started.dns.join(',') !== config.dns.join(',') ||
    started.dnsForeign.join(',') !== config.dnsForeign.join(',') ||
    started.dnsForeignDoh !== config.dnsForeignDoh ||
    started.proxyDomains.join(',') !== config.proxyDomains.join(',') ||
    started.proxyCidr.join(',') !== config.proxyCidr.join(',');
}

function sleep(ms: number): Promise<void> {
  return new Promise<void>((resolve) => {
    setTimeout(() => {
      resolve();
    }, ms);
  });
}

/** 界面 -> VPN 扩展的控制通道请求. */
interface UiRequest {
  jsonrpc: string;
  id: number;
  method: string;
  params: Record<string, Object>;
}

interface UiMessage {
  jsonrpc?: string;
  id?: number;
  method?: string;
  params?: Object;
  result?: Record<string, Object>;
  error?: Object;
}

interface LogParams {
  lines?: string[];
}

