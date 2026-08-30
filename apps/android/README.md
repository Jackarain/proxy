# apps/android — Android 客户端

本目录包含 Android 端的两个组成部分：

- `libproxy/` — C++ 核心库的 Android 封装（JNI/SWIG），编译产物为 `libxproxy.so`。
- `xproxy/` — Flutter 客户端应用，加载 `libxproxy.so` 并以 `VpnService` 建立 TUN 隧道。

## libproxy

基于仓库根 `proxy/` 的 C++ proxy 服务（tun2socks 模式）的 Android 封装，提供最小
JNI 接口（`xproxy.hpp`，经 `xproxy.i` SWIG 生成 `com.jackarain` 包下的 Java 包装）：

- `xproxy.start(json)` — 以 JSON 配置启动 proxy 服务，成功返回 0。
- `xproxy.stop()` — 停止 proxy 服务。
- `xproxy.build_version()` / `min_sdk_version()` — 编译期 git 版本与最低 SDK 版本。

启动配置由 `config_to_option()` 解析为 `proxy_server_option`，支持的键包括：
`proxy_pass`、`tun`、`tun_mtu`、`tun_wait_fd`、`proxy_pass_pool_size`、
`proxy_domains`、`proxy_cidr`、`dns_domestic`、`dns_foreign`、`dns_doh`、
`dns_cache_size`、`dns_cache_ttl`、`dns_no_ipv6`、`disable_check_cert`、
`ssl_sni`（指定与 `proxy_pass` 建立 TLS 连接时使用的 SNI，空则用主机名）、
`udp_timeout`、`launcher_url`。

## xproxy

Flutter 客户端，在应用进程内直接调用 `libxproxy.so` 运行 proxy：

- **配置**：多条配置以 JSON 存于 SharedPreferences，启动时经 Kotlin 桥
  （`XproxyBridge`）翻译为 libproxy 配置后调用 `xproxy.start(json)`。
- **TUN**：`VpnService.establish()` detach 的 fd 经控制通道 `set_tun_fd` 注入
  libproxy（`tun_wait_fd` 模式），同进程直接使用。
- **protect**：libproxy 创建出站 socket 后经控制通道 `protect` 请求由 Kotlin 侧
  调用 `VpnService.protect(fd)` 放行，避免回环进 TUN。
- **控制通道**：Flutter 内置本地 WS 服务（`127.0.0.1:<port>`），proxy 主动连接并
  上报 `register/status/log`；应用可下发 `get_status` / `set_config` /
  `set_tun_fd` / `shutdown` RPC。`set_config` 为运行期热更新，TUN/SNI 等字段变更
  时整体重建 VPN。

## 构建

```sh
# 1. 编译 libxproxy.so (仓库根, 参见 build.android.sh)
#    脚本自动把 libxproxy.so 同步到 xproxy/android/app/src/main/jniLibs/<abi>/.
./build.android.sh /root/proxy <ndk-path> linux-x86_64

# 2. 同步 SWIG 生成的 Java 包装文件到 xproxy 工程
cp /root/proxy/outputs/*.java apps/android/xproxy/android/app/src/main/java/com/jackarain/

# 3. 构建 APK
cd apps/android/xproxy
flutter pub get
flutter build apk --debug
```

xproxy 的单元测试与静态分析：

```sh
cd apps/android/xproxy
flutter analyze
flutter test   # 配置序列化/校验/存储、WS JSON-RPC 协议、列表页交互
```
