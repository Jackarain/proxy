# xproxy — proxy Android 客户端 (Flutter)

基于 `libproxy` 编译出的 `libxproxy.so`, 通过 Android `VpnService` 建立 TUN,
在同一个进程内直接调用 `xproxy.start(json)` 运行 proxy (tun2socks 模式).

## 架构

```
Flutter (Dart)                          Android 原生 (Kotlin)                 libxproxy.so (C++)
┌────────────────────────┐  MethodChannel ┌─────────────────────────┐  JNI   ┌──────────────────────┐
│ 配置管理/存储/UI        │ ──────────────▶ │ MainActivity            │ ─────▶ │ xproxy.start(json)     │
│ 本地 WS 控制端 (Dart)   │                │ VpnService (TUN+protect)│        │ libproxy 服务          │
│ LauncherServer       │ ◀── ws jsonrpc ─┤                         │ ◀───── │ launcher 客户端     │
└────────────────────────┘                └─────────────────────────┘        └──────────────────────┘
```

- **配置**: 多条配置以 JSON 存于 SharedPreferences; 启动时经 json 传入 `libxproxy.so`.
- **TUN**: `VpnService.establish()` detach 的 fd 经控制通道 `set_tun_fd` 注入
  libproxy (tun_wait_fd 模式), 同进程直接使用.
- **protect**: libproxy 创建到上游代理/目标的出站 socket 后, 经控制通道
  `protect` 请求由 Kotlin 侧调用 `VpnService.protect(fd)` 放行, 避免回环进 TUN.
- **控制通道**: Flutter 内置本地 WS 服务 (`127.0.0.1:<port>`), 经 `launcher_url`
  字段交给 proxy, proxy 主动连接并上报 `register/status/log`;
  应用可下发 `get_status` / `set_config` / `set_tun_fd` / `shutdown` RPC.
- **线程模型**: VpnService 的建立/启停/teardown 全部在专用工作线程串行执行,
  不阻塞主线程, 也天然避免了 START/STOP 竞态.

## 构建

```sh
# 1. 编译 libxproxy.so (仓库根目录, 参见 build.android.sh)
#    脚本会自动把 libxproxy.so 同步到 android/app/src/main/jniLibs/<abi>/,
#    无需手工拷贝.
./build.android.sh /root/proxy /opt/android-sdk/ndk/26.3.11579264 linux-x86_64

# 2. 同步 SWIG 生成的 Java 包装文件到本工程
cp /root/proxy/outputs/*.java android/app/src/main/java/com/jackarain/

# 3. 构建 APK
flutter pub get
flutter build apk --debug
```

## 配置字段

- proxy 参数: `proxy_pass` (如 `https://user:pass@host:443`), `tun_mtu`,
  `proxy_domains` (后缀匹配, 命中走代理), `proxy_cidr` (命中走代理),
  `disable_check_cert`.
- Android VpnService 专用: `tunAddress`, `tunPrefix`, `routes` (CIDR, 默认全隧道),
  `dns`, `name`.
- 运行时注入 (无需手填): `tun_wait_fd`, `launcher_url`.
- 保存前做基础校验 (需填写上游代理、MTU/测试 URL 范围等).

## 测试

```sh
flutter analyze
flutter test   # 配置序列化/校验/存储、WS JSON-RPC 协议、列表页交互
```
