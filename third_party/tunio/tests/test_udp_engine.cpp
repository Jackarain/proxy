//
// test_udp_engine.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#define BOOST_TEST_MODULE udp_engine
#include <boost/test/included/unit_test.hpp>
#include "test_harness.hpp"
#include "tunio/detail/udp_engine.hpp"

#include <algorithm>
#include <atomic>
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

BOOST_AUTO_TEST_CASE(test_datagram_roundtrip)
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
    TEST_ASSERT(!aec);

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
    TEST_ASSERT(!rec && rn == query.size());
    TEST_ASSERT(std::string(buf, rn) == query);
    TEST_ASSERT(sender == net::ip::udp::endpoint(
        net::ip::make_address_v4("8.8.8.8"), 53));

    // 客户端端点
    auto cli = session.client_endpoint();
    TEST_ASSERT(cli.address().to_v4().to_uint() == CLIENT_IP);
    TEST_ASSERT(cli.port() == 53000);

    // 发送回复数据报
    const std::string reply = "dns-answer";
    std::promise<std::pair<boost::system::error_code, size_t>> send_done;
    session.async_send_to(sender, net::buffer(reply),
        [&](boost::system::error_code ec, size_t n) {
            send_done.set_value({ec, n});
        });
    auto [sec, sn] = future_get(send_done.get_future());
    TEST_ASSERT(!sec && sn == reply.size());

    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no UDP reply packet");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    TEST_ASSERT(ipi.src == DEST_IP && ipi.dst == CLIENT_IP && ipi.proto == 17);
    udp_hdr_info ui;
    if (!parse_udp(ipi.payload, ipi.payload_len, ui)) {
        TEST_THROW("parse_udp failed");
    }
    TEST_ASSERT(ui.sport == 53 && ui.dport == 53000);
    TEST_ASSERT(std::string(reinterpret_cast<const char *>(ui.data), ui.n) == reply);

    session.close();
}

BOOST_AUTO_TEST_CASE(test_receive_truncates_queued_datagram)
{
    engine_env env;
    tun_udp_acceptor acceptor(env.engine);
    tun_udp_socket session(env.io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(session, [&](boost::system::error_code ec) {
        accept_done.set_value(ec);
    });

    env.dev.send(make_udp(CLIENT_IP, DEST_IP, 53210, 53,
        std::vector<uint8_t>{1, 2, 3, 4, 5}));
    TEST_ASSERT(!future_get(accept_done.get_future()));

    char buf[3] = {};
    net::ip::udp::endpoint sender;
    std::promise<std::pair<boost::system::error_code, size_t>> recv_done;
    session.async_receive_from(net::buffer(buf), sender,
        [&](boost::system::error_code ec, size_t n) {
            recv_done.set_value({ec, n});
        });

    auto [ec, n] = future_get(recv_done.get_future());
    TEST_ASSERT(!ec);
    TEST_ASSERT(n == sizeof(buf));
    TEST_ASSERT(buf[0] == 1 && buf[1] == 2 && buf[2] == 3);
    TEST_ASSERT(sender == net::ip::udp::endpoint(
        net::ip::make_address_v4("8.8.8.8"), 53));

    session.close();
}

BOOST_AUTO_TEST_CASE(test_receive_truncates_pending_datagram)
{
    engine_env env;
    tun_udp_acceptor acceptor(env.engine);
    tun_udp_socket session(env.io.get_executor());

    std::promise<boost::system::error_code> accept_done;
    acceptor.async_accept(session, [&](boost::system::error_code ec) {
        accept_done.set_value(ec);
    });
    env.dev.send(make_udp(
        CLIENT_IP, DEST_IP, 53211, 53, std::vector<uint8_t>{0}));
    TEST_ASSERT(!future_get(accept_done.get_future()));

    // 消费会话建立时的首个数据报，确保后续读进入挂起路径。
    char drain[1];
    net::ip::udp::endpoint sender;
    std::promise<std::pair<boost::system::error_code, size_t>> drain_done;
    session.async_receive_from(net::buffer(drain), sender,
        [&](boost::system::error_code ec, size_t n) {
            drain_done.set_value({ec, n});
        });
    auto [drain_ec, drain_n] = future_get(drain_done.get_future());
    TEST_ASSERT(!drain_ec && drain_n == 1);

    char buf[2] = {};
    std::promise<std::pair<boost::system::error_code, size_t>> recv_done;
    session.async_receive_from(net::buffer(buf), sender,
        [&](boost::system::error_code ec, size_t n) {
            recv_done.set_value({ec, n});
        });

    env.dev.send(make_udp(CLIENT_IP, DEST_IP, 53211, 53,
        std::vector<uint8_t>{9, 8, 7, 6}));
    auto [ec, n] = future_get(recv_done.get_future());
    TEST_ASSERT(!ec);
    TEST_ASSERT(n == sizeof(buf));
    TEST_ASSERT(buf[0] == 9 && buf[1] == 8);

    // 零长度接收同样消费一个数据报，并成功返回 0。
    std::promise<std::pair<boost::system::error_code, size_t>> zero_done;
    session.async_receive_from(net::buffer(buf, 0), sender,
        [&](boost::system::error_code zero_ec, size_t zero_n) {
            zero_done.set_value({zero_ec, zero_n});
        });
    env.dev.send(make_udp(
        CLIENT_IP, DEST_IP, 53211, 53, std::vector<uint8_t>{5, 4, 3}));
    auto [zero_ec, zero_n] = future_get(zero_done.get_future());
    TEST_ASSERT(!zero_ec);
    TEST_ASSERT(zero_n == 0);

    // 下一个数据报仍可正常读取，证明零长度读已消费前一个数据报。
    std::promise<std::pair<boost::system::error_code, size_t>> next_done;
    session.async_receive_from(net::buffer(buf, 1), sender,
        [&](boost::system::error_code next_ec, size_t next_n) {
            next_done.set_value({next_ec, next_n});
        });
    env.dev.send(make_udp(
        CLIENT_IP, DEST_IP, 53211, 53, std::vector<uint8_t>{6}));
    auto [next_ec, next_n] = future_get(next_done.get_future());
    TEST_ASSERT(!next_ec && next_n == 1 && buf[0] == 6);

    session.close();
}

BOOST_AUTO_TEST_CASE(test_completion_honors_socket_executor_without_session)
{
    net::io_context io;
    auto strand = net::make_strand(io);
    tun_udp_socket session(strand);

    int completed = 0;
    bool send_on_strand = false;
    bool receive_on_strand = false;
    boost::system::error_code send_ec;
    boost::system::error_code receive_ec;
    char buf[1];
    net::ip::udp::endpoint sender;

    session.async_send_to(net::ip::udp::endpoint(
                              net::ip::make_address_v4("8.8.8.8"), 53),
        net::buffer("x", 1),
        [&](boost::system::error_code ec, size_t)
        {
            ++completed;
            send_ec = ec;
            send_on_strand = strand.running_in_this_thread();
        });
    session.async_receive_from(net::buffer(buf),
        sender,
        [&](boost::system::error_code ec, size_t)
        {
            ++completed;
            receive_ec = ec;
            receive_on_strand = strand.running_in_this_thread();
        });

    TEST_ASSERT(completed == 0);
    io.run();
    TEST_ASSERT(completed == 2);
    TEST_ASSERT(send_on_strand);
    TEST_ASSERT(receive_on_strand);
    TEST_ASSERT(send_ec == net::error::bad_descriptor);
    TEST_ASSERT(receive_ec == net::error::bad_descriptor);
}

BOOST_AUTO_TEST_CASE(test_immediate_completion_is_not_inline)
{
    net::io_context io;
    auto strand = net::make_strand(io);
    tun_udp_socket session(strand);
    bool send_called = false;
    bool receive_called = false;
    char buf[1];
    net::ip::udp::endpoint sender;

    net::dispatch(strand,
        [&]
        {
            session.async_send_to(net::ip::udp::endpoint(
                                      net::ip::make_address_v4("8.8.8.8"), 53),
                net::buffer("x", 1),
                [&](boost::system::error_code, size_t)
                {
                    send_called = true;
                });
            TEST_ASSERT(!send_called);

            session.async_receive_from(net::buffer(buf),
                sender,
                [&](boost::system::error_code, size_t)
                {
                    receive_called = true;
                });
            TEST_ASSERT(!receive_called);
        });

    io.run();
    TEST_ASSERT(send_called);
    TEST_ASSERT(receive_called);
}

BOOST_AUTO_TEST_CASE(test_accept_completion_honors_associated_executor)
{
    engine_env env;
    tun_udp_acceptor acceptor(env.engine);
    auto peer_strand = net::make_strand(env.io);
    tun_udp_socket session(peer_strand);

    net::io_context target_io;
    auto target_strand = net::make_strand(target_io);
    auto target_guard = net::make_work_guard(target_io);
    std::atomic<bool> called{false};
    bool on_target = false;
    boost::system::error_code result;

    acceptor.async_accept(session,
        net::bind_executor(target_strand,
            [&](boost::system::error_code ec) {
                result = ec;
                on_target = target_strand.running_in_this_thread();
                called.store(true, std::memory_order_release);
            }));
    env.dev.send(make_udp(
        CLIENT_IP, DEST_IP, 53200, 53, std::vector<uint8_t>{1, 2, 3}));

    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::seconds(3);
    while (!called.load(std::memory_order_acquire) &&
        std::chrono::steady_clock::now() < deadline) {
        target_io.run_for(std::chrono::milliseconds(10));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    TEST_ASSERT(called.load(std::memory_order_acquire));
    TEST_ASSERT(on_target);
    TEST_ASSERT(!result);
    TEST_ASSERT(session.is_open());
    target_guard.reset();
    session.close();
}

BOOST_AUTO_TEST_CASE(test_accept_after_close_completes)
{
    engine_env env;
    tun_udp_acceptor acceptor(env.engine);
    tun_udp_socket session(env.io.get_executor());
    std::promise<boost::system::error_code> done;

    env.engine.close();
    acceptor.async_accept(session, [&](boost::system::error_code ec) {
        done.set_value(ec);
    });

    TEST_ASSERT(future_get(done.get_future(), 1000) ==
        net::error::bad_descriptor);
}

BOOST_AUTO_TEST_CASE(test_multiple_remotes)
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
    TEST_ASSERT(!aec);

    // 两份数据报的 sender 各自为对应远端
    std::promise<std::pair<boost::system::error_code, size_t>> r1;
    char buf[512];
    net::ip::udp::endpoint s1;
    session.async_receive_from(
        net::buffer(buf), s1, [&](boost::system::error_code ec, size_t n) {
            r1.set_value({ec, n});
        });
    auto [ec1, n1] = future_get(r1.get_future());
    TEST_ASSERT(!ec1 && n1 == q1.size());
    TEST_ASSERT(std::string(buf, n1) == q1);
    TEST_ASSERT(s1 == net::ip::udp::endpoint(
        net::ip::make_address_v4("8.8.8.8"), 53));

    std::promise<std::pair<boost::system::error_code, size_t>> r2;
    net::ip::udp::endpoint s2;
    session.async_receive_from(
        net::buffer(buf), s2, [&](boost::system::error_code ec, size_t n) {
            r2.set_value({ec, n});
        });
    auto [ec2, n2] = future_get(r2.get_future());
    TEST_ASSERT(!ec2 && n2 == q2.size());
    TEST_ASSERT(std::string(buf, n2) == q2);
    TEST_ASSERT(s2 == net::ip::udp::endpoint(
        net::ip::make_address_v4("1.1.1.1"), 80));

    // 两路 send_to 各自构造正确的响应报文
    std::promise<std::pair<boost::system::error_code, size_t>> d1;
    session.async_send_to(s1, net::buffer(q1),
        [&](boost::system::error_code ec, size_t n) {
            d1.set_value({ec, n});
        });
    auto [e1, sn1] = future_get(d1.get_future());
    TEST_ASSERT(!e1 && sn1 == q1.size());
    std::vector<uint8_t> pkt1;
    if (!env.dev.read_packet(pkt1)) {
        TEST_THROW("no first udp reply");
    }
    ip_hdr_info i1;
    if (!parse_ip(pkt1, i1)) {
        TEST_THROW("parse first reply failed");
    }
    TEST_ASSERT(i1.src == DEST_IP && i1.dst == CLIENT_IP);
    udp_hdr_info u1;
    if (!parse_udp(i1.payload, i1.payload_len, u1)) {
        TEST_THROW("parse first udp failed");
    }
    TEST_ASSERT(u1.sport == 53 && u1.dport == 53100);

    std::promise<std::pair<boost::system::error_code, size_t>> d2;
    session.async_send_to(s2, net::buffer(q2),
        [&](boost::system::error_code ec, size_t n) {
            d2.set_value({ec, n});
        });
    auto [e2, sn2] = future_get(d2.get_future());
    TEST_ASSERT(!e2 && sn2 == q2.size());
    std::vector<uint8_t> pkt2;
    if (!env.dev.read_packet(pkt2)) {
        TEST_THROW("no second udp reply");
    }
    ip_hdr_info i2;
    if (!parse_ip(pkt2, i2)) {
        TEST_THROW("parse second reply failed");
    }
    TEST_ASSERT(i2.src == DEST_IP2 && i2.dst == CLIENT_IP);
    udp_hdr_info u2;
    if (!parse_udp(i2.payload, i2.payload_len, u2)) {
        TEST_THROW("parse second udp failed");
    }
    TEST_ASSERT(u2.sport == 80 && u2.dport == 53100);

    session.close();
}

BOOST_AUTO_TEST_CASE(test_session_timeout)
{
    // 短空闲超时（1s）
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
    TEST_ASSERT(!fec && fn == 3);

    // 挂起第二个读取，等待空闲超时唤醒
    std::promise<std::pair<boost::system::error_code, size_t>> timeout_read;
    session.async_receive_from(
        net::buffer(buf), sender,
        [&](boost::system::error_code ec, size_t n) {
            timeout_read.set_value({ec, n});
        });
    auto [tec, tn] = future_get(timeout_read.get_future(), 5000);
    TEST_ASSERT(tec == net::error::operation_aborted);
    TEST_ASSERT(!session.is_open());
}

namespace {

struct udp_engine_fixture
{
    net::io_context io;
    std::shared_ptr<engine_stats> stats = std::make_shared<engine_stats>();
    std::shared_ptr<tunio::tun_device> dev;
    std::shared_ptr<tunio::detail::tun_queue_writer> writer;
    std::shared_ptr<tunio::detail::buffer_accountant> account;
    tun_config cfg;
    std::shared_ptr<tunio::detail::udp_engine> engine;

    explicit udp_engine_fixture(std::chrono::seconds timeout)
    {
        cfg.udp_idle_timeout = timeout;
        cfg.max_total_buffer = 8 * 1024 * 1024;
        dev = std::make_shared<tunio::tun_device>(io);
        writer = std::make_shared<tunio::detail::tun_queue_writer>(
            io.get_executor(), dev, stats);
        account = std::make_shared<tunio::detail::buffer_accountant>();
        account->limit = cfg.max_total_buffer;
        engine = std::make_shared<tunio::detail::udp_engine>(
            io.get_executor(), writer, cfg, *stats, account);
    }

    ~udp_engine_fixture()
    {
        engine->close_all();
    }
};

void feed_udp(tunio::detail::udp_engine& engine, uint16_t sport)
{
    const auto pkt = make_udp(CLIENT_IP, DEST_IP, sport, 53, {1});
    tunio::detail::ip_packet_info ipi;
    ipi.family = 4;
    ipi.protocol = tunio::detail::IPPROTO_UDP_V;
    const auto src = net::ip::address_v4(CLIENT_IP).to_bytes();
    const auto dst = net::ip::address_v4(DEST_IP).to_bytes();
    std::copy(src.begin(), src.end(), ipi.src_ip);
    std::copy(dst.begin(), dst.end(), ipi.dst_ip);
    engine.on_packet(ipi, pkt.data() + 20, pkt.size() - 20);
}

} // namespace

BOOST_AUTO_TEST_CASE(test_expired_pending_session_released)
{
    udp_engine_fixture f(std::chrono::seconds(1));
    feed_udp(*f.engine, 53110);
    TEST_ASSERT(f.engine->session_count() == 1);
    TEST_ASSERT(f.engine->pending_session_count() == 1);

    f.io.run_for(std::chrono::milliseconds(1500));

    TEST_ASSERT(f.engine->session_count() == 0);
    TEST_ASSERT(f.engine->pending_session_count() == 0);
}

BOOST_AUTO_TEST_CASE(test_pending_session_limit)
{
    udp_engine_fixture f(std::chrono::seconds(30));
    constexpr size_t extra = 32;
    for (size_t i = 0;
        i < tunio::detail::udp_engine::max_pending_new_sessions + extra;
        ++i)
    {
        feed_udp(*f.engine, static_cast<uint16_t>(20000 + i));
    }

    TEST_ASSERT(f.engine->session_count() ==
        tunio::detail::udp_engine::max_pending_new_sessions);
    TEST_ASSERT(f.engine->pending_session_count() ==
        tunio::detail::udp_engine::max_pending_new_sessions);
    TEST_ASSERT(f.stats->rx_dropped.load(std::memory_order_relaxed) == extra);
}

BOOST_AUTO_TEST_CASE(test_session_recreated_after_expiry)
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

BOOST_AUTO_TEST_CASE(test_large_datagram_fragmented)
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
    TEST_ASSERT(!aec);

    char buf[512];
    net::ip::udp::endpoint sender;
    std::promise<std::pair<boost::system::error_code, size_t>> recv_done;
    session.async_receive_from(
        net::buffer(buf), sender,
        [&](boost::system::error_code ec, size_t n) {
            recv_done.set_value({ec, n});
        });
    auto [rec, rn] = future_get(recv_done.get_future());
    TEST_ASSERT(!rec && rn == 1);

    // 发送 2000 字节大响应：MTU 1500 下应拆为 2 个 IP 分片
    std::string big(2000, 'x');
    std::promise<std::pair<boost::system::error_code, size_t>> send_done;
    session.async_send_to(sender, net::buffer(big),
        [&](boost::system::error_code ec, size_t n) {
            send_done.set_value({ec, n});
        });
    auto [sec, sn] = future_get(send_done.get_future());
    TEST_ASSERT(!sec && sn == big.size());

    std::vector<uint8_t> f1, f2;
    if (!env.dev.read_packet(f1)) {
        TEST_THROW("no first fragment");
    }
    if (!env.dev.read_packet(f2)) {
        TEST_THROW("no second fragment");
    }

    ip_hdr_info i1, i2;
    if (!parse_ip(f1, i1) || !parse_ip(f2, i2)) {
        TEST_THROW("parse fragment failed");
    }
    TEST_ASSERT(i1.proto == 17 && i2.proto == 17);
    TEST_ASSERT(i1.src == DEST_IP && i2.src == DEST_IP);
    TEST_ASSERT(i1.dst == CLIENT_IP && i2.dst == CLIENT_IP);

    const uint16_t id1 = static_cast<uint16_t>((f1[4] << 8) | f1[5]);
    const uint16_t id2 = static_cast<uint16_t>((f2[4] << 8) | f2[5]);
    TEST_ASSERT(id1 == id2); // 分片共享同一 IP Identification

    const uint16_t fo1 = static_cast<uint16_t>((f1[6] << 8) | f1[7]);
    const uint16_t fo2 = static_cast<uint16_t>((f2[6] << 8) | f2[7]);
    TEST_ASSERT((fo1 & 0x2000) != 0 && (fo1 & 0x1fff) == 0);     // 首片带 MF
    TEST_ASSERT((fo2 & 0x2000) == 0 && (fo2 & 0x1fff) == 185);   // 末片偏移 1480/8
    TEST_ASSERT(i1.total_len == 1500 && i2.total_len == 548);

    // UDP 头与校验和位于首片，覆盖完整数据报
    const uint8_t *udp = i1.payload;
    const size_t ulen = static_cast<size_t>((udp[4] << 8) | udp[5]);
    TEST_ASSERT(ulen == 8 + big.size());

    std::vector<uint8_t> full(ulen);
    std::memcpy(full.data(), udp, 8);
    std::memcpy(full.data() + 8, udp + 8, 1472);
    std::memcpy(full.data() + 8 + 1472, i2.payload, i2.payload_len);
    const uint32_t pseudo = (i1.src >> 16) + (i1.src & 0xffff) +
        (i1.dst >> 16) + (i1.dst & 0xffff) + 17 + ulen;
    TEST_ASSERT(csum16(full.data(), ulen, pseudo) == 0);
    TEST_ASSERT(std::string(reinterpret_cast<const char *>(full.data()) + 8,
        ulen - 8) == big);
}

// 分片发送的设备写失败必须上报调用方：每片失败都计入 tx_dropped，完成
// 回调带首个错误（此前"入队即成功"会把设备写失败误报为成功）
BOOST_AUTO_TEST_CASE(test_fragmented_write_failure_reported)
{
    net::io_context io;
    net::any_io_executor strand = net::make_strand(io);
    // 未 assign 队列 fd：设备写立即以 bad_descriptor 失败
    auto dev = std::make_shared<tunio::tun_device>(io);
    auto stats = std::make_shared<tunio::engine_stats>();
    auto writer = std::make_shared<tunio::detail::tun_queue_writer>(
        strand, dev, stats);

    const uint8_t src[4] = {10, 0, 0, 1};
    const uint8_t dst[4] = {10, 0, 0, 2};
    // 4000 字节数据报在 MTU 1500 下切为 3 片（1480 + 1480 + 1040）
    std::vector<uint8_t> datagram(4000, 0x77);

    bool called = false;
    boost::system::error_code result;
    tunio::detail::udp_write_ipv4_fragments(*writer, 1500, src, dst,
        datagram,
        [&](boost::system::error_code ec) {
            called = true;
            result = ec;
        });
    io.run();

    TEST_ASSERT(called);
    TEST_ASSERT(result == net::error::bad_descriptor);
    TEST_ASSERT(stats->tx_packets.load() == 0);
    TEST_ASSERT(stats->tx_dropped.load() == 3);
}

// 退化 MTU（无法拆分）：以 message_size 完成且不写入任何分片
BOOST_AUTO_TEST_CASE(test_fragmented_degenerate_mtu_reports_message_size)
{
    net::io_context io;
    net::any_io_executor strand = net::make_strand(io);
    auto dev = std::make_shared<tunio::tun_device>(io);
    auto stats = std::make_shared<tunio::engine_stats>();
    auto writer = std::make_shared<tunio::detail::tun_queue_writer>(
        strand, dev, stats);

    const uint8_t src[4] = {10, 0, 0, 1};
    const uint8_t dst[4] = {10, 0, 0, 2};
    std::vector<uint8_t> datagram(64, 0x11);

    bool called = false;
    boost::system::error_code result;
    // MTU 20 = IP 头长度：第二片可用载荷为 0，无法继续拆分
    tunio::detail::udp_write_ipv4_fragments(*writer, 20, src, dst, datagram,
        [&](boost::system::error_code ec) {
            called = true;
            result = ec;
        });

    TEST_ASSERT(called);
    TEST_ASSERT(result == net::error::message_size);
    TEST_ASSERT(stats->tx_packets.load() == 0);
    TEST_ASSERT(stats->tx_dropped.load() == 0);
}
