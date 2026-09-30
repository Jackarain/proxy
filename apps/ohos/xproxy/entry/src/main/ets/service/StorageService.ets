import { preferences } from '@kit.ArkData';
import { common } from '@kit.AbilityKit';
import { VpnConfig, VpnConfigJson } from '../model/VpnConfig';

const STORE_NAME: string = 'xproxy_store';

/** 配置持久化: 配置列表以 json 数组存于 preferences, 运行状态单独一条记录. */
export class StorageService {
  static readonly configsKey: string = 'xproxy_configs_v1';
  /** 解析失败时另存的原始内容 (避免下一次保存把损坏数据彻底覆盖). */
  static readonly corruptKey: string = 'xproxy_configs_v1_corrupt';
  /** 运行状态记录 (configId/运行目录/控制通道地址). */
  static readonly runStateKey: string = 'xproxy_run_state_v1';

  private store: preferences.Preferences;

  constructor(context: common.Context) {
    this.store = preferences.getPreferencesSync(context, { name: STORE_NAME });
  }

  loadConfigs(): VpnConfig[] {
    const raw: string = this.store.getSync(StorageService.configsKey, '') as string;
    if (raw.length === 0) {
      return [];
    }
    try {
      const list = JSON.parse(raw) as VpnConfigJson[];
      return list.map((item: VpnConfigJson) => VpnConfig.fromRecord(item));
    } catch (err) {
      if ((this.store.getSync(StorageService.corruptKey, '') as string).length === 0) {
        this.store.putSync(StorageService.corruptKey, raw);
        this.store.flush();
      }
      return [];
    }
  }

  /** 是否存在解析失败的配置备份 (供界面提示). */
  hasCorruptBackup(): boolean {
    return (this.store.getSync(StorageService.corruptKey, '') as string).length > 0;
  }

  saveConfigs(configs: VpnConfig[]): void {
    this.store.putSync(StorageService.configsKey, JSON.stringify(configs));
    this.store.flush();
  }

  saveRunState(configId: string, dir: string, port: number, token: string): void {
    const state: SavedRunState = { configId: configId, dir: dir, port: port, token: token };
    this.store.putSync(StorageService.runStateKey, JSON.stringify(state));
    this.store.flush();
  }

  /** 运行状态原始 JSON; 为空表示无运行记录. */
  loadRunStateRaw(): string {
    return this.store.getSync(StorageService.runStateKey, '') as string;
  }

  clearRunState(): void {
    this.store.deleteSync(StorageService.runStateKey);
    this.store.flush();
  }
}

/** 持久化的运行状态. */
export interface SavedRunState {
  configId: string;
  dir: string;
  port: number;
  token: string;
}
