一个使用 c++20 的 proxy 的实现
<BR>
[![CircleCI](https://dl.circleci.com/status-badge/img/gh/Jackarain/proxy/tree/master.svg?style=shield)](https://dl.circleci.com/status-badge/redirect/gh/Jackarain/proxy/tree/master)
[![actions workflow](https://github.com/jackarain/proxy/actions/workflows/Build.yml/badge.svg)](https://github.com/Jackarain/proxy/actions)
<BR>
<BR>
=======================================

使用 **C++20协程** 通过编写为数不多的代码，实现支持标准 socks4/socks4a/socks5/http/https 的 server/client proxy 实现，并且 client 与 server 之间可配置通过 ssl 加密通信，可配置多级代理，如下架构：
<BR>

~~~

                  +--------------+     |      +--------------+
  browser/app --> | proxy server | ---ssl---> | proxy server | --> target server
                  +--------------+     |      +--------------+
                       local          GFW          remote
~~~
<BR>

以及比 trojan 更通用的 https proxy 服务，因为 https proxy 在很多环境下可以直接使用而不需要安装任何东西就可以使用，如 shell 中声名 HTTPS_PROXY 环境变量就可以了，如果（配置了认证参数，无论访问的是 socks 或是 http）认证出错，这时的 proxy server 将伪装成一个 nginx 服务，返回 nginx 页面信息。
<BR>

~~~
                      |            +--------------+
  browser/app --> https proxy ---> | proxy server | --> target server
                      |            +--------------+
    local            GFW               remote
~~~

同时 proxy server 能接收 socks/http 代理请求，也能同时接受正常 http 请求，且所有功能（socks、http）亦可运行在同一端口上，由 server 主动探测协议类型。
<BR>

### Linux 平台下编译:

<br>

首先执行git克隆源码

```
git clone <source url>
```

然后进入源码目录，执行如下操作：

```
mkdir build && cd build
```
```
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

上面命令中，CMAKE_BUILD_TYPE=Debug指定了编译为Debug的类型，如果需要更好的性能，则需要编译为Release。

在cmake命令成功执行完成后，开始输入以下命令编译：

```
make
```

通常编译过程不会出现问题，如果出现任何问题，请联系作者，并将完整的错误信息保留并报告给作者。

成功编译后，example 生成的可执行程序将在 bin 目录下生成，其它相关 lib 的编译结果 .a 文件将在相应项目名称的目录下。


具体使用参考 example 中的 proxy server，它提供了较完整功能的 proxy server，它可以非常完美的作为 tun2socks 后级用于科学上网等事宜，example 中还有几个其它编程参考示例。

proxy server 不仅是一个 proxy 服务器，同时还可以做为一个真实的 静态文件 http 服务，运行效果（浏览器打开）：

<BR>

![image](https://user-images.githubusercontent.com/378220/211153949-74a84038-f899-4e48-99c7-bd6af6bef82d.png)


<BR>
有任何问题可加tg账号: https://t.me/jackarain 或tg群组: https://t.me/joinchat/C3WytT4RMvJ4lqxiJiIVhg
