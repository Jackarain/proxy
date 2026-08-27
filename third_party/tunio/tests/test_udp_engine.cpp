//
// test_udp_engine.cpp
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
#include <cstdint>
#include <cstring>
#include <future>
#include <string>
#include <thread>
#include <vector>

using namespace test;

namespace {
namespace net = boost::asio;
constexpr uint32_t CLIENT_IP = 0x0a000002; // 10.0.0.2
constexpr uint32_t DEST_IP = 0x08080808;   // 8.8.8.8
constexpr uint32_t DEST_IP2 = 0x01010101;  // 1.1.1.1
} // namespace

static void test_datagram_roundtrip()
{
    engine_env env;
    auto &io = env.io;
    tun_udp_acceptor acceptor(env.engine);
    tun_udp_socket session(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(session, [&](boost::system::error_code ec) {
        accept_done.set_value(ec);
    });

    const std::string query = "dns-query-bytes";
    env.dev.send(make_udp(CLIENT_IP, DEST_IP, 53000, 53,
                          std::vector<uint8_t>(query.begin(), query.end())));

    // 新会话通知
    auto aec = future_get(accept_done.get_future());
    assert(!aec);

    // 接收完整数据报
    std::promise<std::pair<boost::system::error_code, size_t>> recv_done;
    char buf[512];
    net::ip::udp::endpoint sender;
    session.async_receive_from(
        net::buffer(buf), sender,
        [&](boost::system::error_code ec, size_t n) {
            recv_done.set_value({ec, n});
        });
    auto [rec, rn] = future_get(recv_done.get_future());
    assert(!rec && rn == query.size());
    assert(std::string(buf, rn) == query);
    assert(sender == net::ip::udp::endpoint(
                         net::ip::make_address_v4("8.8.8.8"), 53));

    // 客户端端点
    auto cli = session.client_endpoint();
    assert(cli.address().to_v4().to_uint() == CLIENT_IP);
    assert(cli.port() == 53000);

    // 发送回复数据报
    const std::string reply = "dns-answer";
    std::promise<std::pair<boost::system::error_code, size_t>> send_done;
    session.async_send_to(sender, net::buffer(reply),
                          [&](boost::system::error_code ec, size_t n) {
                              send_done.set_value({ec, n});
                          });
    auto [sec, sn] = future_get(send_done.get_future());
    assert(!sec && sn == reply.size());

    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        throw std::runtime_error("no UDP reply packet");
    }
    if (!verify_packet(pkt)) {
        throw std::runtime_error("verify_packet failed");
    }
    ip_hdr_info ipi;
    if (!parse_ip(pkt, ipi)) {
        throw std::runtime_error("parse_ip failed");
    }
    assert(ipi.src == DEST_IP && ipi.dst == CLIENT_IP && ipi.proto == 17);
    udp_hdr_info ui;
    if (!parse_udp(ipi.payload, ipi.payload_len, ui)) {
        throw std::runtime_error("parse_udp failed");
    }
    assert(ui.sport == 53 && ui.dport == 53000);
    assert(std::string(reinterpret_cast<const char *>(ui.data), ui.n) == reply);

    session.close();
}

static void test_multiple_remotes()
{
    engine_env env;
    auto &io = env.io;
    tun_udp_acceptor acceptor(env.engine);
    tun_udp_socket session(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(session, [&](boost::system::error_code ec) {
        accept_done.set_value(ec);
    });

    // 同一客户端端口发往两个不同远端：仅创建一次会话（1 对 N）
    const std::string q1 = "query-to-8.8.8.8";
    env.dev.send(make_udp(CLIENT_IP, DEST_IP, 53100, 53,
                          std::vector<uint8_t>(q1.begin(), q1.end())));
    const std::string q2 = "query-to-1.1.1.1";
    env.dev.send(make_udp(CLIENT_IP, DEST_IP2, 53100, 80,
                          std::vector<uint8_t>(q2.begin(), q2.end())));
    auto aec = future_get(accept_done.get_future());
    assert(!aec);

    // 两份数据报的 sender 各自为对应远端
    std::promise<std::pair<boost::system::error_code, size_t>> r1;
    char buf[512];
    net::ip::udp::endpoint s1;
    session.async_receive_from(
        net::buffer(buf), s1, [&](boost::system::error_code ec, size_t n) {
            r1.set_value({ec, n});
        });
    auto [ec1, n1] = future_get(r1.get_future());
    assert(!ec1 && n1 == q1.size());
    assert(std::string(buf, n1) == q1);
    assert(s1 == net::ip::udp::endpoint(
                    net::ip::make_address_v4("8.8.8.8"), 53));

    std::promise<std::pair<boost::system::error_code, size_t>> r2;
    net::ip::udp::endpoint s2;
    session.async_receive_from(
        net::buffer(buf), s2, [&](boost::system::error_code ec, size_t n) {
            r2.set_value({ec, n});
        });
    auto [ec2, n2] = future_get(r2.get_future());
    assert(!ec2 && n2 == q2.size());
    assert(std::string(buf, n2) == q2);
    assert(s2 == net::ip::udp::endpoint(
                    net::ip::make_address_v4("1.1.1.1"), 80));

    // 两路 send_to 各自构造正确的响应报文
    std::promise<std::pair<boost::system::error_code, size_t>> d1;
    session.async_send_to(s1, net::buffer(q1),
                          [&](boost::system::error_code ec, size_t n) {
                              d1.set_value({ec, n});
                          });
    auto [e1, sn1] = future_get(d1.get_future());
    assert(!e1 && sn1 == q1.size());
    std::vector<uint8_t> pkt1;
    if (!env.dev.read_packet(pkt1)) {
        throw std::runtime_error("no first udp reply");
    }
    ip_hdr_info i1;
    if (!parse_ip(pkt1, i1)) {
        throw std::runtime_error("parse first reply failed");
    }
    assert(i1.src == DEST_IP && i1.dst == CLIENT_IP);
    udp_hdr_info u1;
    if (!parse_udp(i1.payload, i1.payload_len, u1)) {
        throw std::runtime_error("parse first udp failed");
    }
    assert(u1.sport == 53 && u1.dport == 53100);

    std::promise<std::pair<boost::system::error_code, size_t>> d2;
    session.async_send_to(s2, net::buffer(q2),
                          [&](boost::system::error_code ec, size_t n) {
                              d2.set_value({ec, n});
                          });
    auto [e2, sn2] = future_get(d2.get_future());
    assert(!e2 && sn2 == q2.size());
    std::vector<uint8_t> pkt2;
    if (!env.dev.read_packet(pkt2)) {
        throw std::runtime_error("no second udp reply");
    }
    ip_hdr_info i2;
    if (!parse_ip(pkt2, i2)) {
        throw std::runtime_error("parse second reply failed");
    }
    assert(i2.src == DEST_IP2 && i2.dst == CLIENT_IP);
    udp_hdr_info u2;
    if (!parse_udp(i2.payload, i2.payload_len, u2)) {
        throw std::runtime_error("parse second udp failed");
    }
    assert(u2.sport == 80 && u2.dport == 53100);

    session.close();
}

static void test_session_timeout()
{
    // 短空闲超时（500ms）
    engine_env env(1500, std::chrono::seconds(1));
    auto &io = env.io;
    tun_udp_acceptor acceptor(env.engine);
    tun_udp_socket session(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(session, [&](boost::system::error_code ec) {
        accept_done.set_value(ec);
    });

    env.dev.send(make_udp(CLIENT_IP, DEST_IP, 53001, 53, {1, 2, 3}));
    future_get(accept_done.get_future());

    // 消费首个数据报
    std::promise<std::pair<boost::system::error_code, size_t>> first_read;
    char buf[512];
    net::ip::udp::endpoint sender;
    session.async_receive_from(
        net::buffer(buf), sender,
        [&](boost::system::error_code ec, size_t n) {
            first_read.set_value({ec, n});
        });
    auto [fec, fn] = future_get(first_read.get_future());
    assert(!fec && fn == 3);

    // 挂起第二个读取，等待空闲超时唤醒
    std::promise<std::pair<boost::system::error_code, size_t>> timeout_read;
    session.async_receive_from(
        net::buffer(buf), sender,
        [&](boost::system::error_code ec, size_t n) {
            timeout_read.set_value({ec, n});
        });
    auto [tec, tn] = future_get(timeout_read.get_future(), 5000);
    assert(tec == net::error::operation_aborted);
    assert(!session.is_open());
}

static void test_session_recreated_after_expiry()
{
    engine_env env(1500, std::chrono::seconds(1));
    auto &io = env.io;
    tun_udp_acceptor acceptor(env.engine);

    // 第一个会话
    {
        tun_udp_socket s1(io.get_executor());
        std::promise<boost::system::error_code> a1;
        acceptor.async_accept(
            s1, [&](boost::system::error_code ec) { a1.set_value(ec); });
        env.dev.send(make_udp(CLIENT_IP, DEST_IP, 53002, 53, {9}));
        future_get(a1.get_future());
        s1.close();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2200));

    // 相同五元组的新流量应创建全新会话
    tun_udp_socket s2(io.get_executor());
    std::promise<boost::system::error_code> a2;
    acceptor.async_accept(
        s2, [&](boost::system::error_code ec) { a2.set_value(ec); });
    env.dev.send(make_udp(CLIENT_IP, DEST_IP, 53002, 53, {7, 7}));
    future_get(a2.get_future());
    s2.close();
}

static void test_large_datagram_fragmented()
{
    engine_env env(1500);
    auto &io = env.io;
    tun_udp_acceptor acceptor(env.engine);
    tun_udp_socket session(io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(session, [&](boost::system::error_code ec) {
        accept_done.set_value(ec);
    });

    env.dev.send(make_udp(CLIENT_IP, DEST_IP, 53000, 53,
                          std::vector<uint8_t>{'q'}));
    auto aec = future_get(accept_done.get_future());
    assert(!aec);

    char buf[512];
    net::ip::udp::endpoint sender;
    std::promise<std::pair<boost::system::error_code, size_t>> recv_done;
    session.async_receive_from(
        net::buffer(buf), sender,
        [&](boost::system::error_code ec, size_t n) {
            recv_done.set_value({ec, n});
        });
    auto [rec, rn] = future_get(recv_done.get_future());
    assert(!rec && rn == 1);

    // 发送 2000 字节大响应：MTU 1500 下应拆为 2 个 IP 分片
    std::string big(2000, 'x');
    std::promise<std::pair<boost::system::error_code, size_t>> send_done;
    session.async_send_to(sender, net::buffer(big),
                          [&](boost::system::error_code ec, size_t n) {
                              send_done.set_value({ec, n});
                          });
    auto [sec, sn] = future_get(send_done.get_future());
    assert(!sec && sn == big.size());

    std::vector<uint8_t> f1, f2;
    if (!env.dev.read_packet(f1)) {
        throw std::runtime_error("no first fragment");
    }
    if (!env.dev.read_packet(f2)) {
        throw std::runtime_error("no second fragment");
    }

    ip_hdr_info i1, i2;
    if (!parse_ip(f1, i1) || !parse_ip(f2, i2)) {
        throw std::runtime_error("parse fragment failed");
    }
    assert(i1.proto == 17 && i2.proto == 17);
    assert(i1.src == DEST_IP && i2.src == DEST_IP);
    assert(i1.dst == CLIENT_IP && i2.dst == CLIENT_IP);

    const uint16_t id1 = static_cast<uint16_t>((f1[4] << 8) | f1[5]);
    const uint16_t id2 = static_cast<uint16_t>((f2[4] << 8) | f2[5]);
    assert(id1 == id2); // 分片共享同一 IP Identification

    const uint16_t fo1 = static_cast<uint16_t>((f1[6] << 8) | f1[7]);
    const uint16_t fo2 = static_cast<uint16_t>((f2[6] << 8) | f2[7]);
    assert((fo1 & 0x2000) != 0 && (fo1 & 0x1fff) == 0);     // 首片带 MF
    assert((fo2 & 0x2000) == 0 && (fo2 & 0x1fff) == 185);   // 末片偏移 1480/8
    assert(i1.total_len == 1500 && i2.total_len == 548);

    // UDP 头与校验和位于首片，覆盖完整数据报
    const uint8_t *udp = i1.payload;
    const size_t ulen = static_cast<size_t>((udp[4] << 8) | udp[5]);
    assert(ulen == 8 + big.size());

    std::vector<uint8_t> full(ulen);
    std::memcpy(full.data(), udp, 8);
    std::memcpy(full.data() + 8, udp + 8, 1472);
    std::memcpy(full.data() + 8 + 1472, i2.payload, i2.payload_len);
    const uint32_t pseudo = (i1.src >> 16) + (i1.src & 0xffff) +
                            (i1.dst >> 16) + (i1.dst & 0xffff) + 17 + ulen;
    assert(csum16(full.data(), ulen, pseudo) == 0);
    assert(std::string(reinterpret_cast<const char *>(full.data()) + 8,
                       ulen - 8) == big);
}

int main(int argc, char **argv)
{
    if (argc > 1 && std::string(argv[1]) == "roundtrip") {
        test_datagram_roundtrip();
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "multi") {
        test_multiple_remotes();
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "timeout") {
        test_session_timeout();
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "recreate") {
        test_session_recreated_after_expiry();
        return 0;
    }

    test_multiple_remotes();
    test_large_datagram_fragmented();
    test_session_timeout();
    test_session_recreated_after_expiry();
    return 0;
}
