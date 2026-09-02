//
// test_multi_queue.cpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Linux TUN 多队列（IFF_MULTI_QUEUE）支持测试：
//   1. pick_tx_queue：写队列选择的确定性 / 同流同队列 / 多流分布；
//   2. 引擎级多队列：多句柄注入（多 socketpair 模拟多队列 fd），验证
//      读侧各队列并发处理、写侧按哈希分发后仍能读回；
//   3. 真实 TUN 设备环回（仅 Linux：需 root + /dev/net/tun，不可用时
//      跳过）：自主打开 num_queues 队列，经独立注入 fd 发送 ICMP Echo，
//      验证内核回包经多队列读回.

#define BOOST_TEST_MODULE multi_queue
#include <boost/test/included/unit_test.hpp>
#include "test_harness.hpp"
#include "tun_queue_writer.hpp"

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <future>
#include <iostream>
#include <set>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/if.h>
#include <linux/if_tun.h>
#endif

using namespace test;

namespace {

// 把报文拷入 packet_buffer（headroom 与引擎一致）
tunio::packet_buffer to_buffer(const std::vector<uint8_t> &pkt)
{
    tunio::packet_buffer buf(pkt.size() + 64, 64);
    std::memcpy(buf.writable_data(), pkt.data(), pkt.size());
    buf.resize(pkt.size());
    return buf;
}

// 1. 写队列选择：确定性 / 同流同队列 / 多流分布 / 非法报文回退
BOOST_AUTO_TEST_CASE(test_pick_tx_queue)
{
    // 单队列（及 0 队列数）短路
    auto b0 = to_buffer(make_tcp(ip("10.0.0.2"), ip("8.8.8.8"), 1000, 80,
                                 0x10, 1, 1, 65535, {}));
    TEST_ASSERT(tunio::detail::pick_tx_queue(b0, 1) == 0);
    TEST_ASSERT(tunio::detail::pick_tx_queue(b0, 0) == 0);

    // 确定性 + 同流同队列（不同负载/序号仍同队列，保持流内顺序）
    auto b1 = to_buffer(make_tcp(ip("10.0.0.2"), ip("8.8.8.8"), 1000, 80,
                                 0x18, 1, 1, 65535,
                                 std::vector<uint8_t>(100, 0xaa)));
    auto b2 = to_buffer(make_tcp(ip("10.0.0.2"), ip("8.8.8.8"), 1000, 80,
                                 0x18, 101, 1, 65535,
                                 std::vector<uint8_t>(50, 0xbb)));
    const size_t q1 = tunio::detail::pick_tx_queue(b1, 4);
    const size_t q2 = tunio::detail::pick_tx_queue(b2, 4);
    TEST_ASSERT(q1 < 4 && q2 < 4);
    TEST_ASSERT(q1 == q2);

    // IPv4 多流分布：64 个不同源端口至少覆盖 2 个队列
    std::set<size_t> q4;
    for (uint16_t sport = 1000; sport < 1064; ++sport) {
        q4.insert(tunio::detail::pick_tx_queue(
            to_buffer(make_udp(ip("10.0.0.2"), ip("8.8.8.8"), sport, 53,
                               {0x01})),
            4));
    }
    TEST_ASSERT(q4.size() >= 2);

    // IPv6 同样分布
    std::set<size_t> q6;
    for (uint16_t sport = 2000; sport < 2064; ++sport) {
        q6.insert(tunio::detail::pick_tx_queue(
            to_buffer(make_udp6(v6("fd00::2"), v6("2001:db8::1"), sport, 53,
                                {0x02})),
            4));
    }
    TEST_ASSERT(q6.size() >= 2);

    // 非 IP / 残缺报文回退队列 0
    tunio::packet_buffer junk(64, 16);
    std::memcpy(junk.writable_data(), "not-an-ip", 9);
    junk.resize(9);
    TEST_ASSERT(tunio::detail::pick_tx_queue(junk, 4) == 0);

    // 残缺 IPv6 头（< 40 字节固定头）：回退队列 0，不得越界读地址区
    tunio::packet_buffer short6(64, 16);
    short6.writable_data()[0] = 0x60; // version 6
    short6.resize(20);
    TEST_ASSERT(tunio::detail::pick_tx_queue(short6, 4) == 0);
}

// 2. 引擎级多队列：多 socketpair 注入，读侧各队列并发、写侧哈希分发
BOOST_AUTO_TEST_CASE(test_engine_multi_queue)
{
    constexpr size_t k_queues = 4;
    engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
                   std::chrono::seconds(30), 1024 * 1024,
                   std::chrono::milliseconds(5000),
                   std::chrono::milliseconds(200), 8, k_queues);
    TEST_ASSERT(env.engine.queue_count() == k_queues);
    TEST_ASSERT(env.dev.queue_count() == k_queues);

    // 向每个队列注入 ICMP Echo（目的 = 引擎本地虚拟 IP），引擎应回复
    // 4 个 Echo Reply；Reply 按五元组哈希进入任意队列写回，测试轮询
    // 所有队列读回并核对 ID/序号保持.
    constexpr uint16_t k_base_id = 5000;
    const std::vector<uint8_t> payload = {0xde, 0xad, 0xbe, 0xef};
    for (size_t q = 0; q < k_queues; ++q) {
        env.dev.send(make_icmp_echo(ip("10.0.0.2"), ip("10.0.0.1"),
                                    static_cast<uint16_t>(k_base_id + q),
                                    static_cast<uint16_t>(q), payload),
                     q);
    }
    bool seen[k_queues] = {};
    size_t replies = 0;
    for (size_t i = 0; i < 8 && replies < k_queues; ++i) {
        std::vector<uint8_t> pkt;
        if (!env.dev.read_packet(pkt, 3000)) {
            break;
        }
        ip_hdr_info ipi;
        if (!parse_ip(pkt, ipi) || ipi.proto != 1 ||
            ipi.payload_len < 8 + payload.size() || ipi.payload[0] != 0) {
            continue; // 非 Echo Reply（如 TCP 相关包）
        }
        const uint16_t id =
            static_cast<uint16_t>((ipi.payload[4] << 8) | ipi.payload[5]);
        const uint16_t seq =
            static_cast<uint16_t>((ipi.payload[6] << 8) | ipi.payload[7]);
        const size_t idx = static_cast<size_t>(id - k_base_id);
        if (idx < k_queues && !seen[idx]) {
            TEST_ASSERT(seq == idx);
            seen[idx] = true;
            ++replies;
        }
    }
    TEST_ASSERT(replies == k_queues);
    TEST_ASSERT(env.engine.stats().icmp_replies.load() == k_queues);
}

// 2.1 引擎级多队列 + 多线程 io（Strand 串行化）：与 2 相同的注入验证，
// 覆盖 multi-thread 模式下读槽按队列分配与写侧哈希分发路径
BOOST_AUTO_TEST_CASE(test_engine_multi_queue_multithread)
{
    constexpr size_t k_queues = 4;
    net::io_context io;
    tunio::tunio engine(io, false); // multi-thread：Strand 串行化
    fake_device dev(k_queues);

    tun_config cfg;
    cfg.external_handles = dev.inject_fds();
    cfg.external_mtu = 1500;
    cfg.ipv4_addr = "10.0.0.1";
    cfg.netmask = "255.255.255.0";
    cfg.udp_idle_timeout = std::chrono::seconds(1);
    boost::system::error_code ec;
    if (!engine.open(cfg, ec)) {
        TEST_THROW("engine open failed: " + ec.message());
    }
    TEST_ASSERT(engine.queue_count() == k_queues);

    net::executor_work_guard<net::io_context::executor_type> guard(
        net::make_work_guard(io));
    std::thread t1([&] { io.run(); });
    std::thread t2([&] { io.run(); });
    // 异常路径兜底：断言失败时也停止 io 并 join 线程，避免 std::thread
    // 析构未 join 触发 terminate.
    struct io_cleanup
    {
        net::io_context &io;
        std::thread &t1;
        std::thread &t2;
        ~io_cleanup()
        {
            io.stop();
            if (t1.joinable()) {
                t1.join();
            }
            if (t2.joinable()) {
                t2.join();
            }
        }
    } cleanup{io, t1, t2};

    constexpr uint16_t k_base_id = 6000;
    const std::vector<uint8_t> payload = {0xca, 0xfe};
    for (size_t q = 0; q < k_queues; ++q) {
        dev.send(make_icmp_echo(ip("10.0.0.2"), ip("10.0.0.1"),
                                static_cast<uint16_t>(k_base_id + q),
                                static_cast<uint16_t>(q), payload),
                 q);
    }
    bool seen[k_queues] = {};
    size_t replies = 0;
    for (size_t i = 0; i < 8 && replies < k_queues; ++i) {
        std::vector<uint8_t> pkt;
        if (!dev.read_packet(pkt, 3000)) {
            break;
        }
        ip_hdr_info ipi;
        if (!parse_ip(pkt, ipi) || ipi.proto != 1 ||
            ipi.payload_len < 8 + payload.size() || ipi.payload[0] != 0) {
            continue;
        }
        const uint16_t id =
            static_cast<uint16_t>((ipi.payload[4] << 8) | ipi.payload[5]);
        const size_t idx = static_cast<size_t>(id - k_base_id);
        if (idx < k_queues && !seen[idx]) {
            seen[idx] = true;
            ++replies;
        }
    }
    TEST_ASSERT(replies == k_queues);

    engine.close();
    guard.reset();
    t1.join();
    t2.join();
}

// 2.2 队列数超过读槽基准（32）时每队列仍至少一个读槽：向每个队列注入
// ICMP Echo，验证所有队列都能被读取并回复（回归：start_read 曾只启动
// 前 k_read_slots 个槽，队列 32+ 无读槽导致入站包无人读取）.
BOOST_AUTO_TEST_CASE(test_engine_many_queues)
{
    constexpr size_t k_queues = 40; // > k_read_slots（32）
    engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
                   std::chrono::seconds(30), 1024 * 1024,
                   std::chrono::milliseconds(5000),
                   std::chrono::milliseconds(200), 8, k_queues);
    TEST_ASSERT(env.engine.queue_count() == k_queues);

    constexpr uint16_t k_base_id = 7000;
    const std::vector<uint8_t> payload = {0x11, 0x22};
    for (size_t q = 0; q < k_queues; ++q) {
        env.dev.send(make_icmp_echo(ip("10.0.0.2"), ip("10.0.0.1"),
                                    static_cast<uint16_t>(k_base_id + q),
                                    static_cast<uint16_t>(q), payload),
                     q);
    }
    bool seen[k_queues] = {};
    size_t replies = 0;
    for (size_t i = 0; i < 2 * k_queues && replies < k_queues; ++i) {
        std::vector<uint8_t> pkt;
        if (!env.dev.read_packet(pkt, 3000)) {
            break;
        }
        ip_hdr_info ipi;
        if (!parse_ip(pkt, ipi) || ipi.proto != 1 ||
            ipi.payload_len < 8 + payload.size() || ipi.payload[0] != 0) {
            continue;
        }
        const uint16_t id =
            static_cast<uint16_t>((ipi.payload[4] << 8) | ipi.payload[5]);
        const size_t idx = static_cast<size_t>(id - k_base_id);
        if (idx < k_queues && !seen[idx]) {
            seen[idx] = true;
            ++replies;
        }
    }
    TEST_ASSERT(replies == k_queues);
}

// 3. 真实 TUN 多队列环回（仅 Linux：需要 root + /dev/net/tun；
//    不可用时跳过；macOS 等平台无 /dev/net/tun 与 IFF_MULTI_QUEUE）
#if defined(__linux__)
BOOST_AUTO_TEST_CASE(test_real_multi_queue_tun)
{
    // 真实多队列 TUN 环回：需要 root + /dev/net/tun + 内核支持
    // IFF_MULTI_QUEUE；任一前置不可用时跳过.
    const int probe = ::open("/dev/net/tun", O_RDWR | O_CLOEXEC);
    if (probe < 0) {
        std::cout << "[skip] /dev/net/tun 不可用，跳过真实多队列测试"
            << std::endl;
        return;
    }
    ::close(probe);

    constexpr size_t k_queues = 4;
    constexpr size_t k_echo_count = 16;

    net::io_context io;
    tunio::tun_device dev(io);
    std::vector<tunio::packet_buffer> bufs;
    bufs.reserve(k_queues);
    for (size_t i = 0; i < k_queues; ++i) {
        bufs.emplace_back(2048, 128);
    }
    std::atomic<size_t> engine_replies{0};

    // 设备名冲突或权限不足时换名重试；全部失败则跳过
    std::string dev_name;
    for (int idx = 0; idx < 8; ++idx) {
        dev_name = "tunio-mq" + std::to_string(idx);
        tunio::device_config dc;
        dc.name = dev_name;
        // 独立测试网段，避免与本机既有 tun 网段（如 10.99.0.0/24）冲突
        dc.ipv4 = "10.97.0.1";
        dc.netmask = "255.255.255.0";
        dc.num_queues = k_queues;
        boost::system::error_code ec;
        if (dev.open(dc, ec)) {
            break;
        }
        if (idx == 7) {
            std::cout << "[skip] 无法打开多队列 tun 设备: " << ec.message()
                << std::endl;
            return;
        }
    }
    TEST_ASSERT(dev.queue_count() == k_queues);

    // 测试进程额外打开一个队列 fd 作为注入通道（多队列允许任意附加队列）
    const int inject_fd = ::open("/dev/net/tun", O_RDWR | O_CLOEXEC);
    if (inject_fd < 0) {
        std::cout << "[skip] 无法打开注入队列 fd: " << std::strerror(errno)
            << std::endl;
        return;
    }
    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::snprintf(ifr.ifr_name, IFNAMSIZ, "%s", dev_name.c_str());
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI | IFF_MULTI_QUEUE;
    if (::ioctl(inject_fd, TUNSETIFF, &ifr) < 0) {
        std::cout << "[skip] 附加注入队列失败: " << std::strerror(errno)
            << std::endl;
        ::close(inject_fd);
        io.stop();
        return;
    }

    // 每队列持续异步读：收到 ICMP Echo Reply 即计数，读后立即重新发起，
    // 避免队列被杂散包（邻居发现等）占用后漏读后续到达的 Reply.
    for (size_t q = 0; q < k_queues; ++q) {
        auto reread = [&, q](auto &&self) -> void {
            dev.async_read_packet(bufs[q], q,
                [&, q, self](const boost::system::error_code &ec, size_t n) {
                    if (ec) {
                        return;
                    }
                    bufs[q].commit(n);
                    const uint8_t *base = bufs[q].data();
                    if (n >= 28 && (base[0] >> 4) == 4 && base[9] == 1 &&
                        base[20] == 0) {
                        engine_replies.fetch_add(1);
                    }
                    bufs[q].reset();
                    self(self);
                });
        };
        reread(reread);
    }
    std::thread th([&] { io.run(); });

    // RAII 清理：即使后续断言失败（抛异常）也停止 io 并 join 线程，
    // 避免 std::thread 析构时未 join 触发 terminate.
    struct io_cleanup
    {
        net::io_context &io;
        std::thread &th;
        ~io_cleanup()
        {
            io.stop();
            if (th.joinable()) {
                th.join();
            }
        }
    } cleanup{io, th};

    // 向注入队列写入不同源地址的 ICMP Echo（目的 = tun 本机 IP）：
    // 内核应答后按流哈希把 Reply 分发到各队列 fd，引擎侧至少收到一个
    for (size_t i = 0; i < k_echo_count; ++i) {
        const uint32_t src = ip("10.97.0.10") + static_cast<uint32_t>(i);
        const auto req = make_icmp_echo(src, ip("10.97.0.1"),
                                        static_cast<uint16_t>(3000 + i), 0,
                                        {0x01, 0x02, 0x03});
        size_t off = 0;
        while (off < req.size()) {
            const ssize_t n =
                ::write(inject_fd, req.data() + off, req.size() - off);
            TEST_ASSERT(n > 0);
            off += static_cast<size_t>(n);
        }
    }

    // 等待引擎收到至少一个 Echo Reply（5s 超时）
    for (int i = 0; i < 100 && engine_replies.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    TEST_ASSERT(engine_replies.load() >= 1);

    ::close(inject_fd);
}
#endif // __linux__

} // namespace
