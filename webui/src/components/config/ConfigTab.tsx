import { useEffect, useMemo, useState } from "react";
import { useApp } from "@/store/app";
import { api } from "@/lib/api";
import { showToast } from "@/lib/toast";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Badge } from "@/components/ui/badge";
import type { ApplyResult, InstanceDetail, OptionDef } from "@/lib/types";
import { FlagSection } from "./FlagSection";
import { optionSearchText } from "./FlagItem";

// 常用配置置顶，其余按 category 分组（分区名排序）。
function buildSections(options: OptionDef[]): { name: string; opts: OptionDef[] }[] {
  const common = options.filter((o) => o.common);
  const rest = options.filter((o) => !o.common);
  const groups: Record<string, OptionDef[]> = {};
  for (const o of rest) {
    (groups[o.category] ||= []).push(o);
  }
  const sections: { name: string; opts: OptionDef[] }[] = [];
  if (common.length) sections.push({ name: "常用配置", opts: common });
  for (const cat of Object.keys(groups).sort()) {
    sections.push({ name: cat, opts: groups[cat] });
  }
  return sections;
}

// 收集控件当前值并归一化（int 空串→0；stringlist 取非空行）。
function collectConfig(
  options: OptionDef[],
  values: Record<string, unknown>
): Record<string, unknown> {
  const cfg: Record<string, unknown> = {};
  for (const o of options) {
    if (!(o.name in values)) continue;
    const v = values[o.name];
    if (o.kind === "int") {
      const s = String(v ?? "").trim();
      cfg[o.name] = s === "" ? 0 : Number(s);
    } else if (o.kind === "stringlist") {
      cfg[o.name] = Array.isArray(v) ? v.filter((x) => String(x) !== "") : [];
    } else {
      cfg[o.name] = v;
    }
  }
  return cfg;
}

export default function ConfigTab({ id, active }: { id: string; active: boolean }) {
  const st = useApp((s) => s.perInst[id]);
  const options = useApp((s) => s.options);
  const tick = useApp((s) => s.tick);
  const patchInstState = useApp((s) => s.patchInstState);
  const [collapsed, setCollapsed] = useState<Record<string, boolean>>({});
  const [busy, setBusy] = useState(false);

  const cfgSearch = st?.cfgSearch ?? "";
  const configValues = st?.configValues ?? null;
  const configLoaded = st?.configLoaded ?? false;

  // 切换实例时重置折叠状态（与旧版重建表单行为一致）。
  useEffect(() => {
    setCollapsed({});
  }, [id]);

  // 首次进入（configLoaded=false）加载实例配置 + 选项注册表，初始化表单值。
  useEffect(() => {
    if (!active) return;
    if (useApp.getState().perInst[id]?.configLoaded) return;
    let cancelled = false;
    (async () => {
      try {
        const opts = useApp.getState().options;
        const [inst] = await Promise.all([
          api<InstanceDetail>(`/api/instances/${id}`),
          opts.length
            ? Promise.resolve(opts)
            : api<OptionDef[]>("/api/options"),
        ]);
        if (cancelled || useApp.getState().curId !== id) return; // 竞态防护
        if (!useApp.getState().options.length) {
          useApp.getState().setOptions(opts as OptionDef[]);
        }
        const st2 = useApp.getState().perInst[id];
        // 用该实例未保存的草稿覆盖服务端配置（保留切走前的编辑内容）。
        const base = { ...(inst.config || {}), ...(st2?.configValues || {}) };
        patchInstState(id, { configLoaded: true, configValues: base });
      } catch (e) {
        showToast("加载配置表单失败: " + (e as Error).message, "err");
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [active, id, tick, patchInstState]);

  const sections = useMemo(() => buildSections(options), [options]);
  const q = cfgSearch.trim().toLowerCase();

  // 每个分区可见项数（搜索时据此隐藏无匹配分区）。
  const sectionVisible = useMemo(() => {
    const out: Record<string, number> = {};
    for (const sec of sections) {
      let n = 0;
      for (const o of sec.opts) {
        const v = configValues?.[o.name];
        if (!q || optionSearchText(o, v).includes(q)) n++;
      }
      out[sec.name] = n;
    }
    return out;
  }, [sections, q, configValues]);

  const visibleTotal = useMemo(
    () => Object.values(sectionVisible).reduce((a, b) => a + b, 0),
    [sectionVisible]
  );

  // 搜索时自动展开有匹配的分区。
  const isCollapsed = (name: string) =>
    q ? false : !!collapsed[name];

  const toggleAll = () => {
    const anyOpen = sections.some((s) => !isCollapsed(s.name));
    const next: Record<string, boolean> = {};
    for (const s of sections) next[s.name] = anyOpen;
    setCollapsed(next);
  };

  const resetAll = () => {
    const values: Record<string, unknown> = {};
    for (const o of options) values[o.name] = o.default;
    patchInstState(id, { configValues: values });
    showToast("已恢复全部选项为默认值", "ok");
  };

  const save = async () => {
    if (!configValues) return;
    setBusy(true);
    try {
      const cfg = collectConfig(options, configValues);
      const res = await api<ApplyResult>(`/api/instances/${id}/config`, {
        method: "PUT",
        body: JSON.stringify({ config: cfg }),
      });
      if (useApp.getState().curId !== id) return; // 竞态防护
      const parts: string[] = [];
      if (res.applied?.length) parts.push("已生效: " + res.applied.join(", "));
      if (res.needs_restart?.length)
        parts.push("需重启生效: " + res.needs_restart.join(", "));
      const errCount = Object.keys(res.errors || {}).length;
      if (errCount)
        parts.push(
          "失败: " +
            Object.entries(res.errors || {}).map(([k, v]) => `${k}=${v}`).join(", ")
        );
      patchInstState(id, {
        applyResult: {
          text: parts.join("；") || "配置已保存",
          kind: errCount ? "err" : "ok",
          needsRestart: res.needs_restart || [],
        },
      });
    } catch (e) {
      showToast((e as Error).message, "err");
    } finally {
      setBusy(false);
    }
  };

  return (
    <div>
      {/* 工具栏 */}
      <div className="mb-3 flex flex-wrap items-center justify-between gap-3">
        <div className="flex max-w-[520px] flex-1 items-center gap-2">
          <Input
            type="search"
            placeholder="搜索配置选项…"
            value={cfgSearch}
            onChange={(e) =>
              patchInstState(id, { cfgSearch: e.target.value })
            }
            className="h-8"
          />
          {q && (
            <span className="whitespace-nowrap text-xs text-muted-foreground">
              匹配 {visibleTotal} 项
            </span>
          )}
        </div>
        <div className="flex gap-2">
          {sections.length > 1 && (
            <Button size="sm" onClick={toggleAll}>
              {sections.some((s) => !isCollapsed(s.name))
                ? "全部折叠"
                : "全部展开"}
            </Button>
          )}
          <Button
            size="sm"
            title="恢复全部选项为默认值"
            onClick={resetAll}
          >
            全部重置
          </Button>
          <Button
            variant="primary"
            size="sm"
            title="保存并应用配置"
            onClick={save}
            disabled={busy || !configLoaded}
          >
            保存并应用
          </Button>
        </div>
      </div>

      {/* 图例 */}
      <div className="mb-2.5 flex items-center gap-1.5 text-xs text-muted-foreground">
        <Badge variant="restart" className="px-1.5 py-0 text-[10px]">
          需重启
        </Badge>
        <span>标记的选项运行期修改需要重启实例生效。</span>
      </div>

      {/* 分区 */}
      {sections.map(
        (sec) =>
          sectionVisible[sec.name] > 0 && (
            <FlagSection
              key={sec.name}
              id={id}
              name={sec.name}
              opts={sec.opts}
              collapsed={isCollapsed(sec.name)}
              onToggle={() =>
                setCollapsed((c) => ({ ...c, [sec.name]: !c[sec.name] }))
              }
            />
          )
      )}
      {q && visibleTotal === 0 && (
        <div className="py-12 text-center text-muted-foreground">
          没有匹配的配置选项
        </div>
      )}

      {/* 保存 */}
      <div className="mt-2.5">
        <Button
          variant="primary"
          onClick={save}
          disabled={busy || !configLoaded}
        >
          保存并应用
        </Button>
      </div>
    </div>
  );
}
