//
// test_integration.cpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// 端到端集成测试：模拟 tun2socks 的桥接模式
//
// 客户端（经虚拟设备）通过引擎建立 TCP 连接，应用层把虚拟连接桥接到
// 本地回显服务，验证完整数据通路与 C++20 协程 API。
#include "test_harness.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <future>
#include <memory>
#include <string>
#include <vector>

using namespace test;

namespace {
namespace net = boost::asio;
constexpr uint32_t CLIENT_IP = 0x0a000002; // 10.0.0.2
constexpr uint32_t DEST_IP = 0x08080808;   // 8.8.8.8
constexpr uint16_t CLIENT_PORT = 20000;
constexpr uint16_t DEST_PORT = 8080;
} // namespace

static net::awaitable<void> echo_server(net::ip::tcp::acceptor &srv)
{
    auto ex = co_await net::this_coro::executor;
    net::ip::tcp::socket s(ex);
    boost::system::error_code ec;
    co_await srv.async_accept(s, net::redirect_error(net::use_awaitable, ec));
    if (ec) {
        co_return;
    }
    std::array<char, 4096> buf;
    for (;;) {
        size_t n = co_await s.async_read_some(
            net::buffer(buf), net::redirect_error(net::use_awaitable, ec));
        if (ec) {
            co_return;
        }
        co_await net::async_write(s, net::buffer(buf, n),
                                  net::redirect_error(net::use_awaitable, ec));
        if (ec) {
            co_return;
        }
    }
}

static net::awaitable<void> bridge(tun_stream client,
                                   net::ip::tcp::endpoint target)
{
    auto ex = co_await net::this_coro::executor;
    auto c = std::make_shared<tun_stream>(std::move(client));
    auto p = std::make_shared<net::ip::tcp::socket>(ex);
    boost::system::error_code ec;

    co_await p->async_connect(target,
                              net::redirect_error(net::use_awaitable, ec));
    if (ec) {
        c->reset(); // 后端连接失败：RST 客户端
        co_return;
    }

    // 客户端 -> 后端
    net::co_spawn(
        ex,
        [c, p]() -> net::awaitable<void> {
            std::array<char, 8192> buf;
            boost::system::error_code ec;
            for (;;) {
                size_t n = co_await c->async_read_some(
                    net::buffer(buf),
                    net::redirect_error(net::use_awaitable, ec));
                if (ec || n == 0) {
                    break;
                }
                co_await net::async_write(
                    *p, net::buffer(buf, n),
                    net::redirect_error(net::use_awaitable, ec));
                if (ec) {
                    break;
                }
            }
            p->shutdown(net::ip::tcp::socket::shutdown_send, ec);
        },
        net::detached);

    // 后端 -> 客户端
    net::co_spawn(
        ex,
        [c, p]() -> net::awaitable<void> {
            std::array<char, 8192> buf;
            boost::system::error_code ec;
            for (;;) {
                size_t n = co_await p->async_read_some(
                    net::buffer(buf),
                    net::redirect_error(net::use_awaitable, ec));
                if (ec || n == 0) {
                    break;
                }
                co_await net::async_write(
                    *c, net::buffer(buf, n),
                    net::redirect_error(net::use_awaitable, ec));
                if (ec) {
                    break;
                }
            }
            c->close();
        },
        net::detached);
}

int main()
{
    engine_env env;
    auto &io = env.io;

    // 本地回显服务
    net::ip::tcp::acceptor srv(io, {net::ip::tcp::v4(), 0});
    const uint16_t port = srv.local_endpoint().port();
    net::co_spawn(io, echo_server(srv), net::detached);

    // 桥接器：接受虚拟连接并转发到回显服务
    tun_acceptor acceptor(env.engine);
    net::co_spawn(
        io,
        [&]() -> net::awaitable<void> {
            auto ex = co_await net::this_coro::executor;
            tun_stream client(ex);
            boost::system::error_code ec;
            co_await acceptor.async_accept(
                client, net::redirect_error(net::use_awaitable, ec));
            if (ec) {
                co_return;
            }
            (void)client
                .original_destination(); // 原始目标（本例忽略，固定转发到本地服务）
            net::co_spawn(ex,
                          bridge(std::move(client),
                                 {net::ip::address_v4::loopback(), port}),
                          net::detached);
        },
        net::detached);

    // ---- 客户端握手 ----
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x02,
                          5000, 0, 65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no SYN-ACK");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
    }
    const uint32_t engine_iss = ti.seq;
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x10,
                          5001, engine_iss + 1, 65535, {}));

    // ---- 客户端发送数据，等待回显 ----
    const std::string msg = "ping-through-tun";
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x18,
                          5001, engine_iss + 1, 65535,
                          std::vector<uint8_t>(msg.begin(), msg.end())));

    // 回显数据段携带对客户端数据的 ACK（delayed ACK 合并，可能先出现纯 ACK）
    bool echo_seen = false;
    for (int i = 0; i < 4 && !echo_seen; ++i) {
        if (!env.dev.read_packet(pkt)) {
            throw std::runtime_error("no echo packet");
        }
        if (!parse_ip(pkt, ipi)) {
            throw std::runtime_error("parse_ip failed");
        }
        if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
            throw std::runtime_error("parse_tcp failed");
        }
        assert((ti.flags & 0x10) != 0 && ti.ack == 5001 + msg.size());
        if (ti.len > 0) {
            assert(ipi.src == DEST_IP && ipi.dst == CLIENT_IP);
            assert(ti.sport == DEST_PORT && ti.dport == CLIENT_PORT);
            assert(std::string(reinterpret_cast<const char *>(ti.data),
                               ti.len) == msg);
            echo_seen = true;
        }
    }
    assert(echo_seen);

    // ---- 客户端 FIN 关闭 ----
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x11,
                          5001 + msg.size(), engine_iss + 1 + msg.size(), 65535,
                          {}));
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no fin ack");
    }
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
    }
    assert((ti.flags & 0x10) != 0 && ti.ack == 5001 + msg.size() + 1);

    // 引擎应最终发送 FIN（桥接器检测到 EOF 后关闭虚拟流）
    bool fin_seen = false;
    for (int i = 0; i < 4 && !fin_seen; ++i) {
        if (!env.dev.read_packet(pkt, 2000)) {
            break;
        }
        if (!parse_ip(pkt, ipi)) {
            throw std::runtime_error("parse_ip failed");
        }
        if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
            throw std::runtime_error("parse_tcp failed");
        }
        if ((ti.flags & 0x01) != 0) {
            fin_seen = true;
        }
    }
    assert(fin_seen);

    // 客户端 ACK 引擎 FIN
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x10,
                          5001 + msg.size() + 1,
                          engine_iss + 1 + msg.size() + 1, 65535, {}));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return 0;
}
