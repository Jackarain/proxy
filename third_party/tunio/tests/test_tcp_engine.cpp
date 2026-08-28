//
// test_tcp_engine.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

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

static void test_handshake_data_fin()
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
        throw std::runtime_error("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    ip_hdr_info ipi;
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    assert(ipi.src == DEST_IP && ipi.dst == CLIENT_IP && ipi.proto == 6);
    tcp_hdr_info ti;
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
    }
    assert((ti.flags & 0x12) == 0x12); // SYN|ACK
    assert(ti.ack == 1001);
    const uint32_t engine_iss = ti.seq;
    assert(ipi.payload_len >= 24); // MSS 选项

    // 客户端 ACK
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x10,
                          1001, engine_iss + 1, 65535, {}));

    // accept 完成，原始目标地址正确
    auto aec = future_get(accept_done.get_future());
    assert(!aec);
    auto dest = peer.original_destination();
    assert(dest.address().to_v4().to_uint() == DEST_IP);
    assert(dest.port() == DEST_PORT);
    auto rmt = peer.remote_endpoint();
    assert(rmt.address().to_v4().to_uint() == CLIENT_IP);
    assert(rmt.port() == CLIENT_PORT);
    assert(peer.is_open());

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
    assert(!rec && rn == hello.size());
    assert(std::string(buf, rn) == hello);

    // 引擎 ACK 客户端数据
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no ACK");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
    }
    assert((ti.flags & 0x10) != 0 && ti.ack == 1001 + hello.size());

    // 应用写入 "world"，设备收到数据段
    const std::string world = "world";
    std::promise<std::pair<boost::system::error_code, size_t>> write_done;
    peer.async_write_some(net::buffer(world),
                          [&](boost::system::error_code ec, size_t n) {
                              write_done.set_value({ec, n});
                          });
    auto [wec, wn] = future_get(write_done.get_future());
    assert(!wec && wn == world.size());

    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no data packet");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
    }
    assert(ipi.src == DEST_IP && ipi.dst == CLIENT_IP);
    assert(ti.sport == DEST_PORT && ti.dport == CLIENT_PORT);
    assert(ti.seq ==
           engine_iss + 1); // 首个数据段 seq = iss + 1（SYN 消耗一个序号）
    assert(ti.ack == 1001 + hello.size());
    assert((ti.flags & 0x18) == 0x18); // PSH|ACK
    assert(std::string(reinterpret_cast<const char *>(ti.data), ti.len) ==
           world);

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
    assert(!eec && en == 0); // EOF

    // 引擎 ACK FIN
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no FIN ACK");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
    }
    assert(ti.ack == 1001 + hello.size() + 1);

    // 应用关闭 -> 引擎发送 FIN
    peer.close();
    for (int i = 0; i < 100 && peer.is_open(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    assert(!peer.is_open());
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no FIN from engine");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
    }
    assert((ti.flags & 0x01) != 0);
    const uint32_t fin_seq = ti.seq;

    // 客户端 ACK 引擎 FIN
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x10,
                          1001 + hello.size() + 1, fin_seq + 1, 65535, {}));
}

static void test_fin_retransmit_reacked()
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
        throw std::runtime_error("no SYN-ACK");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse SYN-ACK failed");
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
    assert(!eec && en == 0);

    // 引擎 ACK FIN
    if (!env.dev.read_packet(pkt) || !parse_ip(pkt, ipi) ||
        !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("no FIN ACK");
    }
    assert(ti.ack == 3002);

    // 客户端重传 FIN：引擎必须重新 ACK（避免客户端长时间重复重传）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x11,
                          3001, engine_iss + 1, 65535, {}));
    if (!env.dev.read_packet(pkt) || !parse_ip(pkt, ipi) ||
        !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("no FIN re-ACK");
    }
    assert((ti.flags & 0x10) != 0 && ti.ack == 3002);
}

static void test_zero_window_flow_control()
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
        throw std::runtime_error("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    ip_hdr_info ipi;
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    tcp_hdr_info ti;
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
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
        throw std::runtime_error("write should be blocked by zero window");
    }

    // 窗口更新 ACK -> 写入恢复
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12346, DEST_PORT, 0x10, 2001,
                          engine_iss + 1, 4096, {}));
    auto [wec, wn] = future_get(std::move(wf));
    assert(!wec && wn == 1);

    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no data after window update");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
    }
    assert(ti.len == 1 && ti.data[0] == 'x');
    peer.close();
}

static void test_rst()
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
        throw std::runtime_error("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
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
    assert(rec == net::error::connection_reset);
    assert(!peer.is_open());
}

static void test_app_reset()
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
        throw std::runtime_error("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
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
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12348, DEST_PORT, 0x10, 4001,
                          engine_iss + 1, 65535, {}));
    future_get(accept_done.get_future());

    // 应用主动 reset()：后端连接失败等场景
    peer.reset();
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no RST");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
    }
    assert((ti.flags & 0x04) != 0); // RST
    assert(!peer.is_open());
}

static void test_data_with_fin()
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
        throw std::runtime_error("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
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
    assert(!rec && rn == data.size());
    assert(std::string(buf, rn) == tail);

    // 再次读取应得到 EOF（同段 FIN 已被正确处理）
    std::promise<std::pair<boost::system::error_code, size_t>> eof_done;
    peer.async_read_some(net::buffer(buf),
                         [&](boost::system::error_code ec, size_t n) {
                             eof_done.set_value({ec, n});
                         });
    auto [eec, en] = future_get(eof_done.get_future());
    assert(!eec && en == 0);

    // delayed ACK 合并：数据与 FIN 同段到达，引擎以单次 ACK 确认数据并捎带 FIN
    // 确认（ack = 5001 + data.size() + 1）
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no FIN ACK");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
    }
    assert((ti.flags & 0x10) != 0);
    assert(ti.ack == 5001 + data.size() + 1);

    peer.close();
}

static void test_write_after_shutdown_send()
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
        throw std::runtime_error("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
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
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12350, DEST_PORT, 0x10, 6001,
                          engine_iss + 1, 65535, {}));
    future_get(accept_done.get_future());
    // accept_done 在收到 SYN 时即触发，早于三次握手完成：等待引擎处理
    // 客户端 ACK 进入 ESTABLISHED，避免 shutdown 落在 SYN_ACK_SENT 上发 RST。
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // shutdown(send)：发出 FIN
    boost::system::error_code sec;
    peer.shutdown(net::ip::tcp::socket::shutdown_send, sec);
    assert(!sec);

    // 之后写入应被拒绝（fin_sent 已置位）
    std::promise<std::pair<boost::system::error_code, size_t>> write_done;
    peer.async_write_some(net::buffer("x", 1),
                          [&](boost::system::error_code ec, size_t n) {
                              write_done.set_value({ec, n});
                          });
    auto [wec, wn] = future_get(write_done.get_future());
    assert(wec == net::error::bad_descriptor);
    assert(wn == 0);

    // 设备应只收到 FIN，无数据段
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no FIN");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
    }
    assert((ti.flags & 0x01) != 0); // FIN
    assert(ti.len == 0);

    peer.close();
}

static void test_unaccepted_connection_cleanup()
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
        throw std::runtime_error("no RST after syn timeout");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
    }
    assert((ti.flags & 0x04) != 0); // RST
}

static void test_write_queue_limit()
{
    // 发送队列字节数上限：窗口为 0 时排队写满上限后应返回 no_buffer_space
    engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
                   std::chrono::seconds(30), 1024 * 1024, 16);
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
        throw std::runtime_error("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
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
    // 客户端 ACK（窗口 0）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12352, DEST_PORT, 0x10, 8001,
                          engine_iss + 1, 0, {}));
    future_get(accept_done.get_future());

    // 两个 8 字节写排队（合计 16 字节 = 上限）
    std::promise<std::pair<boost::system::error_code, size_t>> w1, w2;
    peer.async_write_some(
        net::buffer("aaaaaaaa", 8),
        [&](boost::system::error_code ec, size_t n) { w1.set_value({ec, n}); });
    peer.async_write_some(
        net::buffer("bbbbbbbb", 8),
        [&](boost::system::error_code ec, size_t n) { w2.set_value({ec, n}); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 第三个写超出上限：立即返回 no_buffer_space
    std::promise<std::pair<boost::system::error_code, size_t>> w3;
    peer.async_write_some(
        net::buffer("c", 1),
        [&](boost::system::error_code ec, size_t n) { w3.set_value({ec, n}); });
    auto [w3ec, w3n] = future_get(w3.get_future());
    assert(w3ec == net::error::no_buffer_space);
    assert(w3n == 0);

    // 窗口更新后，前两个写完成
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12352, DEST_PORT, 0x10, 8001,
                          engine_iss + 1, 4096, {}));
    auto [w1ec, w1n] = future_get(w1.get_future());
    assert(!w1ec && w1n == 8);
    auto [w2ec, w2n] = future_get(w2.get_future());
    assert(!w2ec && w2n == 8);
    peer.close();
}

static void test_close_reopen()
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
        throw std::runtime_error("reopen failed: " + ec.message());
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
        throw std::runtime_error("no SYN-ACK after reopen");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
    }
    dev2.send(make_tcp(CLIENT_IP, DEST_IP, 12353, DEST_PORT, 0x10, 9001,
                       ti.seq + 1, 65535, {}));
    auto aec = future_get(accept_done.get_future());
    assert(!aec);
    assert(peer.is_open());
    peer.close();
}

static void test_fragmented_packet_dropped()
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
        throw std::runtime_error("fragmented SYN should be dropped");
    }
    assert(env.engine.stats().rx_dropped.load() >= 1);
}

static void test_oversized_declared_length()
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
        throw std::runtime_error("no SYN-ACK after oversized junk");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    ip_hdr_info ipi;
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    assert(ipi.proto == 6);
}

static void test_reentrant_reset_in_handler()
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
        throw std::runtime_error("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    ip_hdr_info ipi;
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    tcp_hdr_info ti;
    if (!parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse_tcp failed");
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

static void test_reject_handshake()
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
    assert(!aec);
    assert(!peer.is_open());

    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no RST after reject");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse RST failed");
    }
    assert((ti.flags & 0x04) != 0); // RST
}

static void test_syn_retransmit_reack()
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
        throw std::runtime_error("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse SYN-ACK failed");
    }
    assert((ti.flags & 0x12) == 0x12);
    const uint32_t engine_iss = ti.seq;

    // 客户端重传 SYN：引擎重发 SYN-ACK（相同 iss，ack = irs + 1）
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12359, DEST_PORT, 0x02, 15000, 0,
                          65535, {}));
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no SYN-ACK retransmit");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse SYN-ACK failed");
    }
    assert((ti.flags & 0x12) == 0x12);
    assert(ti.seq == engine_iss);
    assert(ti.ack == 15001);
}

static void test_implicit_accept_on_first_write()
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
    assert(!aec);

    std::promise<std::pair<boost::system::error_code, size_t>> write_done;
    peer.async_write_some(net::buffer("hello", 5),
                          [&](boost::system::error_code ec, size_t n) {
                              write_done.set_value({ec, n});
                          });

    // 隐式 accept 回复 SYN-ACK，随后数据段立即发出
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no SYN-ACK from implicit accept");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse SYN-ACK failed");
    }
    assert((ti.flags & 0x12) == 0x12);
    const uint32_t engine_iss = ti.seq;

    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no data after implicit accept");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse data failed");
    }
    assert((ti.flags & 0x18) == 0x18); // PSH|ACK
    assert(ti.seq == engine_iss + 1);
    assert(ti.len == 5);
    assert(std::string(reinterpret_cast<const char *>(ti.data), ti.len) ==
           "hello");

    // 客户端 ACK 完成握手，写完成
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12360, DEST_PORT, 0x10, 16001,
                          engine_iss + 1, 65535, {}));
    auto [wec, wn] = future_get(write_done.get_future());
    assert(!wec && wn == 5);
}

static void test_implicit_accept_on_first_read()
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
    assert(!aec);

    std::promise<std::pair<boost::system::error_code, size_t>> read_done;
    char buf[64];
    peer.async_read_some(net::buffer(buf),
                         [&](boost::system::error_code ec, size_t n) {
                             read_done.set_value({ec, n});
                         });

    // 隐式 accept 回复 SYN-ACK
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no SYN-ACK from implicit accept");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse SYN-ACK failed");
    }
    assert((ti.flags & 0x12) == 0x12);
    const uint32_t engine_iss = ti.seq;

    // 客户端 ACK 完成握手并发送数据
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12361, DEST_PORT, 0x10, 17001,
                          engine_iss + 1, 65535, {}));
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, 12361, DEST_PORT, 0x18, 17001,
                          engine_iss + 1, 65535,
                          std::vector<uint8_t>({'h', 'i'})));
    auto [rec, rn] = future_get(read_done.get_future());
    assert(!rec && rn == 2);
    assert(std::string(buf, rn) == "hi");
}

static void test_accepted_no_ack_cleanup()
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
    assert(!aec);

    // 读掉 SYN-ACK，客户端不再响应
    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no SYN-ACK");
    }

    // 等待引擎 SYN 超时清理（发送 RST）
    if (!env.dev.read_packet(pkt, 5000)) {
        throw std::runtime_error("no RST after syn timeout");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse RST failed");
    }
    assert((ti.flags & 0x04) != 0); // RST
    assert(!peer.is_open());
}

static void test_accept_idempotent()
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
    assert(!aec);

    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no SYN-ACK");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    ip_hdr_info ipi;
    tcp_hdr_info ti;
    if (!parse_ip(pkt, ipi) || !parse_tcp(ipi.payload, ipi.payload_len, ti)) {
        throw std::runtime_error("parse SYN-ACK failed");
    }
    assert((ti.flags & 0x12) == 0x12);

    // 重复 accept：已回复过 SYN-ACK，应忽略
    peer.accept();
    if (env.dev.read_packet(pkt, 300)) {
        throw std::runtime_error("duplicate SYN-ACK after idempotent accept");
    }
}

static void test_loopback_local_address_guard()
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
            throw std::runtime_error("guard packet should be dropped");
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
        throw std::runtime_error("rx_dropped not incremented");
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

int main()
{
    // socketpair 注入场景下，关闭读端后引擎仍可能写回（FIN 等），忽略 SIGPIPE
    std::signal(SIGPIPE, SIG_IGN);
    test_handshake_data_fin();
    test_data_with_fin();
    test_fin_retransmit_reacked();
    test_zero_window_flow_control();
    test_rst();
    test_app_reset();
    test_write_after_shutdown_send();
    test_unaccepted_connection_cleanup();
    test_write_queue_limit();
    test_close_reopen();
    test_fragmented_packet_dropped();
    test_oversized_declared_length();
    test_reentrant_reset_in_handler();
    test_reject_handshake();
    test_syn_retransmit_reack();
    test_implicit_accept_on_first_write();
    test_implicit_accept_on_first_read();
    test_accepted_no_ack_cleanup();
    test_accept_idempotent();
    test_loopback_local_address_guard();
    return 0;
}
