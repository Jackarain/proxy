//
// tun2socks.cpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// tun2socks：通过 tunio 库实现的透明代理示例
//
// 用法示例：
//   sudo ./tun2socks --tun tun0 --ip 10.0.0.1 --netmask 255.255.255.0 \
//                    --proxy socks5://127.0.0.1:1080
//
// 功能：
//   - TCP：引擎终止虚拟连接，应用层经代理 CONNECT 连到目标后全双工桥接，
//          上游拨号超时后向客户端发送 RST；
//   - UDP：引擎维护 NAT 会话，应用层经代理中继转发（socks5 UDP ASSOCIATE
//          或 direct 直发）；
//   - 代理：socks5（含 RFC1929 认证）、http CONNECT、direct、reject；
//   - 半关闭：一侧关闭写后，另一侧在超时窗口内继续收尾，避免连接悬挂。
#include "tunio/tun_tcp_acceptor.hpp"
#include "tunio/tun_config.hpp"
#include "tunio/tun_tcp_socket.hpp"
#include "tunio/tun_udp_acceptor.hpp"
#include "tunio/tun_udp_socket.hpp"
#include "tunio/tunio.hpp"

#include "app_log.hpp"
#include "proxy.hpp"

#include <boost/asio.hpp>

#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace net = boost::asio;
namespace te = tunio;
namespace t2s = tun2socks_example;

struct options
{
    std::string dev_name = "tun0";
    std::string ipv4_addr = "10.0.0.1";
    std::string netmask = "255.255.255.0";
    std::string ipv6_addr;
    uint8_t ipv6_prefix_len = 64;
    size_t mtu = 1500;
    std::string proxy_url = "socks5://127.0.0.1:1080";
    bool udp = true;
    bool utun_prefix = false;
    int inject_fd = -1;
    size_t threads = 1;
    size_t num_queues = 1;
    t2s::log_level loglevel = t2s::log_level::info;
    std::chrono::milliseconds connect_timeout{5000};
    std::chrono::seconds half_close_timeout{60};
    std::chrono::seconds udp_timeout{60};
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
        << "  --queues <n>           Linux TUN 多队列数（IFF_MULTI_QUEUE，默认 1）\n"
        << "  --proxy <url>          代理地址，默认 socks5://127.0.0.1:1080；\n"
        << "                         格式 [scheme://][user:pass@]host[:port]，\n"
        << "                         scheme 取 socks5|http|direct|reject\n"
        << "  --utun-prefix          注入的 fd 为 macOS utun（读写带 4 字节家族前缀）\n"
        << "  --no-udp               禁用 UDP 转发\n"
        << "  --inject-fd <fd>       注入外部已打开的 TUN 文件描述符\n"
        << "  --threads <n>          io_context 线程数（默认 1）\n"
        << "  --loglevel <level>     debug|info|warn|error|silent（默认 info）\n"
        << "  --connect-timeout <ms> 上游连接超时毫秒数（默认 5000）\n"
        << "  --half-close-timeout <s> 半关闭收尾超时秒数（默认 60）\n"
        << "  --udp-timeout <s>      UDP 会话空闲超时秒数（默认 60）\n";
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
        } else if (arg == "--queues") {
            opt.num_queues = static_cast<size_t>(std::stoul(next()));
            if (opt.num_queues < 1 ||
                opt.num_queues > tunio::max_multi_queues) {
                throw std::runtime_error(
                    "--queues 需在 1.." +
                    std::to_string(tunio::max_multi_queues) + " 之间");
            }
        } else if (arg == "--proxy") {
            opt.proxy_url = next();
        } else if (arg == "--utun-prefix") {
            opt.utun_prefix = true;
        } else if (arg == "--no-udp") {
            opt.udp = false;
        } else if (arg == "--inject-fd") {
            opt.inject_fd = std::stoi(next());
        } else if (arg == "--threads") {
            opt.threads = static_cast<size_t>(std::stoul(next()));
        } else if (arg == "--loglevel") {
            const std::string val = next();
            if (!t2s::parse_log_level(val, opt.loglevel)) {
                throw std::runtime_error("非法日志级别: " + val);
            }
        } else if (arg == "--connect-timeout") {
            opt.connect_timeout =
                std::chrono::milliseconds(std::stoul(next()));
        } else if (arg == "--half-close-timeout") {
            opt.half_close_timeout = std::chrono::seconds(std::stoul(next()));
        } else if (arg == "--udp-timeout") {
            opt.udp_timeout = std::chrono::seconds(std::stoul(next()));
        } else if (arg == "-h" || arg == "--help") {
            usage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("未知参数: " + arg);
        }
    }
    return opt;
}

// ---- TCP 全双工数据泵 ----
//
// 两个方向各自独立收尾：一侧读结束后对另一侧发送 FIN（半关闭），随后
// 任一方向先完成写关闭时启动收尾定时器，超时仍未结束则强制关闭双方。
struct tcp_bridge_state
{
    explicit tcp_bridge_state(net::any_io_executor ex)
        : timer(ex)
    {
    }

    net::steady_timer timer;
    int pending = 2;
};

void on_direction_done(std::shared_ptr<tcp_bridge_state> state,
    std::shared_ptr<tunio::tun_tcp_socket> client,
    std::shared_ptr<net::ip::tcp::socket> upstream,
    std::chrono::seconds half_close_timeout)
{
    if (--state->pending <= 0) {
        // Boost 1.92 移除了带 error_code 的 timer::cancel 重载，使用无参版本；
        // cancel 的失败（定时器已到期）在收尾路径中无副作用。
        state->timer.cancel();
        return;
    }

    state->timer.expires_after(half_close_timeout);
    state->timer.async_wait([state, client, upstream](
        const boost::system::error_code &ec) {
        if (ec) {
            return; // 另一方向已正常收尾，定时器被取消
        }
        t2s::log_debug("[tcp] 半关闭超时，关闭连接");
        client->close();
        boost::system::error_code ignore;
        upstream->close(ignore);
    });
}

net::awaitable<void> pump_client_to_upstream(
    std::shared_ptr<tunio::tun_tcp_socket> client,
    std::shared_ptr<net::ip::tcp::socket> upstream,
    std::shared_ptr<tcp_bridge_state> state,
    std::chrono::seconds half_close_timeout)
{
    std::array<char, 65536> buf;
    try {
        for (;;) {
            const size_t n = co_await client->async_read_some(
                net::buffer(buf), net::use_awaitable);
            co_await net::async_write(
                *upstream, net::buffer(buf, n), net::use_awaitable);
        }
    } catch (const std::exception &e) {
        t2s::log_debug("[tcp] client->upstream 结束: ", e.what());
    }
    boost::system::error_code ec;
    upstream->shutdown(net::ip::tcp::socket::shutdown_send, ec);
    on_direction_done(state, client, upstream, half_close_timeout);
}

net::awaitable<void> pump_upstream_to_client(
    std::shared_ptr<tunio::tun_tcp_socket> client,
    std::shared_ptr<net::ip::tcp::socket> upstream,
    std::shared_ptr<tcp_bridge_state> state,
    std::chrono::seconds half_close_timeout)
{
    std::array<char, 65536> buf;
    try {
        for (;;) {
            const size_t n = co_await upstream->async_read_some(
                net::buffer(buf), net::use_awaitable);
            co_await client->async_write_some(
                net::buffer(buf, n), net::use_awaitable);
        }
    } catch (const std::exception &e) {
        t2s::log_debug("[tcp] upstream->client 结束: ", e.what());
    }
    boost::system::error_code ec;
    client->shutdown(net::ip::tcp::socket::shutdown_send, ec);
    on_direction_done(state, client, upstream, half_close_timeout);
}

net::awaitable<void> tcp_bridge(tunio::tun_tcp_socket client,
    std::shared_ptr<t2s::proxy> proxy,
    std::chrono::seconds half_close_timeout)
{
    auto ex = co_await net::this_coro::executor;
    const auto dest = client.original_destination();
    const std::string host = dest.address().to_string();

    net::ip::tcp::socket upstream(ex);
    try {
        upstream = co_await proxy->connect(host, dest.port());
    } catch (const std::exception &e) {
        t2s::log_warn("[tcp] ", dest, " -> ", proxy->name(), ": ", e.what());
        client.reset(); // 后端失败：立即 RST 客户端
        co_return;
    }
    t2s::log_debug("[tcp] ", dest, " <-> ", proxy->name());

    auto c = std::make_shared<tunio::tun_tcp_socket>(std::move(client));
    auto u = std::make_shared<net::ip::tcp::socket>(std::move(upstream));
    auto state = std::make_shared<tcp_bridge_state>(ex);

    net::co_spawn(ex,
        pump_client_to_upstream(c, u, state, half_close_timeout),
        net::detached);
    net::co_spawn(ex,
        pump_upstream_to_client(c, u, state, half_close_timeout),
        net::detached);
}

net::awaitable<void> tcp_listener(tunio::tunio &engine,
    std::shared_ptr<t2s::proxy> proxy,
    std::chrono::seconds half_close_timeout)
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
        net::co_spawn(ex,
            tcp_bridge(std::move(client), proxy, half_close_timeout),
            net::detached);
    }
}

// ---- UDP：每个会话经代理中继转发 ----
net::awaitable<void> udp_bridge(tunio::tun_udp_socket session,
    std::shared_ptr<t2s::proxy> proxy,
    std::chrono::seconds udp_timeout)
{
    auto ex = co_await net::this_coro::executor;
    std::shared_ptr<t2s::udp_relay> relay;
    try {
        relay = co_await proxy->associate_udp();
    } catch (const std::exception &e) {
        t2s::log_warn("[udp] associate ", proxy->name(), ": ", e.what());
        session.close();
        co_return;
    }
    if (!relay) {
        session.close();
        co_return;
    }
    session.set_timeout(udp_timeout);

    auto s = std::make_shared<tunio::tun_udp_socket>(std::move(session));

    // 会话 -> 中继
    net::co_spawn(ex,
        [s, relay]() -> net::awaitable<void> {
            std::array<char, 2048> buf;
            try {
                for (;;) {
                    net::ip::udp::endpoint target;
                    const size_t n = co_await s->async_receive_from(
                        net::buffer(buf), target, net::use_awaitable);
                    co_await relay->send(
                        std::vector<uint8_t>(buf.data(), buf.data() + n),
                        target);
                }
            } catch (const std::exception &e) {
                t2s::log_debug("[udp] session->relay 结束: ", e.what());
            }
            relay->close();
        },
        net::detached);

    // 中继 -> 会话
    net::co_spawn(ex,
        [s, relay]() -> net::awaitable<void> {
            try {
                for (;;) {
                    auto [payload, target] = co_await relay->receive();
                    co_await s->async_send_to(target, net::buffer(payload),
                        net::use_awaitable);
                }
            } catch (const std::exception &e) {
                t2s::log_debug("[udp] relay->session 结束: ", e.what());
            }
            s->close();
            relay->close();
        },
        net::detached);
}

net::awaitable<void> udp_listener(tunio::tunio &engine,
    std::shared_ptr<t2s::proxy> proxy,
    std::chrono::seconds udp_timeout)
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
        net::co_spawn(ex, udp_bridge(std::move(session), proxy, udp_timeout),
            net::detached);
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

    t2s::logger::instance().set_level(opt.loglevel);

    net::io_context io(opt.threads);
    // 单线程 io 使用无 Strand 派发开销的单线程模式；多线程 io 时引擎
    // 内部以 Strand 串行化，保证线程安全.
    tunio::tunio engine(io, opt.threads == 1);

    // 解析代理 URL，并同步解析代理主机（支持 IP 与域名）。此处在
    // io.run() 之前调用，同步解析不会阻塞事件循环.
    t2s::proxy_config pcfg;
    std::string perr;
    if (!t2s::parse_proxy_url(opt.proxy_url, pcfg, perr)) {
        std::cerr << "代理参数错误: " << perr << std::endl;
        return 1;
    }
    if (pcfg.scheme == "socks5" || pcfg.scheme == "http") {
        boost::system::error_code rec;
        net::ip::tcp::resolver resolver(io);
        const auto results =
            resolver.resolve(pcfg.host, std::to_string(pcfg.port), rec);
        if (rec || results.empty()) {
            std::cerr << "无法解析代理地址 " << pcfg.host << ":"
                << pcfg.port << ": " << rec.message() << std::endl;
            return 1;
        }
        pcfg.endpoint = results.begin()->endpoint();
    }

    auto upstream_proxy =
        t2s::make_proxy(pcfg, io.get_executor(), opt.connect_timeout);
    if (!upstream_proxy) {
        std::cerr << "无法创建代理: " << pcfg.scheme << std::endl;
        return 1;
    }
    if (opt.udp && !upstream_proxy->supports_udp()) {
        t2s::log_warn("代理 ", upstream_proxy->name(),
            " 不支持 UDP，已禁用 UDP 转发");
        opt.udp = false;
    }

    tunio::tun_config cfg;
    cfg.dev_name = opt.dev_name;
    cfg.ipv4_addr = opt.ipv4_addr;
    cfg.netmask = opt.netmask;
    cfg.ipv6_addr = opt.ipv6_addr;
    cfg.ipv6_prefix_len = opt.ipv6_prefix_len;
    cfg.mtu = opt.mtu;
    cfg.num_queues = opt.num_queues;
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

    std::string proxy_desc = upstream_proxy->name() + "://";
    if (!pcfg.host.empty()) {
        proxy_desc += pcfg.host + ":" + std::to_string(pcfg.port);
    }
    t2s::log_info("tun2socks 已启动: ", cfg.dev_name, " ", cfg.ipv4_addr,
        (cfg.ipv6_addr.empty() ? "" : " / " + cfg.ipv6_addr), " -> ",
        proxy_desc, " (队列 x", engine.queue_count(), ")");

    net::co_spawn(io,
        tcp_listener(engine, upstream_proxy, opt.half_close_timeout),
        net::detached);
    if (opt.udp) {
        net::co_spawn(io,
            udp_listener(engine, upstream_proxy, opt.udp_timeout),
            net::detached);
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
                << " tx_dropped=" << st.tx_dropped.load()
                << " rx_ooo=" << st.rx_ooo.load()
                << " tcp_connections=" << st.tcp_connections.load()
                << " udp_sessions=" << st.udp_sessions.load()
                << " icmp_replies=" << st.icmp_replies.load()
                << std::endl;
            signals.async_wait(on_signal);
            return;
        }
#endif
        t2s::log_info("正在关闭...");
        engine.close();
    };
    signals.async_wait(on_signal);

    io.run();
    return 0;
}
