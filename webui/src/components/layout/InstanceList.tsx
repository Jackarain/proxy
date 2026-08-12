import { useMemo } from "react";
import { PanelLeftClose, PanelLeftOpen } from "lucide-react";
import { useApp } from "@/store/app";
import { Input } from "@/components/ui/input";
import { fmtRate, fmtState } from "@/lib/format";
import type { InstanceSummary } from "@/lib/types";

// buildMeta 实例列表项元信息行（状态点/PID/连接数/速率）。
function buildMeta(inst: InstanceSummary) {
  const dotCls = inst.online
    ? "bg-ok"
    : inst.state === "starting"
      ? "bg-warn"
      : "bg-muted-foreground";
  const parts = [
    <span key="state">
      <span className={`mr-1 inline-block h-2 w-2 ${dotCls}`} />
      {fmtState(inst.state)}
    </span>,
  ];
  if (inst.pid) parts.push(<span key="pid"> · PID {inst.pid}</span>);
  if (inst.active) parts.push(<span key="active"> · {inst.active} 连接</span>);
  if (inst.rx_rate_bps > 0)
    parts.push(<span key="rx"> · ↓{fmtRate(inst.rx_rate_bps)}</span>);
  if (inst.tx_rate_bps > 0)
    parts.push(<span key="tx"> · ↑{fmtRate(inst.tx_rate_bps)}</span>);
  return parts;
}

function InstanceItem({
  inst,
  active,
  onClick,
}: {
  inst: InstanceSummary;
  active: boolean;
  onClick: () => void;
}) {
  return (
    <li
      className={`mb-1 cursor-pointer border px-2.5 py-2 transition-colors hover:bg-secondary ${
        active ? "border-primary bg-secondary" : "border-transparent"
      }`}
      onClick={onClick}
    >
      <div className="font-semibold">{inst.name}</div>
      <div className="truncate text-xs text-muted-foreground">
        {(inst.listen || []).join(" ") || "未配置监听"}
      </div>
      <div className="mt-0.5 text-xs text-muted-foreground">
        {buildMeta(inst)}
      </div>
    </li>
  );
}

export default function InstanceList() {
  const instances = useApp((s) => s.instances);
  const instFilter = useApp((s) => s.instFilter);
  const curId = useApp((s) => s.curId);
  const instCollapsed = useApp((s) => s.instCollapsed);
  const toggleInstCollapsed = useApp((s) => s.toggleInstCollapsed);
  const setInstFilter = useApp((s) => s.setInstFilter);
  const selectInstance = useApp((s) => s.selectInstance);

  // 按 名称 或 ID 的包含匹配过滤。
  const filtered = useMemo(() => {
    if (!instFilter) return instances;
    const f = instFilter.toLowerCase();
    return instances.filter(
      (i) =>
        i.name.toLowerCase().includes(f) || i.id.toLowerCase().includes(f)
    );
  }, [instances, instFilter]);

  // 折叠状态：窄条上以首字符按钮显示全部实例，可切换选中；顶部按钮用于展开。
  if (instCollapsed) {
    return (
      <aside className="flex w-9 min-w-9 flex-col items-center border-r border-border bg-card py-2">
        <button
          type="button"
          title="展开实例列表"
          className="mb-2 cursor-pointer rounded p-1.5 text-muted-foreground transition-colors hover:bg-secondary hover:text-foreground"
          onClick={toggleInstCollapsed}
        >
          <PanelLeftOpen size={16} />
        </button>
        <ul className="flex w-full flex-1 flex-col items-center gap-1 overflow-y-auto">
          {instances.map((inst) => {
            const active = inst.id === curId;
            return (
              <li key={inst.id} className="flex w-full justify-center">
                <button
                  type="button"
                  title={inst.name}
                  className={`flex h-7 w-7 cursor-pointer items-center justify-center rounded border text-xs font-semibold transition-colors ${
                    active
                      ? "border-primary bg-secondary text-primary"
                      : "border-transparent text-muted-foreground hover:bg-secondary hover:text-foreground"
                  }`}
                  onClick={() => selectInstance(inst.id)}
                >
                  {inst.name.charAt(0) || "?"}
                </button>
              </li>
            );
          })}
        </ul>
      </aside>
    );
  }

  return (
    <aside className="flex w-[280px] min-w-[280px] flex-col border-r border-border bg-card p-2.5">
      <div className="flex items-center justify-between px-1.5 py-1">
        <span className="text-xs uppercase tracking-wider text-muted-foreground">
          实例列表
        </span>
        <button
          type="button"
          title="收起实例列表"
          className="cursor-pointer rounded p-0.5 text-muted-foreground transition-colors hover:bg-secondary hover:text-foreground"
          onClick={toggleInstCollapsed}
        >
          <PanelLeftClose size={14} />
        </button>
      </div>
      <Input
        type="search"
        placeholder="搜索实例…"
        value={instFilter}
        onChange={(e) => setInstFilter(e.target.value)}
        className="mb-1 h-8"
      />
      <ul className="flex-1 overflow-y-auto">
        {filtered.map((inst) => (
          <InstanceItem
            key={inst.id}
            inst={inst}
            active={inst.id === curId}
            onClick={() => selectInstance(inst.id)}
          />
        ))}
      </ul>
    </aside>
  );
}
