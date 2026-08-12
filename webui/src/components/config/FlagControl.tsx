import { Checkbox } from "@/components/ui/checkbox";
import { Input } from "@/components/ui/input";
import { Button } from "@/components/ui/button";
import type { OptionDef } from "@/lib/types";

// 单个配置项的输入控件（按 kind：bool/int/string/stringlist）。
// 全部受控：值来自 configValues，onChange 回写 store（int 存原始字符串，
// 保存时归一化），保证切换实例/重置后表单值正确且不打断输入。

export function FlagControl({
  opt,
  value,
  onChange,
}: {
  opt: OptionDef;
  value: unknown;
  onChange: (v: unknown) => void;
}) {
  if (opt.kind === "bool") {
    return (
      <Checkbox
        checked={!!value}
        onCheckedChange={(c) => onChange(!!c)}
        aria-label={opt.name}
      />
    );
  }

  if (opt.kind === "int") {
    const s =
      value === undefined || value === null || value === "" ? "" : String(value);
    return (
      <Input
        type="number"
        step="1"
        value={s}
        onChange={(e) => onChange(e.target.value)}
        className="w-[200px]"
      />
    );
  }

  if (opt.kind === "stringlist") {
    const rows = Array.isArray(value)
      ? value.map(String)
      : value
        ? [String(value)]
        : [];
    const emit = (next: string[]) => onChange(next.filter((x) => x !== ""));
    return (
      <div className="flex w-[220px] flex-col gap-1">
        {rows.map((r, i) => (
          <div key={i} className="flex gap-1">
            <Input
              type="text"
              value={r}
              onChange={(e) => {
                const next = rows.slice();
                next[i] = e.target.value;
                emit(next);
              }}
              className="h-8 flex-1"
            />
            <Button
              type="button"
              size="sm"
              variant="danger"
              title="删除该项"
              // 与编辑框 h-8 同高，避免行内高度不齐
              className="h-8"
              onClick={() => emit(rows.filter((_, j) => j !== i))}
            >
              ×
            </Button>
          </div>
        ))}
        <Button
          type="button"
          size="sm"
          className="self-start"
          // 直接追加空行：不走 emit 的空项过滤，否则追加的 "" 会被立即滤掉，
          // 添加按钮失效；空行在保存时由 collectConfig 统一过滤。
          onClick={() => onChange([...rows, ""])}
        >
          ＋ 添加
        </Button>
      </div>
    );
  }

  // string
  return (
    <Input
      type="text"
      value={value === undefined || value === null ? "" : String(value)}
      onChange={(e) => onChange(e.target.value)}
      className="w-[200px]"
    />
  );
}
