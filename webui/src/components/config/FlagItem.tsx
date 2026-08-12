import { useMemo, type ReactNode } from "react";
import { useApp } from "@/store/app";
import { Badge } from "@/components/ui/badge";
import type { OptionDef } from "@/lib/types";
import { FlagControl } from "./FlagControl";

// optionSearchText 生成选项的搜索索引文本。
export function optionSearchText(o: OptionDef, val: unknown): string {
  const list = Array.isArray(val) ? val.map(String) : [String(val ?? "")];
  return [o.name, o.category, o.help, o.hint, ...list]
    .filter(Boolean)
    .join(" ")
    .toLowerCase();
}

// 单个配置项（flags 风格行：左侧名称/说明，右侧控件 + 重置）。
export function FlagItem({ id, opt }: { id: string; opt: OptionDef }) {
  const value = useApp((s) => s.perInst[id]?.configValues?.[opt.name]);
  const cfgSearch = useApp((s) => s.perInst[id]?.cfgSearch);
  const setConfigValue = useApp((s) => s.setConfigValue);

  const q = cfgSearch.trim().toLowerCase();

  const searchText = useMemo(() => optionSearchText(opt, value), [opt, value]);

  // 搜索过滤：命中才显示。
  if (q && !searchText.includes(q)) return null;

  // 描述文本高亮匹配词。
  const desc = opt.hint || opt.help || "";
  let descNode: ReactNode = desc;
  if (q) {
    const i = desc.toLowerCase().indexOf(q);
    if (i >= 0) {
      descNode = (
        <>
          {desc.slice(0, i)}
          <mark className="bg-warn px-0.5 text-[#0b1020]">
            {desc.slice(i, i + q.length)}
          </mark>
          {desc.slice(i + q.length)}
        </>
      );
    }
  }

  return (
    <div
      className={`flex items-center justify-between gap-4 border-b border-border px-3 py-2 last:border-b-0 hover:bg-primary/5 ${
        opt.restart_only ? "bg-warn/5" : ""
      }`}
    >
      <div className="min-w-0 flex-1">
        <div className="flex flex-wrap items-center gap-1.5">
          <code className="font-semibold text-primary">{opt.name}</code>
          {opt.restart_only && (
            <Badge variant="restart" className="px-1.5 py-0 text-[10px]">
              需重启
            </Badge>
          )}
        </div>
        <div className="mt-0.5 text-xs leading-relaxed text-muted-foreground">
          {descNode}
        </div>
      </div>
      <div className="flex shrink-0 items-start gap-2">
        <FlagControl
          opt={opt}
          value={value === undefined ? opt.default : value}
          onChange={(v) => setConfigValue(id, opt.name, v)}
        />
        <button
          type="button"
          title="重置为默认值"
          className="cursor-pointer text-sm opacity-65 transition-opacity hover:opacity-100"
          onClick={() => setConfigValue(id, opt.name, opt.default)}
        >
          ↺
        </button>
      </div>
    </div>
  );
}
