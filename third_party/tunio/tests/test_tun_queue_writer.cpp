//
// test_tun_queue_writer.cpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#define BOOST_TEST_MODULE tun_queue_writer
#include <boost/test/included/unit_test.hpp>
#include "tunio/detail/tun_queue_writer.hpp"
#include "tunio/tun_config.hpp"
#include "test_throw.hpp"

#include <boost/asio.hpp>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

// 验证 tun_queue_writer 对瞬时设备写失败的处理：macOS 非阻塞数据报写满时
// 返回 ENOBUFS（而非 EAGAIN），Asio 立即以错误完成，tun_queue_writer 应延迟
// 重试而不是上报写失败中断发送链；对端长期不排空时经有限次重试后以错误
// 完成（不永久挂起）。Linux 同一场景走 EAGAIN + 可写通知路径，本测试的
// 排空场景同样通过（验证全部写入最终成功）.
namespace {
namespace net = boost::asio;

constexpr size_t k_payload = 1400; // 数据段大小，接近 1500 MTU 场景

struct write_result
{
    boost::system::error_code ec;
    size_t n = 0;
};

write_result wait_future(std::future<write_result> fut, int timeout_ms)
{
    if (fut.wait_for(std::chrono::milliseconds(timeout_ms)) !=
        std::future_status::ready) {
        TEST_THROW("write future timeout");
    }
    return fut.get();
}

template <typename T>
T wait_future_value(std::future<T> fut, int timeout_ms)
{
    if (fut.wait_for(std::chrono::milliseconds(timeout_ms)) !=
        std::future_status::ready) {
        TEST_THROW("future timeout");
    }
    return fut.get();
}

// 阻塞轮询读端并取走一个完整报文
void drain_one(int fd, std::vector<uint8_t> &stash)
{
    struct pollfd pfd{fd, POLLIN, 0};
    if (::poll(&pfd, 1, 5000) <= 0) {
        TEST_THROW("drain poll timeout");
    }
    uint8_t buf[65536];
    const ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n <= 0) {
        TEST_THROW("drain read failed");
    }
    stash.insert(stash.end(), buf, buf + n);
}

// 在 Strand 上发起一次 async_write，返回完成 future
std::future<write_result> post_write(
    const std::shared_ptr<tunio::detail::tun_queue_writer> &writer,
    const net::any_io_executor &strand,
    const std::vector<uint8_t> &payload)
{
    std::promise<write_result> p;
    auto fut = p.get_future();
    net::post(strand, [writer, payload, p = std::move(p)]() mutable {
        tunio::packet_buffer buf(payload.size() + 64, 64);
        std::memcpy(buf.writable_data(), payload.data(), payload.size());
        buf.resize(payload.size());
        writer->async_write(std::move(buf),
            [p = std::move(p)](boost::system::error_code ec, size_t n) mutable {
                p.set_value(write_result{ec, n});
            });
    });
    return fut;
}

// io 线程守卫：异常路径也回收线程与设备，避免未 join 线程触发 terminate
struct io_guard
{
    net::io_context &io;
    net::executor_work_guard<net::io_context::executor_type> guard;
    std::shared_ptr<tunio::tun_device> dev;
    int fd;
    std::thread thread;

    io_guard(net::io_context &i, std::shared_ptr<tunio::tun_device> d, int f)
        : io(i)
        , guard(net::make_work_guard(io))
        , dev(std::move(d))
        , fd(f)
        , thread([&] { io.run(); })
    {
    }

    ~io_guard()
    {
        guard.reset();
        io.stop();
        if (thread.joinable()) {
            thread.join();
        }
        dev->close();
        ::close(fd);
    }
};

// 背压环境：对端不读的小缓冲 socketpair，制造设备写挂起与写队列积压
struct backpressure_env
{
    net::io_context io;
    net::any_io_executor strand = net::make_strand(io);
    std::shared_ptr<tunio::tun_device> dev;
    std::shared_ptr<tunio::engine_stats> stats;
    std::shared_ptr<tunio::detail::tun_queue_writer> writer;
    net::executor_work_guard<net::io_context::executor_type> guard;
    std::thread thread;
    int peer = -1;

    backpressure_env()
        : dev(std::make_shared<tunio::tun_device>(io))
        , stats(std::make_shared<tunio::engine_stats>())
        , guard(net::make_work_guard(io))
    {
        int sv[2];
        if (::socketpair(AF_UNIX, SOCK_DGRAM, 0, sv) != 0) {
            TEST_THROW("socketpair failed");
        }
        peer = sv[0];
        // 收缩收发缓冲：少量报文即填满，设备写随即挂起
        const int bufsz = 2048;
        ::setsockopt(sv[0], SOL_SOCKET, SO_RCVBUF, &bufsz, sizeof(bufsz));
        ::setsockopt(sv[1], SOL_SOCKET, SO_SNDBUF, &bufsz, sizeof(bufsz));
        const int flags = ::fcntl(sv[1], F_GETFL, 0);
        ::fcntl(sv[1], F_SETFL, flags | O_NONBLOCK);
        boost::system::error_code ec;
        if (!dev->assign(sv[1], 1500, false, ec)) {
            TEST_THROW("device assign failed: " + ec.message());
        }
        writer = std::make_shared<tunio::detail::tun_queue_writer>(
            strand, dev, stats);
        thread = std::thread([this] { io.run(); });
    }

    ~backpressure_env()
    {
        io.stop();
        if (thread.joinable()) {
            thread.join();
        }
        dev->close();
        ::close(peer);
    }

    // 在 Strand 上连续投递 count 条出包（丢弃路径：无完成回调）
    void post_forgets(size_t count, size_t payload_len)
    {
        std::vector<uint8_t> payload(payload_len, 0xee);
        std::promise<void> posted;
        auto fut = posted.get_future();
        auto w = writer;
        net::post(strand, [w, payload, count, &posted]() {
            for (size_t i = 0; i < count; ++i) {
                tunio::packet_buffer buf(payload.size() + 64, 64);
                std::memcpy(
                    buf.writable_data(), payload.data(), payload.size());
                buf.resize(payload.size());
                w->async_write_and_forget(std::move(buf));
            }
            posted.set_value();
        });
        wait_future_value(std::move(fut), 5000);
    }

    // 非阻塞读尽对端当前可用报文，返回读取数量
    size_t drain_available()
    {
        size_t n = 0;
        for (;;)
        {
            struct pollfd pfd{peer, POLLIN, 0};
            if (::poll(&pfd, 1, 5) <= 0) {
                break;
            }
            uint8_t buf[65536];
            const ssize_t r = ::read(peer, buf, sizeof(buf));
            if (r <= 0) {
                break;
            }
            ++n;
        }
        return n;
    }

    // 已结算的出包数（写出设备 + 丢弃）
    uint64_t settled() const
    {
        return stats->tx_packets.load() + stats->tx_dropped.load();
    }

    // 排空对端直到出包全部结算；超时返回 false
    bool wait_settled(uint64_t total, int timeout_ms)
    {
        const auto deadline = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(timeout_ms);
        while (settled() < total)
        {
            drain_available();
            if (std::chrono::steady_clock::now() >= deadline) {
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return true;
    }
};

} // namespace

BOOST_AUTO_TEST_CASE(tun_queue_writer)
{
    int sv[2];
    if (::socketpair(AF_UNIX, SOCK_DGRAM, 0, sv) != 0) {
        TEST_THROW("socketpair failed");
    }
    // 收缩接收缓冲：放大写满概率，覆盖瞬时写失败路径
    const int bufsz = 2048;
    ::setsockopt(sv[0], SOL_SOCKET, SO_RCVBUF, &bufsz, sizeof(bufsz));
    ::setsockopt(sv[0], SOL_SOCKET, SO_SNDBUF, &bufsz, sizeof(bufsz));
    ::setsockopt(sv[1], SOL_SOCKET, SO_RCVBUF, &bufsz, sizeof(bufsz));
    ::setsockopt(sv[1], SOL_SOCKET, SO_SNDBUF, &bufsz, sizeof(bufsz));
    // 写端非阻塞：Linux 写满返回 EAGAIN（Asio 等待可写通知），macOS 返回
    // ENOBUFS（走 tun_queue_writer 重试）；避免阻塞写挂住 io 线程.
    const int flags = ::fcntl(sv[1], F_GETFL, 0);
    ::fcntl(sv[1], F_SETFL, flags | O_NONBLOCK);

    net::io_context io;
    net::any_io_executor strand = net::make_strand(io);
    auto dev = std::make_shared<tunio::tun_device>(io);
    boost::system::error_code ec;
    if (!dev->assign(sv[1], 1500, false, ec)) {
        TEST_THROW("device assign failed: " + ec.message());
    }
    auto stats = std::make_shared<tunio::engine_stats>();
    auto writer = std::make_shared<tunio::detail::tun_queue_writer>(
        strand, dev, stats);
    io_guard guard(io, dev, sv[0]);

    std::vector<uint8_t> payload(k_payload, 0xab);

#if defined(__APPLE__)
    // 写满接收队列（读端未启动）：ENOBUFS 应经有限次重试后以错误完成，
    // 而不是永久挂起；排空后写入恢复正常.
    {
        int successes = 0;
        bool got_error = false;
        for (int i = 0; i < 16 && !got_error; ++i) {
            auto r = wait_future(post_write(writer, strand, payload), 5000);
            if (r.ec) {
                got_error = true;
            } else {
                TEST_ASSERT(r.n == k_payload);
                ++successes;
            }
        }
        TEST_ASSERT(got_error);
        std::vector<uint8_t> stash;
        for (int i = 0; i < successes; ++i) {
            drain_one(sv[0], stash);
        }
        TEST_ASSERT(stash.size() == successes * k_payload);
        auto r = wait_future(post_write(writer, strand, payload), 5000);
        TEST_ASSERT(!r.ec && r.n == k_payload);
    }
#endif

    // 小缓冲 + 并发排空：瞬时拥塞不得中断发送链，全部写入须成功完成
    {
        constexpr size_t k_packets = 32;
        std::vector<std::future<write_result>> futs;
        futs.reserve(k_packets);
        for (size_t i = 0; i < k_packets; ++i) {
            futs.push_back(post_write(writer, strand, payload));
        }
        std::vector<uint8_t> stash;
        for (size_t i = 0; i < k_packets; ++i) {
            drain_one(sv[0], stash);
        }
        for (auto &f : futs) {
            auto r = wait_future(std::move(f), 30000);
            TEST_ASSERT(!r.ec && r.n == k_payload);
        }
        TEST_ASSERT(stash.size() == k_packets * k_payload);
    }
}

// 设备写失败：出包计入 tx_dropped，不计入 tx_packets
BOOST_AUTO_TEST_CASE(tun_queue_writer_failed_write_counted_as_dropped)
{
    net::io_context io;
    net::any_io_executor strand = net::make_strand(io);
    // 未 assign 队列 fd：写立即以 bad_descriptor 完成
    auto dev = std::make_shared<tunio::tun_device>(io);
    auto stats = std::make_shared<tunio::engine_stats>();
    auto writer = std::make_shared<tunio::detail::tun_queue_writer>(
        strand, dev, stats);

    std::vector<uint8_t> payload(k_payload, 0x5a);
    auto fut = post_write(writer, strand, payload);
    io.run(); // 写失败立即完成，无挂起 I/O：处理完本次写即返回
    const auto r = wait_future(std::move(fut), 5000);

    TEST_ASSERT(r.ec == net::error::bad_descriptor);
    TEST_ASSERT(stats->tx_packets.load() == 0);
    TEST_ASSERT(stats->tx_dropped.load() == 1);
}

// 写队列安全阀：设备写停滞（对端不读）时超限出包被丢弃，丢弃计入
// tx_dropped（此前完全静默）；排空后写出与丢弃之和恰为投递总数
BOOST_AUTO_TEST_CASE(tun_queue_writer_queue_full_counted_as_dropped)
{
    backpressure_env env;
    // 队列上限 16384 条，另有 1 条在写：多投 32 条保证触发安全阀
    const uint64_t total = 16384 + 1 + 32;
    env.post_forgets(total, 128);

    TEST_ASSERT(env.wait_settled(total, 20000));
    TEST_ASSERT(env.stats->tx_dropped.load() >= 1);
    TEST_ASSERT(env.stats->tx_packets.load() >= 1);
    TEST_ASSERT(env.settled() == total);
}

// 引擎关闭清理：排队中的出包被丢弃并计入 tx_dropped，不计入 tx_packets
BOOST_AUTO_TEST_CASE(tun_queue_writer_cancel_counted_as_dropped)
{
    backpressure_env env;
    const uint64_t posts = 64;
    env.post_forgets(posts, k_payload);

    std::promise<void> cancelled;
    auto fut = cancelled.get_future();
    auto w = env.writer;
    net::post(env.strand, [w, &cancelled]() {
        w->cancel_all();
        cancelled.set_value();
    });
    wait_future_value(std::move(fut), 5000);

    // 在飞的一条仍会写出设备，其余排队条目在 cancel_all 中丢弃
    TEST_ASSERT(env.wait_settled(posts, 20000));
    TEST_ASSERT(env.stats->tx_dropped.load() >= posts - 4);
    TEST_ASSERT(env.stats->tx_packets.load() <= 4);

    // 取消后再入队：不再挂起（无回调路径直接计丢弃，带回调路径以
    // operation_aborted 完成），避免滞留队列且永不触发回调
    const uint64_t dropped = env.stats->tx_dropped.load();
    env.post_forgets(1, k_payload);
    const auto r = wait_future(
        post_write(env.writer, env.strand, std::vector<uint8_t>(k_payload, 0x5b)),
        5000);
    TEST_ASSERT(r.ec == net::error::operation_aborted);
    TEST_ASSERT(env.stats->tx_dropped.load() == dropped + 2);
}
