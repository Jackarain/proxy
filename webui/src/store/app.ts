import { create } from "zustand";
import type {
  InstanceSummary,
  OptionDef,
  LogLine,
  ConnectionInfo,
} from "@/lib/types";

export type TabId = "status" | "users" | "config" | "logs";

export interface ConnSort {
  key: string;
  dir: "asc" | "desc";
}

// 每个实例完全独立的状态：页签、配置草稿与搜索、日志缓冲/过滤/自动滚动/
// 清空标记、连接展开/排序、用户限速/配额回显、配置应用结果等全部归属各自
// 实例，切换实例互不串扰、互不覆盖。
export interface InstanceState {
  activeTab: TabId;
  configLoaded: boolean;
  configValues: Record<string, unknown> | null; // 配置表单未保存草稿
  cfgSearch: string;
  // 日志
  lastLogSeq: number;
  lastLogGen: number;
  logLines: LogLine[];
  logClear: boolean;
  logClearSeq: number;
  logFilter: string;
  logRegex: boolean; // 日志过滤是否按正则匹配
  autoscroll: boolean;
  // 状态页
  expanded: Record<string, boolean>;
  connSort: ConnSort | null;
  userConns: Record<string, ConnectionInfo[]>;
  // 用户页
  userRates: Record<string, string>;
  userQuotas: Record<string, string>;
  // 配置应用结果条
  applyResult: { text: string; kind: "ok" | "err"; needsRestart: string[] } | null;
}

export function defaultInstanceState(): InstanceState {
  return {
    activeTab: "status",
    configLoaded: false,
    configValues: null,
    cfgSearch: "",
    lastLogSeq: 0,
    lastLogGen: -1,
    logLines: [],
    logClear: false,
    logClearSeq: 0,
    logFilter: "",
    logRegex: false,
    autoscroll: true,
    expanded: {},
    connSort: null,
    userConns: {},
    userRates: {},
    userQuotas: {},
    applyResult: null,
  };
}

interface AppStore {
  version: string;
  options: OptionDef[];
  instances: InstanceSummary[];
  listCache: Record<string, InstanceSummary>;
  curId: string | null;
  paused: boolean;
  instFilter: string;
  instCollapsed: boolean; // 实例列表侧栏是否收起
  tick: number; // 轮询节拍：每次轮询 +1，页签数据据此刷新
  perInst: Record<string, InstanceState>;

  setVersion: (v: string) => void;
  setOptions: (o: OptionDef[]) => void;
  setInstances: (list: InstanceSummary[]) => void;
  selectInstance: (id: string) => void;
  clearSelection: () => void;
  setPaused: (p: boolean) => void;
  setInstFilter: (f: string) => void;
  toggleInstCollapsed: () => void;
  bumpTick: () => void;
  setActiveTab: (id: string, tab: TabId) => void;
  patchInstState: (id: string, patch: Partial<InstanceState>) => void;
  resetInstState: (id: string) => void;
  setConfigValue: (id: string, name: string, value: unknown) => void;
}

export const useApp = create<AppStore>((set, get) => ({
  version: "",
  options: [],
  instances: [],
  listCache: {},
  curId: null,
  paused: false,
  instFilter: "",
  instCollapsed: false,
  tick: 0,
  perInst: {},

  setVersion: (v) => set({ version: v }),
  setOptions: (o) => set({ options: o }),

  setInstances: (list) => {
    const listCache: Record<string, InstanceSummary> = {};
    for (const inst of list) listCache[inst.id] = inst;

    // 清理已删除实例的独立状态，避免无限增长；为现存实例确保状态存在。
    const perInst: Record<string, InstanceState> = {};
    for (const inst of list) {
      perInst[inst.id] = get().perInst[inst.id] || defaultInstanceState();
    }

    let curId = get().curId;
    // 选中项已不存在（被删除）时自动回落第一个；否则保持。
    if (!listCache[curId || ""] && list.length) curId = list[0].id;

    set({ instances: list, listCache, perInst, curId });
  },

  selectInstance: (id) => {
    const { curId } = get();
    if (curId === id) {
      // 点击已选中实例：立即刷新当前页签。
      set({ tick: get().tick + 1 });
      return;
    }
    set({ curId: id });
  },

  clearSelection: () => set({ curId: null }),

  setPaused: (p) => set({ paused: p }),

  setInstFilter: (f) => set({ instFilter: f }),

  toggleInstCollapsed: () =>
    set({ instCollapsed: !get().instCollapsed }),

  bumpTick: () => set({ tick: get().tick + 1 }),

  setActiveTab: (id, tab) => {
    const st = get().perInst[id];
    if (!st) return;
    set({ perInst: { ...get().perInst, [id]: { ...st, activeTab: tab } } });
  },

  patchInstState: (id, patch) => {
    const st = get().perInst[id];
    if (!st) return;
    set({ perInst: { ...get().perInst, [id]: { ...st, ...patch } } });
  },

  resetInstState: (id) => {
    set({ perInst: { ...get().perInst, [id]: defaultInstanceState() } });
  },

  setConfigValue: (id, name, value) => {
    const st = get().perInst[id];
    if (!st) return;
    const values = { ...(st.configValues || {}) };
    values[name] = value;
    set({
      perInst: { ...get().perInst, [id]: { ...st, configValues: values } },
    });
  },
}));
