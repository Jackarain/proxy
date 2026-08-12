import { api } from "@/lib/api";
import { useApp } from "@/store/app";
import type { InstanceSummary } from "@/lib/types";

// 刷新实例列表并立即刷新当前页签（bump tick）。
export async function refreshInstances() {
  try {
    const list = await api<InstanceSummary[]>("/api/instances");
    const st = useApp.getState();
    st.setInstances(list);
    st.bumpTick();
  } catch {
    /* 网络错误静默，等下一轮轮询 */
  }
}
