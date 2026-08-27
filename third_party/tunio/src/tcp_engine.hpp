//
// tcp_engine.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "ip_headers.hpp"
#include "tunio/tun_config.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tunio {
namespace net = boost::asio;
namespace detail {

class device_writer;

// 全局缓冲区记账：跨 TCP/UDP 队列统计占用，施加 max_total_buffer 上限
struct buffer_accountant
{
    size_t limit = 0;
    size_t used = 0;

    bool reserve(size_t n)
    {
        if (used + n > limit) {
            return false;
        }
        used += n;
        return true;
    }

    void release(size_t n)
    {
        used -= n;
    }
};

enum class tcp_state : uint8_t {
    CLOSED,
    SYN_RCVD,
    ESTABLISHED,
    FIN_WAIT_1,
    FIN_WAIT_2,
    CLOSE_WAIT,
    LAST_ACK,
    TIME_WAIT,
};

class tcp_engine;

// TCP 最小控制块：仅维护转发所需的最精简状态
struct tcp_flow : public std::enable_shared_from_this<tcp_flow>
{
    static constexpr uint32_t fixed_rcv_wnd = 65535;

    five_tuple key;
    // 弱引用避免与引擎（flows_ 持有流强引用）构成循环引用；访问前须
    // lock()，引擎已销毁时以 bad_descriptor / 空操作优雅失败。
    std::weak_ptr<tcp_engine> eng;

    // ---- 序列号跟踪（主机字节序）----
    uint32_t snd_nxt = 0; // 本端将要发送的下一个序列号
    uint32_t snd_una = 0; // 最低未确认序列号
    uint32_t rcv_nxt = 0; // 本端期望接收的下一个序列号
    uint32_t iss = 0;     // 本端初始发送序号
    uint32_t irs = 0;     // 对端初始发送序号

    // ---- 状态机 ----
    tcp_state state = tcp_state::CLOSED;
    uint16_t peer_wnd = 0; // 客户端通告的接收窗口
    bool fin_sent = false;
    bool fin_received = false;
    bool rst = false;         // 收到 RST 或主动 RST
    bool app_closed = false;  // 应用层已关闭
    bool rx_shutdown = false; // 应用层已关闭接收侧
    bool accepted = false;    // 已交付给 accept
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point destroy_at;
    // 关闭流程开始时间（发送 FIN 时记录），用于 FIN_WAIT/LAST_ACK 强制清理
    // 超时计时；避免以连接创建时间为基准导致长连接关闭时立即被清理。
    std::chrono::steady_clock::time_point close_started_at;

    // 客户端（虚拟网内）端点：key 中的源地址与源端口
    net::ip::tcp::endpoint remote_endpoint() const
    {
        if (key.family == 6) {
            net::ip::address_v6::bytes_type b{};
            std::copy(key.src_ip.begin(), key.src_ip.end(), b.begin());
            return {net::ip::address_v6(b), ntohs(key.src_port)};
        }
        net::ip::address_v4::bytes_type b{};
        std::copy(key.src_ip.begin(), key.src_ip.begin() + 4, b.begin());
        return {net::ip::address_v4(b), ntohs(key.src_port)};
    }

    // ---- 接收队列（已按序确认的字节流，连续缓冲 + 消费偏移）----
    std::vector<uint8_t> rx_data;
    size_t rx_head = 0;  // 已消费偏移（rx_data 头部）
    size_t rx_bytes = 0; // 未消费字节数

    // ---- delayed ACK ----
    uint8_t ack_pending = 0; // 待确认的数据段计数
    bool ack_deferred = false;

    // ---- 挂起操作 ----
    struct read_op
    {
        std::vector<net::mutable_buffer> buffers;
        size_t total = 0;
        net::any_completion_handler<void(boost::system::error_code, size_t)>
            handler;
    };
    std::deque<read_op> pending_reads;

    struct write_op
    {
        std::vector<net::const_buffer> buffers; // 用户数据引用，回调 handler 前由调用方保证有效
        size_t total = 0;                       // 待发送总字节数（buffers 求和）
        size_t offset = 0;
        net::any_completion_handler<void(boost::system::error_code, size_t)>
            handler;
    };
    std::deque<write_op> pending_writes;
    size_t tx_bytes = 0; // 排队待发送的字节数（含未发送部分）

    net::ip::tcp::endpoint original_destination() const;
    bool is_open() const;
};

template <typename Handler>
void tcp_flow_start_read(std::shared_ptr<tcp_flow>,
                         std::vector<net::mutable_buffer>, size_t, Handler);
template <typename Handler>
void tcp_flow_start_write(std::shared_ptr<tcp_flow>,
                          std::vector<net::const_buffer>, size_t, Handler);

class tcp_engine : public std::enable_shared_from_this<tcp_engine>
{
public:
    tcp_engine(net::any_io_executor strand, device_writer &writer,
               const tun_config &cfg, engine_stats &stats,
               std::shared_ptr<buffer_accountant> account);
    ~tcp_engine();

    // 处理一个 TCP 报文段（Strand 上调用）
    void on_packet(const ip_packet_info &ip, const uint8_t *payload,
                   size_t len);

    // 等待新连接；完成回调签名 void(error_code, shared_ptr<tcp_flow>)
    template <typename Handler> void async_accept(Handler handler);
    void cancel_accepts();
    void close_all();
    void start_sweep();
    size_t flow_count() const
    {
        return flows_.size();
    }

    net::any_io_executor strand() const
    {
        return strand_;
    }
    device_writer &writer()
    {
        return writer_;
    }
    engine_stats &stats()
    {
        return stats_;
    }
    buffer_accountant &account()
    {
        return *account_;
    }
    size_t mss(int family) const
    {
        return family == 6 ? mss6_ : mss4_;
    }
    size_t max_tx_queue() const
    {
        return cfg_.max_tx_queue_per_flow;
    }

    // ---- 由 tcp_flow 调用的发送辅助 ----
    void send_segment(tcp_flow &f, uint32_t seq, uint8_t flags,
                      const uint8_t *payload, size_t len, bool with_mss);
    void send_fin(tcp_flow &f);
    void abort_flow(tcp_flow &f);
    void close_flow(tcp_flow &f, const boost::system::error_code &err);

private:
    friend struct tcp_flow;
    template <typename Handler>
    friend void tcp_flow_start_read(std::shared_ptr<tcp_flow>,
                                    std::vector<net::mutable_buffer>, size_t,
                                    Handler);
    template <typename Handler>
    friend void tcp_flow_start_write(std::shared_ptr<tcp_flow>,
                                     std::vector<net::const_buffer>, size_t,
                                     Handler);

    void handle_segment(const std::shared_ptr<tcp_flow> &f,
                        const tcp_header &th, const uint8_t *data,
                        size_t data_len);
    void send_ack(tcp_flow &f);
    void defer_ack(tcp_flow &f);
    void on_ack_timer(const boost::system::error_code &ec);
    void deliver_data(tcp_flow &f, const uint8_t *data, size_t len);
    void flush_reads(tcp_flow &f);
    void flush_writes(tcp_flow &f);
    void notify_accept(tcp_flow &f);
    void on_sweep(const boost::system::error_code &ec);

    net::any_io_executor strand_;
    device_writer &writer_;
    tun_config cfg_;
    engine_stats &stats_;
    std::shared_ptr<buffer_accountant> account_;
    size_t mss4_ = 536;  // IPv4 MSS = MTU - 20(IP) - 20(TCP)
    size_t mss6_ = 1220; // IPv6 MSS = MTU - 40(IP) - 20(TCP)

    std::unordered_map<five_tuple, std::shared_ptr<tcp_flow>> flows_;
    std::deque<net::any_completion_handler<void(boost::system::error_code,
                                                std::shared_ptr<tcp_flow>)>>
        pending_accepts_;
    std::deque<std::shared_ptr<tcp_flow>> pending_flows_;
    net::steady_timer sweep_timer_;
    net::steady_timer ack_timer_;
    std::deque<std::shared_ptr<tcp_flow>> ack_deferred_;
    bool ack_timer_waiting_ = false;
};

// ---- 供 tun_stream 调用的入口（内部自动派发到 Strand）----
template <typename Handler>
void tcp_flow_start_read(std::shared_ptr<tcp_flow> flow,
                         std::vector<net::mutable_buffer> buffers, size_t total,
                         Handler handler)
{
    if (!flow) {
        handler(boost::system::error_code(net::error::bad_descriptor), 0);
        return;
    }
    auto eng = flow->eng.lock();
    if (!eng) {
        handler(boost::system::error_code(net::error::bad_descriptor), 0);
        return;
    }
    auto strand = eng->strand();
    net::dispatch(strand, [f = std::move(flow), eng,
                           buffers = std::move(buffers), total,
                           handler = std::move(handler)]() mutable {
        auto &flow = *f;
        if (flow.state == tcp_state::CLOSED || flow.app_closed ||
            flow.rx_shutdown) {
            handler(boost::system::error_code(net::error::bad_descriptor), 0);
            return;
        }
        if (flow.rst) {
            handler(boost::system::error_code(net::error::connection_reset), 0);
            return;
        }
        if (total == 0) {
            handler(boost::system::error_code{}, 0);
            return;
        }
        flow.pending_reads.push_back(
            {std::move(buffers), total, std::move(handler)});
        eng->flush_reads(flow);
    });
}

template <typename Handler>
void tcp_flow_start_write(std::shared_ptr<tcp_flow> flow,
                          std::vector<net::const_buffer> buffers, size_t total,
                          Handler handler)
{
    if (!flow) {
        handler(boost::system::error_code(net::error::bad_descriptor), 0);
        return;
    }
    auto eng = flow->eng.lock();
    if (!eng) {
        handler(boost::system::error_code(net::error::bad_descriptor), 0);
        return;
    }
    auto strand = eng->strand();
    net::dispatch(strand, [f = std::move(flow), eng,
                           buffers = std::move(buffers), total,
                           handler = std::move(handler)]() mutable {
        auto &flow = *f;
        if (flow.state == tcp_state::CLOSED || flow.app_closed ||
            flow.fin_sent) {
            handler(boost::system::error_code(net::error::bad_descriptor), 0);
            return;
        }
        if (flow.rst) {
            handler(boost::system::error_code(net::error::connection_reset), 0);
            return;
        }
        if (total == 0) {
            handler(boost::system::error_code{}, 0);
            return;
        }
        if (flow.tx_bytes + total > eng->max_tx_queue()) {
            // 发送队列积压超限：拒绝本次写入以施加背压，避免内存无限增长
            handler(boost::system::error_code(net::error::no_buffer_space), 0);
            return;
        }
        flow.pending_writes.push_back(
            {std::move(buffers), total, 0, std::move(handler)});
        flow.tx_bytes += total;
        eng->flush_writes(flow);
    });
}

template <typename Handler> void tcp_engine::async_accept(Handler handler)
{
    while (!pending_flows_.empty()) {
        auto f = std::move(pending_flows_.front());
        pending_flows_.pop_front();
        if (f->state == tcp_state::CLOSED) {
            continue;
        }
        f->accepted = true;
        handler(boost::system::error_code{}, std::move(f));
        return;
    }
    pending_accepts_.push_back(std::move(handler));
}

void tcp_flow_shutdown_send(std::shared_ptr<tcp_flow> flow);
void tcp_flow_shutdown_receive(std::shared_ptr<tcp_flow> flow);
void tcp_flow_close(std::shared_ptr<tcp_flow> flow);
void tcp_flow_reset(std::shared_ptr<tcp_flow> flow);
bool tcp_flow_is_open(const std::shared_ptr<tcp_flow> &flow);

} // namespace detail
} // namespace tunio
