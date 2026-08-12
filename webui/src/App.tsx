import { useEffect } from "react";
import { Toaster } from "sonner";
import { useApp } from "@/store/app";
import { api } from "@/lib/api";
import type { InstanceSummary, OptionDef } from "@/lib/types";
import Header from "@/components/layout/Header";
import InstanceList from "@/components/layout/InstanceList";
import DetailView from "@/components/layout/DetailView";
import PromptDialog from "@/components/dialogs/PromptDialog";
import SelectDialog from "@/components/dialogs/SelectDialog";
import NewInstanceDialog from "@/components/dialogs/NewInstanceDialog";

export default function App() {
  // 全局一次性数据：launcher 版本 + 选项注册表。
  useEffect(() => {
    api<{ version: string }>("/api/version")
      .then((v) => useApp.getState().setVersion(v?.version || ""))
      .catch(() => {});
    api<OptionDef[]>("/api/options")
      .then((o) => useApp.getState().setOptions(o || []))
      .catch(() => {});
  }, []);

  // 2 秒轮询：刷新实例列表 → bump tick 让当前页签刷新数据。
  useEffect(() => {
    const tick = async () => {
      if (useApp.getState().paused) return; // 暂停刷新
      try {
        const list = await api<InstanceSummary[]>("/api/instances");
        const st = useApp.getState();
        if (st.paused) return; // 轮询期间被暂停，丢弃
        st.setInstances(list);
        st.bumpTick();
      } catch {
        /* 网络错误静默，等下一轮 */
      }
    };
    tick();
    const iv = setInterval(tick, 2000);
    return () => clearInterval(iv);
  }, []);

  return (
    <div className="flex h-full flex-col">
      <Header />
      <main className="flex min-h-0 flex-1">
        <InstanceList />
        <DetailView />
      </main>
      <PromptDialog />
      <SelectDialog />
      <NewInstanceDialog />
      <Toaster
        position="top-center"
        duration={3500}
        richColors
        theme="dark"
        toastOptions={{
          style: { borderRadius: 0, fontSize: 13 },
        }}
      />
    </div>
  );
}
