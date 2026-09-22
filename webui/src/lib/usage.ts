// 用户累计流量本地缓存（IndexedDB）。
//
// 实例上报的 users[].rx_bytes/tx_bytes 是"本次运行期会话级"累计值，实例重启
// 后归零；launcher 侧仅持久化含续接基线的总额（usage_total），无法区分上行/
// 下行。这里在 WebUI 侧按用户累计上行（上传）与下行（下载）并写入 IndexedDB，
// 使「累计上传/累计下载」跨实例重启、页面刷新与 launcher 重启延续，并支持按
// 用户或整体重置。

const DB_NAME = "cproxy-usage";
const DB_VERSION = 1;
const STORE = "usage";

export interface UsageTotals {
  rx: number;
  tx: number;
}

export interface UsageSample {
  user: string;
  rx: number;
  tx: number;
}

// 单条缓存记录：rx/tx 为累计值；lastRx/lastTx 为最近一次上报的会话级原始值，
// 用于折算增量（原始值回退视为实例重启，从当前值重新累计）。
interface UsageRecord {
  instance: string;
  user: string;
  rx: number;
  tx: number;
  lastRx: number;
  lastTx: number;
}

// 内存镜像：避免每 2 秒轮询都异步读库，也避免并发轮询之间的读写竞态。
const memCache = new Map<string, Map<string, UsageRecord>>();
const loadTasks = new Map<string, Promise<Map<string, UsageRecord>>>();

let dbPromise: Promise<IDBDatabase | null> | null = null;

function openDb(): Promise<IDBDatabase | null> {
  if (dbPromise) return dbPromise;
  dbPromise = new Promise((resolve) => {
    if (typeof indexedDB === "undefined") {
      resolve(null);
      return;
    }
    let req: IDBOpenDBRequest;
    try {
      req = indexedDB.open(DB_NAME, DB_VERSION);
    } catch {
      resolve(null);
      return;
    }
    req.onupgradeneeded = () => {
      const db = req.result;
      if (!db.objectStoreNames.contains(STORE)) {
        const store = db.createObjectStore(STORE, {
          keyPath: ["instance", "user"],
        });
        store.createIndex("instance", "instance", { unique: false });
      }
    };
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => resolve(null);
    req.onblocked = () => resolve(null);
  });
  return dbPromise;
}

function onRequest<T>(r: IDBRequest<T>): Promise<T> {
  return new Promise((resolve, reject) => {
    r.onsuccess = () => resolve(r.result);
    r.onerror = () => reject(r.error);
  });
}

function onTransactionDone(t: IDBTransaction): Promise<void> {
  return new Promise((resolve, reject) => {
    t.oncomplete = () => resolve();
    t.onerror = () => reject(t.error);
    t.onabort = () => reject(t.error);
  });
}

async function loadFromDb(instance: string): Promise<Map<string, UsageRecord>> {
  const map = new Map<string, UsageRecord>();
  const db = await openDb();
  if (db) {
    try {
      const store = db.transaction(STORE, "readonly").objectStore(STORE);
      const rows = (await onRequest(
        store.index("instance").getAll(instance)
      )) as UsageRecord[];
      for (const row of rows) map.set(row.user, row);
    } catch {
      /* 读库失败降级为内存缓存 */
    }
  }
  memCache.set(instance, map);
  loadTasks.delete(instance);
  return map;
}

// 载入某实例的缓存（含并发去重）：返回可继续变异的同一份内存镜像。
function ensureLoaded(instance: string): Promise<Map<string, UsageRecord>> {
  const cached = memCache.get(instance);
  if (cached) return Promise.resolve(cached);
  let task = loadTasks.get(instance);
  if (!task) {
    task = loadFromDb(instance);
    loadTasks.set(instance, task);
  }
  return task;
}

async function persist(records: UsageRecord[]): Promise<void> {
  if (!records.length) return;
  const db = await openDb();
  if (!db) return;
  try {
    const t = db.transaction(STORE, "readwrite");
    const store = t.objectStore(STORE);
    for (const rec of records) store.put(rec);
    await onTransactionDone(t);
  } catch {
    /* 写库失败仅影响持久化，内存缓存仍可用 */
  }
}

function toTotals(map: Map<string, UsageRecord>): Record<string, UsageTotals> {
  const out: Record<string, UsageTotals> = {};
  for (const [user, rec] of map) out[user] = { rx: rec.rx, tx: rec.tx };
  return out;
}

function toNonNegativeInt(v: number): number {
  const n = Math.floor(Number(v));
  return Number.isFinite(n) && n > 0 ? n : 0;
}

// 读取某实例已缓存的累计值（不推进会话增量）。
export async function loadUsage(
  instance: string
): Promise<Record<string, UsageTotals>> {
  return toTotals(await ensureLoaded(instance));
}

// 用最新一份状态报告的会话级原始值推进累计并持久化，返回全部用户的累计值。
export async function accumulateUsage(
  instance: string,
  samples: UsageSample[]
): Promise<Record<string, UsageTotals>> {
  const map = await ensureLoaded(instance);
  const dirty: UsageRecord[] = [];
  for (const s of samples) {
    const rawRx = toNonNegativeInt(s.rx);
    const rawTx = toNonNegativeInt(s.tx);
    let rec = map.get(s.user);
    const isNew = !rec;
    if (!rec) {
      rec = { instance, user: s.user, rx: 0, tx: 0, lastRx: 0, lastTx: 0 };
      map.set(s.user, rec);
    }
    // 会话级原始值单调递增；回退说明实例重启，增量从当前值起算。
    const deltaRx = rawRx >= rec.lastRx ? rawRx - rec.lastRx : rawRx;
    const deltaTx = rawTx >= rec.lastTx ? rawTx - rec.lastTx : rawTx;
    rec.rx += deltaRx;
    rec.tx += deltaTx;
    rec.lastRx = rawRx;
    rec.lastTx = rawTx;
    if (isNew || deltaRx || deltaTx) dirty.push(rec);
  }
  await persist(dirty);
  return toTotals(map);
}

// 重置指定用户的累计上传/下载（保留会话基线，后续增量继续累加）。
export async function resetUserUsage(
  instance: string,
  user: string
): Promise<Record<string, UsageTotals>> {
  const map = await ensureLoaded(instance);
  const rec = map.get(user);
  if (rec && (rec.rx || rec.tx)) {
    rec.rx = 0;
    rec.tx = 0;
    await persist([rec]);
  }
  return toTotals(map);
}

// 重置该实例所有用户的累计上传/下载。
export async function resetAllUsage(
  instance: string
): Promise<Record<string, UsageTotals>> {
  const map = await ensureLoaded(instance);
  const dirty: UsageRecord[] = [];
  for (const rec of map.values()) {
    if (rec.rx || rec.tx) {
      rec.rx = 0;
      rec.tx = 0;
      dirty.push(rec);
    }
  }
  await persist(dirty);
  return toTotals(map);
}
