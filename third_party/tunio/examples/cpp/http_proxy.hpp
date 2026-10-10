//
// http_proxy.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

// HTTP 代理实现：使用 CONNECT 方法建立 TCP 隧道，支持 Basic 认证。
// HTTP 代理不转发 UDP，associate_udp 返回 nullptr。

#include "proxy.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace tun2socks_example {

namespace detail {

inline std::string base64_encode(const std::string& input)
{
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= input.size()) {
        const uint32_t v = (static_cast<uint8_t>(input[i]) << 16) |
            (static_cast<uint8_t>(input[i + 1]) << 8) |
            static_cast<uint8_t>(input[i + 2]);
        out.push_back(table[(v >> 18) & 0x3f]);
        out.push_back(table[(v >> 12) & 0x3f]);
        out.push_back(table[(v >> 6) & 0x3f]);
        out.push_back(table[v & 0x3f]);
        i += 3;
    }

    const size_t rem = input.size() - i;
    if (rem == 1) {
        const uint32_t v = static_cast<uint8_t>(input[i]) << 16;
        out.push_back(table[(v >> 18) & 0x3f]);
        out.push_back(table[(v >> 12) & 0x3f]);
        out.push_back('=');
        out.push_back('=');
    } else if (rem == 2) {
        const uint32_t v = (static_cast<uint8_t>(input[i]) << 16) |
            (static_cast<uint8_t>(input[i + 1]) << 8);
        out.push_back(table[(v >> 18) & 0x3f]);
        out.push_back(table[(v >> 12) & 0x3f]);
        out.push_back(table[(v >> 6) & 0x3f]);
        out.push_back('=');
    }
    return out;
}

} // namespace detail

class http_proxy : public proxy
{
public:
    http_proxy(proxy_config cfg, net::any_io_executor ex,
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

        const std::string authority = host + ":" + std::to_string(port);
        std::string req = "CONNECT " + authority + " HTTP/1.1\r\n";
        req += "Host: " + authority + "\r\n";
        if (!cfg_.user.empty()) {
            req += "Proxy-Authorization: Basic " +
                detail::base64_encode(cfg_.user + ":" + cfg_.pass) + "\r\n";
        }
        req += "Proxy-Connection: Keep-Alive\r\n\r\n";

        boost::system::error_code ec;
        co_await net::async_write(sock, net::buffer(req),
            net::redirect_error(net::use_awaitable, ec));
        if (ec) {
            throw boost::system::system_error(ec, "http: send connect");
        }

        std::string resp;
        co_await net::async_read_until(sock, net::dynamic_buffer(resp),
            "\r\n\r\n", net::redirect_error(net::use_awaitable, ec));
        if (ec) {
            throw boost::system::system_error(ec, "http: read reply");
        }

        // 状态行形如 "HTTP/1.1 200 Connection established"
        const auto sp = resp.find(' ');
        if (sp == std::string::npos || resp.size() < sp + 4 ||
            resp.compare(sp + 1, 3, "200") != 0) {
            throw boost::system::system_error(net::error::connection_refused,
                "http: proxy rejected CONNECT");
        }
        co_return sock;
    }

    net::awaitable<std::shared_ptr<udp_relay>> associate_udp() override
    {
        co_return nullptr;
    }

    std::string name() const override
    {
        return "http";
    }

    bool supports_udp() const noexcept override
    {
        return false;
    }

private:
    proxy_config cfg_;
    net::any_io_executor ex_;
    std::chrono::milliseconds connect_timeout_;
};

} // namespace tun2socks_example
