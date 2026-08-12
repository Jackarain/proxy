import { create } from "zustand";

export interface PromptOptions {
  password?: boolean;
  value?: string;
  placeholder?: string;
}

export interface SelectOption {
  label: string;
  value: string;
}

interface DialogState {
  prompt: {
    title: string;
    opts: PromptOptions;
    resolve: (v: string | null) => void;
  } | null;
  select: {
    title: string;
    options: SelectOption[];
    resolve: (v: string | null) => void;
  } | null;
  newOpen: boolean;
  openPrompt: (title: string, opts?: PromptOptions) => Promise<string | null>;
  openSelect: (title: string, options: SelectOption[]) => Promise<string | null>;
  closePrompt: (v: string | null) => void;
  closeSelect: (v: string | null) => void;
  openNew: () => void;
  closeNew: () => void;
}

export const useDialogs = create<DialogState>((set) => ({
  prompt: null,
  select: null,
  newOpen: false,
  openPrompt: (title, opts = {}) =>
    new Promise((resolve) => set({ prompt: { title, opts, resolve } })),
  openSelect: (title, options) =>
    new Promise((resolve) => set({ select: { title, options, resolve } })),
  closePrompt: (v) => {
    const p = useDialogs.getState().prompt;
    set({ prompt: null });
    if (p) p.resolve(v);
  },
  closeSelect: (v) => {
    const s = useDialogs.getState().select;
    set({ select: null });
    if (s) s.resolve(v);
  },
  openNew: () => set({ newOpen: true }),
  closeNew: () => set({ newOpen: false }),
}));
