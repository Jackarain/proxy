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

## 发布签名

release 包使用 `android/key.properties` 指定的正式密钥签名; 该文件不入库
(`android/.gitignore` 已忽略 `key.properties` / `*.keystore` / `*.jks`).

本地日常开发不需要它: 文件不存在时 release 构建回退 debug 签名, `flutter run` /
`flutter build apk` 照常可用.

CI 行为: secrets 齐全时设置 `REQUIRE_RELEASE_SIGNING=1`, 强制用正式密钥签名并校验指纹;
secrets 缺失时(镜像等未配置的仓库)只打 warning, 不中断 job — 仍然编译一次 release APK
验证构建(回退 debug 签名), 但跳过签名校验与产物上传, 避免流出装不上的包. 结论写在
job 的 Summary 里.

生成密钥并转 base64 (口令与别名务必另存备份, 丢失后已安装用户只能卸载重装):

```sh
keytool -genkeypair -v -keystore release.keystore -alias xproxy \
  -keyalg RSA -keysize 4096 -validity 10000 -storetype PKCS12 \
  -dname "CN=xProxy, O=jackarain, C=CN"
# PKCS12 下 keyPassword 必须与 storePassword 相同.
# -validity 必须显式给: keytool 默认只有 90 天.
base64 -w0 release.keystore > release.keystore.b64
# macOS: base64 -i release.keystore -o release.keystore.b64
```

在 Settings -> Secrets and variables -> Actions 配置:

| 类型 | 名称 | 说明 |
| --- | --- | --- |
| Secret | `ANDROID_KEYSTORE_BASE64` | `release.keystore.b64` 的内容 |
| Secret | `ANDROID_KEYSTORE_PASSWORD` | keystore 口令 |
| Secret | `ANDROID_KEY_ALIAS` | 密钥别名 |
| Secret | `ANDROID_KEY_PASSWORD` | 密钥口令 (PKCS12 下同 keystore 口令) |
| Variable | `ANDROID_SIGNING_CERT_SHA256` | 期望的证书 SHA-256 指纹, CI 构建后校验; 留空则只打印不比对 |

指纹取值 (含冒号, 大小写不敏感):

```sh
apksigner verify --print-certs build/app/outputs/flutter-apk/app-release.apk
```

本地要出正式签名包时, 在本工程写 `android/key.properties` (勿提交):

```properties
storeFile=/绝对路径/release.keystore
storePassword=...
keyAlias=xproxy
keyPassword=...
```

`storeFile` 必须是绝对路径: Gradle 的 `file()` 以 app 模块目录(`android/app`)为基准.

注意: 从 debug 签名切到正式签名后, 已安装用户需要**卸载重装**一次才能继续更新
(签名不一致时系统会拒绝覆盖安装).

## 自动更新

应用启动后(延迟 3s)在后台检查更新, 顶部工具栏的 `检查更新` 按钮可手动触发.
更新源是 CI 产出的 artifact, 经 [nightly.link](https://nightly.link) 提供匿名直链
(不需要 token, 普通用户也能下载):

```
https://nightly.link/Jackarain/proxy/workflows/Build/master/proxy_server-android-release-apk.zip
```

流程:

1. **探测**: 带 `Range: bytes=0-0` 只取 1 字节, 用响应头里的 `ETag` 作为内容指纹
   (缺失时退化为 `Last-Modified` + 长度), 服务端不支持 Range 时读完响应头即断开,
   不会为了比对而拉整个包.
2. **提示**: 指纹与本机记录(已安装或「跳过此版本」)不同才提示 `发现新版本`;
   自动检查 24h 内只做一次, VPN 运行中不自动检查(此时应用自身流量受 TUN 影响).
3. **下载**: 落地到应用私有外部目录 `update/update.zip`, 弹进度条显示百分比与速度, 可取消.
4. **校验**: 解压出 APK 后用 `PackageManager` 读取其 `versionCode` 与签名证书 SHA-256,
   与当前应用比对: `versionCode` 不大于当前版本则视为已是最新(记录指纹后静默结束);
   签名不一致则直接说明需要卸载重装, 而不是让系统安装器抛出难以理解的失败.
5. **安装**: 更新包目录经 `FileProvider` 交给系统安装器(`REQUEST_INSTALL_PACKAGES`),
   Android 8.0+ 未授权时会跳到「安装未知应用」页, 授权后返回即可继续.
   安装会终止应用进程, 因此发起安装时先记录「待核对」状态, 下次启动发现版本已提升才
   记为已处理; 用户若在系统安装器里取消, 记录会被丢弃, 之后仍会再次提示该版本.

版本号约定: CI 用 `--build-number="$GITHUB_RUN_NUMBER"` 构建, 即 APK 的 `versionCode`
等于该次 workflow 的 run number, 客户端据此判断新旧的唯一依据. CI 末尾会
`aapt2 dump badging` 校验 `versionCode` 与 run number 一致, 防止注入失效后客户端失去
更新判据. 因此本地构建(`versionCode` 仍是 pubspec 的 1)不会覆盖 CI 产物.

已知限制:

- 上游 `master` 每次 push 都会产出新构建, 客户端因此可能频繁提示; 只想偶尔发版时应改
  用打 tag 触发 CI.
- nightly.link 是第三方服务, 偶发 404/不可用(如构建仍在进行)时只会提示检查失败,
  不会影响其它功能.
- 首次检查(本机没有指纹记录)一定会有一次完整下载才能读到包内 `versionCode`;
  之后靠指纹比对, 未变化时只花 1 个字节.
- 从 debug 签名的旧包切到正式签名包, 必须先卸载(见上一节), 否则校验阶段就会拒绝.
- 上架 Google Play 的版本不能自带更新(政策限制), 该功能仅用于自签名分发的包.

## 测试

```sh
flutter analyze
flutter test   # 配置序列化/校验/存储、WS JSON-RPC 协议、列表页交互
```
