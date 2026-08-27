# aproxy — proxy Android 客户端 (纯 Dart)

基于纯 Dart 实现 tun2socks 代理引擎, 通过 Android `VpnService` 建立 TUN,
以 Dart 为主、Kotlin 配合, **不使用 C++**.

## 架构

```
Flutter/Dart (引擎)                          Android 原生 (Kotlin)
┌─────────────────────────────────┐  数据面/长帧  ┌──────────────────────────┐
│ 配置管理/存储/三页面 (Dart)      │ ───────────▶ │ AproxyVpnService:         │
│     │                           │ ◀─────────── │   VpnService.establish()  │
│     ▼                           │              │   TUN fd <-> 数据面桥接    │
│ DartEngine (aengine)            │              │   受保护转发器 (protect)    │
│  ├ TunBridge 收 TUN 原始 IP 包  │              └──────────────────────────┘
│  ├ TunnelEngine 分流+TCP+DNS    │
│  ├ Forwarder 出站 (protect)     │
│  └ EngineAgent 经 WS 上报/控制  │ ◀─ JSON-RPC ─▶ LauncherServer (Dart)
└─────────────────────────────────┘               (ws://127.0.0.1:port)
```

- **纯 Dart 引擎**: `lib/engine/` 内实现 IP 报文解析、TCP 连接状态机、UDP/DNS
  处理、SOCKS4/5 + HTTP(S) CONNECT 上游、TLS（经 `SecureSocket`）、分流路由与
  中国大陆绕过。引擎在 Flutter 进程内运行。
- **Kotlin 只做原生侧**:
  - `AproxyVpnService` 经 `VpnService.establish()` 建立 TUN;
  - 线程把 TUN fd 读到的原始 IP 包以"4 字节长度 + 报文"帧推给 Dart, 并写回
    Dart 生成的报文 (数据面桥接);
  - 本地"受保护转发器": Dart 无法把自身 socket fd 交给 `VpnService.protect`,
    因此出站连接统一经本地转发器建立 (Kotlin 创建真实 Socket, protect 后
    connect 上游, 双向转发)。
- **控制通道**: Dart 内置本地 WS JSON-RPC 服务 (`LauncherServer`), 引擎作为
  WS 客户端连接, 上报 `register/status/log`, 处理 `set_config/get_status/
  set_tun_fd/shutdown`, wire 协议与 libproxy 对齐。

## 模块

| 文件                                | 职责                                            |
| ----------------------------------- | ----------------------------------------------- |
| `lib/models/vpn_config.dart`        | 配置模型、校验、序列化                          |
| `lib/services/storage_service.dart` | 配置与运行状态持久化 (SharedPreferences)        |
| `lib/services/vpn_channel.dart`     | Flutter <-> Kotlin MethodChannel/EventChannel   |
| `lib/services/launcher_server.dart` | 本地 JSON-RPC over WebSocket 控制端              |
| `lib/services/app_session.dart`     | 全局运行状态 + 引擎/服务启停编排                 |
| `lib/engine/aengine.dart`           | 引擎编排 (数据面/控制通道生命周期)               |
| `lib/engine/ip_packet.dart`         | IPv4/IPv6 + TCP/UDP 报文编解码                  |
| `lib/engine/tunnel_engine.dart`     | 分流决策 + TCP/DNS 分发                          |
| `lib/engine/tcp_session.dart`       | TCP 连接状态机 (顺序转发, 不作窗口/重传)        |
| `lib/engine/forwarder.dart`         | 受保护转发器客户端 (含 TLS 包裹)                |
| `lib/engine/upstream.dart`          | SOCKS4/5 + HTTP(S) CONNECT 上游, 支持用户认证   |
| `lib/engine/dns.dart`               | DNS over TCP 客户端 + 报文解析/构造             |
| `lib/engine/route_table.dart`       | proxy_cidr/proxy_domains/CN 绕过 分流表          |
| `lib/engine/agent.dart`             | 控制通道客户端 (register/status/log + RPC)      |

## 构建

```sh
flutter pub get
flutter analyze
flutter test
flutter build apk --debug
```

## 配置字段

- 上游代理 `proxy_pass`: 支持 `http(s)://user:pass@host:port`、
  `socks4://`、`socks5://`、`socks5s://` (经 TLS)。
- 分流: `proxy_domains`(后缀)、`proxy_cidr`、`bypassCn`(绕过中国大陆)。
- DNS: `dns`(国内)、`dnsForeign`(国外)、`dnsForeignDoh`、`dnsCache`、`noIpv6`。
- 其它: `tunMtu`、`tunAddress`/`tunPrefix`、`disableCheckCert`、`testUrl`、
  `udpTimeout`、`proxyPassPoolSize`。
