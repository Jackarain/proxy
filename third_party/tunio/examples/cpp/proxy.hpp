//
// proxy.hpp
// ~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

// 代理抽象层：把上游连接与 UDP 中继从具体协议（socks5/http/direct/reject）
// 中解耦。tun2socks 只依赖本文件的 proxy / udp_relay 接口。

#include <boost/asio.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tun2socks_example {

namespace net = boost::asio;

// 代理配置：[scheme://][user:pass@]host[:port]
//
// endpoint 由调用方在解析 host/port 后填充（socks5/http 需要；direct/reject
// 忽略）。user/pass 为空表示不启用认证。
struct proxy_config
{
    std::string scheme = "socks5";
    std::string host;
    uint16_t port = 0;
    std::string user;
    std::string pass;
    net::ip::tcp::endpoint endpoint;
};

// 解析代理 URL：无 scheme 时默认 socks5，缺省端口按 scheme 取值
// （socks5=1080，http=8080）。成功返回 true，失败通过 err 返回原因。
bool parse_proxy_url(
    std::string_view text, proxy_config& out, std::string& err);

// UDP 中继：屏蔽代理差异（socks5 带 RSV/ATYP 封装头，direct 无封装）。
// send/receive 均为一个完整数据报，receive 返回远端端点。
class udp_relay
{
public:
    virtual ~udp_relay() = default;

    virtual net::awaitable<void> send(
        std::vector<uint8_t> payload, net::ip::udp::endpoint target) = 0;

    virtual net::awaitable<std::pair<std::vector<uint8_t>,
        net::ip::udp::endpoint>> receive() = 0;

    virtual void close() = 0;
};

// 代理抽象：connect 建立到目标地址的 TCP 隧道；associate_udp 建立 UDP
// 中继，返回 nullptr 表示该代理不支持 UDP。
class proxy
{
public:
    virtual ~proxy() = default;

    virtual net::awaitable<net::ip::tcp::socket> connect(
        std::string host, uint16_t port) = 0;

    virtual net::awaitable<std::shared_ptr<udp_relay>> associate_udp() = 0;

    virtual std::string name() const = 0;

    virtual bool supports_udp() const noexcept = 0;
};

// 按 proxy_config.scheme 创建代理实现。
std::shared_ptr<proxy> make_proxy(const proxy_config& cfg,
    net::any_io_executor ex, std::chrono::milliseconds connect_timeout);

// 带超时的 TCP 连接，供各代理实现复用；超时或失败抛 system_error。
net::awaitable<net::ip::tcp::socket> connect_tcp_with_timeout(
    net::any_io_executor ex, net::ip::tcp::endpoint target,
    std::chrono::milliseconds timeout);

} // namespace tun2socks_example
