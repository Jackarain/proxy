/** libxproxy 暴露的原生接口 (NAPI). */
export const start: (config: string) => number;
export const stop: () => void;
export const build_version: () => string;
export const min_sdk_version: () => string;
