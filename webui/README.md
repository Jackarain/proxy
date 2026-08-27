# cproxy Launcher WebUI（React + Vite + Tailwind + shadcn/ui）

基于 React 19 + Vite + TypeScript + Tailwind CSS v4 + shadcn/ui + zustand 实现的
launcher WebUI：实例管理、状态监控、用户管理、配置热改与日志查看。

## 环境要求

- **Node.js 18+**（推荐 20 LTS 或更高，本仓库以 v26 验证）
- **npm**（依赖由 `package-lock.json` 锁定；若改用 yarn 请删除该锁文件后重新生成 `yarn.lock`，两者勿共存）
- 安装依赖与构建需**联网**访问 npm registry

## 目录结构

```
web/
├── index.html            # Vite 入口
├── vite.config.ts        # 构建输出到 apps/launcher/webui（launcher 编译期内嵌）
├── src/
│   ├── main.tsx          # 入口
│   ├── App.tsx           # 布局 + 2 秒轮询调度
│   ├── index.css         # Tailwind v4 + 深色主题 CSS 变量
│   ├── lib/              # api 客户端、格式化工具、日志增量、复制地址等
│   ├── store/            # zustand：app（实例/每实例状态/轮询）+ dialogs（弹窗）
│   └── components/
│       ├── ui/           # shadcn/ui 组件（button/input/dialog/checkbox/switch/badge）
│       ├── layout/       # Header / InstanceList / DetailView / DetailHeader
│       ├── status/       # 状态页（摘要条/用户表/连接明细）
│       ├── users/        # 用户页
│       ├── config/       # 配置页（flags 风格）
│       ├── logs/         # 日志页（增量渲染）
│       └── dialogs/      # prompt / select / 新建实例弹窗
```

## 开发

```bash
cd web
npm install

# 1) 先启动后端 launcher（默认 WebUI 端口 18080）：
#    ./bin/launcher --listen 127.0.0.1:18080 --proxy_server ./bin/proxy_server

# 2) 启动 Vite 开发服务器（/api 自动代理到 127.0.0.1:18080，热更新）：
npm run dev        # http://localhost:5173
```

如需改代理目标端口，编辑 `web/vite.config.ts` 的 `server.proxy`。

## 构建

```bash
cd web
npm run build      # 产物输出到 apps/launcher/webui/（含 assets/ 与 index.html）
```

要点：

- `vite.config.ts` 的 `build.outDir` 指向 `../apps/launcher/webui`，即 launcher
  在编译期内嵌 WebUI 资源的目录（由 `embed_webui.cmake` 生成 `webui_embedded.cpp`，
  静态资源直接编入可执行文件），构建后无需手动拷贝；更新 webui 文件后 CMake
  （`CONFIGURE_DEPENDS`）会自动重新收集。
- `base: './'` 相对路径，保证任意子路径可访问。
- 静态资源经 launcher 以 `no-store` 禁用缓存，产物文件名带内容 hash，更新即生效。

## npm 脚本

| 命令 | 说明 |
|---|---|
| `npm run dev` | 启动 Vite 开发服务器（/api 代理到 127.0.0.1:18080，热更新） |
| `npm run build` | TypeScript 类型检查 + 生产构建（输出到 `apps/launcher/webui/`） |
| `npm run preview` | 本地预览构建产物 |
| `npm run typecheck` | 仅 TypeScript 类型检查 |

## 后端 API 契约

类型定义见 `src/lib/types.ts`，前端只对接 REST（`/api/*`），不直接接触 `/rpc` 控制通道：

- `GET /api/version`、`GET /api/options`（选项注册表）
- `GET/POST /api/instances`、`GET/PUT/DELETE /api/instances/:id`
- `POST /api/instances/:id/{start|stop|restart}`
- `GET /api/instances/:id/status`（实时报告）、`GET .../logs?since=N`（日志快照/增量）
- `PUT /api/instances/:id/config`（热改配置，返回 applied / needs_restart / errors）
- `POST/DELETE /api/instances/:id/users...`、`PUT .../users/:name/rate|quota`（用户管理）

## 状态架构

- 全局轮询：App 每 2 秒拉取 `/api/instances` 并 `bumpTick()`；页签组件监听
  `tick` 仅活跃页签刷新数据；「暂停刷新」勾选时跳过。
- 每实例独立状态（`store/app.ts` 的 `perInst`）：当前页签、配置草稿与搜索词、
  日志缓冲/过滤/自动滚动/清空标记、连接展开/排序、用户限速/配额回显等，切换
  实例互不串扰。
- 竞态防护：异步响应返回后校验 `curId` 未变化，否则丢弃过期响应。
- 日志增量渲染：快照序号（seq）作 React key，由 keyed reconciliation 完成
  「移除顶部旧行 + 追加新行」，保持滚动位置与选中；实例重启（gen 变化）时
  重建缓冲。

## 常见问题

- **`npm run dev` 报 /api 连接失败**：launcher 未启动，或监听端口不是 18080
  （修改 `vite.config.ts` 的 `server.proxy.target`）。
- **修改 `vite.config.ts` 后**：需重启 dev server 生效。
- **WebUI 显示旧样式/旧功能**：确认已执行 `npm run build`（`emptyOutDir` 会清空
  旧产物），构建产物带内容 hash 且后端 `no-store`，无需手动强刷。
- **新增 shadcn/ui 组件**：`npx shadcn@latest add <组件名>`，按 `components.json`
  放入 `src/components/ui/`；需保持 `base: './'` 与深色主题变量约定。
