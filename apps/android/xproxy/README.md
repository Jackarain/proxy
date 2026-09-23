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
更新源是站点上一个固定地址的正式签名包:

```
https://www.jackarain.org/download/app-release.apk
```

发布方式: 把 `master` 的 CI 产物(artifact `proxy_server-android-release-apk`)传到该地址即可;
客户端只认这个地址和包内的 `versionCode`, 不依赖 CI 本身.

流程:

1. **探测**: 带 `Range: bytes=0-65535` 只取头部 64KB 计算 FNV-1a 指纹. 该地址没有
   `ETag`/`Last-Modified`, 只能按内容比对; 头部含 zip 目录项与清单/代码, 同一份包必然
   同值, 重新构建必然变值. 服务端若忽略 Range, 读满头部即断开, 不会拉整个包.
2. **比对**: 指纹与本机记录(已安装或「跳过此版本」)相同即无更新; 另外会读取已安装 APK
   的头部指纹, 与远端一致时(刚装完或首次检查)不下载也能确认是最新.
3. **下载**: 落地到应用私有外部目录 `update/app-release.apk`, 弹进度条显示百分比与速度, 可取消.
4. **校验**: 读下载包的 `versionCode` 与签名证书 SHA-256: 不大于当前版本则视为已是最新
   (记录指纹后静默结束); 签名不一致则直接说明需要卸载重装, 而不是让系统安装器报出
   难以理解的失败.
5. **安装**: 经 `FileProvider` 交给系统安装器(`REQUEST_INSTALL_PACKAGES`), Android 8.0+
   未授权时会跳到「安装未知应用」页, 授权后返回即可继续. 安装会终止应用进程, 因此发起
   安装时先记录「待核对」状态, 下次启动发现版本已提升才记为已处理; 用户若在系统安装器里
   取消, 记录会被丢弃, 之后仍会再次提示该版本.

版本号约定: CI 用 `--build-number="$GITHUB_RUN_NUMBER"` 构建, APK 的 `versionCode` 即该次
workflow 的 run number, 保证每次发布都大于上一版(客户端判断更新的唯一依据). CI 末尾会
`aapt2 dump badging` 校验 `versionCode` 与 run number 一致, 防止注入失效后客户端失去判据.
本地构建(`versionCode` 仍是 pubspec 里的 1)不会覆盖线上正式包.

已知限制:

- 是否更新只看包内 `versionCode`: 发布新包必须递增版本号(pubspec 的 `+N` 被 CI 覆盖,
  手工发版需自己传 `--build-number`).
- 头部 64KB 指纹相同即视为同一份包, 因此只有 `versionCode` 或头部内容变化时才会提示.
- 上游每次 push `master` 都会产出新构建, 频繁发布时客户端提示也会频繁; 想低频发版就
  只在需要时上传新包.
- 只支持自签名分发: debug 签名的旧包必须先卸载(签名不同系统会拒绝覆盖安装);
  上架 Google Play 的版本不能自带更新(政策限制).
- 每次检查固定消耗约 64KB 流量; 自动检查 24h 内只做一次, VPN 运行中不做自动检查.

## 测试

```sh
flutter analyze
flutter test   # 配置序列化/校验/存储、WS JSON-RPC 协议、列表页交互
```
