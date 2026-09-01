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
#include "tun_queue_writer.hpp"
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
