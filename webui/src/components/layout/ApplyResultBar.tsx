import { useEffect } from "react";
import { useApp } from "@/store/app";
import { api } from "@/lib/api";
import { showToast } from "@/lib/toast";
import { refreshInstances } from "@/lib/actions";
import { Button } from "@/components/ui/button";

// 配置应用结果条：已生效 / 需重启生效 / 失败；8 秒后自动隐藏。
export default function ApplyResultBar({ id }: { id: string }) {
  const result = useApp((s) => s.perInst[id]?.applyResult);

  useEffect(() => {
    if (!result) return;
    const t = setTimeout(() => {
      useApp.getState().patchInstState(id, { applyResult: null });
    }, 8000);
    return () => clearTimeout(t);
  }, [result, id]);

  if (!result) return null;

  const restart = async () => {
    try {
      await api(`/api/instances/${id}/restart`, { method: "POST" });
      showToast("已发送重启命令", "ok");
      await refreshInstances();
    } catch (e) {
      showToast((e as Error).message, "err");
    }
  };

  return (
    <div
      className={`my-2 flex flex-wrap items-center border px-3 py-2 text-[13px] ${
        result.kind === "err"
          ? "border-err bg-err/10 text-err"
          : "border-ok bg-ok/10 text-ok"
      }`}
    >
      <span>{result.text}</span>
      {result.needsRestart.length > 0 && (
        <Button size="sm" className="ml-2.5" onClick={restart}>
          重启实例生效
        </Button>
      )}
    </div>
  );
}
