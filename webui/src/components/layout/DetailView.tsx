import { useApp, type TabId } from "@/store/app";
import DetailHeader from "./DetailHeader";
import ApplyResultBar from "./ApplyResultBar";
import StatusTab from "@/components/status/StatusTab";
import UsersTab from "@/components/users/UsersTab";
import ConfigTab from "@/components/config/ConfigTab";
import LogsTab from "@/components/logs/LogsTab";

const TABS: { id: TabId; label: string }[] = [
  { id: "status", label: "状态" },
  { id: "users", label: "用户" },
  { id: "config", label: "配置" },
  { id: "logs", label: "日志" },
];

export default function DetailView() {
  const curId = useApp((s) => s.curId);
  const tab = useApp((s) => (curId ? s.perInst[curId]?.activeTab : undefined));
  const setActiveTab = useApp((s) => s.setActiveTab);

  // 未选中实例：占位文案。
  if (!curId || !tab) {
    return (
      <section className="flex-1 p-4">
        <div className="mt-[20%] text-center text-muted-foreground">
          <p>选择一个实例查看详情，或新建一个实例。</p>
        </div>
      </section>
    );
  }

  return (
    <section className="flex-1 overflow-y-auto p-4">
      <DetailHeader id={curId} />
      <ApplyResultBar id={curId} />
      <nav className="mb-3 mt-3 flex gap-1 border-b border-border">
        {TABS.map((t) => (
          <button
            key={t.id}
            className={`cursor-pointer border-b-2 bg-transparent px-4 py-2 text-sm transition-colors hover:text-foreground ${
              tab === t.id
                ? "border-primary text-primary"
                : "border-transparent text-muted-foreground"
            }`}
            onClick={() => setActiveTab(curId, t.id)}
          >
            {t.label}
          </button>
        ))}
      </nav>
      {/* 四个页签全部保持挂载（隐藏不活跃），保留各自的 DOM/焦点状态；
          仅活跃页签在轮询时刷新数据。 */}
      <div hidden={tab !== "status"}>
        <StatusTab id={curId} active={tab === "status"} />
      </div>
      <div hidden={tab !== "users"}>
        <UsersTab id={curId} active={tab === "users"} />
      </div>
      <div hidden={tab !== "config"}>
        <ConfigTab id={curId} active={tab === "config"} />
      </div>
      <div hidden={tab !== "logs"}>
        <LogsTab id={curId} active={tab === "logs"} />
      </div>
    </section>
  );
}
