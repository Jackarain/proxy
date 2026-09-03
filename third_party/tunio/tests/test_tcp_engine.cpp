//
// test_tcp_engine.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#define BOOST_TEST_MODULE tcp_engine
#include <boost/test/included/unit_test.hpp>
#include "test_harness.hpp"

#include <cassert>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace test;

namespace {
namespace net = boost::asio;
constexpr uint32_t CLIENT_IP = 0x0a000002; // 10.0.0.2
constexpr uint32_t DEST_IP = 0x08080808;   // 8.8.8.8
const auto CLIENT_V6 = v6("fd00::2");
const auto DEST_V6 = v6("2001:4860:4860::8888");
constexpr uint16_t CLIENT_PORT = 12345;
constexpr uint16_t DEST_PORT = 80;
} // namespace

BOOST_AUTO_TEST_CASE(test_handshake_data_fin)
{
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    // 客户端 SYN
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x02,
        1000, 0, 65535, {}));

    // 引擎 SYN-ACK（携带 MSS 选项）
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    TEST_ASSERT(ipi.src == DEST_IP && ipi.dst == CLIENT_IP && ipi.proto == 6);
    tcp_hdr_info ti;
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT((ti.flags & 0x12) == 0x12); // SYN|ACK
    TEST_ASSERT(ti.ack == 1001);
    const uint32_t engine_iss = ti.seq;
    TEST_ASSERT(ipi.payload_len >= 24); // MSS 选项

    // 客户端 ACK
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x10,
        1001, engine_iss + 1, 65535, {}));

    // accept 完成，原始目标地址正确
    auto aec = future_get(accept_done.get_future());
    TEST_ASSERT(!aec);
    auto dest = peer.original_destination();
    TEST_ASSERT(dest.address().to_v4().to_uint() == DEST_IP);
    TEST_ASSERT(dest.port() == DEST_PORT);
    auto rmt = peer.remote_endpoint();
    TEST_ASSERT(rmt.address().to_v4().to_uint() == CLIENT_IP);
    TEST_ASSERT(rmt.port() == CLIENT_PORT);
    TEST_ASSERT(peer.is_open());

    // 客户端发送数据 "hello"
    const std::string hello = "hello";
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x18,
        1001, engine_iss + 1, 65535,
        std::vector<uint8_t>(hello.begin(), hello.end())));

    // 应用读取到字节流
    std::promise<std::pair<boost::system::error_code, size_t>> read_done;
    char buf[64];
    peer.async_read_some(net::buffer(buf),
        [&](boost::system::error_code ec, size_t n) {
            read_done.set_value({ec, n});
        });
    auto [rec, rn] = future_get(read_done.get_future());
    TEST_ASSERT(!rec && rn == hello.size());
    TEST_ASSERT(std::string(buf, rn) == hello);

    // 引擎 ACK 客户端数据
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT((ti.flags & 0x10) != 0 && ti.ack == 1001 + hello.size());

    // 应用写入 "world"，设备收到数据段
    const std::string world = "world";
    std::promise<std::pair<boost::system::error_code, size_t>> write_done;
    peer.async_write_some(net::buffer(world),
        [&](boost::system::error_code ec, size_t n) {
            write_done.set_value({ec, n});
        });

    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no data packet");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT(ipi.src == DEST_IP && ipi.dst == CLIENT_IP);
    TEST_ASSERT(ti.sport == DEST_PORT && ti.dport == CLIENT_PORT);
    TEST_ASSERT(ti.seq ==
        engine_iss + 1); // 首个数据段 seq = iss + 1（SYN 消耗一个序号）
    TEST_ASSERT(ti.ack == 1001 + hello.size());
    TEST_ASSERT((ti.flags & 0x18) == 0x18); // PSH|ACK
    TEST_ASSERT(std::string(reinterpret_cast<const char *>(ti.data), ti.len) ==
        world);

    // 客户端 ACK 数据：写操作在数据被确认后才完成（ACK 确认制）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x10,
        1001 + hello.size(),
        engine_iss + 1 + world.size(), 65535, {}));
    auto [wec, wn] = future_get(write_done.get_future());
    TEST_ASSERT(!wec && wn == world.size());

    // 客户端 FIN -> 应用读到 EOF
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x11,
        1001 + hello.size(), engine_iss + 1 + world.size(),
        65535, {}));
    std::promise<std::pair<boost::system::error_code, size_t>> eof_done;
    peer.async_read_some(net::buffer(buf),
        [&](boost::system::error_code ec, size_t n) {
            eof_done.set_value({ec, n});
        });
    auto [eec, en] = future_get(eof_done.get_future());
    TEST_ASSERT(eec == net::error::eof && en == 0); // EOF

    // 引擎 ACK FIN
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no FIN ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT(ti.ack == 1001 + hello.size() + 1);

    // 应用关闭 -> 引擎发送 FIN
    peer.close();
    for (int i = 0; i < 100 && peer.is_open(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_ASSERT(!peer.is_open());
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no FIN from engine");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT((ti.flags & 0x01) != 0);
    const uint32_t fin_seq = ti.seq;

    // 客户端 ACK 引擎 FIN
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x10,
        1001 + hello.size() + 1, fin_seq + 1, 65535, {}));
}

BOOST_AUTO_TEST_CASE(test_fin_retransmit_reacked)
{
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x02,
        3000, 0, 65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse SYN-ACK failed");
    }
    const uint32_t engine_iss = ti.seq;

    // 客户端 ACK 完成握手
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x10,
        3001, engine_iss + 1, 65535, {}));
    future_get(accept_done.get_future());

    // 客户端 FIN -> 应用读到 EOF
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x11,
        3001, engine_iss + 1, 65535, {}));
    std::promise<std::pair<boost::system::error_code, size_t>> eof_done;
    char buf[64];
    peer.async_read_some(net::buffer(buf),
        [&](boost::system::error_code ec, size_t n) {
            eof_done.set_value({ec, n});
        });
    auto [eec, en] = future_get(eof_done.get_future());
    TEST_ASSERT(eec == net::error::eof && en == 0);

    // 引擎 ACK FIN
    if (!env.dev.read_packet(pkt) || !parse_ip(pkt, ipi) ||
        !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("no FIN ACK");
    }
    TEST_ASSERT(ti.ack == 3002);

    // 客户端重传 FIN：引擎必须重新 ACK（避免客户端长时间重复重传）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x11,
        3001, engine_iss + 1, 65535, {}));
    if (!env.dev.read_packet(pkt) || !parse_ip(pkt, ipi) ||
        !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("no FIN re-ACK");
    }
    TEST_ASSERT((ti.flags & 0x10) != 0 && ti.ack == 3002);
}

// 解析 SYN-ACK 中的 Window Scale 选项（kind=3, len=3）；返回是否携带及值
static bool synack_wscale(const std::vector<uint8_t> &pkt, int *out_ws)
{
    ip_hdr_info ipi;
    if (!parse_ip(pkt, ipi)) {
        return false;
    }
    const uint8_t *tp = ipi.payload;
    const size_t hlen = static_cast<size_t>((tp[12] >> 4) * 4);
    for (size_t o = 20; o + 2 <= hlen && o < ipi.payload_len;) {
        const uint8_t kind = tp[o];
        if (kind == 0) {
            break; // EOL
        }
        if (kind == 1) {
            ++o; // NOP
            continue;
        }
        const uint8_t olen = tp[o + 1];
        if (olen < 2 || o + olen > hlen) {
            break;
        }
        if (kind == 3 && olen == 3) {
            *out_ws = tp[o + 2];
            return true;
        }
        o += olen;
    }
    return false;
}

// 读取引擎发出的数据段并累计载荷字节；返回累计值
static size_t drain_data_segments(engine_env &env, std::vector<uint8_t> &pkt,
    ip_hdr_info &ipi, tcp_hdr_info &ti,
    size_t target)
{
    size_t sent = 0;
    while (sent < target) {
        if (!env.dev.read_packet(pkt)) {
            TEST_THROW("no data segment");
        }
        if (!verify_packet(pkt)) {
            TEST_THROW("verify_packet failed");
        }
        if (!parse_ip(pkt, ipi)) {
            TEST_THROW("parse_ip failed");
        }
        if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
            TEST_THROW("parse_tcp failed");
        }
        if (ti.len == 0) {
            TEST_THROW("unexpected control segment");
        }
        sent += ti.len;
    }
    return sent;
}

BOOST_AUTO_TEST_CASE(test_wscale_negotiation)
{
    // 场景 1: 对端通告 WS=3（macOS/iOS 常见）。本端 SYN-ACK 通告 7；对端
    // 窗口字段按对端通告原值 3 放大（1000 << 3 = 8000）后才限发送.
    {
        engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
            std::chrono::seconds(30), 1024 * 1024,
            std::chrono::milliseconds(5000),
            std::chrono::milliseconds(5000));
        auto &io = env.io;
        tun_tcp_acceptor acceptor(env.engine);
        tun_tcp_socket peer(io.get_executor());
        std::promise<boost::system::error_code> accept_done;
        acceptor.async_accept(peer, [&](boost::system::error_code ec) {
            if (!ec) {
                peer.accept();
            }
            accept_done.set_value(ec);
        });

        env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12380, DEST_PORT, 0x02, 5000,
            0, 4096, {}, true, 3));
        std::vector<uint8_t> pkt;
        if (!env.dev.read_packet(pkt)) {
            TEST_THROW("no SYN-ACK");
        }
        ip_hdr_info ipi;
        if (!parse_ip(pkt, ipi)) {
            TEST_THROW("parse_ip failed");
        }
        tcp_hdr_info ti;
        if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
            TEST_THROW("parse_tcp failed");
        }
        TEST_ASSERT((ti.flags & 0x12) == 0x12);
        int ws = -1;
        TEST_ASSERT(synack_wscale(pkt, &ws) && ws == 7); // 本端固定通告 7
        const uint32_t engine_iss = ti.seq;

        env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12380, DEST_PORT, 0x10, 5001,
            engine_iss + 1, 1000, {}, false, 3));
        future_get(accept_done.get_future());

        std::promise<std::pair<boost::system::error_code, size_t>> write_done;
        std::string payload(100000, 'x');
        peer.async_write_some(
            net::buffer(payload), [&](boost::system::error_code ec, size_t n) {
                write_done.set_value({ec, n});
            });

        // 窗口 1000<<3=8000：引擎发送 8000 字节后挂起，不得超发
        const size_t sent = drain_data_segments(env, pkt, ipi, ti, 8000);
        TEST_ASSERT(sent == 8000);
        if (env.dev.read_packet(pkt, 200)) {
            TEST_THROW("over-sent beyond scaled peer window");
        }

        // 客户端 ACK 推进并放大窗口 -> 恢复发送，全部发出后最终 ACK 完成写
        env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12380, DEST_PORT, 0x10, 5001,
            engine_iss + 1 + static_cast<uint32_t>(sent),
            32767, {}, false, 3));
        const size_t sent2 = drain_data_segments(env, pkt, ipi, ti, 92000);
        TEST_ASSERT(sent + sent2 == 100000);
        env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12380, DEST_PORT, 0x10, 5001,
            engine_iss + 1 + 100000, 32767, {}, false, 3));
        auto [wec, wn] = future_get(write_done.get_future(), 8000);
        TEST_ASSERT(!wec && wn == 100000);
        peer.close();
    }

    // 场景 2: 对端未通告 WS。本端 SYN-ACK 不携带 WS 选项；对端窗口字段
    // 不缩放（1000 原值），引擎发送 1000 字节后挂起.
    {
        engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
            std::chrono::seconds(30), 1024 * 1024,
            std::chrono::milliseconds(5000),
            std::chrono::milliseconds(5000));
        auto &io = env.io;
        tun_tcp_acceptor acceptor(env.engine);
        tun_tcp_socket peer(io.get_executor());
        std::promise<boost::system::error_code> accept_done;
        acceptor.async_accept(peer, [&](boost::system::error_code ec) {
            if (!ec) {
                peer.accept();
            }
            accept_done.set_value(ec);
        });

        env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12381, DEST_PORT, 0x02, 6000,
            0, 4096, {}, true));
        std::vector<uint8_t> pkt;
        if (!env.dev.read_packet(pkt)) {
            TEST_THROW("no SYN-ACK");
        }
        ip_hdr_info ipi;
        if (!parse_ip(pkt, ipi)) {
            TEST_THROW("parse_ip failed");
        }
        tcp_hdr_info ti;
        if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
            TEST_THROW("parse_tcp failed");
        }
        int ws = -1;
        TEST_ASSERT(!synack_wscale(pkt, &ws)); // 未协商: SYN-ACK 不带 WS
        const uint32_t engine_iss = ti.seq;

        env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12381, DEST_PORT, 0x10, 6001,
            engine_iss + 1, 1000, {}));
        future_get(accept_done.get_future());

        std::promise<std::pair<boost::system::error_code, size_t>> write_done;
        std::string payload(100000, 'x');
        peer.async_write_some(
            net::buffer(payload), [&](boost::system::error_code ec, size_t n) {
                write_done.set_value({ec, n});
            });

        const size_t sent = drain_data_segments(env, pkt, ipi, ti, 1000);
        TEST_ASSERT(sent == 1000);
        if (env.dev.read_packet(pkt, 200)) {
            TEST_THROW("over-sent beyond unscaled peer window");
        }
        // 未协商 WS：窗口字段上限 65535，逐段 ACK 推进（对端实际行为）
        size_t acked = sent;
        while (acked < 100000) {
            // 引擎挂起在窗口耗尽时：先 ACK 推进窗口，再读新数据
            env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12381, DEST_PORT, 0x10,
                6001, engine_iss + 1 +
                static_cast<uint32_t>(acked),
                65535, {}));
            if (!env.dev.read_packet(pkt)) {
                TEST_THROW("no data segment");
            }
            if (!verify_packet(pkt)) {
                TEST_THROW("verify_packet failed");
            }
            if (!parse_ip(pkt, ipi)) {
                TEST_THROW("parse_ip failed");
            }
            if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
                TEST_THROW("parse_tcp failed");
            }
            if (ti.len == 0) {
                TEST_THROW("unexpected control segment");
            }
            acked += ti.len;
        }
        TEST_ASSERT(acked == 100000);
        // 循环末次读到的段尚未确认：补最终 ACK 完成写操作
        env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12381, DEST_PORT, 0x10,
            6001, engine_iss + 1 + 100000, 65535, {}));
        auto [wec, wn] = future_get(write_done.get_future(), 8000);
        TEST_ASSERT(!wec && wn == 100000);
        peer.close();
    }

    // 场景 3: 对端通告 WS=8（> 本端 7）。本端 SYN-ACK 仍通告 7；解释对端
    // 窗口字段用对端原值 8（1000 << 8 = 256000），不截断到本端 7.
    {
        engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
            std::chrono::seconds(30), 1024 * 1024,
            std::chrono::milliseconds(5000),
            std::chrono::milliseconds(5000));
        auto &io = env.io;
        tun_tcp_acceptor acceptor(env.engine);
        tun_tcp_socket peer(io.get_executor());
        std::promise<boost::system::error_code> accept_done;
        acceptor.async_accept(peer, [&](boost::system::error_code ec) {
            if (!ec) {
                peer.accept();
            }
            accept_done.set_value(ec);
        });

        env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12382, DEST_PORT, 0x02, 7000,
            0, 4096, {}, true, 8));
        std::vector<uint8_t> pkt;
        if (!env.dev.read_packet(pkt)) {
            TEST_THROW("no SYN-ACK");
        }
        ip_hdr_info ipi;
        if (!parse_ip(pkt, ipi)) {
            TEST_THROW("parse_ip failed");
        }
        tcp_hdr_info ti;
        if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
            TEST_THROW("parse_tcp failed");
        }
        int ws = -1;
        TEST_ASSERT(synack_wscale(pkt, &ws) && ws == 7); // 本端始终通告 7
        const uint32_t engine_iss = ti.seq;

        env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12382, DEST_PORT, 0x10, 7001,
            engine_iss + 1, 1000, {}, false, 8));
        future_get(accept_done.get_future());

        std::promise<std::pair<boost::system::error_code, size_t>> write_done;
        std::string payload(100000, 'x');
        peer.async_write_some(
            net::buffer(payload), [&](boost::system::error_code ec, size_t n) {
                write_done.set_value({ec, n});
            });

        // 窗口 256000 > 100000：全部数据一次发出，无需窗口恢复
        drain_data_segments(env, pkt, ipi, ti, 100000);
        env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12382, DEST_PORT, 0x10, 7001,
            engine_iss + 1 + 100000, 32767, {}, false, 8));
        auto [wec, wn] = future_get(write_done.get_future(), 8000);
        TEST_ASSERT(!wec && wn == 100000);
        peer.close();
    }

    // 场景 4: 对端通告 WS=15（RFC 7323 规定 >14 视为无效，忽略该选项）。
    // 本端 SYN-ACK 不带 WS；对端窗口字段不缩放.
    {
        engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
            std::chrono::seconds(30), 1024 * 1024,
            std::chrono::milliseconds(5000),
            std::chrono::milliseconds(5000));
        auto &io = env.io;
        tun_tcp_acceptor acceptor(env.engine);
        tun_tcp_socket peer(io.get_executor());
        std::promise<boost::system::error_code> accept_done;
        acceptor.async_accept(peer, [&](boost::system::error_code ec) {
            if (!ec) {
                peer.accept();
            }
            accept_done.set_value(ec);
        });

        env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12383, DEST_PORT, 0x02, 8000,
            0, 4096, {}, true, 15));
        std::vector<uint8_t> pkt;
        if (!env.dev.read_packet(pkt)) {
            TEST_THROW("no SYN-ACK");
        }
        ip_hdr_info ipi;
        if (!parse_ip(pkt, ipi)) {
            TEST_THROW("parse_ip failed");
        }
        tcp_hdr_info ti;
        if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
            TEST_THROW("parse_tcp failed");
        }
        int ws = -1;
        TEST_ASSERT(!synack_wscale(pkt, &ws)); // 无效 WS 按未通告处理
        const uint32_t engine_iss = ti.seq;

        env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12383, DEST_PORT, 0x10, 8001,
            engine_iss + 1, 1000, {}));
        future_get(accept_done.get_future());

        std::promise<std::pair<boost::system::error_code, size_t>> write_done;
        std::string payload(100000, 'x');
        peer.async_write_some(
            net::buffer(payload), [&](boost::system::error_code ec, size_t n) {
                write_done.set_value({ec, n});
            });

        const size_t sent = drain_data_segments(env, pkt, ipi, ti, 1000);
        TEST_ASSERT(sent == 1000);
        if (env.dev.read_packet(pkt, 200)) {
            TEST_THROW("over-sent with invalid WS");
        }
        size_t acked = sent;
        while (acked < 100000) {
            // 引擎挂起在窗口耗尽时：先 ACK 推进窗口，再读新数据
            env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12383, DEST_PORT, 0x10,
                8001, engine_iss + 1 +
                static_cast<uint32_t>(acked),
                65535, {}));
            if (!env.dev.read_packet(pkt)) {
                TEST_THROW("no data segment");
            }
            if (!verify_packet(pkt)) {
                TEST_THROW("verify_packet failed");
            }
            if (!parse_ip(pkt, ipi)) {
                TEST_THROW("parse_ip failed");
            }
            if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
                TEST_THROW("parse_tcp failed");
            }
            if (ti.len == 0) {
                TEST_THROW("unexpected control segment");
            }
            acked += ti.len;
        }
        TEST_ASSERT(acked == 100000);
        // 循环末次读到的段尚未确认：补最终 ACK 完成写操作
        env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12383, DEST_PORT, 0x10,
            8001, engine_iss + 1 + 100000, 65535, {}));
        auto [wec, wn] = future_get(write_done.get_future(), 8000);
        TEST_ASSERT(!wec && wn == 100000);
        peer.close();
    }
}

BOOST_AUTO_TEST_CASE(test_zero_window_flow_control)
{
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(
        make_tcp(CLIENT_IP, DEST_IP, 12346, DEST_PORT, 0x02, 2000, 0, 0, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    tcp_hdr_info ti;
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    const uint32_t engine_iss = ti.seq;

    // 客户端 ACK（窗口 0）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12346, DEST_PORT, 0x10, 2001,
        engine_iss + 1, 0, {}));
    future_get(accept_done.get_future());

    // 写入应因窗口为 0 而挂起
    std::promise<std::pair<boost::system::error_code, size_t>> write_done;
    peer.async_write_some(net::buffer("x", 1),
        [&](boost::system::error_code ec, size_t n) {
            write_done.set_value({ec, n});
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto wf = write_done.get_future();
    if (wf.wait_for(std::chrono::milliseconds(0)) ==
        std::future_status::ready) {
        TEST_THROW("write should be blocked by zero window");
    }

    // 窗口更新 ACK -> 写入恢复
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12346, DEST_PORT, 0x10, 2001,
        engine_iss + 1, 4096, {}));

    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no data after window update");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT(ti.len == 1 && ti.data[0] == 'x');

    // 客户端 ACK 数据字节：写操作在数据确认后完成
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12346, DEST_PORT, 0x10, 2001,
        engine_iss + 2, 4096, {}));
    auto [wec, wn] = future_get(std::move(wf));
    TEST_ASSERT(!wec && wn == 1);

    peer.close();
}

BOOST_AUTO_TEST_CASE(test_zero_window_persist_probe)
{
    // 零窗口持久计时器：客户端通告窗口 0 且不主动发窗口更新时，引擎必须
    // 周期性发送窗口探测；探测字节被确认后写操作完成且不丢失/重复数据.
    engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
        std::chrono::seconds(30), 1024 * 1024,
        std::chrono::milliseconds(150));
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(
        make_tcp(CLIENT_IP, DEST_IP, 12355, DEST_PORT, 0x02, 4000, 0, 0, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    const uint32_t engine_iss = ti.seq;

    // 客户端 ACK（窗口 0）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12355, DEST_PORT, 0x10, 4001,
        engine_iss + 1, 0, {}));
    future_get(accept_done.get_future());

    // 写入因窗口 0 挂起
    std::promise<std::pair<boost::system::error_code, size_t>> write_done;
    peer.async_write_some(net::buffer("x", 1),
        [&](boost::system::error_code ec, size_t n) {
            write_done.set_value({ec, n});
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto wf = write_done.get_future();
    if (wf.wait_for(std::chrono::milliseconds(0)) ==
        std::future_status::ready) {
        TEST_THROW("write should be blocked by zero window");
    }

    // 第一轮窗口探测（约 150ms 后）：1 字节数据，seq = iss + 1
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no first window probe");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT((ti.flags & 0x10) != 0);
    TEST_ASSERT(ti.seq == engine_iss + 1);
    TEST_ASSERT(ti.len == 1 && ti.data[0] == 'x');

    // 客户端仍通告窗口 0：探测字节被丢弃，引擎应继续周期探测
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12355, DEST_PORT, 0x10, 4001,
        engine_iss + 1, 0, {}));
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no second window probe");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT((ti.flags & 0x10) != 0);
    TEST_ASSERT(ti.seq == engine_iss + 1);
    TEST_ASSERT(ti.len == 1 && ti.data[0] == 'x');

    // 窗口恢复并接收探测字节：写操作完成，探测字节不重复发送
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12355, DEST_PORT, 0x10, 4001,
        engine_iss + 2, 4096, {}));
    auto [wec, wn] = future_get(std::move(wf));
    TEST_ASSERT(!wec && wn == 1);
    // 探测字节已作为数据交付，引擎不应再发送数据段；若有也必须是后续序号
    if (env.dev.read_packet(pkt, 200)) {
        if (!verify_packet(pkt)) {
            TEST_THROW("verify_packet failed");
        }
        if (!parse_ip(pkt, ipi)) {
            TEST_THROW("parse_ip failed");
        }
        if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
            TEST_THROW("parse_tcp failed");
        }
        TEST_ASSERT(ti.seq == engine_iss + 2);
    }

    peer.close();
}

BOOST_AUTO_TEST_CASE(test_zero_window_persist_exhausted)
{
    // 零窗口持久探测上限：客户端持续通告窗口 0 且静默（窗口更新/ACK 缺失）
    // 时，引擎在探测次数超过上限后以 RST 中断连接并以 connection_reset
    // 完成挂起写（回归：修复前无限探测，流表条目与挂起写永久滞留）.
    engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
        std::chrono::seconds(30), 1024 * 1024,
        std::chrono::milliseconds(150), std::chrono::milliseconds(200), 8,
        1, false, 3);
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12399, DEST_PORT, 0x02, 5000,
        0, 0, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    const uint32_t engine_iss = ti.seq;

    // 客户端 ACK（窗口 0）后写入挂起
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12399, DEST_PORT, 0x10, 5001,
        engine_iss + 1, 0, {}));
    future_get(accept_done.get_future());

    std::promise<std::pair<boost::system::error_code, size_t>> write_done;
    peer.async_write_some(net::buffer("x", 1),
        [&](boost::system::error_code ec, size_t n) {
            write_done.set_value({ec, n});
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    auto wf = write_done.get_future();
    if (wf.wait_for(std::chrono::milliseconds(0)) ==
        std::future_status::ready) {
        TEST_THROW("write should be blocked by zero window");
    }

    // 三轮窗口探测（150ms 起指数退避），客户端始终不回应
    for (int i = 0; i < 3; ++i) {
        if (!env.dev.read_packet(pkt)) {
            TEST_THROW("no window probe");
        }
        if (!verify_packet(pkt)) {
            TEST_THROW("verify_packet failed");
        }
        if (!parse_ip(pkt, ipi)) {
            TEST_THROW("parse_ip failed");
        }
        if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
            TEST_THROW("parse_tcp failed");
        }
        TEST_ASSERT((ti.flags & 0x10) != 0);
        TEST_ASSERT(ti.seq == engine_iss + 1);
        TEST_ASSERT(ti.len == 1 && ti.data[0] == 'x');
    }

    // 探测超限：引擎发送 RST 并以 connection_reset 完成挂起写
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no RST after persist probes exhausted");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT((ti.flags & 0x04) != 0); // RST

    auto [wec, wn] = future_get(std::move(wf));
    TEST_ASSERT(wec == net::error::connection_reset);

    peer.close();
}

BOOST_AUTO_TEST_CASE(test_synack_wscale_buffer_reuse)
{
    // 回归：缓冲池复用不得让"不带 WS"的 SYN-ACK 残留上一连接的 WS 选项。
    // 先建立带 WS 连接（SYN-ACK 携带 WS 选项后缓冲回收入池），再建立不带
    // WS 连接，断言其 SYN-ACK 选项区无 WS（修复前 opt[4..7] 残留垃圾字节）.
    engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
        std::chrono::seconds(30), 1024 * 1024,
        std::chrono::milliseconds(5000),
        std::chrono::milliseconds(5000));
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer_a(io.get_executor());
    tun_tcp_socket peer_b(io.get_executor());

    std::promise<boost::system::error_code> accept_a;
    acceptor.async_accept(peer_a, [&](boost::system::error_code ec) {
        if (!ec) {
            peer_a.accept();
        }
        accept_a.set_value(ec);
    });

    // 连接 A：对端通告 WS=3，SYN-ACK 携带 WS 选项，缓冲随后回收入池
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12390, DEST_PORT, 0x02, 9000, 0,
        4096, {}, true, 3));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK A");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    tcp_hdr_info ti;
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    int ws = -1;
    TEST_ASSERT(synack_wscale(pkt, &ws) && ws == 7);
    // 不回复 ACK；等待 SYN-ACK 缓冲异步回收入池（写完成回调在 Strand 上）
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 连接 B：对端未通告 WS，复用缓冲的 SYN-ACK 不得携带 WS 选项
    std::promise<boost::system::error_code> accept_b;
    acceptor.async_accept(peer_b, [&](boost::system::error_code ec) {
        if (!ec) {
            peer_b.accept();
        }
        accept_b.set_value(ec);
    });
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12391, DEST_PORT, 0x02, 9100, 0,
        4096, {}, true));
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK B");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT(!synack_wscale(pkt, &ws)); // 复用缓冲不得残留 WS 选项
    peer_a.close();
    peer_b.close();
}

BOOST_AUTO_TEST_CASE(test_syn_with_data)
{
    // TFO：客户端 SYN 携带数据。引擎缓存数据并在 SYN-ACK 中捎带确认，
    // 握手完成后交付给应用；数据不丢失也不重复.
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    const std::string hello = "hello-tfo";
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12356, DEST_PORT, 0x02, 5000, 0,
        65535, {hello.begin(), hello.end()}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT(ti.ack == 5001 + hello.size()); // SYN-ACK 捎带确认 TFO 数据
    const uint32_t engine_iss = ti.seq;
    future_get(accept_done.get_future());

    // 握手完成前注册读（挂起，握手完成后交付 TFO 数据）
    std::promise<std::pair<boost::system::error_code, size_t>> read_done;
    char buf[64];
    peer.async_read_some(net::buffer(buf),
        [&](boost::system::error_code ec, size_t n) {
            read_done.set_value({ec, n});
        });

    // 客户端 ACK 完成握手
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12356, DEST_PORT, 0x10,
        5001 + hello.size(), engine_iss + 1, 65535, {}));

    auto [rec, rn] = future_get(read_done.get_future());
    TEST_ASSERT(!rec && rn == hello.size());
    TEST_ASSERT(std::string(buf, rn) == hello);

    // 客户端重传数据（seq 已确认过）应被当作重复段丢弃，不重复交付
    std::promise<std::pair<boost::system::error_code, size_t>> again_done;
    peer.async_read_some(net::buffer(buf),
        [&](boost::system::error_code ec, size_t n) {
            again_done.set_value({ec, n});
        });
    auto agf = again_done.get_future();
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12356, DEST_PORT, 0x18, 5001,
        engine_iss + 1, 65535, {hello.begin(),
        hello.end()}));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (agf.wait_for(std::chrono::milliseconds(0)) ==
        std::future_status::ready) {
        TEST_THROW("duplicate TFO data must not be delivered");
    }

    // 客户端发送新的按序数据：挂起读交付该数据后完成，避免 close 时
    // 引擎异步完成读回调引用已析构的 promise.
    const std::string tail = "tail";
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12356, DEST_PORT, 0x18,
        5001 + hello.size(), engine_iss + 1, 65535,
        {tail.begin(), tail.end()}));
    auto [aec, an] = future_get(std::move(agf));
    TEST_ASSERT(!aec && an == tail.size());
    TEST_ASSERT(std::string(buf, an) == tail);

    peer.close();
}

// 部分重叠重传：ACK 丢失后对端从 snd_una 重传整段（头低于 rcv_nxt、尾超出），
// 引擎必须修剪已确认前缀并交付尾部，而非整段丢弃（否则死锁至对端 RST）。
BOOST_AUTO_TEST_CASE(test_partial_overlap_retransmit)
{
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12341, DEST_PORT, 0x02, 1000, 0,
        65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse failed");
    }
    const uint32_t engine_iss = ti.seq;
    future_get(accept_done.get_future());

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12341, DEST_PORT, 0x10, 1001,
        engine_iss + 1, 65535, {}));

    // 客户端发送 "hello"（seq 1001），引擎交付
    const std::string hello = "hello";
    std::promise<std::pair<boost::system::error_code, size_t>> r1;
    char buf[64];
    peer.async_read_some(net::buffer(buf),
        [&](boost::system::error_code ec, size_t n) {
            r1.set_value({ec, n});
        });
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12341, DEST_PORT, 0x18, 1001,
        engine_iss + 1, 65535, {hello.begin(), hello.end()}));
    auto [e1, n1] = future_get(r1.get_future());
    TEST_ASSERT(!e1 && n1 == hello.size());
    TEST_ASSERT(std::string(buf, n1) == hello);

    // 引擎 ACK 1006（假设该 ACK 在链路上丢失）
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no ack1");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse failed");
    }
    TEST_ASSERT((ti.flags & 0x10) != 0 && ti.ack == 1006);

    // 对端重传 [1001, 1011) = "helloworld"：头 1001 < rcv_nxt 1006、尾超出，
    // 须修剪掉已确认的 "hello" 前缀，仅交付尾部 "world"
    const std::string world = "world";
    std::promise<std::pair<boost::system::error_code, size_t>> r2;
    peer.async_read_some(net::buffer(buf),
        [&](boost::system::error_code ec, size_t n) {
            r2.set_value({ec, n});
        });
    const std::string hw = hello + world;
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12341, DEST_PORT, 0x18, 1001,
        engine_iss + 1, 65535, {hw.begin(), hw.end()}));
    auto [e2, n2] = future_get(r2.get_future());
    TEST_ASSERT(!e2 && n2 == world.size());
    TEST_ASSERT(std::string(buf, n2) == world);

    // 引擎 ACK 推进到 1011
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no ack2");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse failed");
    }
    TEST_ASSERT((ti.flags & 0x10) != 0 && ti.ack == 1011);

    peer.close();
}

// 批量 ACK（延迟 ACK）：无读挂起时数据进入缓冲路径，按序数据段不逐段
// 确认，每 2 段合并补发一次；应用读取后由读完成补发挂起的 ACK.
BOOST_AUTO_TEST_CASE(test_delayed_ack_batching)
{
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    // 握手
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x02,
        1000, 0, 65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse failed");
    }
    const uint32_t engine_iss = ti.seq;
    future_get(accept_done.get_future());
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x10,
        1001, engine_iss + 1, 65535, {}));

    // 无读挂起时，首个数据段不立即 ACK（挂起等待合并）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x18,
        1001, engine_iss + 1, 65535, {'a', 'b', 'c', 'd'}));
    if (env.dev.read_packet(pkt, 200)) {
        TEST_THROW("unexpected immediate ACK for first segment");
    }

    // 连续第 2 段：合并补发一个 ACK，覆盖两段（ack = 1009）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x18,
        1005, engine_iss + 1, 65535, {'e', 'f', 'g', 'h'}));
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no batched ACK");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse failed");
    }
    TEST_ASSERT((ti.flags & 0x10) != 0 && ti.ack == 1001 + 8);
    if (env.dev.read_packet(pkt, 200)) {
        TEST_THROW("duplicate ACK after batch");
    }

    // 第 3 段再次挂起；应用读取后由读完成补发 ACK（ack = 1013）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x18,
        1009, engine_iss + 1, 65535, {'i', 'j', 'k', 'l'}));
    if (env.dev.read_packet(pkt, 200)) {
        TEST_THROW("unexpected immediate ACK for third segment");
    }

    std::promise<std::pair<boost::system::error_code, size_t>> read_done;
    char buf[64];
    peer.async_read_some(net::buffer(buf),
        [&](boost::system::error_code ec, size_t n) {
            read_done.set_value({ec, n});
        });
    auto [rec, rn] = future_get(read_done.get_future());
    TEST_ASSERT(!rec && rn == 12);
    TEST_ASSERT(std::string(buf, rn) == "abcdefghijkl");

    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no ACK after read");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse failed");
    }
    TEST_ASSERT((ti.flags & 0x10) != 0 && ti.ack == 1001 + 12);

    peer.close();
}

// 整段已被确认的重复段（对端 ACK 丢失后重传）：引擎必须补发 re-ACK 使对端
// 立即推进 snd_una，而非静默丢弃等下一个 RTO（否则连接死锁）。
BOOST_AUTO_TEST_CASE(test_duplicate_segment_reack)
{
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12342, DEST_PORT, 0x02, 2000, 0,
        65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse failed");
    }
    const uint32_t engine_iss = ti.seq;
    future_get(accept_done.get_future());

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12342, DEST_PORT, 0x10, 2001,
        engine_iss + 1, 65535, {}));

    // 交付完整 10 字节（rcv_nxt = 2011）
    const std::string data = "helloworld";
    std::promise<std::pair<boost::system::error_code, size_t>> r1;
    char buf[64];
    peer.async_read_some(net::buffer(buf),
        [&](boost::system::error_code ec, size_t n) {
            r1.set_value({ec, n});
        });
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12342, DEST_PORT, 0x18, 2001,
        engine_iss + 1, 65535, {data.begin(), data.end()}));
    auto [e1, n1] = future_get(r1.get_future());
    TEST_ASSERT(!e1 && n1 == data.size());
    TEST_ASSERT(std::string(buf, n1) == data);

    // 引擎 ACK 2011
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no ack1");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse failed");
    }
    TEST_ASSERT((ti.flags & 0x10) != 0 && ti.ack == 2011);

    // 对端重传已确认的前 5 字节 [2001, 2006)：整段低于 rcv_nxt，引擎须
    // 补发 re-ACK 2011（含 no-data），而非静默丢弃
    const std::string hello = "hello";
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12342, DEST_PORT, 0x18, 2001,
        engine_iss + 1, 65535, {hello.begin(), hello.end()}));

    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no re-ack");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse failed");
    }
    TEST_ASSERT((ti.flags & 0x10) != 0 && (ti.flags & 0x08) == 0); // ACK 且无数据
    TEST_ASSERT(ti.ack == 2011);
    TEST_ASSERT(ipi.payload_len == 20); // 纯 ACK 段

    peer.close();
}

// TIME_WAIT 收到同元组新 SYN：须重建连接（RFC 1122 §4.2.2.13），
// 否则关闭后 tcp_time_wait_timeout 内对端重连全部失败。
BOOST_AUTO_TEST_CASE(test_time_wait_reconnect)
{
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12343, DEST_PORT, 0x02, 4000, 0,
        65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse failed");
    }
    const uint32_t engine_iss = ti.seq;
    future_get(accept_done.get_future());

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12343, DEST_PORT, 0x10, 4001,
        engine_iss + 1, 65535, {}));

    // 同步握手完成：客户端发 1 字节数据，应用读到即证明已 ESTABLISHED
    //（避免 shutdown 的 Strand 派发与握手 ACK 处理竞态，后者在
    // SYN_ACK_SENT 状态下会以 RST 关闭）.
    {
        const uint8_t sync = 0x01;
        std::promise<std::pair<boost::system::error_code, size_t>> sr;
        char sbuf[8];
        peer.async_read_some(net::buffer(sbuf),
            [&](boost::system::error_code ec, size_t n) {
                sr.set_value({ec, n});
            });
        env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12343, DEST_PORT, 0x18,
            4001, engine_iss + 1, 65535, {sync}));
        auto [se, sn] = future_get(sr.get_future());
        TEST_ASSERT(!se && sn == 1 && sbuf[0] == sync);
        // 引擎 ACK 4002
        if (!env.dev.read_packet(pkt)) {
            TEST_THROW("no sync ack");
        }
        if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len,
                ti)) {
            TEST_THROW("parse failed");
        }
        TEST_ASSERT((ti.flags & 0x10) != 0 && ti.ack == 4002);
    }

    // 应用关闭发送侧：引擎发 FIN 进入 FIN_WAIT_1
    boost::system::error_code sec;
    peer.shutdown(net::ip::tcp::socket::shutdown_send, sec);
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no FIN");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse failed");
    }
    TEST_ASSERT((ti.flags & 0x01) != 0); // FIN
    const uint32_t fin_seq = ti.seq;
    TEST_ASSERT(fin_seq == engine_iss + 1);

    // 客户端 ACK FIN -> FIN_WAIT_2（客户端序号已推进到 4002）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12343, DEST_PORT, 0x10, 4002,
        fin_seq + 1, 65535, {}));

    // 客户端 FIN -> 引擎 TIME_WAIT（引擎 ACK 4003）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12343, DEST_PORT, 0x11, 4002,
        fin_seq + 1, 65535, {}));
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no FIN ack");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse failed");
    }
    TEST_ASSERT((ti.flags & 0x10) != 0 && ti.ack == 4003);

    // 同元组新 SYN：引擎须重建连接而非静默丢弃
    std::promise<boost::system::error_code> accept2;
    tun_tcp_socket peer2(io.get_executor());
    acceptor.async_accept(
        peer2, [&](boost::system::error_code ec) {
            if (!ec) {
                peer2.accept(); // 延迟握手：批准后引擎才回复新 SYN-ACK
            }
            accept2.set_value(ec);
        });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12343, DEST_PORT, 0x02, 8000, 0,
        65535, {}));
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK2");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse failed");
    }
    TEST_ASSERT((ti.flags & 0x12) == 0x12); // 新 SYN-ACK
    TEST_ASSERT(ti.ack == 8001);            // 新连接的 irs + 1
    auto aec2 = future_get(accept2.get_future());
    TEST_ASSERT(!aec2);

    peer.close();
    peer2.close();
}

BOOST_AUTO_TEST_CASE(test_rst)
{
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12347, DEST_PORT, 0x02, 3000, 0,
        65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    const uint32_t engine_iss = ti.seq;
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12347, DEST_PORT, 0x10, 3001,
        engine_iss + 1, 65535, {}));
    future_get(accept_done.get_future());

    // 挂起读取，等待 RST
    std::promise<std::pair<boost::system::error_code, size_t>> read_done;
    char buf[64];
    peer.async_read_some(net::buffer(buf),
        [&](boost::system::error_code ec, size_t n) {
            read_done.set_value({ec, n});
        });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12347, DEST_PORT, 0x04, 3001,
        engine_iss + 1, 0, {}));
    auto [rec, rn] = future_get(read_done.get_future());
    TEST_ASSERT(rec == net::error::connection_reset);
    TEST_ASSERT(!peer.is_open());
}

BOOST_AUTO_TEST_CASE(test_app_reset)
{
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12348, DEST_PORT, 0x02, 4000, 0,
        65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    const uint32_t engine_iss = ti.seq;
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12348, DEST_PORT, 0x10, 4001,
        engine_iss + 1, 65535, {}));
    future_get(accept_done.get_future());

    // 应用主动 reset()：后端连接失败等场景
    peer.reset();
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no RST");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT((ti.flags & 0x04) != 0); // RST
    TEST_ASSERT(!peer.is_open());
}

BOOST_AUTO_TEST_CASE(test_data_with_fin)
{
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12349, DEST_PORT, 0x02, 5000, 0,
        65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    const uint32_t engine_iss = ti.seq;
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12349, DEST_PORT, 0x10, 5001,
        engine_iss + 1, 65535, {}));
    future_get(accept_done.get_future());

    // 客户端发送 FIN|PSH|ACK 且携带数据（常见关闭方式）
    const std::string tail = "bye";
    const std::vector<uint8_t> data(tail.begin(), tail.end());
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12349, DEST_PORT, 0x19, 5001,
        engine_iss + 1, 65535, data));

    // 应用读到数据
    std::promise<std::pair<boost::system::error_code, size_t>> read_done;
    char buf[64];
    peer.async_read_some(net::buffer(buf),
        [&](boost::system::error_code ec, size_t n) {
            read_done.set_value({ec, n});
        });
    auto [rec, rn] = future_get(read_done.get_future());
    TEST_ASSERT(!rec && rn == data.size());
    TEST_ASSERT(std::string(buf, rn) == tail);

    // 再次读取应得到 EOF（同段 FIN 已被正确处理）
    std::promise<std::pair<boost::system::error_code, size_t>> eof_done;
    peer.async_read_some(net::buffer(buf),
        [&](boost::system::error_code ec, size_t n) {
            eof_done.set_value({ec, n});
        });
    auto [eec, en] = future_get(eof_done.get_future());
    TEST_ASSERT(eec == net::error::eof && en == 0);

    // 每段立即确认：数据 ACK 与 FIN ACK 分开发送，最终必须确认到
    // FIN 序号（ack = 5001 + data.size() + 1）
    bool fin_acked = false;
    for (int i = 0; i < 4; ++i) {
        if (!env.dev.read_packet(pkt)) {
            break;
        }
        if (!verify_packet(pkt)) {
            TEST_THROW("verify_packet failed");
        }
        if (!parse_ip(pkt, ipi)) {
            TEST_THROW("parse_ip failed");
        }
        if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
            TEST_THROW("parse_tcp failed");
        }
        if ((ti.flags & 0x10) != 0 && ti.ack == 5001 + data.size() + 1) {
            fin_acked = true;
            break;
        }
    }
    if (!fin_acked) {
        TEST_THROW("FIN not acked with data+1");
    }

    peer.close();
}

BOOST_AUTO_TEST_CASE(test_handshake_ack_with_fin)
{
    // 客户端在握手 ACK 中合并 FIN（快速关闭）：引擎必须完成 FIN 处理
    // （fin_received + CLOSE_WAIT），挂起的读收到 EOF；原实现漏掉同段
    // FIN，读侧永远等不到 EOF，直到 30s 超时被强制清理.
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12352, DEST_PORT, 0x02, 6000, 0,
        65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    const uint32_t engine_iss = ti.seq;
    future_get(accept_done.get_future());

    // 握手尚未完成时注册读：读操作挂起
    std::promise<std::pair<boost::system::error_code, size_t>> eof_done;
    char buf[64];
    peer.async_read_some(net::buffer(buf),
        [&](boost::system::error_code ec, size_t n) {
            eof_done.set_value({ec, n});
        });

    // 客户端 ACK+FIN 同段完成握手并立即关闭
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12352, DEST_PORT, 0x11, 6001,
        engine_iss + 1, 65535, {}));

    auto [rec, rn] = future_get(eof_done.get_future());
    TEST_ASSERT(rec == net::error::eof && rn == 0); // EOF
    TEST_ASSERT(peer.is_open());  // CLOSE_WAIT 仍可写

    // 引擎确认 FIN（ack = 6000 + 1 + 1）
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no FIN ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT((ti.flags & 0x10) != 0);
    TEST_ASSERT(ti.ack == 6002);

    peer.close();
}

BOOST_AUTO_TEST_CASE(test_shutdown_receive_discards)
{
    // 应用 shutdown(receive) 后，客户端数据应被丢弃并正常确认（rcv_nxt
    // 推进），而不是继续写入无人消费的 rx_data 占用缓冲记账.
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12353, DEST_PORT, 0x02, 7000, 0,
        65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    const uint32_t engine_iss = ti.seq;
    future_get(accept_done.get_future());

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12353, DEST_PORT, 0x10, 7001,
        engine_iss + 1, 65535, {}));

    boost::system::error_code sec;
    peer.shutdown(net::ip::tcp::socket::shutdown_receive, sec);
    TEST_ASSERT(!sec);

    // 两段数据：第二段触发 delayed ACK 立即发送，确认全部丢弃的字节
    const std::string a = "junk";
    const std::string b = "more";
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12353, DEST_PORT, 0x18, 7001,
        engine_iss + 1, 65535,
        {a.begin(), a.end()}));
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12353, DEST_PORT, 0x18,
        7001 + a.size(), engine_iss + 1, 65535,
        {b.begin(), b.end()}));

    // 每段立即确认：各段 ACK 分开发送，最终必须确认全部丢弃字节
    bool all_acked = false;
    for (int i = 0; i < 4; ++i) {
        if (!env.dev.read_packet(pkt)) {
            break;
        }
        if (!verify_packet(pkt)) {
            TEST_THROW("verify_packet failed");
        }
        if (!parse_ip(pkt, ipi)) {
            TEST_THROW("parse_ip failed");
        }
        if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
            TEST_THROW("parse_tcp failed");
        }
        if ((ti.flags & 0x10) != 0 &&
            ti.ack == 7001 + a.size() + b.size()) {
            all_acked = true;
            break;
        }
    }
    if (!all_acked) {
        TEST_THROW("discarded bytes not fully acked");
    }

    peer.close();
}

BOOST_AUTO_TEST_CASE(test_write_after_shutdown_send)
{
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12350, DEST_PORT, 0x02, 6000, 0,
        65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    const uint32_t engine_iss = ti.seq;
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12350, DEST_PORT, 0x10, 6001,
        engine_iss + 1, 65535, {}));
    future_get(accept_done.get_future());
    // accept_done 在收到 SYN 时即触发，早于三次握手完成：等待引擎处理
    // 客户端 ACK 进入 ESTABLISHED，避免 shutdown 落在 SYN_ACK_SENT 上发 RST。
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // shutdown(send)：发出 FIN
    boost::system::error_code sec;
    peer.shutdown(net::ip::tcp::socket::shutdown_send, sec);
    TEST_ASSERT(!sec);

    // 之后写入应被拒绝（fin_sent 已置位）
    std::promise<std::pair<boost::system::error_code, size_t>> write_done;
    peer.async_write_some(net::buffer("x", 1),
        [&](boost::system::error_code ec, size_t n) {
            write_done.set_value({ec, n});
        });
    auto [wec, wn] = future_get(write_done.get_future());
    TEST_ASSERT(wec == net::error::bad_descriptor);
    TEST_ASSERT(wn == 0);

    // 设备应只收到 FIN，无数据段
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no FIN");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT((ti.flags & 0x01) != 0); // FIN
    TEST_ASSERT(ti.len == 0);

    peer.close();
}

BOOST_AUTO_TEST_CASE(test_unaccepted_connection_cleanup)
{
    // SYN 超时设为 1 秒：收到 SYN 后若不 async_accept，
    // 引擎应发送 RST 并回收资源
    engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(1),
        std::chrono::seconds(1));

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12351, DEST_PORT, 0x02, 7000, 0,
        65535, {}));

    // 不调用 async_accept，等待引擎 SYN 超时清理（发送 RST）
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt, 5000)) {
        TEST_THROW("no RST after syn timeout");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    TEST_ASSERT((ti.flags & 0x04) != 0); // RST
}

BOOST_AUTO_TEST_CASE(test_write_queue_limit)
{
    // 单写模型：窗口为 0 时写操作挂起等待窗口更新，
    // 上一写未完成时重叠写立即返回 no_buffer_space.
    engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
        std::chrono::seconds(30), 1024 * 1024);
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(
        make_tcp(CLIENT_IP, DEST_IP, 12352, DEST_PORT, 0x02, 8000, 0, 0, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    const uint32_t engine_iss = ti.seq;
    // 客户端 ACK（窗口 0）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12352, DEST_PORT, 0x10, 8001,
        engine_iss + 1, 0, {}));
    future_get(accept_done.get_future());

    // 第一个写：窗口为 0，挂起等待窗口更新
    std::promise<std::pair<boost::system::error_code, size_t>> w1;
    peer.async_write_some(
        net::buffer("aaaaaaaa", 8),
        [&](boost::system::error_code ec, size_t n) { w1.set_value({ec, n}); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 第二个写：上一写未完成，重叠写立即返回 no_buffer_space
    std::promise<std::pair<boost::system::error_code, size_t>> w2;
    peer.async_write_some(
        net::buffer("bbbbbbbb", 8),
        [&](boost::system::error_code ec, size_t n) { w2.set_value({ec, n}); });
    auto [w2ec, w2n] = future_get(w2.get_future());
    TEST_ASSERT(w2ec == net::error::no_buffer_space);
    TEST_ASSERT(w2n == 0);

    // 窗口更新后数据发出；客户端 ACK 数据后第一个写完成
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12352, DEST_PORT, 0x10, 8001,
        engine_iss + 1, 4096, {}));
    std::vector<uint8_t> pkt2;
    if (!env.dev.read_packet(pkt2)) {
        TEST_THROW("no data after window update");
    }
    if (!verify_packet(pkt2)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi2;
    tcp_hdr_info ti2;
    if (!parse_ip(pkt2, ipi2) ||
        !parse_tcp(ipi2.payload, ipi2.payload_len, ti2)) {
        TEST_THROW("parse data failed");
    }
    TEST_ASSERT(ti2.len == 8 &&
        std::string(reinterpret_cast<const char *>(ti2.data), ti2.len) ==
        "aaaaaaaa");
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12352, DEST_PORT, 0x10, 8001,
        engine_iss + 9, 4096, {}));
    auto [w1ec, w1n] = future_get(w1.get_future());
    TEST_ASSERT(!w1ec && w1n == 8);
    peer.close();
}

BOOST_AUTO_TEST_CASE(test_write_large_single_op)
{
    // 单写模型：单次写入可大于队列上限，数据按 MSS 分片全部发送后
    // 才回调，不受"队列空间"限制（无排队字节记账）.
    engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
        std::chrono::seconds(30), 1024 * 1024);
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(
        make_tcp(CLIENT_IP, DEST_IP, 12355, DEST_PORT, 0x02, 10000, 0, 0, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    const uint32_t engine_iss = ti.seq;
    // 客户端 ACK（窗口充足）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12355, DEST_PORT, 0x10, 10001,
        engine_iss + 1, 4096, {}));
    future_get(accept_done.get_future());

    // 单次写入 3000 字节（超过队列上限 16 与单段 MSS）：应全部接受，
    // 按 MSS 分 3 段发送，全部确认后才回调.
    const std::string payload(3000, 'a');
    std::promise<std::pair<boost::system::error_code, size_t>> w1;
    peer.async_write_some(
        net::buffer(payload),
        [&](boost::system::error_code ec, size_t n) { w1.set_value({ec, n}); });

    // 设备收到 3 段，载荷合计等于写入字节数（MSS 1460: 1460+1460+80）
    size_t received = 0;
    for (int i = 0; i < 3; ++i) {
        if (!env.dev.read_packet(pkt)) {
            TEST_THROW("no data packet");
        }
        if (!verify_packet(pkt)) {
            TEST_THROW("verify_packet failed");
        }
        if (!parse_ip(pkt, ipi)) {
            TEST_THROW("parse_ip failed");
        }
        if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
            TEST_THROW("parse_tcp failed");
        }
        received += ti.len;
    }
    TEST_ASSERT(received == payload.size());

    // 客户端 ACK 全部数据：写操作在数据确认后完成
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12355, DEST_PORT, 0x10, 10001,
        engine_iss + 1 + static_cast<uint32_t>(payload.size()),
        4096, {}));
    auto [w1ec, w1n] = future_get(w1.get_future());
    TEST_ASSERT(!w1ec && w1n == payload.size());

    peer.close();
}

BOOST_AUTO_TEST_CASE(test_write_completion_requires_ack)
{
    // 写操作在数据被对端确认后才完成（ACK 确认制）：设备仅收到数据段时
    // 写仍挂起，ACK 到达后回调.
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(
        make_tcp(CLIENT_IP, DEST_IP, 12362, DEST_PORT, 0x02, 18000, 0, 0,
        {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse SYN-ACK failed");
    }
    const uint32_t engine_iss = ti.seq;
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12362, DEST_PORT, 0x10, 18001,
        engine_iss + 1, 65535, {}));
    future_get(accept_done.get_future());

    std::promise<std::pair<boost::system::error_code, size_t>> write_done;
    peer.async_write_some(net::buffer("x", 1),
        [&](boost::system::error_code ec, size_t n) {
            write_done.set_value({ec, n});
        });
    auto wf = write_done.get_future();

    // 设备收到数据段，但写未完成（等待对端 ACK）
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no data packet");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse data failed");
    }
    TEST_ASSERT(ti.seq == engine_iss + 1 && ti.len == 1 && ti.data[0] == 'x');
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    if (wf.wait_for(std::chrono::milliseconds(0)) ==
        std::future_status::ready) {
        TEST_THROW("write must not complete before ACK");
    }

    // 客户端 ACK 数据：写完成
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12362, DEST_PORT, 0x10, 18001,
        engine_iss + 2, 65535, {}));
    auto [wec, wn] = future_get(std::move(wf));
    TEST_ASSERT(!wec && wn == 1);
    peer.close();
}

BOOST_AUTO_TEST_CASE(test_rto_retransmit)
{
    // RTO 重传：数据段在链路丢失（对端不 ACK）时，引擎按 RTO 周期重传
    // 未确认数据（相同序号与载荷），确认后写完成且不再重传.
    engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
        std::chrono::seconds(30), 1024 * 1024,
        std::chrono::milliseconds(5000),
        std::chrono::milliseconds(50));
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(
        make_tcp(CLIENT_IP, DEST_IP, 12363, DEST_PORT, 0x02, 19000, 0, 0,
        {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse SYN-ACK failed");
    }
    const uint32_t engine_iss = ti.seq;
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12363, DEST_PORT, 0x10, 19001,
        engine_iss + 1, 65535, {}));
    future_get(accept_done.get_future());

    const std::string payload = "hello-rto";
    std::promise<std::pair<boost::system::error_code, size_t>> write_done;
    peer.async_write_some(
        net::buffer(payload),
        [&](boost::system::error_code ec, size_t n) {
            write_done.set_value({ec, n});
        });
    auto wf = write_done.get_future();

    // 首段发出
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no first data packet");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse data failed");
    }
    TEST_ASSERT(ti.seq == engine_iss + 1 && ti.len == payload.size());
    TEST_ASSERT(std::string(reinterpret_cast<const char *>(ti.data), ti.len) ==
        payload);

    // 对端不 ACK：RTO（50ms）后引擎重传相同序号与载荷
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no RTO retransmit");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse retransmit failed");
    }
    TEST_ASSERT(ti.seq == engine_iss + 1 && ti.len == payload.size());
    TEST_ASSERT(std::string(reinterpret_cast<const char *>(ti.data), ti.len) ==
        payload);

    // 客户端 ACK：写完成
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12363, DEST_PORT, 0x10, 19001,
        engine_iss + 1 +
        static_cast<uint32_t>(payload.size()),
        65535, {}));
    auto [wec, wn] = future_get(std::move(wf));
    TEST_ASSERT(!wec && wn == payload.size());
    peer.close();
}

BOOST_AUTO_TEST_CASE(test_fin_deferred_until_acked)
{
    // shutdown(send) 时仍有未确认数据：FIN 推迟到数据确认后发送，FIN
    // 序号紧跟数据末尾，不与在途数据段重叠.
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(
        make_tcp(CLIENT_IP, DEST_IP, 12364, DEST_PORT, 0x02, 20000, 0, 0,
        {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse SYN-ACK failed");
    }
    const uint32_t engine_iss = ti.seq;
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12364, DEST_PORT, 0x10, 20001,
        engine_iss + 1, 65535, {}));
    future_get(accept_done.get_future());

    // 写入数据但不确认，随后 shutdown(send)：FIN 必须推迟
    const std::string payload = "deferred";
    std::promise<std::pair<boost::system::error_code, size_t>> write_done;
    peer.async_write_some(
        net::buffer(payload),
        [&](boost::system::error_code ec, size_t n) {
            write_done.set_value({ec, n});
        });
    auto wf = write_done.get_future();
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no data packet");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse data failed");
    }
    TEST_ASSERT(ti.seq == engine_iss + 1 && ti.len == payload.size());

    boost::system::error_code sec;
    peer.shutdown(net::ip::tcp::socket::shutdown_send, sec);
    TEST_ASSERT(!sec);

    // 数据未确认前不得发送 FIN（短超时内设备无任何输出）
    if (env.dev.read_packet(pkt, 100)) {
        TEST_THROW("FIN must be deferred until data ACKed");
    }

    // 客户端 ACK 数据：写完成，随后引擎补发 FIN（seq = 数据末尾）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12364, DEST_PORT, 0x10, 20001,
        engine_iss + 1 +
        static_cast<uint32_t>(payload.size()),
        65535, {}));
    auto [wec, wn] = future_get(std::move(wf));
    TEST_ASSERT(!wec && wn == payload.size());
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no FIN after data ACKed");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse FIN failed");
    }
    TEST_ASSERT((ti.flags & 0x01) != 0);
    TEST_ASSERT(ti.seq ==
        engine_iss + 1 + static_cast<uint32_t>(payload.size()));
    peer.close();
}

BOOST_AUTO_TEST_CASE(test_close_with_unacked_sends_rst)
{
    // close() 时仍有未确认数据：无法优雅 FIN（FIN 序号超前于对端期望会被
    // 当作乱序丢弃），引擎改发 RST 快速释放连接.
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(
        make_tcp(CLIENT_IP, DEST_IP, 12365, DEST_PORT, 0x02, 21000, 0, 0,
        {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse SYN-ACK failed");
    }
    const uint32_t engine_iss = ti.seq;
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12365, DEST_PORT, 0x10, 21001,
        engine_iss + 1, 65535, {}));
    future_get(accept_done.get_future());

    std::promise<std::pair<boost::system::error_code, size_t>> write_done;
    peer.async_write_some(net::buffer("x", 1),
        [&](boost::system::error_code ec, size_t n) {
            write_done.set_value({ec, n});
        });
    auto wf = write_done.get_future();
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no data packet");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse data failed");
    }
    TEST_ASSERT(ti.len == 1 && ti.data[0] == 'x');

    // 数据未确认时 close：写以 operation_aborted 完成，设备收到 RST
    peer.close();
    auto [wec, wn] = future_get(std::move(wf));
    TEST_ASSERT(wec == net::error::operation_aborted && wn == 0);
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no RST");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse RST failed");
    }
    TEST_ASSERT((ti.flags & 0x04) != 0); // RST
}

BOOST_AUTO_TEST_CASE(test_rto_exhaustion)
{
    // RTO 重传超限：对端始终不确认，重传次数超过上限后引擎发送 RST
    // 并以 connection_reset 完成挂起写，避免连接永久悬挂.
    engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
        std::chrono::seconds(30), 1024 * 1024,
        std::chrono::milliseconds(5000),
        std::chrono::milliseconds(20), 3);
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(
        make_tcp(CLIENT_IP, DEST_IP, 12366, DEST_PORT, 0x02, 22000, 0, 0,
        {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse SYN-ACK failed");
    }
    const uint32_t engine_iss = ti.seq;
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12366, DEST_PORT, 0x10, 22001,
        engine_iss + 1, 65535, {}));
    future_get(accept_done.get_future());

    std::promise<std::pair<boost::system::error_code, size_t>> write_done;
    peer.async_write_some(net::buffer("x", 1),
        [&](boost::system::error_code ec, size_t n) {
            write_done.set_value({ec, n});
        });
    auto wf = write_done.get_future();
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no data packet");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse data failed");
    }
    TEST_ASSERT(ti.len == 1 && ti.data[0] == 'x');

    // 重传超限后：设备收到 RST，写以 connection_reset 完成
    bool rst_seen = false;
    for (int i = 0; i < 8 && !rst_seen; ++i) {
        if (!env.dev.read_packet(pkt)) {
            break;
        }
        if (!verify_packet(pkt)) {
            TEST_THROW("verify_packet failed");
        }
        if (!parse_ip(pkt, ipi) ||
            !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
            TEST_THROW("parse packet failed");
        }
        rst_seen = (ti.flags & 0x04) != 0;
    }
    TEST_ASSERT(rst_seen);
    auto [wec, wn] = future_get(std::move(wf));
    TEST_ASSERT(wec == net::error::connection_reset && wn == 0);
}

BOOST_AUTO_TEST_CASE(test_close_reopen)
{
    // close 后立即重新 open：旧异步清理不得误关新引擎（epoch 代际保护）。
    // 注意：引擎 close 会关闭注入的 fd，因此重新 open 需使用新的注入句柄。
    engine_env env;
    auto &io = env.io;

    env.engine.close();
    fake_device dev2;
    tun_config cfg;
    cfg.external_handle = dev2.inject_fd();
    cfg.external_mtu = 1500;
    cfg.ipv4_addr = "10.0.0.1";
    cfg.netmask = "255.255.255.0";
    boost::system::error_code ec;
    if (!env.engine.open(cfg, ec)) {
        TEST_THROW("reopen failed: " + ec.message());
    }

    // 验证新引擎数据通路正常：完成一次握手（经 dev2）
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());
    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code e) {
        if (!e) {
            peer.accept();
        }
        accept_done.set_value(e);
    });
    dev2.send(make_tcp(CLIENT_IP, DEST_IP, 12353, DEST_PORT, 0x02, 9000, 0,
        65535, {}));
    std::vector<uint8_t> pkt;
    if (!dev2.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK after reopen");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    dev2.send(make_tcp(CLIENT_IP, DEST_IP, 12353, DEST_PORT, 0x10, 9001,
        ti.seq + 1, 65535, {}));
    auto aec = future_get(accept_done.get_future());
    TEST_ASSERT(!aec);
    TEST_ASSERT(peer.is_open());
    peer.close();
}

BOOST_AUTO_TEST_CASE(test_fragmented_packet_dropped)
{
    // IPv4 分片包（MF 标志）应被引擎丢弃，不得建立流或回复
    engine_env env;

    // 构造带 MF 标志的 SYN 分片包（frag_off = 0x2000）
    std::vector<uint8_t> pkt = make_tcp(CLIENT_IP, DEST_IP, 12354, DEST_PORT,
        0x02, 10000, 0, 65535, {});
    pkt[6] = 0x20;
    pkt[7] = 0x00;
    const uint16_t c = test::csum16(pkt.data(), 20);
    pkt[10] = static_cast<uint8_t>(c >> 8);
    pkt[11] = static_cast<uint8_t>(c & 0xff);
    env.dev.send(pkt);

    // 引擎不应回复 SYN-ACK
    if (env.dev.read_packet(pkt, 300)) {
        TEST_THROW("fragmented SYN should be dropped");
    }
    TEST_ASSERT(env.engine.stats().rx_dropped.load() >= 1);
}

BOOST_AUTO_TEST_CASE(test_oversized_declared_length)
{
    // 注入流伪造报文头声明长度超过 MTU：引擎应丢弃而非卡死读循环，
    // 后续合法报文仍须正常处理（on_read 拆包防护）。
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    // 声明 total_len = 65535 的伪 IPv4 报文（仅 20 头 + 16 字节）
    std::vector<uint8_t> junk(36, 0);
    junk[0] = 0x45;
    junk[2] = 0xff;
    junk[3] = 0xff;
    const uint16_t c = test::csum16(junk.data(), 20);
    junk[10] = static_cast<uint8_t>(c >> 8);
    junk[11] = static_cast<uint8_t>(c & 0xff);
    env.dev.send(junk);

    // 等待引擎消费并丢弃伪报文，避免与后续 SYN 在同一读中被连带丢弃
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 合法 SYN 必须仍被处理（收到 SYN-ACK）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12356, DEST_PORT, 0x02, 12000, 0,
        65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK after oversized junk");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    TEST_ASSERT(ipi.proto == 6);
}

BOOST_AUTO_TEST_CASE(test_reentrant_reset_in_handler)
{
    // 读完成回调在引擎 Strand 上内联执行时，回调内 reset() 并销毁流：
    // 引擎在回调返回后仍须安全使用流（on_packet 强引用保活），否则 UAF。
    engine_env env;
    tun_tcp_acceptor acceptor(env.engine);
    // 使用引擎 Strand 作为执行器：完成回调在 Strand 上内联执行
    auto stream = std::make_shared<tun_tcp_socket>(env.engine.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(*stream, [&](boost::system::error_code e) {
        if (!e) {
            stream->accept();
        }
        accept_done.set_value(e);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12357, DEST_PORT, 0x02, 13000, 0,
        65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    tcp_hdr_info ti;
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse_tcp failed");
    }
    const uint32_t engine_iss = ti.seq;
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12357, DEST_PORT, 0x10, 13001,
        engine_iss + 1, 65535, {}));
    future_get(accept_done.get_future());

    // 挂起读；回调捕获流的唯一强引用，在回调内 reset 并销毁流
    std::promise<void> read_done;
    char buf[64];
    stream->async_read_some(
        net::buffer(buf),
        [stream, &read_done](boost::system::error_code, size_t) mutable {
            stream->reset(); // 重入：close_flow 擦除 flows_
            stream.reset();  // 销毁流：释放 flow_ 的最后引用
            read_done.set_value();
        });
    stream.reset(); // 外部不再持有

    // 客户端发数据：触发 deliver_data 直接路径内联调用读回调
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12357, DEST_PORT, 0x18, 13001,
        engine_iss + 1, 65535,
        std::vector<uint8_t>{'h', 'i'}));
    future_get(read_done.get_future());
    // 引擎不得崩溃：on_packet 的强引用保证回调返回后 f 仍有效
}

BOOST_AUTO_TEST_CASE(test_reject_handshake)
{
    // 应用 reject() 拒绝握手：引擎应回复 RST 并回收流
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.reject();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12358, DEST_PORT, 0x02, 14000, 0,
        65535, {}));
    auto aec = future_get(accept_done.get_future());
    TEST_ASSERT(!aec);
    TEST_ASSERT(!peer.is_open());

    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no RST after reject");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse RST failed");
    }
    TEST_ASSERT((ti.flags & 0x04) != 0); // RST
}

BOOST_AUTO_TEST_CASE(test_syn_retransmit_reack)
{
    // SYN_ACK_SENT 状态下客户端重传 SYN：引擎应重新发送 SYN-ACK
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12359, DEST_PORT, 0x02, 15000, 0,
        65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse SYN-ACK failed");
    }
    TEST_ASSERT((ti.flags & 0x12) == 0x12);
    const uint32_t engine_iss = ti.seq;

    // 客户端重传 SYN：引擎重发 SYN-ACK（相同 iss，ack = irs + 1）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12359, DEST_PORT, 0x02, 15000, 0,
        65535, {}));
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK retransmit");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse SYN-ACK failed");
    }
    TEST_ASSERT((ti.flags & 0x12) == 0x12);
    TEST_ASSERT(ti.seq == engine_iss);
    TEST_ASSERT(ti.ack == 15001);
}

BOOST_AUTO_TEST_CASE(test_implicit_accept_on_first_write)
{
    // 不调用 accept()：首次 async_write_some 隐式批准握手
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(
        peer, [&](boost::system::error_code ec) { accept_done.set_value(ec); });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12360, DEST_PORT, 0x02, 16000, 0,
        65535, {}));
    auto aec = future_get(accept_done.get_future());
    TEST_ASSERT(!aec);

    std::promise<std::pair<boost::system::error_code, size_t>> write_done;
    peer.async_write_some(net::buffer("hello", 5),
        [&](boost::system::error_code ec, size_t n) {
            write_done.set_value({ec, n});
        });

    // 隐式 accept 回复 SYN-ACK，随后数据段立即发出
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK from implicit accept");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse SYN-ACK failed");
    }
    TEST_ASSERT((ti.flags & 0x12) == 0x12);
    const uint32_t engine_iss = ti.seq;

    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no data after implicit accept");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse data failed");
    }
    TEST_ASSERT((ti.flags & 0x18) == 0x18); // PSH|ACK
    TEST_ASSERT(ti.seq == engine_iss + 1);
    TEST_ASSERT(ti.len == 5);
    TEST_ASSERT(std::string(reinterpret_cast<const char *>(ti.data), ti.len) ==
        "hello");

    // 客户端 ACK 完成握手
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12360, DEST_PORT, 0x10, 16001,
        engine_iss + 1, 65535, {}));
    // 客户端 ACK 数据：写操作在数据确认后才完成
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12360, DEST_PORT, 0x10, 16001,
        engine_iss + 6, 65535, {}));
    auto [wec, wn] = future_get(write_done.get_future());
    TEST_ASSERT(!wec && wn == 5);
}

BOOST_AUTO_TEST_CASE(test_implicit_accept_on_first_read)
{
    // 不调用 accept()：首次 async_read_some 隐式批准握手
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(
        peer, [&](boost::system::error_code ec) { accept_done.set_value(ec); });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12361, DEST_PORT, 0x02, 17000, 0,
        65535, {}));
    auto aec = future_get(accept_done.get_future());
    TEST_ASSERT(!aec);

    std::promise<std::pair<boost::system::error_code, size_t>> read_done;
    char buf[64];
    peer.async_read_some(net::buffer(buf),
        [&](boost::system::error_code ec, size_t n) {
            read_done.set_value({ec, n});
        });

    // 隐式 accept 回复 SYN-ACK
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK from implicit accept");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse SYN-ACK failed");
    }
    TEST_ASSERT((ti.flags & 0x12) == 0x12);
    const uint32_t engine_iss = ti.seq;

    // 客户端 ACK 完成握手并发送数据
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12361, DEST_PORT, 0x10, 17001,
        engine_iss + 1, 65535, {}));
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12361, DEST_PORT, 0x18, 17001,
        engine_iss + 1, 65535,
        std::vector<uint8_t>({'h', 'i'})));
    auto [rec, rn] = future_get(read_done.get_future());
    TEST_ASSERT(!rec && rn == 2);
    TEST_ASSERT(std::string(buf, rn) == "hi");
}

BOOST_AUTO_TEST_CASE(test_accepted_no_ack_cleanup)
{
    // SYN 超时设为 1 秒：accept() 后客户端不 ACK，
    // 引擎应在 SYN_ACK_SENT 超时后发送 RST 并回收资源
    engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(1),
        std::chrono::seconds(1));
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12362, DEST_PORT, 0x02, 18000, 0,
        65535, {}));
    auto aec = future_get(accept_done.get_future());
    TEST_ASSERT(!aec);

    // 读掉 SYN-ACK，客户端不再响应
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }

    // 等待引擎 SYN 超时清理（发送 RST）
    if (!env.dev.read_packet(pkt, 5000)) {
        TEST_THROW("no RST after syn timeout");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse RST failed");
    }
    TEST_ASSERT((ti.flags & 0x04) != 0); // RST
    TEST_ASSERT(!peer.is_open());
}

BOOST_AUTO_TEST_CASE(test_accept_idempotent)
{
    // accept() 幂等：重复调用只回复一次 SYN-ACK
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12363, DEST_PORT, 0x02, 19000, 0,
        65535, {}));
    auto aec = future_get(accept_done.get_future());
    TEST_ASSERT(!aec);

    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse SYN-ACK failed");
    }
    TEST_ASSERT((ti.flags & 0x12) == 0x12);

    // 重复 accept：已回复过 SYN-ACK，应忽略
    peer.accept();
    if (env.dev.read_packet(pkt, 300)) {
        TEST_THROW("duplicate SYN-ACK after idempotent accept");
    }
}

BOOST_AUTO_TEST_CASE(test_out_of_order_reassembly)
{
    // 乱序段缓存重排：超前 seq 的段先缓存（回复 Dup-ACK），缺失段到达后
    // 按序交付，乱序段不计入 rx_dropped.
    engine_env env;
    auto &io = env.io;
    tun_tcp_acceptor acceptor(env.engine);
    tun_tcp_socket peer(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(peer, [&](boost::system::error_code ec) {
        if (!ec) {
            peer.accept();
        }
        accept_done.set_value(ec);
    });

    // 客户端 SYN
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x02,
        1000, 0, 65535, {}));
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        TEST_THROW("parse SYN-ACK failed");
    }
    const uint32_t engine_iss = ti.seq;

    // 客户端 ACK -> ESTABLISHED
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x10,
        1001, engine_iss + 1, 65535, {}));
    future_get(accept_done.get_future());

    const auto base_dropped = env.engine.stats().rx_dropped.load();
    const auto base_ooo = env.engine.stats().rx_ooo.load();

    // 先发超前段 "world"（seq=1006）：引擎静默缓存（不发 Dup-ACK，
    // 避免人为乱序触发对端快速重传降窗）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x18,
        1006, engine_iss + 1, 65535,
        {'w', 'o', 'r', 'l', 'd'}));
    if (env.dev.read_packet(pkt, 200)) {
        TEST_THROW("out-of-order segment should be buffered "
        "without dup-ack");
    }

    // 应用层发起读
    std::promise<std::pair<boost::system::error_code, size_t>> read_done;
    std::array<char, 20> buf;
    peer.async_read_some(net::buffer(buf), [&](boost::system::error_code ec,
        size_t n) {
        read_done.set_value({ec, n});
    });

    // 缺失段 "hello"（seq=1001）到达：直投 5 字节，缓存段随后入队
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x18,
        1001, engine_iss + 1, 65535,
        {'h', 'e', 'l', 'l', 'o'}));
    auto [rec, rn] = future_get(read_done.get_future());
    TEST_ASSERT(!rec);
    TEST_ASSERT(rn == 5);
    TEST_ASSERT(std::string(buf.data(), rn) == "hello");

    // 第二次读：缓存段 "world" 按序交付
    std::promise<std::pair<boost::system::error_code, size_t>> read_done2;
    peer.async_read_some(net::buffer(buf), [&](boost::system::error_code ec,
        size_t n) {
        read_done2.set_value({ec, n});
    });
    auto [rec2, rn2] = future_get(read_done2.get_future());
    TEST_ASSERT(!rec2);
    TEST_ASSERT(rn2 == 5);
    TEST_ASSERT(std::string(buf.data(), rn2) == "world");

    // 乱序段被缓存而非丢弃：rx_ooo 增加，rx_dropped 保持不变
    TEST_ASSERT(env.engine.stats().rx_ooo.load() > base_ooo);
    TEST_ASSERT(env.engine.stats().rx_dropped.load() == base_dropped);

    peer.close();
}

BOOST_AUTO_TEST_CASE(test_loopback_local_address_guard)
{
    // 环路与本地地址防护：源为本机虚拟 IP 或源/目标为保留地址的入包
    // 应被丢弃，不建立流、不回复，并计入 rx_dropped
    engine_env env;
    auto &io = env.io;
    (void)io;
    const auto local4 = ip("10.0.0.1");
    const auto local6 = v6("fd00::1");
    const auto v6_loop = v6("::1");
    const auto v6_unspec = v6("::");
    const auto v6_link = v6("fe80::1");

    auto expect_dropped = [&](const std::vector<uint8_t> &seg, size_t &base) {
        env.dev.send(seg);
        std::vector<uint8_t> pkt;
        if (env.dev.read_packet(pkt, 300)) {
            TEST_THROW("guard packet should be dropped");
        }
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(1);
        while (std::chrono::steady_clock::now() < deadline) {
            if (env.engine.stats().rx_dropped.load() > base) {
                base = env.engine.stats().rx_dropped.load();
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        TEST_THROW("rx_dropped not incremented");
    };

    size_t dropped = env.engine.stats().rx_dropped.load();
    // 源为本机虚拟 IP
    expect_dropped(
        make_tcp(local4, DEST_IP, 12364, DEST_PORT, 0x02, 20000, 0, 65535, {}),
        dropped);
    // 源为 127.0.0.0/8
    expect_dropped(make_tcp(0x7f000001, DEST_IP, 12365, DEST_PORT, 0x02, 20000,
        0, 65535, {}),
        dropped);
    // 源为 0.0.0.0/8
    expect_dropped(make_tcp(0x00000001, DEST_IP, 12366, DEST_PORT, 0x02, 20000,
        0, 65535, {}),
        dropped);
    // 目标为 127.0.0.0/8
    expect_dropped(make_tcp(CLIENT_IP, 0x7f000001, 12367, DEST_PORT, 0x02,
        20000, 0, 65535, {}),
        dropped);
    // 目标为 0.0.0.0/8
    expect_dropped(make_tcp(CLIENT_IP, 0x00000001, 12368, DEST_PORT, 0x02,
        20000, 0, 65535, {}),
        dropped);
    // IPv6: 源为本机虚拟 IP
    expect_dropped(
        make_tcp6(local6, DEST_V6, 12369, DEST_PORT, 0x02, 20000, 0, 65535, {}),
        dropped);
    // IPv6: 源 ::1
    expect_dropped(make_tcp6(v6_loop, DEST_V6, 12370, DEST_PORT, 0x02, 20000, 0,
        65535, {}),
        dropped);
    // IPv6: 源 ::
    expect_dropped(make_tcp6(v6_unspec, DEST_V6, 12371, DEST_PORT, 0x02, 20000,
        0, 65535, {}),
        dropped);
    // IPv6: 源 fe80::/10
    expect_dropped(make_tcp6(v6_link, DEST_V6, 12372, DEST_PORT, 0x02, 20000, 0,
        65535, {}),
        dropped);
    // IPv6: 目标 fe80::/10
    expect_dropped(make_tcp6(CLIENT_V6, v6_link, 12373, DEST_PORT, 0x02, 20000,
        0, 65535, {}),
        dropped);
}

// socketpair 注入场景下，关闭读端后引擎仍可能写回（FIN 等），忽略 SIGPIPE
struct sigpipe_guard
{
    sigpipe_guard() { std::signal(SIGPIPE, SIG_IGN); }
};
BOOST_GLOBAL_FIXTURE(sigpipe_guard);
