# 一个使用 c++20 的 proxy 的高性能实现

[![CircleCI](https://dl.circleci.com/status-badge/img/gh/Jackarain/proxy/tree/master.svg?style=shield)](https://dl.circleci.com/status-badge/redirect/gh/Jackarain/proxy/tree/master)
[![actions workflow](https://github.com/jackarain/proxy/actions/workflows/Build.yml/badge.svg)](https://github.com/Jackarain/proxy/actions)

## proxy 项目简介

使用 `C++20协程` 通过编写为数不多的代码，实现具有极高性能且支持标准 `socks4`/`socks4a`/`socks5`/`http`/`https` 的 `server`/`client proxy` 实现，并且 `client` 与 `server` 之间可配置通过 `ssl` 加密通信，可配置多级代理，如下架构：

```

                  +--------------+     |      +--------------+
  browser/app --> | proxy server | ---ssl---> | proxy server | --> target server
                  +--------------+     |      +--------------+
                       local        Firewall       remote
```

以及服务端可以使用比 `trojan` 更通用的 `https proxy` 协议服务，本质上 `trojan` 的做法就是利用了 `TLS` 而已但却不兼容标准 `https` 协议而失去了灵活生，这是因为 `https proxy` 在很多环境下可以直接，使用而不需要安装任何东西就可以使用（`trojan` 必须要安装本地端），如 `shell` 中声名 `HTTPS_PROXY` 环境变量指向 `proxy server` 的 `url` 就可以了。

如果服务端被他人探测协议，只能得到它是一个 `nginx error page` 服务的结果，因为在探测者不知 `proxy auth` 认证信息的情况下，会导致认证出错，这时的 `proxy server` 将伪装成一个 `nginx` 服务，返回一个 `nginx error page` 页面信息。

```
                      |            +--------------+
  browser/app --> https proxy ---> | proxy server | --> target server
                      |            +--------------+
    local          Firewall             remote
```

`proxy server` 同时能接收 `socks`/`http` 代理请求（`http` 代理使用 `CONNECT`），它能将所有功能（`socks`、`http`）可运行在同一端口上，服务端能自动甄别协议的类型，它通过处理请求协议的前几个字节判定为 `socks` 或 `http` 请求。

`proxy server`也能同时接受正常 `http` 请求（请求 `http` 静态文件），这时它和一个正常的静态 `web` 文件服务器是完全相同的功能。

## 编译 proxy 项目

### 编译环境

要求操作系统安装以下工具:

``` text
版本控制: git

C++ 编译器: Linux 平台 gcc 11 以上, 或 clang 14 以上, windows 平台要求 visual studio 2019 或以上

构建工具: cmake 3.5 以上

如果希望使用 docker 编译, 应当安装好 docker 环境.
```

首先执行 `git` 克隆源码（源码无任何外部依赖）

```bash
git clone <source url>
```

克隆源码完代码后, 就可以开始编译项目了.

### 直接源码编译

进入源码目录，执行如下操作：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

在 `cmake` 命令成功执行完成后，开始输入以下命令编译：

```bash
cmake --build build
```

通常编译过程不会出现问题，如果出现任何问题，请联系作者，并将完整的错误信息保留并报告给作者。

成功编译后，生成的可执行程序将在 `bin` 目录下，其它相关 `lib` 的编译结果 `.a` 文件将在相应项目名称的目录下。

### 在 Docker 中编译

在源码目录运行：

```bash
docker build . -t proxy:v1
```

即可自动编译, 编译完成后, 可执行程序 `proxy_server` 可以在 `docker` 镜像的 `/usr/local/bin/` 目录下找到.

## 使用介绍

`proxy server` 它提供了较完整的功能，它可以非常完美的作为 `tun2socks` 后级用于全局代理上网等事宜，`example` 目录则主要用于编程示例。

`proxy server` 提供了以下参数, 参数除 config 之外, 即可于配置文件也可用于命令行:

|  参    数      | 用法解释  |
|  --------      | -----    |
| config | 可配置一个参数配置文件，如 `server.conf`，配置文件内容以`key=value`的`ini`配置文件的方式保存，如 `server_listen=[::0]:1080` |
| server_listen | 监听的地址以及端口, 格式为 `addr:port`, 支持 `ipv6-only` |
| reuse_port | 启用 `TCP SO_REUSEPORT` 选项, 这个选项只在 `linux` 平台有效 |
| happyeyeballs | 启用 `happyeyeballs` 连接算法, 默认为启用 |
| v4only | 仅使用 ipv4 向目标地址连接, 默认为不启用 |
| v6only | 仅使用 ipv6 向目标地址连接, 默认为不启用 |
| transparent | 启用透明代理, 默认禁用, 此选项仅 linux 平台有效 |
| so_mark | 用于发起向 `proxy_pass` 连接时设置 so_mark 以方便实现代理流量的策略路由, 仅在 transparent 启动时有效. |
| local_ip | 作为中间级联服务时, 用于向上游服务发起连接时所使用的本地网口 `ip` 地址 |
| auth_users | 认证信息列表, 客户端必须满足其中一对用户/密码才能握手通过, 默认用户密码是 `jack:1111`, 若需要设置为无需要认证代理模式, 必须置 `auth_users` 参数为 "" |
| proxy_pass | 当前服务作为中间级联服务时, `proxy_pass` 指定上游代理服务地址, 格式为 `url` 格式, 如果有认证信息并必须包含认证信息, 如: `https://jack:1111@example.com:1080/` |
| proxy_pass_ssl | 向 `proxy_pass` 指定的上游代理服务连接时, 是否通过 `ssl` 安全传输, 注意必须在上游代理服务启用 `ssl` 相关证书域名密钥等信息. |
| ssl_certificate_dir | `ssl` 证书密钥等存放目录, `proxy_server` 会自动查找这个目录下的 `ssl_crt.pem`/`ssl_crt.pwd`/`ssl_key.pem`/`ssl_dh.pem` 这几个固定文件名作为 `ssl` 服务配置, 这样 `ssl` 配置就只需要指定这一个参数 |
| ssl_certificate | 指定证书文件路径 |
| ssl_certificate_key | 指定证书密钥文件路径 |
| ssl_certificate_passwd | 指定证书密钥文件加密密码(如果证书密钥文件有加密), 可以是密码文件也可以直接是密码字符串 |
| ssl_dhparam | 指定 `DH` 参数文件路径(默认可以没有) |
| ssl_sni | 指定 SNI 用于客户端 `ssl` 连接使用(此选项除非你知道在做什么, 否则不需要关心) |
| ssl_ciphers | `ssl` 协议允许的加密算法列表(此选项除非你知道在做什么, 否则不需要关心) |
| ssl_prefer_server_ciphers | 优先使用 server 端加密算法(此选项除非你知道在做什么, 否则不需要关心) |
| http_doc | 启用 `http` 静态文件服务器, 指定静态文件目录 |
| autoindex | 在启用 `http` 静态文件服务的情况下, 启用 `autoindex` 将允许浏览器浏览文件目录列表, 与 `nginx` 的 `autoindex` 选项功能一样 |
| logs_path | 日志目录 |
| disable_logs | 是否禁止生成日志 |
| disable_http | 是否禁止 `http` 协议 |
| disable_socks | 是否禁止 `socks5`/`socks4` 协议 |
| disable_insecure | 是否禁止非安全连接(即 `ssl` 连接) |
| scramble | 数据是否启用噪声加扰(即通过随机噪声数据混淆数据传输), 此选项需要在2端同时开启 |
| noise_length | 在启用 `scramble` 的时候, 随机发送的噪声数据最大长度, 默认最大长度为4k |

## 静态文件 http 服务器

`proxy server` 不仅是一个 `proxy` 服务器，同时还可以做为一个真实的静态文件 `http` 服务，且支持 `http range`，所以也可以作为 `http` 视频文件服务器，播放器播放 `http` 视频文件时通过 `http range` 进行 `seek`（快进快退），使用方法如下

``` bash
./proxy_server --http_doc /user/doc --server_listen 0.0.0.0:8080
```

然后使用浏览器打开地址 `http://localhost:8080/` 运行效果（浏览器打开）：

![image](https://user-images.githubusercontent.com/378220/211153949-74a84038-f899-4e48-99c7-bd6af6bef82d.png)

有任何问题可加tg账号: [https://t.me/jackarain](https://t.me/jackarain) 或tg群组: [https://t.me/joinchat/C3WytT4RMvJ4lqxiJiIVhg](https://t.me/joinchat/C3WytT4RMvJ4lqxiJiIVhg)
