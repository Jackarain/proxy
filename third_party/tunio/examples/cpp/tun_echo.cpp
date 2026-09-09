//
// tun_echo.cpp
// ~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// tun_echo：DESIGN.md §10 的回显示例 —— TCP 桥接本地服务 + UDP 回显
//
// 用法示例：
//   sudo ./tun_echo --tun tun0 --ip 10.0.0.1 --netmask 255.255.255.0
//
// TCP：将虚拟连接转发到本机 127.0.0.1:echo 端口；
// UDP：直接在引擎层面回显数据报。
#include "tunio/tun_tcp_acceptor.hpp"
#include "tunio/tun_config.hpp"
#include "tunio/tun_tcp_socket.hpp"
#include "tunio/tun_udp_acceptor.hpp"
#include "tunio/tun_udp_socket.hpp"
#include "tunio/tunio.hpp"

#include <boost/asio.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {
namespace net = boost::asio;

struct options
{
    std::string dev_name = "tun0";
    std::string ipv4_addr = "10.0.0.1";
    std::string netmask = "255.255.255.0";
    std::string ipv6_addr;
    uint8_t ipv6_prefix_len = 64;
    size_t mtu = 1500;
    uint16_t echo_port = 7;
    int inject_fd = -1;
};

options parse_args(int argc, char **argv)
{
    options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing value for " + arg);
            }
            return argv[++i];
        };
        if (arg == "--tun") {
            opt.dev_name = next();
        } else if (arg == "--ip") {
            opt.ipv4_addr = next();
        } else if (arg == "--netmask") {
            opt.netmask = next();
        } else if (arg == "--ip6") {
            opt.ipv6_addr = next();
        } else if (arg == "--ip6-prefix") {
            opt.ipv6_prefix_len = static_cast<uint8_t>(std::stoul(next()));
        } else if (arg == "--mtu") {
            opt.mtu = static_cast<size_t>(std::stoul(next()));
        } else if (arg == "--echo-port") {
            opt.echo_port = static_cast<uint16_t>(std::stoul(next()));
        } else if (arg == "--inject-fd") {
            opt.inject_fd = std::stoi(next());
        } else {
            throw std::runtime_error("unknown option: " + arg);
        }
    }
    return opt;
}

// ---- TCP 全双工数据泵（DESIGN.md §10.1）----
net::awaitable<void> bidirectional_bridge(tunio::tun_tcp_socket client,
    net::ip::tcp::endpoint target)
{
    auto ex = co_await net::this_coro::executor;
    auto proxy = std::make_shared<net::ip::tcp::socket>(ex);
    boost::system::error_code ec;
    co_await proxy->async_connect(target,
        net::redirect_error(net::use_awaitable, ec));
    if (ec) {
        client.reset();
        co_return;
    }
    auto c = std::make_shared<tunio::tun_tcp_socket>(std::move(client));

    net::co_spawn(
        ex,
        [c, proxy]() -> net::awaitable<void> {
            std::array<char, 8192> buf;
            try {
                for (;;) {
                    size_t n = co_await c->async_read_some(net::buffer(buf),
                        net::use_awaitable);
                    co_await net::async_write(*proxy, net::buffer(buf, n),
                        net::use_awaitable);
                }
            } catch (...) {
            }
            boost::system::error_code sec;
            proxy->shutdown(net::ip::tcp::socket::shutdown_send, sec);
        },
        net::detached);

    net::co_spawn(
        ex,
        [c, proxy]() -> net::awaitable<void> {
            std::array<char, 8192> buf;
            try {
                for (;;) {
                    size_t n = co_await proxy->async_read_some(
                        net::buffer(buf), net::use_awaitable);
                    co_await net::async_write(*c, net::buffer(buf, n),
                        net::use_awaitable);
                }
            } catch (...) {
                c->close();
            }
        },
        net::detached);
}

net::awaitable<void> tcp_listener(tunio::tunio &engine, uint16_t echo_port)
{
    auto ex = co_await net::this_coro::executor;
    tunio::tun_tcp_acceptor acceptor(engine);
    for (;;) {
        tunio::tun_tcp_socket client(ex);
        boost::system::error_code ec;
        co_await acceptor.async_accept(
            client, net::redirect_error(net::use_awaitable, ec));
        if (ec) {
            co_return;
        }
        // 按客户端目标地址族选择本机回环端点
        const auto dest = client.original_destination();
        const net::ip::tcp::endpoint target =
            dest.address().is_v6()
                ? net::ip::tcp::endpoint(net::ip::address_v6::loopback(),
                    echo_port)
                : net::ip::tcp::endpoint(net::ip::address_v4::loopback(),
                    echo_port);
        net::co_spawn(ex, bidirectional_bridge(std::move(client), target),
            net::detached);
    }
}

// ---- UDP 回显会话（DESIGN.md §10.2）----
net::awaitable<void> udp_echo_handler(tunio::tun_udp_socket session)
{
    std::array<char, 2048> buf;
    try {
        for (;;) {
            net::ip::udp::endpoint sender;
            size_t n = co_await session.async_receive_from(
                net::buffer(buf), sender, net::use_awaitable);
            co_await session.async_send_to(sender, net::buffer(buf, n),
                net::use_awaitable);
        }
    } catch (...) {
        session.close();
    }
}

net::awaitable<void> udp_listener(tunio::tunio &engine)
{
    auto ex = co_await net::this_coro::executor;
    tunio::tun_udp_acceptor acceptor(engine);
    for (;;) {
        tunio::tun_udp_socket session(ex);
        boost::system::error_code ec;
        co_await acceptor.async_accept(
            session, net::redirect_error(net::use_awaitable, ec));
        if (ec) {
            co_return;
        }
        net::co_spawn(ex, udp_echo_handler(std::move(session)), net::detached);
    }
}

} // namespace

int main(int argc, char **argv)
{
    options opt;
    try {
        opt = parse_args(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }

    net::io_context io(4);
    tunio::tunio engine(io);

    tunio::tun_config cfg;
    cfg.dev_name = opt.dev_name;
    cfg.ipv4_addr = opt.ipv4_addr;
    cfg.netmask = opt.netmask;
    cfg.ipv6_addr = opt.ipv6_addr;
    cfg.ipv6_prefix_len = opt.ipv6_prefix_len;
    cfg.mtu = opt.mtu;
    if (opt.inject_fd >= 0) {
        cfg.external_handle = tunio::native_handle_from_int(opt.inject_fd);
        cfg.external_mtu = opt.mtu;
    }

    boost::system::error_code ec;
    if (!engine.open(cfg, ec)) {
        std::cerr << "open TUN failed: " << ec.message() << std::endl;
        return 1;
    }
    std::cout << "tun_echo: " << cfg.dev_name << " " << cfg.ipv4_addr
        << (cfg.ipv6_addr.empty() ? "" : " / " + cfg.ipv6_addr)
        << std::endl;

    net::co_spawn(io, tcp_listener(engine, opt.echo_port), net::detached);
    net::co_spawn(io, udp_listener(engine), net::detached);

    net::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait(
        [&](const boost::system::error_code &, int) { engine.close(); });

    io.run();
    return 0;
}
