# 一个使用 c++20 的 proxy 的高性能实现

[![CircleCI](https://dl.circleci.com/status-badge/img/gh/Jackarain/proxy/tree/master.svg?style=shield)](https://dl.circleci.com/status-badge/redirect/gh/Jackarain/proxy/tree/master)
[![actions workflow](https://github.com/jackarain/proxy/actions/workflows/Build.yml/badge.svg)](https://github.com/Jackarain/proxy/actions)
![Badge](https://hitscounter.dev/api/hit?url=https%3A%2F%2Fgithub.com%2FJackarain%2Fproxy&label=&icon=github&color=%23198754&message=&style=flat&tz=Asia%2FShanghai)

## proxy 项目简介

使用 `C++20协程` 通过编写为数不多的代码，实现具有极高性能且支持标准 `socks4`/`socks4a`/`socks5`/`http`/`https` 的 `server`/`client proxy` 实现，该项目不仅可以直接作为 `proxy` 服务端/客户端来使用，并且也可以作为您的网络程序支持代理的开发库来使用。

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

C++ 编译器: Linux 平台 gcc 11 以上，或 clang 14 以上，windows 平台要求 visual studio 2019 或以上。

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

`proxy server` 它提供了较完整的功能，它可以非常完美的作为 `tun2socks` 后级用于全局代理上网等事宜，`example` 目录则主要用于编程示例。

`proxy server` 提供了以下参数，参数除 config 之外，即可于配置文件也可用于命令行:

|  参    数      | 用法解释  |
|  --------      | -----    |
| config | 可配置一个参数配置文件，如 `server.conf`，配置文件内容以`key=value`的`ini`配置文件的方式保存，如 `server_listen=[::0]:1080`, 可参考 doc 目录下的配置文件示例 |
| server_listen | 监听的地址以及端口，格式为 `addr:port`，支持 `v6only`(如: `addr:port-v6only`)，支持同时监听在多个地址上 |
| reuse_port | 启用 `TCP SO_REUSEPORT` 选项，这个选项只在 `linux` 平台有效 |
| happyeyeballs | 启用 `happyeyeballs` 连接算法，默认为启用 |
| v4only | 仅使用 ipv4 向目标地址连接，默认为不启用 |
| v6only | 仅使用 ipv6 向目标地址连接，默认为不启用 |
| transparent | 启用透明代理，默认禁用，此选项仅 linux 平台有效 |
| so_mark | 用于发起向 `proxy_pass` 连接时设置 so_mark 以方便实现代理流量的策略路由，仅在 transparent 启动时有效 |
| local_ip | 用于向上游服务或目标服务发起连接时使用指定的本地网口 `ip` 地址，通常用于机器上有多个 `ip` 时使用特定 `ip` 向外发起连接 |
| auth_users | 认证信息列表，客户端必须满足其中一对用户/密码才能握手通过，默认用户密码是 `jack:1111`（默认需要认证是为了避免不小心被当成别人免费的跳板），若需要设置为无需要认证代理模式，必须置 `auth_users` 参数为 "" |
| proxy_pass | 当前服务作为中间级联服务时，`proxy_pass` 指定上游代理服务地址，格式为 `url` 格式，如果有认证信息并必须包含认证信息，如: `https://jack:1111@example.com:1080/` |
| proxy_pass_ssl | 向 `proxy_pass` 指定的上游代理服务连接时，是否通过 `ssl` 安全传输，注意必须在上游代理服务启用 `ssl` 相关证书域名密钥等信息. |
| ssl_certificate_dir | `ssl` 证书密钥等存放目录，`proxy_server` 会自动查找这个目录下的子目录, 每个目录必须为一个域名相关的证书文件 (如果证书密钥文件加密, 则需要配置 password.txt 用于指定密码, 如果是运行在多个域名下面, 则需要指定 domain.txt 或目录名称以域名命名(如: letsencrypt/live 下类似的域名目录)用于指定当前证书文件所对应的域名, 若没有指定则视为默认证书配置) |
| ssl_cacert_dir | 指定 CA 证书文件目录用于客户端 `ssl` 连接使用, 若不指定则默认使用内置的 cacert 文件 (来自 `https://curl.se/docs/caextract.html`) |
| ssl_sni | 指定 SNI 用于客户端 `ssl` 连接使用(此选项除非你知道在做什么，否则不需要关心) |
| ssl_ciphers | `ssl` 协议允许的加密算法列表(此选项除非你知道在做什么，否则不需要关心) |
| ssl_prefer_server_ciphers | 优先使用 server 端加密算法(此选项除非你知道在做什么，否则不需要关心) |
| http_doc | 启用 `http` 静态文件服务器，指定静态文件目录 |
| htpasswd | 启用 `http` 认证，通过 http 访问静态文件时需要认证，auth_users 用于配置用户/密钥信息 |
| autoindex | 在启用 `http` 静态文件服务的情况下，启用 `autoindex` 将允许浏览器浏览文件目录列表，与 `nginx` 的 `autoindex` 选项功能一样 |
| ipip_db | 指定一个 ipip 数据库文件，用于获取客户端的地区信息，数据库下载地址： [17monipdb](https://github.com/Jackarain/proxy/releases/download/v7.0.0/17monipdb.7z) |
| allow_region | 指定允许连接的客户端地区 (如: `北京\|河南\|武汉\|192.168.1.2\|192.168.1.0/24\|2001:0db8::1\|2001:db8::/32`) |
| deny_region | 指定拒绝连接的客户端地区 (如: `广东\|上海\|山东\|192.168.1.2\|192.168.1.0/24\|2001:0db8::1\|2001:db8::/32`) |
| udp_timeout | 在启用 udp 中转的情况下，超过指定时间即回收 udp socket 等相关资源，时间单位为秒 |
| tcp_timeout | 设置 tcp 连接的读写超时，小于0表示没有超时设置，时间单位为秒 |
| rate_limit | 设置 tcp 连接的 I/O 速率设置，单位为字节/秒，默认无限速 |
| users_rate_limit | 设置指定认证用户的 tcp 连接的 I/O 速率设置，单位为字节/秒，默认无限速，参数格式如：`user1:102400 user2:204800` |
| logs_path | 日志目录 |
| disable_logs | 是否禁止生成日志 |
| disable_http | 是否禁止 `http` 协议 |
| disable_socks | 是否禁止 `socks5`/`socks4` 协议 |
| disable_insecure | 是否禁止非安全连接(即只允许 `ssl/tls` 连接) |
| disable_udp | 是否禁止 udp 协议支持 |
| scramble | 数据是否启用噪声加扰(即通过随机噪声数据混淆数据传输)，此选项需要在2端同时开启 |
| noise_length | 在启用 `scramble` 的时候，随机发送的噪声数据最大长度，默认最大长度为4k |

## 静态文件 http 服务器(可配置为云音乐播放器)

`proxy server` 不仅是一个 `proxy` 服务器，同时还可以做为一个真实的静态文件 `http` 服务，且支持 `http range`，所以也可以作为 `http` 音视频文件服务器，播放器播放 `http` 音视频文件时通过 `http` 的 `bytes range` 协议进行 `seek`（快进快退），使用方法如下

``` bash
./proxy_server --autoindex true --http_doc /user/music --server_listen 0.0.0.0:1080
```

在目录 `/user/music` 中添加音乐文件（以及`.lrc`歌词文件），复制本项目中 `example/music` 下的 `index.html` 放入 `/user/music` 目录中，然后使用浏览器打开地址 `http://localhost:1080/` 运行效果（浏览器打开）：

![image](https://github.com/user-attachments/assets/3910015e-f4d6-4162-912b-8b7594c41d88)

当然，如果你希望只有你自己可以访问，那么在上面命令行添加参数 `--htpasswd true` 并通过参数 `--auth_users` 配置登陆用户名和密码，打开页面时就会要求验证登陆。

本人的 [blog](https://www.jackarain.org) 就是直接运行在 `proxy_server` 上的静态页面(由 `jekyll` 生成)，它同时也是我的代理服务，还是我的 [音乐播放器](https://www.jackarain.org/music/index.html)

## 其它相关

[在路由器上配置全局代理科学上网](https://github.com/Jackarain/proxy/wiki/%E5%9C%A8%E8%B7%AF%E7%94%B1%E5%99%A8%E4%B8%8A%E5%88%9B%E5%BB%BA%E5%85%A8%E5%B1%80%E4%BB%A3%E7%90%86%E6%A8%A1%E5%BC%8F)

---

有任何问题可加tg账号: [https://t.me/jackarain](https://t.me/jackarain) 或tg群组: [https://t.me/joinchat/C3WytT4RMvJ4lqxiJiIVhg](https://t.me/joinchat/C3WytT4RMvJ4lqxiJiIVhg)
