//
// device_writer.hpp
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

// 串行化设备写队列
//
// 底层描述符同一时刻仅允许一个未完成的异步写操作，所有写请求统一进入
// 队列，由 Strand 上的泵循环依次下发；本类所有方法都必须在 Strand 上调用。
//
// 生命周期: 写完成回调捕获自身的 shared_ptr 保活, 引擎重建（reopen）释放
// 旧 writer 时, 在途写操作的迟到完成回调不会访问已释放对象; 设备与统计
// 对象同样以 shared_ptr 共享, 避免回调晚于引擎析构时引用悬垂.
class device_writer : public std::enable_shared_from_this<device_writer>
{
public:
    device_writer(net::any_io_executor strand,
        std::shared_ptr<tun_device> dev,
        std::shared_ptr<engine_stats> stats)
        : strand_(std::move(strand))
        , dev_(std::move(dev))
        , stats_(std::move(stats))
    {
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
        if (queue_.size() >= k_queue_max_entries) {
            return;
        }
        queue_.push_back(entry{std::move(buf), {}});
        pump();
    }

    // 将数据包加入写队列；完成回调在调用方绑定执行器上触发
    template <typename CompletionToken>
    auto async_write(packet_buffer &&buf, CompletionToken &&token)
    {
        return net::async_initiate<CompletionToken,
                                   void(boost::system::error_code, size_t)>(
            [this](auto handler, packet_buffer buf) {
                if (queue_.size() >= k_queue_max_entries) {
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
                queue_.push_back(entry{std::move(buf), std::move(handler)});
                pump();
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
        while (!queue_.empty()) {
            auto e = std::move(queue_.front());
            queue_.pop_front();
            recycle(std::move(e.buf));
            if (e.handler) {
                e.handler(
                    boost::system::error_code(net::error::operation_aborted),
                    0);
            }
        }
    }

private:
    struct entry
    {
        packet_buffer buf;
        device_write_handler handler;
    };

    void pump()
    {
        if (cancelled_ || current_ || queue_.empty()) {
            return;
        }
        // 当前写入的 entry 作为成员保存（零堆分配），cancel_all 清空队列时
        // 正在进行的写不受影响，完成回调仍能安全访问其缓冲与 handler。
        current_ = std::move(queue_.front());
        queue_.pop_front();
        packet_buffer &buf = current_->buf;
        auto self = shared_from_this();
        dev_->async_write_packet(buf,
            net::bind_executor(strand_, [self](
                boost::system::error_code ec, size_t n) {
                self->on_write_done(ec, n);
            }));
    }

    // 写完成回调（在 Strand 上执行）
    void on_write_done(const boost::system::error_code &ec, size_t n)
    {
        if (!ec) {
            stats_->tx_packets.fetch_add(1, std::memory_order_relaxed);
        }
        if (cancelled_) {
            if (current_->handler) {
                current_->handler(
                    boost::system::error_code(net::error::operation_aborted),
                    0);
            }
        } else if (current_->handler) {
            current_->handler(ec, n);
        }
        recycle(std::move(current_->buf));
        current_.reset();
        pump();
    }

    net::any_io_executor strand_;
    std::shared_ptr<tun_device> dev_;
    std::shared_ptr<engine_stats> stats_;
    std::deque<entry> queue_;
    std::vector<packet_buffer> pool_;
    std::optional<entry> current_; // 正在写入的数据包
    bool cancelled_ = false;
    uint16_t ip_id_ = 0;

    static constexpr size_t k_pool_max = 64; // 池上限，避免长期闲置占用内存
    // 写队列条目上限（安全阀）：设备写停滞时丢弃/报错而非无界积压；
    // 16384 条 × ~2KB ≈ 32MB 最坏占用
    static constexpr size_t k_queue_max_entries = 16384;
};

} // namespace detail
} // namespace tunio
