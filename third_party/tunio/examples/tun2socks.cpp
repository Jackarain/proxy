//
// tun2socks.cpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// tun2socks：通过 tunio 库实现的 SOCKS5 透明代理
//
// 用法示例：
//   sudo ./tun2socks --tun tun0 --ip 10.0.0.1 --netmask 255.255.255.0 \
//                    --proxy 127.0.0.1:1080
//
// 功能：
//   - TCP：引擎终止虚拟连接，应用层经 SOCKS5 CONNECT 连到代理后全双工桥接；
//   - UDP：引擎维护 NAT 会话，应用层经 SOCKS5 UDP ASSOCIATE 中继转发；
//   - 后端连接失败时向客户端发送 RST。
#include "tunio/tun_tcp_acceptor.hpp"
#include "tunio/tun_config.hpp"
#include "tunio/tun_tcp_socket.hpp"
#include "tunio/tun_udp_acceptor.hpp"
#include "tunio/tun_udp_socket.hpp"
#include "tunio/tunio.hpp"

#include "socks5_client.hpp"

#include <boost/asio.hpp>

#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
namespace net = boost::asio;
namespace te = tunio;

struct options
{
    std::string dev_name = "tun0";
    std::string ipv4_addr = "10.0.0.1";
    std::string netmask = "255.255.255.0";
    std::string ipv6_addr;
    uint8_t ipv6_prefix_len = 64;
    size_t mtu = 1500;
    std::string proxy_host = "127.0.0.1";
    uint16_t proxy_port = 1080;
    bool udp = true;
    bool utun_prefix = false;
    int inject_fd = -1;
    size_t threads = 1;
};

void usage(const char *prog)
{
    std::cerr
        << "用法: " << prog << " [选项]\n"
        << "  --tun <name>           TUN 设备名（默认 tun0）\n"
        << "  --ip <addr>            本地虚拟 IP（默认 10.0.0.1）\n"
        << "  --netmask <mask>       子网掩码（默认 255.255.255.0）\n"
        << "  --ip6 <addr>           本地虚拟 IPv6 地址（可选，如 fd00::1）\n"
        << "  --ip6-prefix <len>     IPv6 前缀长度（默认 64）\n"
        << "  --mtu <bytes>          MTU（默认 1500）\n"
        << "  --proxy <host:port>    SOCKS5 代理地址（默认 127.0.0.1:1080）\n"
        << "  --utun-prefix          注入的 fd 为 macOS utun（读写带 4 字节家族前缀）\n"
        << "  --no-udp               禁用 UDP 转发\n"
        << "  --inject-fd <fd>       注入外部已打开的 TUN 文件描述符\n"
        << "  --threads <n>          io_context 线程数（默认 1）\n";
}

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
        } else if (arg == "--proxy") {
            const std::string val = next();
            const auto pos = val.rfind(':');
            if (pos == std::string::npos) {
                throw std::runtime_error("proxy 需要 host:port 格式");
            }
            opt.proxy_host = val.substr(0, pos);
            opt.proxy_port =
                static_cast<uint16_t>(std::stoul(val.substr(pos + 1)));
        } else if (arg == "--utun-prefix") {
            opt.utun_prefix = true;
        } else if (arg == "--no-udp") {
            opt.udp = false;
        } else if (arg == "--inject-fd") {
            opt.inject_fd = std::stoi(next());
        } else if (arg == "--threads") {
            opt.threads = static_cast<size_t>(std::stoul(next()));
        } else if (arg == "-h" || arg == "--help") {
            usage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("未知参数: " + arg);
        }
    }
    return opt;
}

// ---- TCP 全双工数据泵（与 DESIGN.md §10.1 一致）----
net::awaitable<void> tcp_bridge(tunio::tun_tcp_socket client,
    net::ip::tcp::endpoint proxy)
{
    auto ex = co_await net::this_coro::executor;
    auto dest = client.original_destination();
    auto upstream = std::make_shared<net::ip::tcp::socket>(ex);

    try {
        *upstream = co_await tun2socks_example::socks5_connect(
            proxy, dest.address().to_string(), dest.port());
    } catch (const boost::system::system_error &e) {
        std::cerr << "[tun2socks] " << dest << " -> " << proxy << " : "
            << e.what() << std::endl;
        client.reset(); // 后端失败：立即 RST 客户端
        co_return;
    }

    auto c = std::make_shared<tunio::tun_tcp_socket>(std::move(client));
    net::co_spawn(
        ex,
        [c, upstream]() -> net::awaitable<void> {
            std::array<char, 65536> buf;
            try {
                for (;;) {
                    size_t n = co_await c->async_read_some(net::buffer(buf),
                        net::use_awaitable);
                    co_await net::async_write(*upstream, net::buffer(buf, n),
                        net::use_awaitable);
                }
            } catch (const boost::system::system_error &e) {
                std::cerr << "[bridge] client->upstream exit: " << e.what()
                    << std::endl;
            } catch (const std::exception &e) {
                std::cerr << "[bridge] client->upstream exit: " << e.what()
                    << std::endl;
            }
            boost::system::error_code sec;
            upstream->shutdown(net::ip::tcp::socket::shutdown_send, sec);
        },
        net::detached);

    net::co_spawn(
        ex,
        [c, upstream]() -> net::awaitable<void> {
            std::array<char, 65536> buf;
            try {
                for (;;) {
                    size_t n = co_await upstream->async_read_some(
                        net::buffer(buf), net::use_awaitable);
                    co_await net::async_write(*c, net::buffer(buf, n),
                        net::use_awaitable);
                }
            } catch (const boost::system::system_error &e) {
                std::cerr << "[bridge] upstream->client exit: " << e.what()
                    << std::endl;
                c->close();
            } catch (const std::exception &e) {
                std::cerr << "[bridge] upstream->client exit: " << e.what()
                    << std::endl;
                c->close();
            }
        },
        net::detached);
}

net::awaitable<void> tcp_listener(tunio::tunio &engine,
    net::ip::tcp::endpoint proxy)
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
        net::co_spawn(ex, tcp_bridge(std::move(client), proxy), net::detached);
    }
}

// ---- UDP：每个会话经 SOCKS5 UDP ASSOCIATE 中继转发 ----
net::awaitable<void> udp_bridge(tunio::tun_udp_socket session,
    net::ip::tcp::endpoint proxy)
{
    auto ex = co_await net::this_coro::executor;
    auto relay = std::make_shared<tun2socks_example::socks5_udp_relay>(ex);
    try {
        co_await relay->associate(proxy);
    } catch (const boost::system::system_error &e) {
        std::cerr << "[tun2socks] udp associate " << proxy << " : " << e.what()
            << std::endl;
        session.close();
        co_return;
    }

    auto s = std::make_shared<tunio::tun_udp_socket>(std::move(session));

    // 客户端 -> 中继
    net::co_spawn(
        ex,
        [s, relay]() -> net::awaitable<void> {
            std::array<char, 2048> buf;
            try {
                for (;;) {
                    net::ip::udp::endpoint target;
                    size_t n = co_await s->async_receive_from(
                        net::buffer(buf), target, net::use_awaitable);
                    co_await relay->send_to(
                        std::vector<uint8_t>(buf.data(), buf.data() + n),
                        target);
                }
            } catch (...) {
                relay->close();
            }
        },
        net::detached);

    // 中继 -> 客户端
    net::co_spawn(
        ex,
        [s, relay]() -> net::awaitable<void> {
            try {
                for (;;) {
                    auto [payload, target] = co_await relay->receive_from();
                    co_await s->async_send_to(target, net::buffer(payload),
                        net::use_awaitable);
                }
            } catch (...) {
                s->close();
                relay->close();
            }
        },
        net::detached);
}

net::awaitable<void> udp_listener(tunio::tunio &engine,
    net::ip::tcp::endpoint proxy)
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
        net::co_spawn(ex, udp_bridge(std::move(session), proxy), net::detached);
    }
}

} // namespace

int main(int argc, char **argv)
{
    options opt;
    try {
        opt = parse_args(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "参数错误: " << e.what() << std::endl;
        usage(argv[0]);
        return 1;
    }

    net::io_context io(opt.threads);
    // 单线程 io 使用无 Strand 派发开销的单线程模式；多线程 io 时引擎
    // 内部以 Strand 串行化，保证线程安全.
    tunio::tunio engine(io, opt.threads == 1);

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
        cfg.utun_prefix = opt.utun_prefix;
    }

    boost::system::error_code ec;
    if (!engine.open(cfg, ec)) {
        std::cerr << "打开 TUN 设备失败: " << ec.message() << std::endl;
        return 1;
    }
    std::cout << "tun2socks 已启动: " << cfg.dev_name << " " << cfg.ipv4_addr
        << (cfg.ipv6_addr.empty() ? "" : " / " + cfg.ipv6_addr) << " -> "
        << opt.proxy_host << ":" << opt.proxy_port << std::endl;

    net::ip::tcp::endpoint proxy(net::ip::make_address(opt.proxy_host),
        opt.proxy_port);
    net::co_spawn(io, tcp_listener(engine, proxy), net::detached);
    if (opt.udp) {
        net::co_spawn(io, udp_listener(engine, proxy), net::detached);
    }

    // SIGUSR1 为 POSIX 信号，Windows CRT 未定义；统计转储功能仅 POSIX 可用.
    net::signal_set signals(io, SIGINT, SIGTERM
#ifndef _WIN32
        , SIGUSR1
#endif
    );
    std::function<void(const boost::system::error_code &, int)> on_signal;
    on_signal = [&](const boost::system::error_code &ec, int signum) {
        if (ec) {
            return;
        }
#ifndef _WIN32
        if (signum == SIGUSR1) {
            const auto &st = engine.stats();
            std::cout << "[stats] rx_packets=" << st.rx_packets.load()
                << " tx_packets=" << st.tx_packets.load()
                << " rx_dropped=" << st.rx_dropped.load()
                << " rx_ooo=" << st.rx_ooo.load()
                << " tcp_connections=" << st.tcp_connections.load()
                << std::endl;
            signals.async_wait(on_signal);
            return;
        }
#endif
        std::cout << "\n正在关闭..." << std::endl;
        engine.close();
    };
    signals.async_wait(on_signal);

    io.run();
    return 0;
}
