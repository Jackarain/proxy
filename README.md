# 使用 C++20 实现的高性能 proxy

[![CircleCI](https://dl.circleci.com/status-badge/img/gh/Jackarain/proxy/tree/master.svg?style=shield)](https://dl.circleci.com/status-badge/redirect/gh/Jackarain/proxy/tree/master)
[![actions workflow](https://github.com/jackarain/proxy/actions/workflows/Build.yml/badge.svg)](https://github.com/Jackarain/proxy/actions)
![Badge](https://hitscounter.dev/api/hit?url=https%3A%2F%2Fgithub.com%2FJackarain%2Fproxy&label=&icon=github&color=%23198754&message=&style=flat&tz=Asia%2FShanghai)

## proxy 项目简介

基于 `C++20 协程`，以极少的代码量实现具有极高性能且支持标准 `socks4`/`socks4a`/`socks5`/`http`/`https` 的 `server`/`client proxy`，该项目不仅可以直接作为 `proxy` 服务端/客户端来使用，也可以作为您的网络程序提供代理支持的开发库来使用。

该项目可以组织出绝大部分代理的使用场景，并且 `client` 与 `server` 之间可配置通过 `ssl` 加密通信，也可配置多级代理，如下架构：

``` text

                  +--------------+     |      +--------------+
  browser/app --> | proxy server | ---ssl---> | proxy server | --> target server
                  +--------------+     |      +--------------+
                       local        Firewall       remote
```

上图中 `proxy server` 即可以作为 `local` 端的客户端，也可以作为 `remote` 端的服务端，它可以同时支持多种代理协议以及普通 `http/https` 协议的支持.

您不必担心你的代理服务器被他人探测，因为探测只会得到它是一个 `nginx error page` 服务的结果，这是因为在探测者不知 `proxy auth` 认证信息的情况下认证会失败，这时的 `proxy server` 将自动伪装成一个 `nginx` 服务，返回一个 `nginx error page` 页面信息。

``` text
                      |            +--------------+
  browser/app --> https proxy ---> | proxy server | --> target server
                      |            +--------------+
    local          Firewall             remote
```

`proxy server` 同时能接收 `socks` 或 `http` 代理请求（`http` 代理使用 `CONNECT`），它能将所有功能（`socks`、`http`）可运行在同一服务端口上，服务端能自动甄别协议的类型，它通过处理请求协议的前几个字节判定为 `socks` 或 `http` 请求，从而实现支持不同协议运行在同一服务端口。

将不同服务运行在同一端口之上，也能达到混淆该服务上的流量特征的目的，通过灵活的取舍配置所需要的协议，这可以让服务具有更强的抗审查能力。

`proxy server` 也能同时接受正常 `http` 请求（请求 `http` 静态文件），这时它和一个正常的静态 `web` 文件服务器是完全相同的功能，并且它支持配置 `http` 访问认证。

## 编译 proxy 项目

### 编译环境

要求操作系统安装以下工具:

``` text
版本控制: git

C++ 编译器: Linux 平台 gcc 11 以上，或 clang 14 以上，windows 平台要求 visual studio 2022 或以上。

构建工具: cmake 3.5 以上。

如果希望使用 docker 编译，应当安装好 docker 环境。
```

首先执行 `git` 克隆源码（源码无任何外部依赖）

``` bash
git clone <source url>
```

克隆源码完代码后，就可以开始编译项目了。

### 直接源码编译

进入源码目录，执行如下操作：

``` bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

在 `cmake` 命令成功执行完成后，开始输入以下命令编译：

``` bash
cmake --build build
```

通常编译过程不会出现问题，如果出现任何问题，请联系作者，并将完整的错误信息保留并报告给作者。

成功编译后，生成的可执行程序将在 `bin` 目录下，其它相关 `lib` 的编译结果 `.a` 文件将在相应项目名称的目录下。

### 在 Docker 中编译

在源码目录运行：

``` bash
docker build . -t proxy:v1
```

即可自动编译，编译完成后，可执行程序 `proxy_server` 可以在 `docker` 镜像的 `/usr/local/bin/` 目录下找到。

## 使用介绍

`proxy server` 提供了较为完整的功能，可以作为 `tun2socks` 后级用于全局代理上网等场景，`example` 目录则主要用于编程示例。

`proxy server` 提供以下参数，参数除 `config` 之外，既可用于配置文件也可用于命令行:

|  参    数      | 用法解释  |
|  --------      | -----    |
| config | 指定参数配置文件，如 `server.conf`，配置文件内容以 `key=value` 的 INI 格式保存，如 `server_listen=[::0]:1080`，可参考 doc 目录下的配置文件示例 |
| server_listen | 监听地址及端口，格式为 `addr:port`；支持 `v6only` 后缀（如 `addr:port-v6only`）；支持同时监听多个地址；亦支持 Unix Domain Socket，格式：`unix://path/to/proxy.sock` |
| stdio | 用于 [proxytunnel](https://github.com/proxytunnel/proxytunnel) 代理隧道功能时，向外发起连接的目标主机和端口，格式为：`host:port` |
| reuse_port | 启用 `TCP SO_REUSEPORT` 选项，仅在 Linux 平台有效 |
| happyeyeballs | 启用 Happy Eyeballs 连接算法，默认为启用 |
| v4only | 仅使用 IPv4 向目标地址发起连接，默认为不启用 |
| v6only | 仅使用 IPv6 向目标地址发起连接，默认为不启用 |
| transparent | 启用透明代理（TPROXY），默认禁用，此选项仅 Linux 平台有效 |
| so_mark | 用于设置所有出站连接的 `SO_MARK` 标记，方便实现代理流量的策略路由，默认不启用。设置方法：`so_mark=100` |
| local_ip | 指定向上游服务或目标服务发起连接时使用的本地网口 IP 地址，通常用于机器有多个 IP 时使用特定 IP 向外发起连接 |
| pam_auth | 指定使用 PAM 认证模块进行认证，参数值为 PAM 服务名称。PAM 可与 `auth_users` 同时用于认证，优先使用 `auth_users`。需在编译时添加 `-DENABLE_USE_PAM_AUTH=ON` 选项以启用 PAM 模块 |
| auth_users | 认证信息列表，客户端必须满足其中一对用户/密码才能握手通过。默认用户密码为 `jack:1111`（默认需要认证以避免被误用作跳板）。若需设为无认证模式，须将 `auth_users` 置为 "" |
| users_rate_limit | 指定认证用户的 TCP 连接 I/O 速率，单位为字节/秒，默认不限速。格式如：`user1:102400 user2:204800` |
| proxy_pass | 当前服务作为中间级联服务时，指定上游代理服务地址，格式为 URL。如有认证信息须包含在内，如：`https://jack:1111@example.com:1080/` |
| proxy_pass_ssl | 向 `proxy_pass` 指定的上游代理发起连接时，是否通过 SSL 加密传输。注意上游代理服务必须启用 SSL 相关证书 |
| proxy_ssl_name | 指定 SNI 用于在单一 IP 上支持多个 SSL 证书的域名匹配，取代已废弃的 `ssl_sni` 选项 |
| ssl_certificate_dir | SSL 证书密钥存放目录。`proxy_server` 会自动递归查找子目录，每个目录对应一个域名的证书文件（若私钥已加密，则需在相应目录下放置 `password.txt` 指明密码）。证书过期后将自动每 5 分钟热加载一次，无需重启服务 |
| ssl_cacert_dir | 指定 CA 证书文件目录，用于客户端 SSL 连接验证。若不指定则默认使用内置的 cacert 文件（来自 <https://curl.se/docs/caextract.html>） |
| ssl_sni | （已废弃，改用 `proxy_ssl_name`）指定 SNI 用于客户端 SSL 连接 |
| ssl_ciphers | SSL 协议允许的加密套件列表（除非了解其作用，否则无需关心） |
| ssl_prefer_server_ciphers | 优先使用服务端加密套件顺序（除非了解其作用，否则无需关心） |
| http_doc | 启用 HTTP 静态文件服务器，指定静态文件根目录 |
| htpasswd | 启用 HTTP 基本认证，通过 HTTP 访问静态文件时需要认证，`auth_users` 用于配置用户/密码信息 |
| autoindex | 在启用 HTTP 静态文件服务时，允许浏览器浏览文件目录列表，与 Nginx 的 `autoindex` 功能相同 |
| ipip_db | 指定 IPIP 数据库文件，用于获取客户端的地区信息。数据库下载地址：[17monipdb](https://github.com/Jackarain/proxy/releases/download/v7.0.0/17monipdb.7z) |
| allow_region | 指定允许连接的客户端地区，如：`北京\|河南\|武汉\|192.168.1.2\|192.168.1.0/24\|2001:0db8::1\|2001:db8::/32` |
| deny_region | 指定拒绝连接的客户端地区，如：`广东\|上海\|山东\|192.168.1.2\|192.168.1.0/24\|2001:0db8::1\|2001:db8::/32` |
| tcp_timeout | 设置 TCP 连接的读写超时，小于 0 表示不设超时，单位为秒，默认 60 秒 |
| udp_timeout | 设置 UDP 会话超时时间，单位为秒，默认 60 秒 |
| rate_limit | 设置全局 TCP 连接 I/O 速率，单位为字节/秒，默认不限速 |
| logs_path | 日志文件输出目录 |
| disable_logs | 禁止在终端输出日志，默认为 Release 版启用、Debug 版禁用 |
| disable_http | 禁用 HTTP 代理协议 |
| disable_socks | 禁用 SOCKS5/SOCKS4 代理协议 |
| disable_udp | 禁用 UDP 代理功能 |
| disable_insecure | 禁止非安全连接（即只允许 SSL/TLS 加密连接） |
| disable_check_cert | 禁用 TLS 证书校验（不安全的选项，通常仅用于测试环境） |
| scramble | 启用数据噪声加扰（通过随机噪声混淆数据传输），此选项需要两端同时开启 |
| noise_length | 发送的随机噪声数据最大长度以对抗长度分析探测，在启用 `scramble` 时决定随机噪声长度，未启用  `scramble` 时用于决定 HTTP Padding 长度 |
| asio_config | 指定从环境变量读取 Asio 配置的变量名前缀，默认值为 `ASIO`（如 `ASIO_SCHEDULER_LOCKING=0` 可禁用调度器锁以提升性能） |

## 代理服务器功能

作为一个 `proxy` 服务器，它主要实现了 `socks5`/`socks4`/`http`/`https` 等协议的代理功能，示例如下：

``` bash
./proxy_server --server_listen 0.0.0.0:1080
```

上述示例中，`proxy_server` 监听在 `0.0.0.0:1080` 端口，客户端可以通过 `socks5`/`socks4`/`http`/`https` 等协议连接到 `proxy_server` 进行代理，请注意，默认情况下需要认证才能连接，认证信息为 `jack:1111`，若需要设置为无需要认证代理模式，必须置 `auth_users` 参数为 ""，如：

``` bash
./proxy_server --server_listen 0.0.0.0:1080 --auth_users ""
```

若是需要作为中间级联服务时，`proxy_pass` 指定上游代理服务地址，格式为 `url` 格式，如有认证信息则须包含在内，如: `https://jack:1111@example.com:1080/`，示例如下：

``` bash
./proxy_server --server_listen 0.0.0.0:1080 --proxy_pass https://jack:1111@example.com:1080/
```

上游代理服务若是 `socks5` 协议且需要使用 `ssl` 加密传输时，`url` 的 `scheme` 必须以 `s` 结尾，如: `socks5s://jack:1111@example.com:1080/`。

当作为服务器启用 SSL 加密传输时，需要配置 `ssl_certificate_dir` 参数指定证书密钥等文件目录，这样客户端就可以通过 `https`/`socks5s` 加密协议连接到 `proxy_server` 进行代理，示例如下：

``` bash
./proxy_server --server_listen 0.0.0.0:1080 --ssl_certificate_dir /path/to/ssl/cert
```

若是运行在路由器作为全局代理，`proxy_server` 支持 `TPROXY` 透明代理功能，示例如下：

``` bash
./proxy_server --transparent true --server_listen 0.0.0.0:1080 --proxy_pass https://jack:1111@example.com:1080/
```

上面示例中，`proxy_server` 监听在 `0.0.0.0:1080` 端口， 通过在路由器上配置 TPROXY 到 `0.0.0.0:1080` 端口，将需要代理的 IP 通过 TPROXY 的方式连接到 `proxy_server`，`proxy_server` 会将客户端请求通过 `https://jack:1111@example.com:1080/` 进行代理，客户端无需做任何配置。

## 代理隧道功能

代理隧道功能实现了 [proxytunnel](https://github.com/proxytunnel/proxytunnel) 的功能，具体使用方式如下，在 `.ssh` 目录中的 `config` 配置文件中添加：

``` text
Host example
    HostName xxx.xxx.xxx.xxx
    IdentityFile /root/.ssh/id_rsa
    User root
    ProxyCommand /path/to/proxy_server --stdio %h:%p --proxy_pass https://user:pass@proxy.server:8443
```

然后当使用 `ssh` 连接主机 `example` 时将会按上述配置文件中参数创建 `proxy_server` 代理隧道，通过 `proxy_pass` 指定的代理服务器连接目标 `HostName` 所指的服务器。

## 使用 PAM 模块认证介绍

`proxy_server` 支持使用 `PAM` 模块进行认证（仅支持 `pam` 的 `linux` 平台，编译 `proxy_server` 时需要在 `cmake` 中添加 `-DENABLE_USE_PAM_AUTH=ON` 选项以启用 `PAM` 模块认证功能），具体使用方法如下：

1. 配置 `PAM` 服务，如将 `doc` 目录下 `pam.example` 下的文件 `proxy-service` 复制到 `/etc/pam.d/` 目录中，文件名即为 `PAM` 服务名称。
2. 在 `proxy_server` 中指定 `PAM` 服务名称，如 `--pam_auth proxy-service`。
3. 使用 `linux` 命令添加用户到 `PAM` 服务中，如 `useradd jack`，其中 `jack` 为用户名，使用 `passwd jack` 设置密码如 `1111`。
4. 测试认证是否生效，如 `curl -x http://jack:1111@localhost:1080/ https://google.com`，如果返回 `200 OK` 则说明认证生效。

`PAM` 模块认证可以使用 `linux` 命令添加或管理用户，可极大方便 `proxy_server` 的用户管理，而不必依赖复杂的数据库系统。如果你是开发人员，也可以开发一个支持数据库认证的 `PAM` 的 `so` 模块（可参考 `doc` 下 `pam.example` 的 `pam_sqlite.c` 了解如何实现 `PAM` 认证模块），或者使用 `PAM` 模块认证来实现 `LDAP` 认证等。

## DNS over HTTPS (DoH) 代理功能

`proxy server` 内置了 **DNS over HTTPS (DoH)** 代理功能，可以将传统的 DNS 查询转为通过 HTTPS 加密传输，有效防止 DNS 劫持和嗅探。

### DoH 配置参数

| 参数 | 用法解释 |
| ---- | ----- |
| dns_upstream | 指定上游 DNS 服务器地址。格式支持传统的 UDP DNS（`ip:port`）和 DoH（`https://host/path`），如 `--dns_upstream 8.8.8.8:53` 或 `--dns_upstream https://dns.google/dns-query` |

相关参数：`disable_check_cert` 用于禁用 TLS 证书校验（不推荐用于正式环境）。

### 使用示例

``` bash
./proxy_server --server_listen 0.0.0.0:1080 \
    --dns_upstream https://dns.google/dns-query \
    --ssl_certificate_dir /path/to/certs
```

### DNS 请求示例

`proxy server` 支持标准的 DoH 接口（RFC 8484），客户端可以通过 HTTPS 协议直接请求 `/dns-query` 路径：

- **GET 请求**: 通过 `?dns=` 参数传递 base64url 编码的 DNS 查询数据。
- **POST 请求**: 将 DNS 查询数据作为请求体直接发送（`Content-Type: application/dns-message`）。
- **JSON 格式**: 支持 `application/dns-json` 格式（Google DNS JSON API 兼容），通过 POST 请求发送 JSON 格式的查询参数。

示例：使用 `curl` 查询 `example.com` 的 A 记录

``` bash
# 使用 GET 方式（base64url 编码）
curl -H "Accept: application/dns-message" \
    "https://domain:1080/dns-query?dns=AAABAAAAAAAAA3d3dwdleGFtcGxlA2NvbQAAAQAB"

# 使用 POST 方式
curl -X POST -H "Content-Type: application/dns-message" \
    -H "Accept: application/dns-message" \
    --data-binary @dns_query.bin \
    https://domain:1080/dns-query

# 使用 JSON API 方式
curl -X POST -H "Content-Type: application/dns-json" \
    -d '{"name":"example.com","type":"A"}' \
    https://domain:1080/dns-query
```

## 静态文件 http 服务器(可配置为云音乐播放器)

`proxy server` 不仅是一个 `proxy` 服务器，同时还可以做为一个真实的静态文件 `http` 服务，且支持 `http range`，所以也可以作为 `http` 音视频文件服务器，播放器播放 `http` 音视频文件时通过 `http` 的 `bytes range` 协议进行 `seek`（快进快退），使用方法如下

``` bash
./proxy_server --autoindex true --http_doc /user/music --server_listen 0.0.0.0:1080
```

在目录 `/user/music` 中添加音乐文件（以及`.lrc`歌词文件），复制本项目中 `example/music` 下的 `index.html` 放入 `/user/music` 目录中，然后使用浏览器打开地址 `http://localhost:1080/` 运行效果（浏览器打开）：

![image](https://github.com/user-attachments/assets/3910015e-f4d6-4162-912b-8b7594c41d88)

当然，如果你希望只有你自己可以访问，那么在上面命令行添加参数 `--htpasswd true` 并通过参数 `--auth_users` 配置登录用户名和密码，打开页面时就会要求验证登录。

本人的 [blog](https://www.jackarain.org) 就是直接运行在 `proxy_server` 上的静态页面(由 `jekyll` 生成)，它同时也是我的代理服务，还是我的 [音乐播放器](https://www.jackarain.org/music/index.html)

## 其它相关

[在路由器上配置全局代理科学上网](https://github.com/Jackarain/proxy/wiki/%E5%9C%A8%E8%B7%AF%E7%94%B1%E5%99%A8%E4%B8%8A%E5%88%9B%E5%BB%BA%E5%85%A8%E5%B1%80%E4%BB%A3%E7%90%86%E6%A8%A1%E5%BC%8F)

---

有任何问题可加tg账号: [https://t.me/jackarain](https://t.me/jackarain) 或tg群组: [https://t.me/joinchat/C3WytT4RMvJ4lqxiJiIVhg](https://t.me/joinchat/C3WytT4RMvJ4lqxiJiIVhg)
