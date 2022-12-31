一个使用现代 c++ 实现的 proxy 的实现
<BR>
[![CircleCI](https://dl.circleci.com/status-badge/img/gh/Jackarain/proxy/tree/master.svg?style=shield)](https://dl.circleci.com/status-badge/redirect/gh/Jackarain/proxy/tree/master)
[![actions workflow](https://github.com/jackarain/proxy/actions/workflows/Build.yml/badge.svg)](https://github.com/Jackarain/proxy/actions)
<BR>
<BR>
=======================================

使用 **C++20协程** ，支持标准socks4/socks4a/socks5/http的server/client proxy实现，并且client与server之间可配置通过ssl加密通信，可配置如下架构：
<BR>

~~~

                  +--------------+     |      +--------------+
  browser/app --> | proxy server | ---ssl---> | proxy server | --> target server
                  +--------------+     |      +--------------+
                       local          GFW          remote
~~~
<BR>

以及比 trojan 更通用的 https proxy 服务，因为 https proxy 在很多环境下可以直接使用而不需要安装任何东西就可以使用，如 shell 中声名 HTTPS_PROXY 环境变量就可以了，这时的 proxy server 将伪装成一个 nginx 服务。
<BR>

~~~
                      |            +--------------+
  browser/app --> https proxy ---> | proxy server | --> target server
                      |            +--------------+
    local            GFW               remote
~~~

同时 proxy server 能接收 socks/http 代理请求。
<BR>

具体使用参考 example，里面提供了较完整功能的 proxy 程序及编程示例。

<BR>
有任何问题可加tg账号: https://t.me/jackarain 或tg群组: https://t.me/joinchat/C3WytT4RMvJ4lqxiJiIVhg
