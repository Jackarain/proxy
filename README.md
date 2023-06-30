# 一个使用 c++20 的 proxy 的实现

[![CircleCI](https://dl.circleci.com/status-badge/img/gh/Jackarain/proxy/tree/master.svg?style=shield)](https://dl.circleci.com/status-badge/redirect/gh/Jackarain/proxy/tree/master)
[![actions workflow](https://github.com/jackarain/proxy/actions/workflows/Build.yml/badge.svg)](https://github.com/Jackarain/proxy/actions)
\
\
使用 `C++20协程` 通过编写为数不多的代码，实现支持标准 `socks4`/`socks4a`/`socks5`/`http`/`https` 的 `server`/`client proxy` 实现，并且 `client` 与 `server` 之间可配置通过 `ssl` 加密通信，可配置多级代理，如下架构：

```bash

                  +--------------+     |      +--------------+
  browser/app --> | proxy server | ---ssl---> | proxy server | --> target server
                  +--------------+     |      +--------------+
                       local          GFW          remote
```

以及服务端可以使用比 `trojan` 更通用的 `https proxy` 协议服务，本质上 `trojan` 的做法就是利用了 `TLS` 而已但却不兼容标准 `https` 协议而失去了灵活生，这是因为 `https proxy` 在很多环境下可以直接使用而不需要安装任何东西就可以使用，如 `shell` 中声名 `HTTPS_PROXY` 环境变量指向 `proxy server` 的 `url` 就可以了，如果服务端被他人探测协议，只能得到它是一个 `nginx web` 服务的结论。因为在探测者不知 `proxy auth` 认证信息的前提下（服务端必须配置了认证参数，无论探测者访问的是 `socks`、 `http` 或是 `https`）认证出错，这时的 `proxy server` 将伪装成一个 `nginx` 服务，返回一个 `nginx` 出错的页面信息。

```bash
                      |            +--------------+
  browser/app --> https proxy ---> | proxy server | --> target server
                      |            +--------------+
    local            GFW               remote
```

`proxy server` 同时能接收 `socks`/`http` 代理请求（`http` 代理使用 `CONNECT`），它将所有功能（`socks`、`http`）可运行在同一端口上，服务端能自动甄别协议的类型，它通过处理请求协议的前几个字节判定为 `socks` 或 `http` 请求。

`proxy server`也能同时接受正常 `http` 请求（请求 `http` 静态文件），这时它和一个正常的静态 `web` 文件服务器是完全相同的功能。

## Linux、Windows 平台下编译:

首先执行 `git` 克隆源码（源码无外部依赖，`linux` 平台安装编译工具链: `clang 14` 或 `gcc 11` 以上、`cmake`，`windows` 平台安装 `vs2019` 以上，以及 `cmake`）

```bash
git clone <source url>
```

然后进入源码目录，执行如下操作：

```bash
mkdir build && cd build
```

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

上面命令中，`CMAKE_BUILD_TYPE=Debug` 指定了编译为 `Debug` 的类型，如果需要更好的性能，则需要编译为 `Release`。

在 `cmake` 命令成功执行完成后，开始输入以下命令编译：

```bash
make
```

通常编译过程不会出现问题，如果出现任何问题，请联系作者，并将完整的错误信息保留并报告给作者。

成功编译后，生成的可执行程序将在 `bin` 目录下，其它相关 `lib` 的编译结果 `.a` 文件将在相应项目名称的目录下。

也可以使用 `docker` 编译，在源码目录运行：

```bash
docker build . -t proxy:v1
```

即可自动编译

具体使用参考 `proxy server` 的参数，`proxy server` 它提供了较完整的功能，它可以非常完美的作为 `tun2socks` 后级用于科学上网等事宜，`example` 主要用于编程示例。

`proxy server` 不仅是一个 `proxy` 服务器，同时还可以做为一个真实的 静态文件 `http` 服务，且支持 `http range`，所以也可以作为 `http` 视频文件服务器，播放器播放 `http` 视频文件时通过 `http range` 进行 `seek`（快进快退），运行效果（浏览器打开）：

![image](https://user-images.githubusercontent.com/378220/211153949-74a84038-f899-4e48-99c7-bd6af6bef82d.png)

有任何问题可加tg账号: [https://t.me/jackarain](https://t.me/jackarain) 或tg群组: [https://t.me/joinchat/C3WytT4RMvJ4lqxiJiIVhg](https://t.me/joinchat/C3WytT4RMvJ4lqxiJiIVhg)
