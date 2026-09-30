import { util } from '@kit.ArkTS';

/** 16 字节随机十六进制串, 用作控制通道 token. */
export function randomToken(): string {
  return util.generateRandomUUID(true).replace(/-/g, '');
}

/** 配置 id: 时间戳(十六进制) + 8 字节随机后缀. */
export function newConfigId(): string {
  const ts: string = Date.now().toString(16);
  const suffix: string = util.generateRandomUUID(true).replace(/-/g, '').substring(0, 8);
  return ts + suffix;
}

/** 人类可读字节数, 如 `127.3 MB`. */
export function formatBytes(bytes: number): string {
  if (!Number.isFinite(bytes) || bytes <= 0) {
    return '0 B';
  }
  const units: string[] = ['B', 'KB', 'MB', 'GB'];
  let value: number = bytes;
  let unit: number = 0;
  while (value >= 1024 && unit < units.length - 1) {
    value = value / 1024;
    unit++;
  }
  if (unit === 0) {
    return `${Math.round(value)} B`;
  }
  const text: string = value >= 100 ? value.toFixed(0) : value.toFixed(1);
  return `${text} ${units[unit]}`;
}

/** 人类可读速率, 如 `1.2 MB/s`; 非正值返回空串. */
export function formatSpeed(bytesPerSecond: number): string {
  if (!Number.isFinite(bytesPerSecond) || bytesPerSecond <= 0) {
    return '';
  }
  return `${formatBytes(bytesPerSecond)}/s`;
}

/** 时间戳(毫秒) 转 `HH:mm:ss`; 容错到仅用于日志行前缀. */
export function formatTime(ms: number): string {
  const d: Date = new Date(ms);
  const hh: string = d.getHours().toString().padStart(2, '0');
  const mm: string = d.getMinutes().toString().padStart(2, '0');
  const ss: string = d.getSeconds().toString().padStart(2, '0');
  return `${hh}:${mm}:${ss}`;
}

/** 拆分输入文本行 (支持空白/逗号/分号分隔). */
export function splitList(value: string): string[] {
  const text: string = value.trim();
  if (text.length === 0) {
    return [];
  }
  return text.split(/[\s,;]+/).filter((s: string) => s.length > 0);
}

/** 列表合并为换行文本. */
export function joinList(list: string[]): string {
  return list.join('\n');
}
