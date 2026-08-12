import { useEffect, useRef, useState } from "react";
import {
  Dialog,
  DialogContent,
  DialogFooter,
  DialogHeader,
  DialogTitle,
} from "@/components/ui/dialog";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { useDialogs } from "@/store/dialogs";
import { useApp } from "@/store/app";
import { api } from "@/lib/api";
import { showToast } from "@/lib/toast";
import { refreshInstances } from "@/lib/actions";

// 新建实例弹窗：输入名称，创建后自动选中并切到「配置」页签。
export default function NewInstanceDialog() {
  const open = useDialogs((s) => s.newOpen);
  const closeNew = useDialogs((s) => s.closeNew);
  const [name, setName] = useState("");
  const [busy, setBusy] = useState(false);
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    if (!open) return;
    setName("");
    const id = requestAnimationFrame(() => inputRef.current?.focus());
    return () => cancelAnimationFrame(id);
  }, [open]);

  const create = async () => {
    const n = name.trim();
    if (!n) {
      showToast("请输入实例名称", "warn");
      return;
    }
    setBusy(true);
    try {
      const created = await api<{ id: string; name: string }>("/api/instances", {
        method: "POST",
        body: JSON.stringify({ name: n }),
      });
      closeNew();
      showToast("实例已创建", "ok");
      await refreshInstances();
      const st = useApp.getState();
      st.selectInstance(created.id);
      st.setActiveTab(created.id, "config"); // 切到配置页，方便立即配置
      // 强制重新加载配置表单（新实例用服务端默认值）。
      st.patchInstState(created.id, { configLoaded: false, configValues: null });
      st.bumpTick();
    } catch (e) {
      showToast("创建失败: " + (e as Error).message, "err");
    } finally {
      setBusy(false);
    }
  };

  return (
    <Dialog open={open} onOpenChange={(o) => !o && closeNew()}>
      <DialogContent
        className="w-[400px] max-w-[92vw]"
        onOpenAutoFocus={(e) => e.preventDefault()}
      >
        <DialogHeader>
          <DialogTitle>新建实例</DialogTitle>
        </DialogHeader>
        <div className="flex flex-col gap-1.5">
          <label className="text-sm">实例名称</label>
          <Input
            ref={inputRef}
            placeholder="例如：主代理"
            value={name}
            onChange={(e) => setName(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === "Enter") create();
            }}
          />
        </div>
        <DialogFooter>
          <Button onClick={closeNew} disabled={busy}>
            取消
          </Button>
          <Button variant="primary" onClick={create} disabled={busy}>
            创建
          </Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  );
}
