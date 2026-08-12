import { FlagItem } from "./FlagItem";
import type { OptionDef } from "@/lib/types";

// 选项分区：标题行（点击折叠）+ 选项列表。
export function FlagSection({
  id,
  name,
  opts,
  collapsed,
  onToggle,
}: {
  id: string;
  name: string;
  opts: OptionDef[];
  collapsed: boolean;
  onToggle: () => void;
}) {
  return (
    <div className="mb-4">
      <div
        className="flex cursor-pointer select-none items-center gap-2 border border-border bg-card px-3 py-1.5 text-[13px] font-semibold uppercase tracking-wide text-muted-foreground hover:text-primary"
        onClick={onToggle}
      >
        <span className="inline-block w-3">{collapsed ? "▸" : "▾"}</span>
        <span>{name}</span>
        <span className="ml-auto text-[11px] font-normal">{opts.length}</span>
      </div>
      {!collapsed && (
        <div className="border border-t-0 border-border">
          {opts.map((o) => (
            <FlagItem key={o.name} id={id} opt={o} />
          ))}
        </div>
      )}
    </div>
  );
}
