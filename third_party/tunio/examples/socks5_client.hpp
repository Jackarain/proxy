//
// socks5_client.hpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

// SOCKS5 客户端（示例应用层，仅依赖 Boost.Asio + C++20 协程）
//
//   socks5_connect        : CONNECT 命令，建立到目标地址的 TCP 隧道
//   socks5_udp_relay      : UDP ASSOCIATE + 带 SOCKS5 UDP 头的数据报收发
//
// 握手采用标准 SOCKS5 协议（NO AUTH）。

#include <boost/asio.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace tun2socks_example {

namespace net = boost::asio;

using error_code = boost::system::error_code;

namespace detail {

// SOCKS5 地址类型
enum atyp : uint8_t {
    atyp_ipv4 = 0x01,
    atyp_domain = 0x03,
    atyp_ipv6 = 0x04,
};

inline std::vector<uint8_t> encode_address(const std::string &host,
    uint16_t port)
{
    std::vector<uint8_t> out;
    error_code ec;
    auto v4 = net::ip::make_address_v4(host, ec);
    if (!ec) {
        out.push_back(atyp_ipv4);
        auto b = v4.to_bytes();
        out.insert(out.end(), b.begin(), b.end());
    } else {
        ec.clear();
        auto v6 = net::ip::make_address_v6(host, ec);
        if (!ec) {
            out.push_back(atyp_ipv6);
            auto b = v6.to_bytes();
            out.insert(out.end(), b.begin(), b.end());
        } else {
            out.push_back(atyp_domain);
            out.push_back(static_cast<uint8_t>(host.size()));
            out.insert(out.end(), host.begin(), host.end());
        }
    }
    out.push_back(static_cast<uint8_t>(port >> 8));
    out.push_back(static_cast<uint8_t>(port & 0xff));
    return out;
}

inline void throw_if(error_code ec, const char *what)
{
    if (ec) {
        throw boost::system::system_error(ec, what);
    }
}

} // namespace detail

// ---- TCP CONNECT：返回已连接的目标隧道 ----
inline net::awaitable<net::ip::tcp::socket>
socks5_connect(net::ip::tcp::endpoint proxy, std::string target_host,
    uint16_t target_port)
{
    auto ex = co_await net::this_coro::executor;
    net::ip::tcp::socket sock(ex);
    error_code ec;
    co_await sock.async_connect(proxy,
        net::redirect_error(net::use_awaitable, ec));
    detail::throw_if(ec, "socks5: connect proxy");

    // 版本协商：5 / 1 种方法 / NO AUTH
    std::array<uint8_t, 3> hello{5, 1, 0};
    co_await net::async_write(sock, net::buffer(hello),
        net::redirect_error(net::use_awaitable, ec));
    detail::throw_if(ec, "socks5: send greeting");
    std::array<uint8_t, 2> resp{};
    co_await net::async_read(sock, net::buffer(resp),
        net::redirect_error(net::use_awaitable, ec));
    detail::throw_if(ec, "socks5: read method");
    if (resp[1] != 0) {
        throw boost::system::system_error(net::error::fault,
            "socks5: auth method not accepted");
    }

    // CONNECT 请求
    std::vector<uint8_t> req{5, 1, 0};
    auto addr = detail::encode_address(target_host, target_port);
    req.insert(req.end(), addr.begin(), addr.end());
    co_await net::async_write(sock, net::buffer(req),
        net::redirect_error(net::use_awaitable, ec));
    detail::throw_if(ec, "socks5: send connect");

    // 解析回复：头部 4 字节 + 按 ATYP 读取剩余
    std::array<uint8_t, 4> head{};
    co_await net::async_read(sock, net::buffer(head),
        net::redirect_error(net::use_awaitable, ec));
    detail::throw_if(ec, "socks5: read reply");
    if (head[1] != 0) {
        throw boost::system::system_error(net::error::connection_refused,
            "socks5: connect failed");
    }
    size_t rest = 0;
    switch (head[3]) {
    case detail::atyp_ipv4:
        rest = 4 + 2;
        break;
    case detail::atyp_ipv6:
        rest = 16 + 2;
        break;
    case detail::atyp_domain: {
        std::array<uint8_t, 1> len{};
        co_await net::async_read(sock, net::buffer(len),
            net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5: read domain len");
        rest = len[0] + 2;
        break;
    }
    default:
        throw boost::system::system_error(net::error::fault,
            "socks5: bad atyp");
    }
    if (rest > 0) {
        std::vector<uint8_t> tmp(rest);
        co_await net::async_read(sock, net::buffer(tmp),
            net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5: read reply tail");
    }
    co_return sock;
}

// ---- UDP ASSOCIATE 中继 ----
class socks5_udp_relay
{
public:
    explicit socks5_udp_relay(net::any_io_executor ex)
        : sock_(ex)
    {
    }

    // 发起 UDP ASSOCIATE 并绑定中继端点；控制连接随对象存活而保持
    net::awaitable<net::ip::udp::endpoint>
    associate(net::ip::tcp::endpoint proxy)
    {
        auto ex = co_await net::this_coro::executor;
        auto control = std::make_shared<net::ip::tcp::socket>(ex);
        error_code ec;
        co_await control->async_connect(
            proxy, net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: connect proxy");

        std::array<uint8_t, 3> hello{5, 1, 0};
        co_await net::async_write(*control, net::buffer(hello),
            net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: greeting");
        std::array<uint8_t, 2> resp{};
        co_await net::async_read(*control, net::buffer(resp),
            net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: method");
        if (resp[1] != 0) {
            throw boost::system::system_error(
                net::error::fault, "socks5-udp: auth method not accepted");
        }

        // UDP ASSOCIATE：BND.ADDR 为 127.0.0.1:0（0.0.0.0:0 亦可）
        std::array<uint8_t, 10> req{5, 3, 0, 1, 127, 0, 0, 1, 0, 0};
        co_await net::async_write(*control, net::buffer(req),
            net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: associate");

        std::array<uint8_t, 4> head{};
        co_await net::async_read(*control, net::buffer(head),
            net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: reply");
        if (head[1] != 0) {
            throw boost::system::system_error(net::error::connection_refused,
                "socks5-udp: associate failed");
        }

        net::ip::udp::endpoint relay;
        switch (head[3]) {
        case detail::atyp_ipv4: {
            std::array<uint8_t, 6> rest{};
            co_await net::async_read(
                *control, net::buffer(rest),
                net::redirect_error(net::use_awaitable, ec));
            detail::throw_if(ec, "socks5-udp: bnd addr");
            net::ip::address_v4::bytes_type b{rest[0], rest[1], rest[2],
                rest[3]};
            relay = {net::ip::address_v4(b),
                static_cast<uint16_t>((rest[4] << 8) | rest[5])};
            break;
        }
        case detail::atyp_domain: {
            std::array<uint8_t, 1> len{};
            co_await net::async_read(
                *control, net::buffer(len),
                net::redirect_error(net::use_awaitable, ec));
            detail::throw_if(ec, "socks5-udp: bnd len");
            std::vector<uint8_t> rest(len[0] + 2);
            co_await net::async_read(
                *control, net::buffer(rest),
                net::redirect_error(net::use_awaitable, ec));
            detail::throw_if(ec, "socks5-udp: bnd addr");
            std::string host(rest.begin(), rest.end() - 2);
            relay = {
                net::ip::make_address(host),
                static_cast<uint16_t>((rest[len[0]] << 8) | rest[len[0] + 1])};
            break;
        }
        case detail::atyp_ipv6: {
            std::array<uint8_t, 18> rest{};
            co_await net::async_read(
                *control, net::buffer(rest),
                net::redirect_error(net::use_awaitable, ec));
            detail::throw_if(ec, "socks5-udp: bnd addr");
            net::ip::address_v6::bytes_type b{};
            std::copy(rest.begin(), rest.begin() + 16, b.begin());
            relay = {net::ip::address_v6(b),
                static_cast<uint16_t>((rest[16] << 8) | rest[17])};
            break;
        }
        default:
            throw boost::system::system_error(net::error::fault,
                "socks5-udp: bad atyp");
        }

        // UDP 套接字绑定中继端点（connect 后仅与中继通信，符合 SOCKS5 语义）
        co_await sock_.async_connect(
            relay, net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: bind relay");
        control_ = std::move(control);
        co_return relay;
    }

    // 封装并发送数据报
    net::awaitable<void> send_to(std::vector<uint8_t> payload,
        net::ip::udp::endpoint target)
    {
        auto hdr =
            detail::encode_address(target.address().to_string(), target.port());
        std::vector<uint8_t> pkt;
        pkt.reserve(3 + hdr.size() + payload.size());
        pkt.insert(pkt.end(), {0, 0, 0}); // RSV, FRAG
        pkt.insert(pkt.end(), hdr.begin(), hdr.end());
        pkt.insert(pkt.end(), payload.begin(), payload.end());
        error_code ec;
        co_await sock_.async_send(net::buffer(pkt),
            net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: send");
    }

    // 接收并剥离封装，返回 (payload, 原始目标地址)
    net::awaitable<std::pair<std::vector<uint8_t>, net::ip::udp::endpoint>>
    receive_from()
    {
        std::array<uint8_t, 2048> buf{};
        error_code ec;
        const size_t n = co_await sock_.async_receive(
            net::buffer(buf), net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: receive");
        const uint8_t *p = buf.data();
        if (n < 4 || p[0] != 0 || p[1] != 0 || p[2] != 0) {
            throw boost::system::system_error(net::error::fault,
                "socks5-udp: bad header");
        }
        size_t off = 3;
        net::ip::udp::endpoint target;
        switch (p[off++]) {
        case detail::atyp_ipv4: {
            if (n < off + 6) {
                throw boost::system::system_error(net::error::fault,
                    "socks5-udp: short packet");
            }
            net::ip::address_v4::bytes_type b{p[off], p[off + 1], p[off + 2],
                p[off + 3]};
            off += 4;
            target = {net::ip::address_v4(b),
                static_cast<uint16_t>((p[off] << 8) | p[off + 1])};
            off += 2;
            break;
        }
        case detail::atyp_domain: {
            if (n < off + 1) {
                throw boost::system::system_error(net::error::fault,
                    "socks5-udp: short packet");
            }
            const uint8_t dlen = p[off++];
            if (n < off + dlen + 2) {
                throw boost::system::system_error(net::error::fault,
                    "socks5-udp: short packet");
            }
            std::string host(reinterpret_cast<const char *>(p + off), dlen);
            off += dlen;
            target = {net::ip::make_address(host),
                static_cast<uint16_t>((p[off] << 8) | p[off + 1])};
            off += 2;
            break;
        }
        case detail::atyp_ipv6: {
            if (n < off + 18) {
                throw boost::system::system_error(net::error::fault,
                    "socks5-udp: short packet");
            }
            net::ip::address_v6::bytes_type b{};
            std::copy(p + off, p + off + 16, b.begin());
            off += 16;
            target = {net::ip::address_v6(b),
                static_cast<uint16_t>((p[off] << 8) | p[off + 1])};
            off += 2;
            break;
        }
        default:
            throw boost::system::system_error(net::error::fault,
                "socks5-udp: bad atyp");
        }
        co_return std::make_pair(std::vector<uint8_t>(p + off, p + n), target);
    }

    void close()
    {
        sock_.close();
        if (control_) {
            control_->close();
        }
    }

private:
    net::ip::udp::socket sock_;
    std::shared_ptr<net::ip::tcp::socket> control_;
};

} // namespace tun2socks_example
