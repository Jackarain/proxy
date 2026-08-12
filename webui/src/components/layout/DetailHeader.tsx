import { useApp } from "@/store/app";
import { useDialogs } from "@/store/dialogs";
import { api } from "@/lib/api";
import { showToast } from "@/lib/toast";
import { refreshInstances } from "@/lib/actions";
import { copyInstanceURL } from "@/lib/instance-url";
import { fmtState } from "@/lib/format";
import { Button } from "@/components/ui/button";
import { Checkbox } from "@/components/ui/checkbox";
import { Badge } from "@/components/ui/badge";

export default function DetailHeader({ id }: { id: string }) {
  const inst = useApp((s) => s.listCache[id]);
  const paused = useApp((s) => s.paused);
  const setPaused = useApp((s) => s.setPaused);
  const openPrompt = useDialogs((s) => s.openPrompt);
  const clearSelection = useApp((s) => s.clearSelection);

  if (!inst) return null;

  const active = inst.state === "running" || inst.state === "starting";

  const action = async (act: string) => {
    try {
      await api(`/api/instances/${id}/${act}`, { method: "POST" });
      const msgs: Record<string, string> = {
        start: "已发送启动命令",
        stop: "已发送停止命令",
        restart: "已发送重启命令",
      };
      showToast(msgs[act] || act, "ok");
      await refreshInstances();
    } catch (e) {
      showToast((e as Error).message, "err");
    }
  };

  const rename = async () => {
    const name = await openPrompt("输入实例新名称：", { value: inst.name || "" });
    if (name == null) return;
    const n = name.trim();
    if (!n) {
      showToast("名称不能为空", "warn");
      return;
    }
    try {
      await api(`/api/instances/${id}`, {
        method: "PUT",
        body: JSON.stringify({ name: n }),
      });
      showToast("实例已重命名", "ok");
      await refreshInstances();
    } catch (e) {
      showToast((e as Error).message, "err");
    }
  };

  const remove = async () => {
    // 防误删：必须输入实例完整名称或 ID 确认（此操作不可恢复）。
    const entered = await openPrompt(
      `删除实例「${inst.name}」？此操作不可恢复。\n请输入实例完整名称或 ID 以确认删除：`,
      { placeholder: "完整名称或 ID" }
    );
    if (entered == null) return;
    const input = String(entered).trim();
    if (input !== inst.name && input !== inst.id) {
      showToast("输入的实例名称或 ID 不匹配，已取消删除", "warn");
      return;
    }
    try {
      await api(`/api/instances/${id}`, { method: "DELETE" });
      useApp.getState().resetInstState(id); // 清理该实例的独立状态
      clearSelection();
      await refreshInstances(); // 刷新列表并自动选中剩余第一个
      showToast("实例已删除", "ok");
    } catch (e) {
      showToast((e as Error).message, "err");
    }
  };

  const autostartChange = async (on: boolean) => {
    try {
      await api(`/api/instances/${id}`, {
        method: "PUT",
        body: JSON.stringify({ autostart: on }),
      });
      showToast(
        on ? "已开启自动启动（launcher 启动时自动拉起）" : "已关闭自动启动",
        "ok"
      );
      await refreshInstances();
    } catch (e) {
      showToast((e as Error).message, "err");
    }
  };

  return (
    <>
      <div className="flex flex-wrap items-center justify-between gap-2">
        <div className="flex items-center gap-2.5">
          <h2 className="text-lg font-semibold">
            {inst.name} ({inst.id})
          </h2>
          <Button
            size="sm"
            title="复制实例连接地址（scheme://用户:密码@主机:端口）"
            onClick={() => copyInstanceURL(id)}
          >
            复制地址
          </Button>
        </div>
        <div className="flex flex-wrap gap-2">
          <Button disabled={active} onClick={() => action("start")}>
            启动
          </Button>
          <Button disabled={!active} onClick={() => action("stop")}>
            停止
          </Button>
          <Button disabled={!active} onClick={() => action("restart")}>
            重启
          </Button>
          <Button onClick={rename}>重命名</Button>
          <Button variant="danger" onClick={remove}>
            删除
          </Button>
        </div>
      </div>
      <div className="my-2 flex flex-wrap items-center gap-2">
        <Badge
          variant={
            inst.online ? "ok" : inst.state === "starting" ? "warn" : "default"
          }
        >
          状态: {fmtState(inst.state)}
        </Badge>
        {inst.pid ? <Badge variant="default">PID {inst.pid}</Badge> : null}
        <Badge variant={inst.online ? "ok" : "default"}>
          {inst.online ? "控制通道在线" : "控制通道离线"}
        </Badge>
        <label className="flex cursor-pointer items-center gap-1.5 text-xs text-muted-foreground">
          <Checkbox
            checked={!!inst.autostart}
            onCheckedChange={(c) => autostartChange(!!c)}
          />
          自动启动
        </label>
        <label className="flex cursor-pointer items-center gap-1.5 text-xs text-muted-foreground">
          <Checkbox checked={paused} onCheckedChange={(c) => setPaused(!!c)} />
          暂停刷新
        </label>
      </div>
    </>
  );
}
