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
//   socks5_proxy    : 代理接口的 SOCKS5 实现，CONNECT 建立 TCP 隧道，
//                     UDP ASSOCIATE 建立带封装头的中继；
//   认证             : 支持 RFC1929 用户名/密码认证（未配置则 NO AUTH）。

#include "proxy.hpp"

#include <boost/asio.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tun2socks_example {

using error_code = boost::system::error_code;

namespace detail {

// SOCKS5 地址类型
enum atyp : uint8_t {
    atyp_ipv4 = 0x01,
    atyp_domain = 0x03,
    atyp_ipv6 = 0x04,
};

inline std::vector<uint8_t> encode_address(
    const std::string& host, uint16_t port)
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

inline void throw_if(error_code ec, const char* what)
{
    if (ec) {
        throw boost::system::system_error(ec, what);
    }
}

// 版本协商 + 认证：user 为空时仅宣告 NO AUTH，否则宣告 NO AUTH 与
// USER/PASS 并支持 RFC1929 子协商。
inline net::awaitable<void> negotiate_auth(net::ip::tcp::socket& sock,
    const std::string& user, const std::string& pass)
{
    std::vector<uint8_t> hello;
    if (user.empty()) {
        hello = {5, 1, 0x00};
    } else {
        hello = {5, 2, 0x00, 0x02};
    }

    error_code ec;
    co_await net::async_write(sock, net::buffer(hello),
        net::redirect_error(net::use_awaitable, ec));
    throw_if(ec, "socks5: send greeting");

    std::array<uint8_t, 2> resp{};
    co_await net::async_read(sock, net::buffer(resp),
        net::redirect_error(net::use_awaitable, ec));
    throw_if(ec, "socks5: read method");

    if (resp[1] == 0x00) {
        co_return;
    }
    if (resp[1] != 0x02) {
        throw boost::system::system_error(
            net::error::fault, "socks5: auth method not accepted");
    }

    std::vector<uint8_t> auth{0x01};
    auth.push_back(static_cast<uint8_t>(user.size()));
    auth.insert(auth.end(), user.begin(), user.end());
    auth.push_back(static_cast<uint8_t>(pass.size()));
    auth.insert(auth.end(), pass.begin(), pass.end());
    co_await net::async_write(sock, net::buffer(auth),
        net::redirect_error(net::use_awaitable, ec));
    throw_if(ec, "socks5: send auth");

    std::array<uint8_t, 2> auth_resp{};
    co_await net::async_read(sock, net::buffer(auth_resp),
        net::redirect_error(net::use_awaitable, ec));
    throw_if(ec, "socks5: read auth");
    if (auth_resp[1] != 0x00) {
        throw boost::system::system_error(
            net::error::access_denied, "socks5: authentication failed");
    }
}

} // namespace detail

// 在已连接的代理 socket 上完成 SOCKS5 CONNECT。
inline net::awaitable<void> socks5_connect(net::ip::tcp::socket& sock,
    const std::string& target_host, uint16_t target_port,
    const std::string& user, const std::string& pass)
{
    co_await detail::negotiate_auth(sock, user, pass);

    error_code ec;
    std::vector<uint8_t> req{5, 1, 0};
    auto addr = detail::encode_address(target_host, target_port);
    req.insert(req.end(), addr.begin(), addr.end());
    co_await net::async_write(sock, net::buffer(req),
        net::redirect_error(net::use_awaitable, ec));
    detail::throw_if(ec, "socks5: send connect");

    std::array<uint8_t, 4> head{};
    co_await net::async_read(sock, net::buffer(head),
        net::redirect_error(net::use_awaitable, ec));
    detail::throw_if(ec, "socks5: read reply");
    if (head[1] != 0) {
        throw boost::system::system_error(
            net::error::connection_refused, "socks5: connect failed");
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
        throw boost::system::system_error(
            net::error::fault, "socks5: bad atyp");
    }
    if (rest > 0) {
        std::vector<uint8_t> tmp(rest);
        co_await net::async_read(sock, net::buffer(tmp),
            net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5: read reply tail");
    }
}

// 读取 SOCKS5 回复中的 BND.ADDR/BND.PORT（UDP ASSOCIATE 中继端点）。
inline net::awaitable<net::ip::udp::endpoint> socks5_read_bnd(
    net::ip::tcp::socket& control)
{
    error_code ec;
    std::array<uint8_t, 4> head{};
    co_await net::async_read(control, net::buffer(head),
        net::redirect_error(net::use_awaitable, ec));
    detail::throw_if(ec, "socks5-udp: reply");
    if (head[1] != 0) {
        throw boost::system::system_error(
            net::error::connection_refused, "socks5-udp: associate failed");
    }

    net::ip::udp::endpoint relay;
    switch (head[3]) {
    case detail::atyp_ipv4: {
        std::array<uint8_t, 6> rest{};
        co_await net::async_read(control, net::buffer(rest),
            net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: bnd addr");
        net::ip::address_v4::bytes_type b{rest[0], rest[1], rest[2], rest[3]};
        relay = {net::ip::address_v4(b),
            static_cast<uint16_t>((rest[4] << 8) | rest[5])};
        break;
    }
    case detail::atyp_domain: {
        std::array<uint8_t, 1> len{};
        co_await net::async_read(control, net::buffer(len),
            net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: bnd len");
        std::vector<uint8_t> rest(len[0] + 2);
        co_await net::async_read(control, net::buffer(rest),
            net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: bnd addr");
        std::string host(rest.begin(), rest.end() - 2);
        relay = {net::ip::make_address(host),
            static_cast<uint16_t>((rest[len[0]] << 8) | rest[len[0] + 1])};
        break;
    }
    case detail::atyp_ipv6: {
        std::array<uint8_t, 18> rest{};
        co_await net::async_read(control, net::buffer(rest),
            net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: bnd addr");
        net::ip::address_v6::bytes_type b{};
        std::copy(rest.begin(), rest.begin() + 16, b.begin());
        relay = {net::ip::address_v6(b),
            static_cast<uint16_t>((rest[16] << 8) | rest[17])};
        break;
    }
    default:
        throw boost::system::system_error(
            net::error::fault, "socks5-udp: bad atyp");
    }
    co_return relay;
}

// ---- UDP ASSOCIATE 中继 ----
class socks5_udp_relay : public udp_relay
{
public:
    explicit socks5_udp_relay(net::any_io_executor ex)
        : sock_(ex)
    {
    }

    // 发起 UDP ASSOCIATE 并将数据 socket 绑定到中继端点；控制连接随
    // 对象存活而保持。
    net::awaitable<void> associate(net::ip::tcp::endpoint proxy,
        const std::string& user, const std::string& pass,
        std::chrono::milliseconds timeout)
    {
        auto ex = co_await net::this_coro::executor;
        auto control = std::make_shared<net::ip::tcp::socket>(
            co_await connect_tcp_with_timeout(ex, proxy, timeout));

        co_await detail::negotiate_auth(*control, user, pass);

        // UDP ASSOCIATE：BND.ADDR 请求 127.0.0.1:0（0.0.0.0:0 亦可）。
        error_code ec;
        std::array<uint8_t, 10> req{5, 3, 0, 1, 127, 0, 0, 1, 0, 0};
        co_await net::async_write(*control, net::buffer(req),
            net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: associate");

        auto relay = co_await socks5_read_bnd(*control);
        co_await sock_.async_connect(relay,
            net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: bind relay");
        control_ = std::move(control);
    }

    net::awaitable<void> send(std::vector<uint8_t> payload,
        net::ip::udp::endpoint target) override
    {
        auto hdr = detail::encode_address(
            target.address().to_string(), target.port());
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

    net::awaitable<std::pair<std::vector<uint8_t>, net::ip::udp::endpoint>>
    receive() override
    {
        std::array<uint8_t, 65535> buf{};
        error_code ec;
        const size_t n = co_await sock_.async_receive(
            net::buffer(buf), net::redirect_error(net::use_awaitable, ec));
        detail::throw_if(ec, "socks5-udp: receive");

        const uint8_t* p = buf.data();
        if (n < 4 || p[0] != 0 || p[1] != 0 || p[2] != 0) {
            throw boost::system::system_error(
                net::error::fault, "socks5-udp: bad header");
        }

        size_t off = 3;
        net::ip::udp::endpoint target;
        switch (p[off++]) {
        case detail::atyp_ipv4: {
            if (n < off + 6) {
                throw boost::system::system_error(
                    net::error::fault, "socks5-udp: short packet");
            }
            net::ip::address_v4::bytes_type b{
                p[off], p[off + 1], p[off + 2], p[off + 3]};
            off += 4;
            target = {net::ip::address_v4(b),
                static_cast<uint16_t>((p[off] << 8) | p[off + 1])};
            off += 2;
            break;
        }
        case detail::atyp_domain: {
            if (n < off + 1) {
                throw boost::system::system_error(
                    net::error::fault, "socks5-udp: short packet");
            }
            const uint8_t dlen = p[off++];
            if (n < off + dlen + 2) {
                throw boost::system::system_error(
                    net::error::fault, "socks5-udp: short packet");
            }
            std::string host(reinterpret_cast<const char*>(p + off), dlen);
            off += dlen;
            target = {net::ip::make_address(host),
                static_cast<uint16_t>((p[off] << 8) | p[off + 1])};
            off += 2;
            break;
        }
        case detail::atyp_ipv6: {
            if (n < off + 18) {
                throw boost::system::system_error(
                    net::error::fault, "socks5-udp: short packet");
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
            throw boost::system::system_error(
                net::error::fault, "socks5-udp: bad atyp");
        }
        co_return std::make_pair(
            std::vector<uint8_t>(p + off, p + n), target);
    }

    void close() override
    {
        boost::system::error_code ec;
        sock_.close(ec);
        if (control_) {
            control_->close(ec);
        }
    }

private:
    net::ip::udp::socket sock_;
    std::shared_ptr<net::ip::tcp::socket> control_;
};

// ---- 代理接口实现 ----
class socks5_proxy : public proxy
{
public:
    socks5_proxy(proxy_config cfg, net::any_io_executor ex,
        std::chrono::milliseconds connect_timeout)
        : cfg_(std::move(cfg))
        , ex_(ex)
        , connect_timeout_(connect_timeout)
    {
    }

    net::awaitable<net::ip::tcp::socket> connect(
        std::string host, uint16_t port) override
    {
        auto sock = co_await connect_tcp_with_timeout(
            ex_, cfg_.endpoint, connect_timeout_);
        co_await socks5_connect(sock, host, port, cfg_.user, cfg_.pass);
        co_return sock;
    }

    net::awaitable<std::shared_ptr<udp_relay>> associate_udp() override
    {
        auto relay = std::make_shared<socks5_udp_relay>(ex_);
        co_await relay->associate(
            cfg_.endpoint, cfg_.user, cfg_.pass, connect_timeout_);
        co_return relay;
    }

    std::string name() const override
    {
        return "socks5";
    }

    bool supports_udp() const noexcept override
    {
        return true;
    }

private:
    proxy_config cfg_;
    net::any_io_executor ex_;
    std::chrono::milliseconds connect_timeout_;
};

} // namespace tun2socks_example
