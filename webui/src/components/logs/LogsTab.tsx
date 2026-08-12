import {
  useEffect,
  useLayoutEffect,
  useMemo,
  useRef,
  type ReactNode,
} from "react";
import { useApp } from "@/store/app";
import { loadLogs } from "@/lib/logs";
import { Input } from "@/components/ui/input";
import { Checkbox } from "@/components/ui/checkbox";
import { Button } from "@/components/ui/button";

// 转义正则特殊字符，避免过滤词中的元字符破坏匹配。
function escapeRegExp(s: string) {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

// 高亮日志文本中所有匹配片段；query 需为已 trim 的过滤词（普通模式为小写化）。
// 普通模式：escapeRegExp 后 split，匹配段以 mark 高亮；
// 正则模式：用 matchAll 高亮全部匹配（内部重建带 g 标志的正则，避免共享 lastIndex）。
function Highlighted({
  text,
  query,
  regex,
}: {
  text: string;
  query: string;
  regex: RegExp | null;
}) {
  if (!query) return <>{text}</>;
  if (regex) {
    const nodes: ReactNode[] = [];
    let last = 0;
    for (const m of text.matchAll(new RegExp(regex.source, "gi"))) {
      const idx = m.index ?? 0;
      if (idx > last) nodes.push(<span key={last}>{text.slice(last, idx)}</span>);
      nodes.push(
        <mark key={idx} className="bg-warn px-0.5 text-[#0b1020]">
          {m[0]}
        </mark>
      );
      last = idx + m[0].length;
    }
    if (last < text.length) nodes.push(<span key={last}>{text.slice(last)}</span>);
    return <>{nodes}</>;
  }
  const parts = text.split(new RegExp(`(${escapeRegExp(query)})`, "gi"));
  return (
    <>
      {parts.map((part, i) =>
        part.toLowerCase() === query ? (
          <mark key={i} className="bg-warn px-0.5 text-[#0b1020]">
            {part}
          </mark>
        ) : (
          <span key={i}>{part}</span>
        )
      )}
    </>
  );
}

export default function LogsTab({ id, active }: { id: string; active: boolean }) {
  const logLines = useApp((s) => s.perInst[id]?.logLines);
  const logFilter = useApp((s) => s.perInst[id]?.logFilter);
  const logRegex = useApp((s) => s.perInst[id]?.logRegex);
  const autoscroll = useApp((s) => s.perInst[id]?.autoscroll);
  const tick = useApp((s) => s.tick);
  const patchInstState = useApp((s) => s.patchInstState);
  const viewRef = useRef<HTMLDivElement>(null);

  // 轮询拉取日志（仅活跃页签）。loadLogs 内部自带竞态防护。
  useEffect(() => {
    if (!active) return;
    void loadLogs(id);
  }, [active, id, tick]);

  // 自动滚动：开启时新日志滚动条始终停在底部；关闭时保持当前位置。
  useLayoutEffect(() => {
    const view = viewRef.current;
    if (view && autoscroll) view.scrollTop = view.scrollHeight;
  }, [logLines, logFilter, autoscroll]);

  // 过滤模式：对全部日志行过滤；退出过滤时恢复全量视图。
  // 输入值保留原样（含空格，方便输入形如 "connection: 12" 的过滤词）。
  // 普通模式：去除首尾空白并统一小写；正则模式：按正则匹配（忽略大小写），
  // 非法正则回退为普通包含匹配，避免崩溃。
  const rawFilter = logFilter?.trim() || "";
  const filter = logRegex ? rawFilter : rawFilter.toLowerCase();

  const regex = useMemo(() => {
    if (!logRegex || !filter) return null;
    try {
      return new RegExp(filter, "i");
    } catch {
      return null; // 非法正则：回退普通包含匹配
    }
  }, [logRegex, filter]);

  const shown = useMemo(() => {
    if (!filter) return logLines || [];
    if (regex) {
      return (logLines || []).filter((l) => regex.test(l.text));
    }
    return (logLines || []).filter((l) =>
      l.text.toLowerCase().includes(filter)
    );
  }, [logLines, filter, regex]);

  const clearLog = () => {
    const st = useApp.getState().perInst[id];
    if (!st) return;
    // 进入清空模式：从当前序号增量拉取，只显示清空后产生的新日志。
    patchInstState(id, {
      logClear: true,
      logClearSeq: st.lastLogSeq,
      logLines: [],
    });
  };

  return (
    <div>
      <div className="mb-2 flex flex-wrap items-center gap-3 text-muted-foreground">
        <Input
          type="search"
          placeholder="过滤日志…"
          value={logFilter || ""}
          onChange={(e) =>
            patchInstState(id, {
              logFilter: e.target.value,
            })
          }
          className="my-0 h-8 w-[220px]"
        />
        <label className="flex cursor-pointer items-center gap-1.5 text-xs">
          <Checkbox
            checked={!!logRegex}
            onCheckedChange={(c) => patchInstState(id, { logRegex: !!c })}
          />
          正则
        </label>
        {filter && (
          <span className="text-xs">{`匹配 ${shown.length} / ${logLines?.length ?? 0}`}</span>
        )}
        <label className="flex cursor-pointer items-center gap-1.5 text-xs">
          <Checkbox
            checked={!!autoscroll}
            onCheckedChange={(c) => patchInstState(id, { autoscroll: !!c })}
          />
          自动滚动
        </label>
        <Button size="sm" onClick={clearLog}>
          清空显示
        </Button>
      </div>
      <div
        ref={viewRef}
        className="h-[60vh] overflow-y-auto border border-border bg-[#0a0d16] p-2.5 text-[#c8d3ea]"
      >
        {shown.map((l) => (
          <div
            key={l.seq}
            className="mono min-h-[1.5em] whitespace-pre-wrap break-all text-xs leading-relaxed hover:bg-primary/5"
          >
            <Highlighted text={l.text} query={filter} regex={regex} />
          </div>
        ))}
      </div>
    </div>
  );
}
