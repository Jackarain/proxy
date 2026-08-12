import { useEffect, useState, type MouseEvent as ReactMouseEvent } from "react";
import { useApp } from "@/store/app";
import { api } from "@/lib/api";
import { fmtBytes, fmtDur, fmtRate } from "@/lib/format";
import type {
  ConnectionInfo,
  StatusData,
  StatusReport,
} from "@/lib/types";

// 连接明细排序比较：文本类用 localeCompare，数值类做差。
export function compareConns(a: ConnectionInfo, b: ConnectionInfo, key: string): number {
  if (key === "client_ip" || key === "proto" || key === "target") {
    return String(a[key] || "").localeCompare(String(b[key] || ""));
  }
  if (key === "region") {
    return String((a.region || []).join(" / ")).localeCompare(
      String((b.region || []).join(" / "))
    );
  }
  return (Number(a[key as "id" | "elapsed" | "rx_bytes" | "tx_bytes"]) || 0) -
    (Number(b[key as "id" | "elapsed" | "rx_bytes" | "tx_bytes"]) || 0);
}

// 状态摘要条（横向单行，可换行）。
function StatsBar({ report }: { report: StatusReport | null }) {
  const r = report;
  return (
    <div className="mb-4 flex flex-wrap items-baseline gap-x-6 gap-y-1 border border-border bg-card px-4 py-2.5 text-[13px]">
      <div className="flex items-baseline gap-1.5 whitespace-nowrap">
        <span className="text-xs text-muted-foreground">运行</span>
        <span className="num font-semibold">{fmtDur(r?.uptime)}</span>
      </div>
      <div className="flex items-baseline gap-1.5 whitespace-nowrap">
        <span className="text-xs text-muted-foreground">存活连接</span>
        <span className="num font-semibold text-primary">
          {r?.active_connections ?? 0}
        </span>
      </div>
      <div className="flex items-baseline gap-1.5 whitespace-nowrap">
        <span className="text-xs text-muted-foreground">累积连接</span>
        <span className="num font-semibold text-primary">{r?.conn_total ?? 0}</span>
      </div>
      <div className="flex items-baseline gap-1.5 whitespace-nowrap">
        <span className="text-xs text-ok">▲</span>
        <span className="num font-semibold text-ok">
          {fmtRate(r?.rates?.rx_rate_bps)}
        </span>
      </div>
      <div className="flex items-baseline gap-1.5 whitespace-nowrap">
        <span className="text-xs text-warn">▼</span>
        <span className="num font-semibold text-warn">
          {fmtRate(r?.rates?.tx_rate_bps)}
        </span>
      </div>
      <div className="flex items-baseline gap-1.5 whitespace-nowrap">
        <span className="text-xs text-muted-foreground">上传</span>
        <span className="num font-semibold opacity-75">
          {fmtBytes(r?.global?.rx_bytes)}
        </span>
      </div>
      <div className="flex items-baseline gap-1.5 whitespace-nowrap">
        <span className="text-xs text-muted-foreground">下载</span>
        <span className="num font-semibold opacity-75">
          {fmtBytes(r?.global?.tx_bytes)}
        </span>
      </div>
    </div>
  );
}

const CONN_COLS: { key: string; label: string; num?: boolean }[] = [
  { key: "id", label: "ID", num: true },
  { key: "client_ip", label: "客户端 IP" },
  { key: "target", label: "目标" },
  { key: "region", label: "地区" },
  { key: "proto", label: "协议" },
  { key: "elapsed", label: "时长", num: true },
  { key: "rx_bytes", label: "上传", num: true },
  { key: "tx_bytes", label: "下载", num: true },
];

// 连接表各列默认宽度（px），表头可拖拽调整并持久化到 localStorage。
const CONN_COL_WIDTHS: Record<string, number> = {
  id: 80,
  client_ip: 180,
  target: 220,
  region: 160,
  proto: 90,
  elapsed: 110,
  rx_bytes: 110,
  tx_bytes: 110,
};
const CONN_WIDTHS_KEY = "cproxy:conn-col-widths";
// 拖拽调整的最小列宽。
const MIN_COL_WIDTH = 60;

function loadConnWidths(): Record<string, number> {
  try {
    const raw = localStorage.getItem(CONN_WIDTHS_KEY);
    if (raw) return JSON.parse(raw) as Record<string, number>;
  } catch {
    /* 忽略损坏数据 */
  }
  return {};
}

function saveConnWidths(w: Record<string, number>) {
  try {
    localStorage.setItem(CONN_WIDTHS_KEY, JSON.stringify(w));
  } catch {
    /* localStorage 不可用时忽略 */
  }
}

// 用户下的连接明细子表（可排序、表头可拖拽调整列宽）。
function ConnectionTable({
  id,
  conns,
}: {
  id: string;
  conns: ConnectionInfo[];
}) {
  const connSort = useApp((s) => s.perInst[id]?.connSort);
  const patchInstState = useApp((s) => s.patchInstState);
  const [colWidths, setColWidths] = useState<Record<string, number>>(loadConnWidths);

  const sorted = connSort
    ? conns.slice().sort((a, b) => {
        const c = compareConns(a, b, connSort.key);
        return connSort.dir === "desc" ? -c : c;
      })
    : conns;

  const onSort = (key: string) => {
    const cur = connSort;
    let dir: "asc" | "desc" = "asc";
    if (cur && cur.key === key) dir = cur.dir === "asc" ? "desc" : "asc";
    patchInstState(id, { connSort: { key, dir } });
  };

  // 表头拖拽调整列宽：按下手柄后跟随鼠标移动更新宽度，松开结束。
  const startResize = (e: ReactMouseEvent, key: string) => {
    e.preventDefault();
    e.stopPropagation();
    const startX = e.clientX;
    const startW = colWidths[key] ?? CONN_COL_WIDTHS[key] ?? 120;
    const onMove = (ev: MouseEvent) => {
      const w = Math.max(MIN_COL_WIDTH, startW + (ev.clientX - startX));
      setColWidths((cur) => {
        const next = { ...cur, [key]: w };
        saveConnWidths(next);
        return next;
      });
    };
    const onUp = () => {
      document.removeEventListener("mousemove", onMove);
      document.removeEventListener("mouseup", onUp);
      document.body.style.cursor = "";
      document.body.style.userSelect = "";
    };
    document.addEventListener("mousemove", onMove);
    document.addEventListener("mouseup", onUp);
    document.body.style.cursor = "col-resize";
    document.body.style.userSelect = "none";
  };

  return (
    <table className="w-full table-fixed border-collapse text-xs">
      <thead>
        <tr>
          {CONN_COLS.map((c) => (
            <th
              key={c.key}
              onClick={() => onSort(c.key)}
              style={{ width: colWidths[c.key] ?? CONN_COL_WIDTHS[c.key] }}
              className={`relative cursor-pointer select-none whitespace-nowrap border-b border-border bg-secondary px-2.5 py-1.5 text-left text-[11px] font-semibold uppercase tracking-wide text-muted-foreground hover:text-primary ${
                c.num ? "text-right" : ""
              }`}
            >
              {c.label}
              {connSort?.key === c.key && (
                <span className="font-bold text-primary">
                  {connSort.dir === "asc" ? " ▲" : " ▼"}
                </span>
              )}
              {/* 列宽拖拽手柄 */}
              <span
                onMouseDown={(e) => startResize(e, c.key)}
                onClick={(e) => e.stopPropagation()}
                title="拖拽调整列宽"
                className="absolute right-0 top-0 h-full w-1.5 cursor-col-resize hover:bg-primary/40"
              />
            </th>
          ))}
        </tr>
      </thead>
      <tbody>
        {sorted.map((c) => (
          <tr key={c.id} className="hover:bg-primary/5">
            <td className="mono num truncate px-2.5 py-1 text-[11px] text-muted-foreground">
              {c.id}
            </td>
            <td className="truncate px-2.5 py-1 text-foreground">
              {c.client_ip}
            </td>
            <td
              className="truncate px-2.5 py-1 text-muted-foreground"
              title={c.target || ""}
            >
              {c.target || "—"}
            </td>
            <td className="truncate px-2.5 py-1 text-primary">
              {(c.region || []).join(" / ") || "未知地区"}
            </td>
            <td className="truncate px-2.5 py-1 text-muted-foreground">{c.proto}</td>
            <td className="num truncate px-2.5 py-1 text-muted-foreground">
              {fmtDur(c.elapsed)}
            </td>
            <td className="num truncate px-2.5 py-1 text-ok">↑{fmtBytes(c.rx_bytes)}</td>
            <td className="num truncate px-2.5 py-1 text-warn">↓{fmtBytes(c.tx_bytes)}</td>
          </tr>
        ))}
      </tbody>
    </table>
  );
}

export default function StatusTab({ id, active }: { id: string; active: boolean }) {
  const [report, setReport] = useState<StatusReport | null>(null);
  const tick = useApp((s) => s.tick);
  const expanded = useApp((s) => s.perInst[id]?.expanded);
  const patchInstState = useApp((s) => s.patchInstState);
  const userConns = useApp((s) => s.perInst[id]?.userConns);

  // 2 秒轮询刷新状态（仅活跃页签）。
  useEffect(() => {
    if (!active) return;
    let cancelled = false;
    (async () => {
      try {
        const s = await api<StatusData>(`/api/instances/${id}/status`);
        if (cancelled || useApp.getState().curId !== id) return; // 竞态防护
        setReport(s.report || null);
        // 缓存连接数据，供排序重渲染。
        const conns: Record<string, ConnectionInfo[]> = {};
        for (const u of s.report?.users || []) {
          if (u.connections?.length) conns[u.user] = u.connections;
        }
        patchInstState(id, { userConns: conns });
      } catch {
        /* 静默，等下一轮 */
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [active, id, tick, patchInstState]);

  const r = report;
  const users = r?.users || [];

  return (
    <div>
      <StatsBar report={r} />
      <div className="mb-2 mt-1 text-[13px] font-semibold text-muted-foreground">
        按用户统计
      </div>
      <table className="w-full border-separate border-spacing-0 border border-border bg-card">
        <thead>
          <tr>
            {["用户", "存活连接", "累积连接", "上传速率", "下载速率", "累计上传", "累计下载"].map(
              (h, i) => (
                <th
                  key={h}
                  className={`whitespace-nowrap bg-secondary px-3 py-2.5 text-left text-xs font-semibold uppercase tracking-wide text-muted-foreground ${
                    i > 0 ? "num" : ""
                  }`}
                >
                  {h}
                </th>
              )
            )}
          </tr>
        </thead>
        <tbody>
          {!users.length && (
            <tr>
              <td colSpan={7} className="px-3 py-7 text-center text-muted-foreground">
                暂无连接
              </td>
            </tr>
          )}
          {users.map((u) => {
            const rates = r?.user_rates?.[u.user] || {};
            const hasConns = !!(u.connections && u.connections.length);
            const overQuota = u.quota > 0 && u.usage_total >= u.quota;
            const isOpen = !!expanded?.[u.user];
            return (
              <UserRow
                key={u.user}
                id={id}
                u={u}
                rates={rates}
                hasConns={hasConns}
                overQuota={overQuota}
                isOpen={isOpen}
                conns={u.connections || []}
                userConns={userConns}
              />
            );
          })}
        </tbody>
      </table>
    </div>
  );
}

function UserRow({
  id,
  u,
  rates,
  hasConns,
  overQuota,
  isOpen,
  conns,
  userConns,
}: {
  id: string;
  u: {
    user: string;
    rx_bytes: number;
    tx_bytes: number;
    active_connections: number;
    conn_total: number;
    quota: number;
    usage_total: number;
  };
  rates: { rx_rate_bps?: number; tx_rate_bps?: number };
  hasConns: boolean;
  overQuota: boolean;
  isOpen: boolean;
  conns: ConnectionInfo[];
  userConns?: Record<string, ConnectionInfo[]>;
}) {
  const patchInstState = useApp((s) => s.patchInstState);
  const toggle = () => {
    if (!hasConns) return;
    patchInstState(id, {
      expanded: { ...useApp.getState().perInst[id]?.expanded, [u.user]: !isOpen },
    });
  };

  const txCell =
    u.quota > 0 ? (
      <span className={overQuota ? "font-semibold text-err" : ""}>
        {fmtBytes(u.usage_total)} / {fmtBytes(u.quota)}
      </span>
    ) : (
      fmtBytes(u.tx_bytes)
    );

  return (
    <>
      <tr
        onClick={toggle}
        title={hasConns ? "点击查看连接明细" : ""}
        className={`border-t border-border ${hasConns ? "cursor-pointer hover:bg-primary/5" : ""}`}
      >
        <td className="px-3 py-2">
          <b>{u.user}</b>
        </td>
        <td className="num px-3 py-2">{u.active_connections || 0}</td>
        <td className="num px-3 py-2 text-muted-foreground">{u.conn_total || 0}</td>
        <td className="num px-3 py-2 text-ok">{fmtRate(rates.rx_rate_bps)}</td>
        <td className="num px-3 py-2 text-warn">{fmtRate(rates.tx_rate_bps)}</td>
        <td className="num px-3 py-2">{fmtBytes(u.rx_bytes)}</td>
        <td className="num px-3 py-2">{txCell}</td>
      </tr>
      {hasConns && isOpen && (
        <tr className="border-t border-border">
          <td colSpan={7} className="bg-primary/5 px-3 py-2 pl-8">
            <ConnectionTable id={id} conns={userConns?.[u.user] || conns} />
          </td>
        </tr>
      )}
    </>
  );
}
