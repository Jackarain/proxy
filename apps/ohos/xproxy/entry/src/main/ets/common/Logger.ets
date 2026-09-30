import { hilog } from '@kit.PerformanceAnalysisKit';

const DOMAIN: number = 0x1001;

/** hilog 封装: 统一 domain 与 tag 前缀. */
export class Logger {
  private tag: string;

  constructor(tag: string) {
    this.tag = `xproxy-${tag}`;
  }

  debug(message: string): void {
    hilog.debug(DOMAIN, this.tag, '%{public}s', message);
  }

  info(message: string): void {
    hilog.info(DOMAIN, this.tag, '%{public}s', message);
  }

  warn(message: string): void {
    hilog.warn(DOMAIN, this.tag, '%{public}s', message);
  }

  error(message: string): void {
    hilog.error(DOMAIN, this.tag, '%{public}s', message);
  }
}
