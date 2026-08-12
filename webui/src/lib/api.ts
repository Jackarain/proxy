// REST API 客户端：基础路径 /api，错误响应 {"error":"..."}（非 2xx）。
// WebUI Basic 鉴权时前端用 Authorization: Basic 头即可（浏览器自动处理
// 401 弹窗后由 fetch 携带；如需手动可在此注入）。

export async function api<T = unknown>(
  path: string,
  opts: RequestInit = {}
): Promise<T> {
  const headers = Object.assign(
    { "Content-Type": "application/json" },
    opts.headers || {}
  );
  const res = await fetch(path, Object.assign({}, opts, { headers }));
  let body: any = null;
  try {
    body = await res.json();
  } catch {
    /* non-json */
  }
  if (!res.ok) {
    throw new Error((body && (body.error || body.message)) || `HTTP ${res.status}`);
  }
  return body as T;
}
