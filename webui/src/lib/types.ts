// 后端 REST API 数据契约（见 doc/webui-spec.md §12）。

export interface InstanceSummary {
  id: string;
  name: string;
  state: string;
  online: boolean;
  pid?: number;
  autostart: boolean;
  listen: string[];
  active: number;
  rx_rate_bps: number;
  tx_rate_bps: number;
}

export interface OptionDef {
  name: string;
  kind: "bool" | "int" | "string" | "stringlist";
  category: string;
  help: string;
  hint?: string;
  default?: unknown;
  restart_only?: boolean;
  common?: boolean;
}

export interface ConnectionInfo {
  id: number;
  client_ip: string;
  target?: string;
  region?: string[];
  proto: string;
  elapsed: number;
  rx_bytes: number;
  tx_bytes: number;
}

export interface UserStat {
  user: string;
  rx_bytes: number;
  tx_bytes: number;
  active_connections: number;
  conn_total: number;
  quota: number;
  usage_total: number;
  connections?: ConnectionInfo[];
}

export interface StatusReport {
  ts: number;
  uptime: number;
  active_connections: number;
  conn_total: number;
  global?: { rx_bytes: number; tx_bytes: number };
  users?: UserStat[];
  rates?: { rx_rate_bps: number; tx_rate_bps: number };
  user_rates?: Record<string, { rx_rate_bps: number; tx_rate_bps: number }>;
}

export interface StatusData {
  online: boolean;
  state: string;
  pid?: number;
  last_seen?: string;
  report?: StatusReport;
}

export interface InstanceDetail {
  id: string;
  name: string;
  state: string;
  online: boolean;
  pid?: number;
  autostart: boolean;
  config: Record<string, unknown>;
  created_at?: string;
}

export interface LogLine {
  seq: number;
  text: string;
}

export interface LogData {
  lines: string[];
  seqs?: number[];
  next: number;
  gen: number;
}

export interface ApplyResult {
  applied?: string[];
  needs_restart?: string[];
  errors?: Record<string, string>;
}

export interface UserState {
  auth_users: string[];
  users_rate_limit: string[];
  users_quota: string[];
}
