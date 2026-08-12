// firstListenPort 取第一个 TCP 监听地址的端口（跳过 unix socket，剥离 -v6only）。
export function firstListenPort(listens?: string[]): string {
  for (const raw of listens || []) {
    const s = String(raw);
    if (s.startsWith("unix://")) continue;
    const m = s.replace(/-?v6only$/i, "").match(/:(\d+)\s*$/);
    if (m) return m[1];
  }
  return "";
}
