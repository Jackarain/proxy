# apps/ohos — OpenHarmony (HarmonyOS NEXT) 客户端

本目录包含 OpenHarmony 端的两个组成部分：

- `libproxy/` — C++ 核心库的 OpenHarmony 封装（NAPI），编译产物为 `libxproxy.so`。
- `xproxy/` — ArkTS/ArkUI 客户端应用（Stage 模型），加载 `libxproxy.so` 并运行 VPN。

## libproxy

基于仓库根 `proxy/` 的 C++ proxy 服务（tun2socks 模式）的 OpenHarmony 封装，通过
NAPI 向 ArkTS 暴露最小接口：

- `start(config: string): number` — 以 JSON 配置启动 proxy 服务，成功返回 0。
- `stop(): void` — 停止 proxy 服务。
- `build_version(): string` / `min_sdk_version(): string` — 编译期 git 版本与最低 SDK 版本。

启动配置由 `config_to_option()` 解析为 `proxy_server_option`，支持的键包括：
`proxy_pass`、`tun`、`tun_mtu`、`tun_wait_fd`、`proxy_pass_pool_size`、
`proxy_domains`、`proxy_cidr`、`dns_domestic`、`dns_foreign`、`dns_doh`、
`dns_cache_size`、`dns_cache_ttl`、`dns_no_ipv6`、`disable_check_cert`、
`ssl_sni`、`udp_timeout`、`launcher_url`。

## xproxy

ArkTS 客户端，配置管理与界面参考 Android 端实现：

- **配置**：多条配置以 JSON 存于 `preferences`，启动时翻译为 libproxy 配置。
- **VPN**：`VpnExtensionAbility`（`@ohos.net.vpnExtension`）经 `create()` 建立
  tun 并拿到 fd，`tun_wait_fd` 模式下经控制通道 `set_tun_fd` 注入 libproxy。
- **protect**：libproxy 创建出站 socket 后经控制通道 `protect` 请求由 VPN 扩展调用
  `VpnConnection.protect(fd)` 放行，避免流量回环进 tun。
- **控制通道**：VPN 扩展进程内置本地 WS 服务（`ws://127.0.0.1:<port>/<token>`），
  proxy 作为客户端连接并上报 `register/status/log`；扩展可下发
  `set_config` / `set_tun_fd` / `shutdown` RPC。界面侧以 `<token>/ui` 连接同一服务
  获取状态与日志，并下发 `ui_set_config` 热更新。

### 进程模型与状态传递

界面进程与 VPN 扩展进程之间不共享内存，因此：

- 启动配置写入共享的运行目录 `<filesDir>/run/<runId>/config.json`，启动参数
  （运行目录 / 控制通道端口 / token）经 `startVpnExtensionAbility` 的 Want 传递。
- 运行状态写入同目录的 `state.json`（含实际监听端口与 token），界面轮询读取；
  native 日志行写入 `xproxy.log`。

### 与 Android 端的差异

- **VPN 能力**：Android 为 `VpnService.establish()`，OpenHarmony 为
  `@ohos.net.vpnExtension` 的 `VpnExtensionAbility`；`protect` 语义一致。
- **路由条数上限**：系统限制 VPN 路由最多 1024 条。"绕过中国大陆" 计算出的
  非中国网段补集通常超过该上限，此时按"最小间隙优先"聚合相邻区间做降级
  （少量中国网段会进入 VPN），保证可启动。
- **IPv6**：与 Android 一致下发 IPv6 默认路由以接管 IPv6 流量；设备不接受 IPv6 路由时
  自动退回仅 IPv4 路由（此时 IPv6 不进入隧道），避免整体建立失败。
- **自更新**：发布目录仍取 SHA-1 判断有无更新，安装包地址为
  `https://www.jackarain.org/download/ohos-release.hap`；下载到应用私有目录后调起
  系统安装器，安装由用户确认。当前无公开的静默安装接口，也没有可读取已安装包
  SHA-1 的接口，因此以"上次已处理的远端 SHA-1"记录更新状态。
- **二维码分享**：与 Android 端完全互通。分享正文以 deflate + 预共享字典
  （`config_share_dict.dart` 中的同一份字典）压缩后做 base64url，前缀
  `xproxy2:`；扫码端同时兼容 `xproxy1:`（gzip）与明文 JSON。压缩/解压为
  ArkTS 自实现（`service/Deflate.ets`、`service/Inflate.ets`）。
- **包标识**：沿用 `com.jackarain.xproxy`。

## 构建

工具链安装在 `/opt/ohos/command-line-tools`（Command Line Tools + HarmonyOS SDK
5.1.0 Release / API 18），环境变量：

``` sh
export DEVECO_SDK_HOME=/opt/ohos/command-line-tools/sdk
export DEVECO_NODE_HOME=/opt/ohos/command-line-tools/tool/node
export PATH=$DEVECO_NODE_HOME/bin:$PATH
```

``` sh
# 1. 编译 libxproxy.so 并同步到客户端工程, 随后构建 HAP (仓库根目录)
./build.ohos.sh /root/proxy arm64-v8a /opt/ohos/command-line-tools/sdk /opt/ohos/command-line-tools

# 产物: release/arm64-v8a/libxproxy.so 与 release/arm64-v8a/xproxy-arm64-v8a.hap

# 2. 仅重新构建 HAP
cd apps/ohos/xproxy
hvigorw assembleHap --mode module -p product=default -p buildMode=release
```

HAP 默认不签名（`build-profile.json5` 未配置 `signingConfigs`）。安装到设备需在
DevEco Studio 中配置自动签名，或用 `hap-sign-tool` 手工签名后 `hdc install`。
