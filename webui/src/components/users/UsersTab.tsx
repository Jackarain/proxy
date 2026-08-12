import { useEffect, useState, type FormEvent } from "react";
import { useApp } from "@/store/app";
import { useDialogs } from "@/store/dialogs";
import { api } from "@/lib/api";
import { showToast } from "@/lib/toast";
import { formatSize, parseSize } from "@/lib/format";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import type { InstanceDetail } from "@/lib/types";

interface UserEntry {
  user: string;
  addr: string;
  proxy: string;
}

// 解析 user:value 配置数组（users_rate_limit / users_quota）。
function parsePairs(arr: unknown): Record<string, string> {
  const out: Record<string, string> = {};
  for (const e of (Array.isArray(arr) ? arr : []) as string[]) {
    const p = String(e).split(":");
    if (p.length >= 2) out[p[0]] = p[1];
  }
  return out;
}

export default function UsersTab({ id, active }: { id: string; active: boolean }) {
  const tick = useApp((s) => s.tick);
  const patchInstState = useApp((s) => s.patchInstState);
  const userRates = useApp((s) => s.perInst[id]?.userRates);
  const userQuotas = useApp((s) => s.perInst[id]?.userQuotas);
  const openPrompt = useDialogs((s) => s.openPrompt);

  const [users, setUsers] = useState<UserEntry[]>([]);
  const [reload, setReload] = useState(0);

  // 表单
  const [uUser, setUUser] = useState("");
  const [uPass, setUPass] = useState("");
  const [uAddr, setUAddr] = useState("");
  const [uProxy, setUProxy] = useState("");

  // 轮询刷新用户列表（仅活跃页签）。
  useEffect(() => {
    if (!active) return;
    let cancelled = false;
    (async () => {
      try {
        const inst = await api<InstanceDetail>(`/api/instances/${id}`);
        if (cancelled || useApp.getState().curId !== id) return;
        const cfg = inst.config || {};
        const list = ((cfg.auth_users as string[]) || []).map(String);
        const rates = parsePairs(cfg.users_rate_limit);
        const quotas = parsePairs(cfg.users_quota);
        patchInstState(id, { userRates: rates, userQuotas: quotas });
        setUsers(
          list.map((entry) => {
            const parts = String(entry).split(":");
            return { user: parts[0], addr: parts[2] || "", proxy: parts[3] || "" };
          })
        );
      } catch {
        /* 静默 */
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [active, id, tick, reload, patchInstState]);

  const addUser = async (e: FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    const user = uUser.trim();
    const pass = uPass;
    if (!user || !pass) {
      showToast("用户名和密码不能为空", "warn");
      return;
    }
    try {
      await api(`/api/instances/${id}/users`, {
        method: "POST",
        body: JSON.stringify({
          user,
          password: pass,
          addr: uAddr.trim(),
          proxy_url: uProxy.trim(),
        }),
      });
      setUUser("");
      setUPass("");
      setUAddr("");
      setUProxy("");
      showToast(`用户 ${user} 已添加`, "ok");
      setReload((n) => n + 1);
    } catch (err) {
      showToast((err as Error).message, "err");
    }
  };

  // 每行操作：改密码 / 限速 / 限额 / 删除。
  const act = async (user: string, action: string) => {
    try {
      if (action === "del") {
        if (!window.confirm(`确认删除用户 ${user} ？`)) return;
        await api(`/api/instances/${id}/users/${encodeURIComponent(user)}`, {
          method: "DELETE",
        });
        showToast(`用户 ${user} 已删除`, "ok");
      } else if (action === "pass") {
        const p = await openPrompt(`输入用户 ${user} 的新密码：`, {
          password: true,
        });
        if (p == null) return;
        if (p === "") {
          showToast("密码不能为空", "warn");
          return;
        }
        await api(`/api/instances/${id}/users/${encodeURIComponent(user)}`, {
          method: "PUT",
          body: JSON.stringify({ password: p }),
        });
        showToast(`用户 ${user} 密码已修改`, "ok");
      } else if (action === "rate") {
        const cur = userRates?.[user]
          ? formatSize(parseSize(userRates[user]))
          : "";
        const r = await openPrompt(
          `输入用户 ${user} 的限速（字节/秒，支持单位如 2.5M、10.8G；0 或留空取消）：`,
          { value: cur }
        );
        if (r == null) return;
        const rate = r.trim() === "" ? 0 : parseSize(r);
        if (isNaN(rate) || rate < 0) {
          showToast("限速格式无效，如 2.5M、10.8G", "warn");
          return;
        }
        await api(`/api/instances/${id}/users/${encodeURIComponent(user)}/rate`, {
          method: "PUT",
          body: JSON.stringify({ rate }),
        });
        showToast(
          rate > 0
            ? `用户 ${user} 限速已设置为 ${formatSize(rate)}/s`
            : `用户 ${user} 限速已取消`,
          "ok"
        );
      } else if (action === "quota") {
        const cur = userQuotas?.[user]
          ? formatSize(parseSize(userQuotas[user]))
          : "";
        const q = await openPrompt(
          `输入用户 ${user} 的总流量配额（上行+下行，支持单位，如 2.5G、10.8M；0 或留空取消限制）：`,
          { value: cur }
        );
        if (q == null) return;
        const quota = q.trim() === "" ? 0 : parseSize(q);
        if (isNaN(quota) || quota < 0) {
          showToast("配额格式无效，如 2.5G、10.8M", "warn");
          return;
        }
        await api(`/api/instances/${id}/users/${encodeURIComponent(user)}/quota`, {
          method: "PUT",
          body: JSON.stringify({ quota }),
        });
        showToast(
          quota > 0
            ? `用户 ${user} 流量配额已设置为 ${formatSize(quota)}`
            : `用户 ${user} 流量配额已取消`,
          "ok"
        );
      }
      setReload((n) => n + 1);
    } catch (err) {
      showToast((err as Error).message, "err");
    }
  };

  return (
    <div>
      <form onSubmit={addUser} className="mb-3 flex flex-wrap gap-2">
        <Input
          placeholder="用户名"
          value={uUser}
          onChange={(e) => setUUser(e.target.value)}
          className="min-w-[120px] flex-1"
        />
        <Input
          type="password"
          placeholder="密码"
          value={uPass}
          onChange={(e) => setUPass(e.target.value)}
          className="min-w-[120px] flex-1"
        />
        <Input
          placeholder="出口 IP（可选）"
          value={uAddr}
          onChange={(e) => setUAddr(e.target.value)}
          className="min-w-[120px] flex-1"
        />
        <Input
          placeholder="专属上游 URL（可选）"
          value={uProxy}
          onChange={(e) => setUProxy(e.target.value)}
          className="min-w-[120px] flex-1"
        />
        <Button variant="primary" type="submit">
          添加用户
        </Button>
      </form>

      <table className="w-full border-separate border-spacing-0 border border-border bg-card">
        <thead>
          <tr>
            {["用户", "出口地址", "专属上游", "操作"].map((h, i) => (
              <th
                key={h}
                className={`whitespace-nowrap bg-secondary px-3 py-2.5 text-left text-xs font-semibold uppercase tracking-wide text-muted-foreground ${
                  i === 3 ? "w-72" : ""
                }`}
              >
                {h}
              </th>
            ))}
          </tr>
        </thead>
        <tbody>
          {!users.length && (
            <tr>
              <td colSpan={4} className="px-3 py-7 text-center text-muted-foreground">
                未配置认证用户（匿名访问）
              </td>
            </tr>
          )}
          {users.map((u) => (
            <tr key={u.user} className="border-t border-border hover:bg-primary/5">
              <td className="px-3 py-2">
                <b>{u.user}</b>
              </td>
              <td className="px-3 py-2">{u.addr || "-"}</td>
              <td className="px-3 py-2">{u.proxy || "-"}</td>
              <td className="px-3 py-2">
                <div className="flex flex-wrap gap-1.5">
                  <Button
                    size="sm"
                    onClick={() => act(u.user, "pass")}
                  >
                    改密码
                  </Button>
                  <Button size="sm" onClick={() => act(u.user, "rate")}>
                    {userRates?.[u.user]
                      ? `限速 ${formatSize(parseSize(userRates[u.user]))}`
                      : "限速"}
                  </Button>
                  <Button size="sm" onClick={() => act(u.user, "quota")}>
                    {userQuotas?.[u.user]
                      ? `限额 ${formatSize(parseSize(userQuotas[u.user]))}`
                      : "限额"}
                  </Button>
                  <Button
                    size="sm"
                    variant="danger"
                    onClick={() => act(u.user, "del")}
                  >
                    删除
                  </Button>
                </div>
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
