# apps/launcher — proxy_server 实例管理器

`launcher` 是 proxy server 的管理端程序：以 WebUI + REST API 集中托管多个 `proxy_server`
进程，负责实例的创建、启动、停止、重启、配置热更新、用户管理、状态与日志采集以及
配置持久化。它自身不承载代理流量，数据面完全由被拉起的 `proxy_server` 进程处理。

``` text

                +-----------------------------+       ws(s) JSON-RPC
  browser --->  |  launcher :18080            | <-----------------------+
                |  （REST API + 内嵌 WebUI）  |                         |
                +--------------+--------------+                         |
                               |  spawn / 信号 / pid 文件               |
                               v                                        |
                +-----------------------------+                         |
                |  proxy_server 实例 1 .. N   +-------------------------+
                +-----------------------------+
```

launcher 启动 `proxy_server` 时通过 `--launcher ws://host:port/rpc?instance=<id>&token=<token>`
把控制通道地址传给实例，实例主动连回 launcher 并保持长连接；launcher 的所有运行期操作与
状态采集都走这条 JSON-RPC 控制通道。

## 主要功能

- **多实例管理**：创建/删除/启动/停止/重启，支持实例重命名与开机自启（autostart）。
- **配置热更新**：经 `set_config` 下发热改，返回 `applied` / `needs_restart` / `errors`。
  需重启项由 `proxy_server` 判定（`stdio`、`transparent`、`ssl_ciphers`、
  `ssl_prefer_server_ciphers`），其余选项立即生效；WebUI 另按选项表的 `restart_only`
  标记（含 `tun` / `tun_name` / `tun_mtu` / `proxy_domains` / `proxy_cidr`）作静态提示。
- **用户管理**：新增/删除用户、修改密码、设置限速、流量配额与连接数限制，配置与运行期双向同步。
- **用量续接与统计**：按用户持久化累计上传/下载（实例上报的会话级计数重启归零，launcher 折算增量后累积）与含续接基线的配额总量；实例重启后经 `set_user_usage` 续接配额计数。WebUI 可直接重置单个用户或全部用户的累计上传/下载（配额总量不变）。
- **状态监控**：实例经控制通道上报状态，WebUI 展示在线状态、PID、连接数、上下行速率与连接明细。
- **日志采集**：每个实例保留最近 2000 行（环形缓冲），支持按序号增量拉取。
- **崩溃自动重启**：实例意外退出后自动拉起；60 秒内连续崩溃超过 3 次则停止重启并告警。
- **持久化**：`instances.json` 保存实例配置、autostart、控制通道 token 与用户用量。
- **内嵌 WebUI**：静态资源由 `embed_webui.cmake` 在构建期内嵌进可执行文件，
  运行时从内存提供，无外部文件依赖。
- **HTTPS 与鉴权**：可选证书目录启用 HTTPS（递归搜索、SNI 多证书、过期热更新），
  WebUI 可加 Basic 鉴权。

## 编译

launcher 随仓库一并构建（`apps/CMakeLists.txt` 引入，Android 构建不包含）：

``` bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

产物为 `build/bin/launcher`。目标要求 C++23（协程），HTTP/WS 与证书基于 Boost.Asio / Beast。

## 运行

``` bash
./bin/launcher --listen 0.0.0.0:18080 --proxy_server ./bin/proxy_server
```

不带参数运行会打印帮助。启动后 WebUI 默认监听 `http://0.0.0.0:18080`。

### 命令行选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `--listen` | `0.0.0.0:18080` | WebUI 与 REST API 的监听地址 |
| `--ssl_certificate_dir` | 空 | HTTPS 证书目录；配置后启用 HTTPS WebUI（证书搜索规则与 `proxy_server` 一致） |
| `--proxy_server` | `./proxy_server` | `proxy_server` 可执行文件路径 |
| `--data_dir` | `launcher_data` | 实例配置持久化目录 |
| `--webui_user` / `--webui_password` | 空 | WebUI Basic 鉴权用户名/密码，留空表示不鉴权 |
| `--no_kill_on_exit` | `false` | launcher 退出时不停止已托管的实例进程 |
| `--help,-h` | | 显示帮助并退出 |

### proxy_server 的查找顺序

未显式指定 `--proxy_server` 时依次尝试：

1. 当前目录下的 `./proxy_server`；
2. 系统 `$PATH` 中的 `proxy_server`；
3. 系统临时目录 `proxy_tmp/proxy_server`（若此前已自动下载则直接复用）；
4. 从 nightly.link 自动下载 `proxy_server-alpine_musl_x64_boringssl.zip` 并解压到该临时目录。

全部失败时打印错误并退出。显式指定 `--proxy_server` 时跳过上述流程，仅校验文件是否存在。

## WebUI

WebUI 由 `webui/` 下的 React + Vite 前端构建（详见 `webui/README.md`），产物输出到
`apps/launcher/webui/` 并在编译期内嵌到可执行文件。页面包含：

- **实例列表 / 详情**：2 秒轮询刷新，展示状态、PID、监听地址、连接数与速率；
  支持启动/停止/重启、重命名、删除、切换开机自启、复制实例地址。
- **状态页**：实例摘要、用户表（用量 / 限速 / 配额）、连接明细。
- **用户页**：用户增删、改密码、限速、配额与连接数管理。
- **配置页**：按选项注册表渲染表单（常用配置置顶），保存后热改并提示需重启项。
- **日志页**：增量渲染、过滤、自动滚动。

## HTTP API

前端只对接 REST（`/api/*`），不直接接触 `/rpc` 控制通道。

| 方法与路径 | 说明 |
| --- | --- |
| `GET /api/version` | launcher 构建版本（git commit 前 6 位） |
| `GET /api/options` | 选项注册表（名称/类型/分组/默认值/常用/需重启） |
| `GET /api/instances` | 实例摘要列表 |
| `POST /api/instances` | 创建实例，body：`{"name","config"}` |
| `GET /api/instances/:id` | 实例详情（含完整配置） |
| `PUT /api/instances/:id` | 更新名称或开机自启，body：`{"name","autostart"}` |
| `DELETE /api/instances/:id` | 删除实例（先停止） |
| `POST /api/instances/:id/start\|stop\|restart` | 生命周期操作 |
| `GET /api/instances/:id/status` | 最近一次状态上报 |
| `GET /api/instances/:id/logs?since=N` | 日志快照/增量，返回 `lines` / `seqs` / `next` / `gen` |
| `PUT /api/instances/:id/config` | 配置热改，body：`{"config":{...}}` |
| `POST /api/instances/:id/users` | 新增用户 |
| `DELETE /api/instances/:id/users/:user` | 删除用户 |
| `PUT /api/instances/:id/users/:user` | 修改密码，body：`{"password"}` |
| `PUT /api/instances/:id/users/:user/rate` | 设置限速，body：`{"rate":<字节/秒>}` |
| `PUT /api/instances/:id/users/:user/quota` | 设置流量配额，body：`{"quota":<字节>}` |
| `PUT /api/instances/:id/users/:user/conn_limit` | 设置最大连接数，body：`{"limit":<整数>}` |

## 控制通道（`/rpc`）

`GET /rpc?instance=<id>&token=<token>` 在通过校验后升级为 WebSocket JSON-RPC 会话，
运行在 launcher 的共享 `io_context` 上。`instance`/`token` 不匹配返回 `401`。

- **launcher → proxy_server（调用）**：`set_config`、`add_user`、`del_user`、
  `set_user_password`、`set_user_rate_limit`、`set_user_quota`、`shutdown`。
- **proxy_server → launcher（通知）**：`register`（上报 PID）、`status`（状态与用户用量）、
  `log`（日志行）。
- **launcher → proxy_server（通知）**：`set_user_usage`，把持久化的历史用量回传给实例以续接配额。

## 数据目录

`--data_dir` 指向的目录（默认 `launcher_data/`）：

``` text
launcher_data/
├── instances.json     # 实例配置、名称、autostart、token、user_usage、created_at
└── pid/
    └── <id>.pid       # 实例进程 PID，供 launcher 重启后清理残余进程
```

## 退出行为

收到 `SIGINT`/`SIGTERM` 后，launcher 关闭监听与所有连接，逐个向实例发送 `shutdown`
RPC 让其优雅退出，再停止进程。使用 `--no_kill_on_exit` 可保留实例进程继续运行；
下次启动时 launcher 会依据 pid 文件清理或接管这些孤儿实例（依赖实例重新连回控制通道）。

## 实现要点

- **并发模型**：C++23 + C++20 协程（`boost.asio`），整个服务运行在单个 `io_context`
  的小线程池上，无每连接线程、无 `poll`；子进程创建/终止等同步 OS 操作与 I/O 协程分离。
- **WebUI 内嵌**：`embed_webui.cmake` 把 `webui/` 下全部资源编译期内嵌，生成
  `webui_embedded.cpp`；`CONFIGURE_DEPENDS` 保证更新前端文件后自动重新嵌入。
- **版本注入**：构建时把 git 版本写入 `version.cpp`，供 `/api/version` 与页面展示。
