//
// direct_proxy.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

// 直连与拒绝两种代理实现：
//   direct : 不经任何代理，直接连接目标（TCP）/ 直接收发数据报（UDP）；
//   reject : 拒绝全部连接与 UDP 会话，供测试与黑名单场景使用。

#include "proxy.hpp"

#include <boost/asio.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace tun2socks_example {

class direct_udp_relay : public udp_relay
{
public:
    explicit direct_udp_relay(net::any_io_executor ex)
        : sock_(ex)
    {
    }

    net::awaitable<void> send(std::vector<uint8_t> payload,
        net::ip::udp::endpoint target) override
    {
        if (!sock_.is_open()) {
            boost::system::error_code ec;
            sock_.open(target.protocol(), ec);
            if (ec) {
                throw boost::system::system_error(ec, "direct-udp: open");
            }
        }
        co_await sock_.async_send_to(
            net::buffer(payload), target, net::use_awaitable);
    }

    net::awaitable<std::pair<std::vector<uint8_t>, net::ip::udp::endpoint>>
    receive() override
    {
        std::array<uint8_t, 65535> buf{};
        net::ip::udp::endpoint from;
        const size_t n = co_await sock_.async_receive_from(
            net::buffer(buf), from, net::use_awaitable);
        co_return std::make_pair(
            std::vector<uint8_t>(buf.data(), buf.data() + n), from);
    }

    void close() override
    {
        boost::system::error_code ec;
        sock_.close(ec);
    }

private:
    net::ip::udp::socket sock_;
};

class direct_proxy : public proxy
{
public:
    direct_proxy(net::any_io_executor ex, std::chrono::milliseconds timeout)
        : ex_(ex)
        , connect_timeout_(timeout)
    {
    }

    net::awaitable<net::ip::tcp::socket> connect(
        std::string host, uint16_t port) override
    {
        boost::system::error_code ec;
        auto addr = net::ip::make_address(host, ec);
        if (ec) {
            throw boost::system::system_error(
                ec, "direct: bad address " + host);
        }
        co_return co_await connect_tcp_with_timeout(
            ex_, net::ip::tcp::endpoint(addr, port), connect_timeout_);
    }

    net::awaitable<std::shared_ptr<udp_relay>> associate_udp() override
    {
        co_return std::make_shared<direct_udp_relay>(ex_);
    }

    std::string name() const override
    {
        return "direct";
    }

    bool supports_udp() const noexcept override
    {
        return true;
    }

private:
    net::any_io_executor ex_;
    std::chrono::milliseconds connect_timeout_;
};

class reject_proxy : public proxy
{
public:
    net::awaitable<net::ip::tcp::socket> connect(
        std::string host, uint16_t port) override
    {
        throw boost::system::system_error(net::error::connection_refused,
            "reject: " + host + ":" + std::to_string(port));
        co_return net::ip::tcp::socket(co_await net::this_coro::executor);
    }

    net::awaitable<std::shared_ptr<udp_relay>> associate_udp() override
    {
        co_return nullptr;
    }

    std::string name() const override
    {
        return "reject";
    }

    bool supports_udp() const noexcept override
    {
        return false;
    }
};

} // namespace tun2socks_example
