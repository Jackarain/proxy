//
// tun_queue_writer.hpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tunio/packet_buffer.hpp"
#include "tunio/tun_device.hpp"
#include "tunio/tun_config.hpp"

#include <boost/asio.hpp>
#include <boost/system/errc.hpp>

#include <deque>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace tunio {
namespace net = boost::asio;
namespace detail {

// 设备写完成处理器（与 Boost.Asio 异步操作完成签名一致）
using device_write_handler =
    net::any_completion_handler<void(boost::system::error_code, size_t)>;

// 写队列选择：按报文的五元组（地址族 + 源/目的地址 + 协议 + 端口）做
// FNV-1a 哈希后取模，把同一流的报文稳定分发到同一队列 fd；多队列下
// 各队列 fd 独立入内核发送队列，写吞吐随队列数扩展。
// 单队列（queue_count <= 1）短路返回 0，零额外开销；非 IP 或残缺报文
// 回退队列 0（写队列选择只影响分布，不影响正确性）.
inline size_t pick_tx_queue(const packet_buffer &buf, size_t queue_count)
{
    if (queue_count <= 1) {
        return 0;
    }
    const uint8_t *p = buf.data();
    const size_t len = buf.size();
    if (len < 20) {
        return 0;
    }
    uint64_t h = 1469598103934665603ULL; // FNV offset basis
    const auto mix = [&](const uint8_t *d, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            h ^= d[i];
            h *= 1099511628211ULL; // FNV prime
        }
    };
    switch (p[0] >> 4) {
    case 4: {
        const size_t ihl = static_cast<size_t>(p[0] & 0x0f) * 4;
        const size_t total = static_cast<size_t>((p[2] << 8) | p[3]);
        mix(p + 12, 4); // 源地址
        mix(p + 16, 4); // 目的地址
        mix(p + 9, 1);  // 协议
        const uint8_t proto = p[9];
        // 端口参与哈希，进一步打散同一地址对下的不同流
        if (ihl >= 20 && total >= ihl && len >= total &&
            (proto == 6 || proto == 17) && total - ihl >= 4) {
            mix(p + ihl, 4); // 源/目的端口（网络字节序）
        }
        break;
    }
    case 6: {
        // 残缺 IPv6 头（< 40 字节固定头）：地址/端口区不可读，回退队列 0.
        if (len < 40) {
            return 0;
        }
        const size_t total = 40 + static_cast<size_t>((p[4] << 8) | p[5]);
        mix(p + 8, 16); // 源地址
        mix(p + 24, 16); // 目的地址
        mix(p + 6, 1);   // 下一头协议
        const uint8_t proto = p[6];
        if (total >= 40 && len >= total && (proto == 6 || proto == 17) &&
            total - 40 >= 4) {
            mix(p + 40, 4); // 源/目的端口
        }
        break;
    }
    default:
        return 0; // 非 IP：回退队列 0
    }
    return static_cast<size_t>(h % queue_count);
}

// 串行化设备写队列（每队列独立写链）
//
// 每个队列 fd 同一时刻仅允许一个未完成的异步写操作：写请求按报文五元组
// 哈希分发到对应队列的独立写链，同队列内由 Strand 上的泵循环依次下发
//（保持同流顺序），不同队列的写链互不阻塞——各 fd 独立，可同时处于
// 未完成写状态，写吞吐随队列数扩展。本类所有方法都必须在 Strand 上调用。
//
// 生命周期: 写完成回调捕获自身的 shared_ptr 保活, 引擎重建（reopen）释放
// 旧 writer 时, 在途写操作的迟到完成回调不会访问已释放对象; 设备与统计
// 对象同样以 shared_ptr 共享, 避免回调晚于引擎析构时引用悬垂.
class tun_queue_writer : public std::enable_shared_from_this<tun_queue_writer>
{
public:
    tun_queue_writer(net::any_io_executor strand,
        std::shared_ptr<tun_device> dev,
        std::shared_ptr<engine_stats> stats)
        : strand_(std::move(strand))
        , dev_(std::move(dev))
        , stats_(std::move(stats))
        , queue_count_(dev_ ? dev_->queue_count() : 1)
    {
        states_.resize(std::max<size_t>(1, queue_count_));
    }

    // 从池中获取发送缓冲：池内存在容量与 headroom 均匹配的缓冲则复用，
    // 否则新建（所有调用方均使用 headroom=64，回收缓冲 headroom 恒定）.
    packet_buffer acquire(size_t capacity, size_t headroom)
    {
        while (!pool_.empty()) {
            auto b = std::move(pool_.back());
            pool_.pop_back();
            if (b.capacity() >= capacity && b.headroom() == headroom) {
                return b;
            }
        }
        return packet_buffer(capacity, headroom);
    }

    // 写完成后回收发送缓冲（必须在 Strand 上调用）
    void recycle(packet_buffer &&buf)
    {
        buf.reset();
        if (pool_.size() < k_pool_max) {
            pool_.push_back(std::move(buf));
        }
    }

    // 写后无需回调的发送路径（引擎内 TCP/UDP/ICMP 出包）：
    // 跳过 CompletionToken 包装与 handler 堆分配，直接在 Strand 上入队。
    // 设备写停滞（对端/宿主变慢）时队列无界积压会耗尽内存：达到上限后
    // 丢弃新包作为安全阀（控制段丢失的代价远小于内存耗尽）.
    void async_write_and_forget(packet_buffer &&buf)
    {
        if (queued_total_ >= k_queue_max_entries) {
            return;
        }
        const size_t q = pick_tx_queue(buf, queue_count_);
        states_[q].queue.push_back(entry{std::move(buf), {}, q});
        ++queued_total_;
        pump(q);
    }

    // 将数据包加入写队列；完成回调在调用方绑定执行器上触发
    template <typename CompletionToken>
    auto async_write(packet_buffer &&buf, CompletionToken &&token)
    {
        return net::async_initiate<CompletionToken,
                                   void(boost::system::error_code, size_t)>(
            [this](auto handler, packet_buffer buf) {
                if (queued_total_ >= k_queue_max_entries) {
                    // 队列饱和：以 no_buffer_space 完成，避免无界积压。
                    // 用 dispatch 在 handler 的关联执行器上完成：新版 Asio
                    // （1.38+）的默认关联执行器为 inline_executor，无法满足
                    // post 的 blocking.never 约束（编译失败），dispatch 无此
                    // 限制，且与引擎其余完成回调派发方式保持一致.
                    net::dispatch(net::get_associated_executor(handler),
                        [h = std::move(handler)]() mutable {
                            h(make_error_code(net::error::no_buffer_space), 0);
                        });
                    return;
                }
                const size_t q = pick_tx_queue(buf, queue_count_);
                states_[q].queue.push_back(
                    entry{std::move(buf), std::move(handler), q});
                ++queued_total_;
                pump(q);
            },
            token, std::move(buf));
    }

    // 分配 IP Identification（引擎内共享计数）
    uint16_t alloc_ip_id()
    {
        return ++ip_id_;
    }

    // 清空队列并以 operation_aborted 完成挂起的写操作（Strand 上调用）
    void cancel_all()
    {
        cancelled_ = true;
        for (auto &st : states_) {
            while (!st.queue.empty()) {
                auto e = std::move(st.queue.front());
                st.queue.pop_front();
                --queued_total_;
                recycle(std::move(e.buf));
                if (e.handler) {
                    e.handler(
                        boost::system::error_code(
                            net::error::operation_aborted),
                        0);
                }
            }
        }
    }

private:
    // 瞬时写失败重试上限：macOS 非阻塞数据报写满返回 ENOBUFS（而非
    // EAGAIN）时直接重试同一缓冲，等待对端/内核排空；上限防止设备真
    // 故障时 io 线程忙等失控.
    static constexpr int k_write_retry_limit = 4096;

    struct entry
    {
        packet_buffer buf;
        device_write_handler handler;
        size_t queue = 0; // 目标队列 fd（入队时按五元组哈希选定，重试沿用）
        int retries_left = k_write_retry_limit; // 瞬时写失败剩余重试次数
    };

    void pump(size_t q)
    {
        auto &st = states_[q];
        if (cancelled_ || st.current || st.queue.empty()) {
            return;
        }
        // 当前写入的 entry 作为成员保存（零堆分配），cancel_all 清空队列时
        // 正在进行的写不受影响，完成回调仍能安全访问其缓冲与 handler。
        st.current = std::move(st.queue.front());
        st.queue.pop_front();
        --queued_total_;
        packet_buffer &buf = st.current->buf;
        const size_t queue = st.current->queue;
        auto self = shared_from_this();
        dev_->async_write_packet(buf, queue,
            net::bind_executor(strand_, [self, q](
                boost::system::error_code ec, size_t n) {
                self->on_write_done(ec, n, q);
            }));
    }

    // 写完成回调（在 Strand 上执行；q 指明所属写链）
    void on_write_done(const boost::system::error_code &ec, size_t n, size_t q)
    {
        auto &st = states_[q];
        // macOS/BSD 非阻塞数据报写满时返回 ENOBUFS（而非 EAGAIN），且内核
        // 不给出可写通知，Asio 将其作为写失败立即完成；排空缓冲的是对端/
        // 内核（独立于 io 线程），因此直接重试同一缓冲即可等到可写空间，
        // 而不是上报写失败中断整条发送链.
        if (!cancelled_ && is_transient_write_error(ec) &&
            st.current->retries_left > 0) {
            --st.current->retries_left;
            packet_buffer &buf = st.current->buf;
            const size_t queue = st.current->queue; // 重试沿用原队列
            auto self = shared_from_this();
            dev_->async_write_packet(buf, queue,
                net::bind_executor(strand_, [self, q](
                    boost::system::error_code ec, size_t n) {
                    self->on_write_done(ec, n, q);
                }));
            return;
        }
        if (!ec) {
            stats_->tx_packets.fetch_add(1, std::memory_order_relaxed);
        }
        if (cancelled_) {
            if (st.current->handler) {
                st.current->handler(
                    boost::system::error_code(net::error::operation_aborted),
                    0);
            }
        } else if (st.current->handler) {
            st.current->handler(ec, n);
        }
        recycle(std::move(st.current->buf));
        st.current.reset();
        pump(q);
    }

    // 设备写瞬时失败（可重试）判定：非阻塞写满返回 ENOBUFS/EAGAIN，与
    // 设备本身损坏（bad_descriptor 等）区分开.
    static bool is_transient_write_error(const boost::system::error_code &ec)
    {
        return ec == boost::system::errc::no_buffer_space
            || ec == boost::system::errc::resource_unavailable_try_again;
    }

    net::any_io_executor strand_;
    std::shared_ptr<tun_device> dev_;
    std::shared_ptr<engine_stats> stats_;
    size_t queue_count_ = 1; // 设备队列数（构造时从设备缓存）
    // 每队列一个写链：同队列串行（保持流内顺序），跨队列并行（各自 fd
    // 独立，可同时处于未完成写状态）.
    struct queue_state
    {
        std::deque<entry> queue;
        std::optional<entry> current; // 正在写入的数据包
    };
    std::vector<queue_state> states_;
    size_t queued_total_ = 0; // 全部队列待写总数（安全阀水位）
    std::vector<packet_buffer> pool_;
    bool cancelled_ = false;
    uint16_t ip_id_ = 0;

    static constexpr size_t k_pool_max = 64; // 池上限，避免长期闲置占用内存
    // 写队列条目上限（安全阀）：设备写停滞时丢弃/报错而非无界积压；
    // 16384 条 × ~2KB ≈ 32MB 最坏占用
    static constexpr size_t k_queue_max_entries = 16384;
};

} // namespace detail
} // namespace tunio
