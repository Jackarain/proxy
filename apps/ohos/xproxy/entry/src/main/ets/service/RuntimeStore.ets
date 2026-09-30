import { fileIo as fs } from '@kit.CoreFileKit';
import { VpnConfig, VpnConfigJson } from '../model/VpnConfig';

/** VPN 运行期状态, 由 VPN 扩展进程写, 界面进程读. */
export interface RuntimeState {
  state: string;
  message: string;
  port: number;
  token: string;
  configId: string;
  updatedAt: number;
}

/** 运行期状态取值. */
export class RuntimeStates {
  static readonly STARTING: string = 'starting';
  static readonly RUNNING: string = 'running';
  static readonly ERROR: string = 'error';
  static readonly STOPPED: string = 'stopped';
}

/**
 * 运行期文件目录 (界面进程与 VPN 扩展进程共享):
 * - `config.json` 启动配置快照
 * - `state.json`  运行状态与控制通道地址
 * - `xproxy.log`  native 日志行
 */
export class RuntimeStore {
  /** 生成一次运行使用的目录 (调用方保证 runId 唯一). */
  static runDir(baseDir: string, runId: string): string {
    return `${baseDir}/run/${runId}`;
  }

  static configPath(dir: string): string {
    return `${dir}/config.json`;
  }

  static statePath(dir: string): string {
    return `${dir}/state.json`;
  }

  static logPath(dir: string): string {
    return `${dir}/xproxy.log`;
  }

  static ensureDir(dir: string): void {
    if (!fs.accessSync(dir)) {
      fs.mkdirSync(dir, true);
    }
  }

  static writeText(path: string, text: string): void {
    const file = fs.openSync(path, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
    try {
      fs.writeSync(file.fd, text);
    } finally {
      fs.closeSync(file);
    }
  }

  static readText(path: string): string {
    if (!fs.accessSync(path)) {
      return '';
    }
    try {
      return fs.readTextSync(path);
    } catch (err) {
      return '';
    }
  }

  static remove(path: string): void {
    if (fs.accessSync(path)) {
      fs.unlinkSync(path);
    }
  }

  /* ------------------------------ 启动配置 ------------------------------ */

  static writeConfig(dir: string, config: VpnConfig): void {
    RuntimeStore.ensureDir(dir);
    RuntimeStore.writeText(RuntimeStore.configPath(dir), config.toJson());
  }

  static readConfig(dir: string): VpnConfig | undefined {
    const raw: string = RuntimeStore.readText(RuntimeStore.configPath(dir));
    if (raw.length === 0) {
      return undefined;
    }
    try {
      return VpnConfig.fromRecord(JSON.parse(raw) as VpnConfigJson);
    } catch (err) {
      return undefined;
    }
  }

  /* ------------------------------ 运行状态 ------------------------------ */

  static writeState(dir: string, state: RuntimeState): void {
    RuntimeStore.ensureDir(dir);
    RuntimeStore.writeText(RuntimeStore.statePath(dir), JSON.stringify(state));
  }

  static readState(dir: string): RuntimeState | undefined {
    const raw: string = RuntimeStore.readText(RuntimeStore.statePath(dir));
    if (raw.length === 0) {
      return undefined;
    }
    try {
      return JSON.parse(raw) as RuntimeState;
    } catch (err) {
      return undefined;
    }
  }

  /* ------------------------------- 日志 -------------------------------- */

  /** 追加日志行, 超过 512KB 时整体截断, 避免无限增长. */
  static appendLog(dir: string, line: string): void {
    const path: string = RuntimeStore.logPath(dir);
    const file = fs.openSync(path, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.APPEND);
    try {
      fs.writeSync(file.fd, `${line}\n`);
    } finally {
      fs.closeSync(file);
    }
  }

  static readLog(dir: string, maxLines: number): string[] {
    const raw: string = RuntimeStore.readText(RuntimeStore.logPath(dir));
    if (raw.length === 0) {
      return [];
    }
    const lines: string[] = raw.split('\n').filter((line: string) => line.length > 0);
    if (lines.length <= maxLines) {
      return lines;
    }
    return lines.slice(lines.length - maxLines);
  }

  static resetLog(dir: string): void {
    RuntimeStore.ensureDir(dir);
    RuntimeStore.writeText(RuntimeStore.logPath(dir), '');
  }

  /** 清理陈旧运行目录 (仅保留最近 [keep] 个). */
  static prune(baseDir: string, keep: number): void {
    const runRoot: string = `${baseDir}/run`;
    if (!fs.accessSync(runRoot)) {
      return;
    }
    try {
      const names: string[] = fs.listFileSync(runRoot);
      const dirs: string[] = names
        .map((name: string) => `${runRoot}/${name}`)
        .filter((path: string) => fs.statSync(path).isDirectory())
        .sort((a: string, b: string) => fs.statSync(b).mtime - fs.statSync(a).mtime);
      for (let i = keep; i < dirs.length; i++) {
        fs.rmdirSync(dirs[i]);
      }
    } catch (err) {
      // 清理失败不影响运行.
    }
  }
}
