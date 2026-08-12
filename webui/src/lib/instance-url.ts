import { useApp } from "@/store/app";
import { firstListenPort } from "@/lib/listen";
import { copyText } from "@/lib/copy";
import { api } from "@/lib/api";
import { useDialogs } from "@/store/dialogs";
import { showToast } from "@/lib/toast";
import type { InstanceDetail } from "@/lib/types";

// 一键复制实例连接地址：
// scheme://[用户:密码@]launcher当前主机:实例端口；启用 SSL 证书则为 https。
export async function copyInstanceURL(id: string) {
  let inst: InstanceDetail;
  try {
    inst = await api<InstanceDetail>(`/api/instances/${id}`);
  } catch (e) {
    showToast((e as Error).message, "err");
    return;
  }
  if (useApp.getState().curId !== id) return; // 已切换实例，丢弃过期结果
  const cfg = inst.config || {};

  const port = firstListenPort(cfg.server_listen as string[]);
  if (!port) {
    showToast("实例未配置 TCP 监听端口", "warn");
    return;
  }

  const scheme = cfg.ssl_certificate_dir ? "https" : "http";
  const host = window.location.hostname || "127.0.0.1";

  // 用户：无认证则匿名；多用户时弹出选择。
  const users = ((cfg.auth_users as string[]) || [])
    .map(String)
    .filter((s) => s && s !== "");
  let userEntry = "";
  if (users.length === 1) {
    userEntry = users[0];
  } else if (users.length > 1) {
    const picked = await useDialogs.getState().openSelect(
      "选择要写入地址的用户：",
      users.map((u) => ({ label: u.split(":")[0], value: u }))
    );
    if (picked == null) return;
    userEntry = picked;
  }

  let url: string;
  if (userEntry) {
    const parts = userEntry.split(":");
    const user = parts[0];
    const pass = parts[1] || "";
    url = `${scheme}://${encodeURIComponent(user)}:${encodeURIComponent(pass)}@${host}:${port}`;
  } else {
    url = `${scheme}://${host}:${port}`;
  }

  const ok = await copyText(url);
  showToast(ok ? `已复制地址: ${url}` : `复制失败，请手动复制: ${url}`, ok ? "ok" : "warn");
}
