import { http } from '@kit.NetworkKit';
import { common, wantConstant, Want } from '@kit.AbilityKit';
import { fileIo as fs, fileUri } from '@kit.CoreFileKit';
import { bundleManager } from '@kit.AbilityKit';
import { preferences } from '@kit.ArkData';
import { BusinessError } from '@kit.BasicServicesKit';
import { Logger } from '../common/Logger';

const log = new Logger('update');

/** 发布目录的列表地址: `hash=1` 时每项带上整文件 SHA-1. */
export const UPDATE_INDEX_URL: string = 'https://www.jackarain.org/download/?q=json&hash=1';
/** 安装包下载地址. */
export const UPDATE_PACKAGE_URL: string = 'https://www.jackarain.org/download/ohos-release.hap';
/** 发布目录里安装包的文件名. */
export const UPDATE_PACKAGE_NAME: string = 'ohos-release.hap';
/** 启动后自动检查更新的最小间隔. */
const CHECK_INTERVAL: number = 24 * 60 * 60 * 1000;
/** 列表响应允许的最大长度. */
const INDEX_MAX_BYTES: number = 256 * 1024;
/** 下载上限 (单次响应体上限). */
const DOWNLOAD_MAX_BYTES: number = 100 * 1024 * 1024;
const USER_AGENT: string = 'xproxy-ohos';

const HANDLED_HASH_KEY: string = 'xproxy_update_hash';
const LAST_CHECK_KEY: string = 'xproxy_update_last_check';

/** 发布目录里的安装包信息. */
export interface RemotePackage {
  hash: string;
  size: number;
  lastWriteTime: string;
}

/** 下载进度. */
export interface DownloadProgress {
  received: number;
  total: number;
}

/** 更新检查/下载中的可预期失败. */
export class UpdateException extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'UpdateException';
  }
}

/** 远端发布信息查询与安装包下载. */
export class UpdateService {
  private context: common.UIAbilityContext;
  private store: preferences.Preferences;
  private downloading: boolean = false;

  constructor(context: common.UIAbilityContext) {
    this.context = context;
    this.store = preferences.getPreferencesSync(context, { name: 'xproxy_store' });
  }

  /** 已处理过的远端 SHA-1 (已发起安装或用户选择跳过). */
  handledHash(): string {
    return this.store.getSync(HANDLED_HASH_KEY, '') as string;
  }

  saveHandledHash(value: string): void {
    this.store.putSync(HANDLED_HASH_KEY, value);
    this.store.flush();
  }

  /** 距离上次检查是否已超过自动检查间隔. */
  isCheckDue(): boolean {
    const last: number = this.store.getSync(LAST_CHECK_KEY, 0) as number;
    return Date.now() - last > CHECK_INTERVAL;
  }

  saveLastCheck(): void {
    this.store.putSync(LAST_CHECK_KEY, Date.now());
    this.store.flush();
  }

  /** 查询发布目录: 取安装包的 SHA-1 与大小. */
  async fetchRemote(): Promise<RemotePackage> {
    const client = http.createHttp();
    try {
      const resp = await client.request(UPDATE_INDEX_URL, {
        method: http.RequestMethod.GET,
        header: { 'Accept-Encoding': 'identity', 'User-Agent': USER_AGENT },
        expectDataType: http.HttpDataType.STRING,
        connectTimeout: 20000,
        readTimeout: 20000,
        maxLimit: INDEX_MAX_BYTES
      });
      if (resp.responseCode !== 200) {
        throw new UpdateException(resp.responseCode === 404
          ? '更新列表不可用 (HTTP 404), 请稍后再试'
          : `更新列表返回 HTTP ${resp.responseCode}`);
      }
      return parseIndex(typeof resp.result === 'string' ? resp.result : '');
    } catch (err) {
      if (err instanceof UpdateException) {
        throw err;
      }
      throw new UpdateException(`网络错误: ${errorText(err)}`);
    } finally {
      client.destroy();
    }
  }

  /** 下载安装包到应用私有目录, 返回落地路径. */
  async download(onProgress: (progress: DownloadProgress) => void): Promise<string> {
    if (this.downloading) {
      throw new UpdateException('已有下载任务进行中');
    }
    this.downloading = true;
    const dir: string = `${this.context.filesDir}/update`;
    if (!fs.accessSync(dir)) {
      fs.mkdirSync(dir, true);
    }
    const path: string = `${dir}/${UPDATE_PACKAGE_NAME}`;
    const file = fs.openSync(path, fs.OpenMode.READ_WRITE | fs.OpenMode.CREATE | fs.OpenMode.TRUNC);
    const client = http.createHttp();
    let received: number = 0;
    let total: number = 0;
    try {
      client.on('dataReceive', (data: ArrayBuffer) => {
        received += data.byteLength;
        fs.writeSync(file.fd, data);
        onProgress({ received: received, total: total });
      });
      client.on('dataReceiveProgress', (info: http.DataReceiveProgressInfo) => {
        total = info.totalSize;
        onProgress({ received: info.receiveSize, total: info.totalSize });
      });
      client.on('headersReceive', (header: Object) => {
        log.debug(`download headers: ${JSON.stringify(header)}`);
      });
      const status: number = await client.requestInStream(UPDATE_PACKAGE_URL, {
        method: http.RequestMethod.GET,
        header: { 'Accept-Encoding': 'identity', 'User-Agent': USER_AGENT },
        connectTimeout: 20000,
        readTimeout: 60000,
        maxLimit: DOWNLOAD_MAX_BYTES
      });
      if (status !== 200) {
        throw new UpdateException(status === 404 ? '安装包不存在 (HTTP 404)'
          : `下载失败: HTTP ${status}`);
      }
      if (total > 0 && received !== total) {
        throw new UpdateException(`下载不完整 (${received}/${total} 字节)`);
      }
      return path;
    } catch (err) {
      fs.closeSync(file);
      removeQuietly(path);
      if (err instanceof UpdateException) {
        throw err;
      }
      throw new UpdateException(`下载失败: ${errorText(err)}`);
    } finally {
      client.destroy();
      this.downloading = false;
      try {
        fs.closeSync(file);
      } catch (err) {
        // 已关闭.
      }
    }
  }

  /** 调起系统安装器安装已下载的安装包. */
  async install(path: string): Promise<void> {
    const uri: string = fileUri.getUriFromPath(path);
    const want: Want = {
      action: 'ohos.want.action.viewData',
      uri: uri,
      type: 'application/vnd.harmonyos.package',
      flags: wantConstant.Flags.FLAG_AUTH_READ_URI_PERMISSION
    };
    await this.context.startAbility(want);
  }

  /** 当前应用版本信息. */
  currentVersion(): string {
    const info = bundleManager.getBundleInfoForSelfSync(bundleManager.BundleFlag.GET_BUNDLE_INFO_DEFAULT);
    return `${info.versionName} (build ${info.versionCode})`;
  }
}

function parseIndex(body: string): RemotePackage {
  let decoded: Object;
  try {
    decoded = JSON.parse(body) as Object;
  } catch (err) {
    throw new UpdateException('更新列表不是合法的 JSON');
  }
  if (!Array.isArray(decoded)) {
    throw new UpdateException('更新列表格式不符合预期');
  }
  for (const item of decoded as ReleaseEntry[]) {
    if (item.filename !== UPDATE_PACKAGE_NAME) {
      continue;
    }
    const hash: string = (item.hash ?? '').trim().toLowerCase();
    if (!isSha1(hash)) {
      throw new UpdateException('发布信息缺少有效的 SHA-1 校验值');
    }
    return {
      hash: hash,
      size: item.filesize ?? -1,
      lastWriteTime: item.last_write_time ?? ''
    };
  }
  throw new UpdateException(`更新列表里没有 ${UPDATE_PACKAGE_NAME}`);
}

/** 发布目录条目. */
interface ReleaseEntry {
  filename?: string;
  hash?: string;
  filesize?: number;
  last_write_time?: string;
}

function isSha1(value: string): boolean {
  return /^[0-9a-f]{40}$/.test(value);
}

function removeQuietly(path: string): void {
  try {
    if (fs.accessSync(path)) {
      fs.unlinkSync(path);
    }
  } catch (err) {
    // 忽略.
  }
}

function errorText(err: Object): string {
  const be = err as BusinessError;
  return be !== undefined && be.message !== undefined ? be.message : JSON.stringify(err);
}
