import { useApp } from "@/store/app";
import { api } from "@/lib/api";
import type { LogData } from "@/lib/types";

// 日志增量加载（见 doc/webui-spec.md §9.3）：
// - 正常模式每次拉全量快照（since=-1），用快照携带的序号精确识别新增行；
//   渲染层用 seq 作 key 由 React 做增量 DOM（移除顶部被挤出的旧行 + 追加
//   新行），保持滚动位置与选中。
// - 清空模式用 since=logClearSeq 增量拉取，只显示清空后产生的新行。
// - 实例重启（gen 变化）时退出清空模式并按全量快照重建。
export async function loadLogs(id: string) {
  const st = useApp.getState().perInst[id];
  if (!st) return;
  const since = st.logClear ? st.logClearSeq : -1;
  let d: LogData;
  try {
    d = await api<LogData>(`/api/instances/${id}/logs?since=${since}`);
  } catch {
    return;
  }
  if (useApp.getState().curId !== id) return; // 竞态防护：已切换实例丢弃
  d.lines = d.lines || [];
  const seqs = d.seqs && d.seqs.length === d.lines.length ? d.seqs : null;

  const st2 = useApp.getState().perInst[id];
  const restart = st2.lastLogGen >= 0 && d.gen !== st2.lastLogGen;

  let logClear = st2.logClear;
  let logClearSeq = st2.logClearSeq;
  let logLines = st2.logLines;

  if (restart) {
    // 实例重启：缓冲重建（gen 变化、序号归零），退出清空模式。
    logClear = false;
    logClearSeq = 0;
    logLines = [];
  }

  if (logClear) {
    // 清空模式：只追加清空后产生的新行，保持清空语义。
    logLines = [
      ...logLines,
      ...d.lines.map((t, i) => ({
        seq: seqs ? seqs[i] : st2.logClearSeq + i + 1,
        text: t,
      })),
    ];
    logClearSeq = d.next;
  } else {
    // 正常模式：全量快照（固定最多 500 行）。
    if (!seqs || st2.lastLogSeq <= 0) {
      logLines = d.lines.map((t, i) => ({ seq: seqs ? seqs[i] : i, text: t }));
    } else {
      logLines = d.lines.map((t, i) => ({ seq: seqs[i], text: t }));
    }
  }

  // 内容未变化时保持原引用，避免每 2 秒全量重渲染日志视图。
  const same =
    logLines.length === st2.logLines.length &&
    logLines.every(
      (l, i) => l.seq === st2.logLines[i].seq && l.text === st2.logLines[i].text
    );
  useApp.getState().patchInstState(id, {
    logLines: same ? st2.logLines : logLines,
    logClear,
    logClearSeq,
    lastLogGen: d.gen,
    lastLogSeq: d.next,
  });
}
