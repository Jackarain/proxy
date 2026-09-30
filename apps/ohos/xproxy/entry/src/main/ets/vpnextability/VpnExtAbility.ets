import { Want } from '@kit.AbilityKit';
import { VpnExtensionAbility } from '@kit.NetworkKit';
import { Logger } from '../common/Logger';
import { RuntimeStates, RuntimeStore, RuntimeState } from '../service/RuntimeStore';
import { VpnRuntime } from '../service/VpnRuntime';
import { VpnConfig } from '../model/VpnConfig';

const log = new Logger('vpnext');

/** VPN 扩展参数名. */
export const PARAM_RUN_DIR: string = 'runDir';
export const PARAM_PORT: string = 'port';
export const PARAM_TOKEN: string = 'token';

/**
 * VPN 扩展能力: 由界面经 startVpnExtensionAbility 拉起, 在本进程内建立 VPN
 * 隧道并持有 libxproxy. 启动参数经 Want 传入 (运行目录/端口/token), 运行
 * 状态写入运行目录由界面读取.
 */
export default class VpnExtAbility extends VpnExtensionAbility {
  private runtime: VpnRuntime | undefined = undefined;

  onCreate(want: Want): void {
    const params: Record<string, Object> = want.parameters ?? {};
    const dir: string = stringParam(params, PARAM_RUN_DIR, '');
    const token: string = stringParam(params, PARAM_TOKEN, '');
    const port: number = numberParam(params, PARAM_PORT, 0);
    log.info(`VpnExtAbility onCreate dir=${dir} port=${port}`);
    if (dir.length === 0 || token.length === 0) {
      log.error('启动参数缺失 (runDir/token)');
      return;
    }
    const config: VpnConfig | undefined = RuntimeStore.readConfig(dir);
    if (config === undefined) {
      RuntimeStore.writeState(dir, {
        state: RuntimeStates.ERROR,
        message: '启动配置读取失败',
        port: 0,
        token: token,
        configId: '',
        updatedAt: Date.now()
      });
      return;
    }
    const runtime = new VpnRuntime(this.context, dir, config);
    this.runtime = runtime;
    runtime.start(port, token).catch((err: Error) => {
      const state: RuntimeState = {
        state: RuntimeStates.ERROR,
        message: err.message,
        port: 0,
        token: token,
        configId: config.id,
        updatedAt: Date.now()
      };
      RuntimeStore.writeState(dir, state);
      log.error(`VPN 启动失败: ${err.message}`);
      runtime.stop();
    });
  }

  onDestroy(): void {
    log.info('VpnExtAbility onDestroy');
    const runtime = this.runtime;
    this.runtime = undefined;
    if (runtime !== undefined) {
      runtime.stop();
    }
  }
}

function stringParam(params: Record<string, Object>, key: string, fallback: string): string {
  const value = params[key];
  return typeof value === 'string' ? value : fallback;
}

function numberParam(params: Record<string, Object>, key: string, fallback: number): number {
  const value = params[key];
  return typeof value === 'number' ? value : fallback;
}
