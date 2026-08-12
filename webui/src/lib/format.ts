// 数值格式化工具（与旧版 Vanilla WebUI 复用同一契约，见 doc/webui-spec.md §3.5）。

// 运行状态中文映射：后端返回英文枚举，界面统一显示中文；未知值原样返回。
const STATE_LABELS: Record<string, string> = {
  running: "运行中",
  starting: "启动中",
  stopped: "已停止",
  error: "异常",
};

export function fmtState(state: string | undefined | null): string {
  if (!state) return "-";
  return STATE_LABELS[state] || state;
}

// fmtBytes 字节数 → B/KB/MB/GB/TB；≥100 取整，否则 1 位小数。
export function fmtBytes(n: number | null | undefined): string {
  if (n == null) return "-";
  const units = ["B", "KB", "MB", "GB", "TB"];
  let i = 0;
  let v = Number(n);
  while (v >= 1024 && i < units.length - 1) {
    v /= 1024;
    i++;
  }
  return v.toFixed(v >= 100 || i === 0 ? 0 : 1) + " " + units[i];
}

// fmtRate 字节/秒 → fmtBytes + "/s"；≤0 显示 0。
export function fmtRate(bps: number | null | undefined): string {
  if (bps == null || bps <= 0) return "0";
  return fmtBytes(bps) + "/s";
}

// fmtDur 秒 → X天 HH:MM:SS（不足一天不显示天）。
export function fmtDur(sec: number | null | undefined): string {
  sec = Number(sec || 0);
  const d = Math.floor(sec / 86400);
  const h = Math.floor((sec % 86400) / 3600);
  const m = Math.floor((sec % 3600) / 60);
  const s = sec % 60;
  return (
    (d ? d + "天 " : "") +
    String(h).padStart(2, "0") +
    ":" +
    String(m).padStart(2, "0") +
    ":" +
    String(s).padStart(2, "0")
  );
}

// parseSize 解析带单位的字节数：纯数字或数字+K/M/G/T（基 1024），非法返回 NaN。
export function parseSize(s: string | number): number {
  if (typeof s === "number") return s;
  s = String(s).trim();
  const m = s.match(/^([\d.]+)\s*([KMGT]?B?)$/i);
  if (!m) return NaN;
  const num = parseFloat(m[1]);
  const unit = (m[2] || "").toUpperCase().replace("B", "");
  const mult: Record<string, number> = {
    "": 1,
    K: 1024,
    M: 1024 * 1024,
    G: 1024 * 1024 * 1024,
    T: 1024 * 1024 * 1024 * 1024,
  };
  if (mult[unit] === undefined) return NaN;
  return Math.floor(num * mult[unit]);
}

// formatSize 紧凑格式：2.5G、10.8M（用于限速/配额按钮回显）。
export function formatSize(n: number | null | undefined): string {
  if (n == null || isNaN(n)) return "";
  const units = ["B", "K", "M", "G", "T"];
  let i = 0;
  let v = Number(n);
  while (v >= 1024 && i < units.length - 1) {
    v /= 1024;
    i++;
  }
  const s = (Math.round(v * 100) / 100).toString();
  return s + units[i];
}
