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
#include "tunio/packet_buffer.hpp"
#include "tunio/tun_config.hpp"

#include <boost/asio.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace tunio {
namespace net = boost::asio;

// 通用缓冲区序列：单缓冲区（最常见调用形式）由 small_vector 栈上存储，
// 避免堆分配；兼容单缓冲区与缓冲区序列两种调用形式。
using mutable_buffer_sequence =
    boost::container::small_vector<net::mutable_buffer, 1>;
using const_buffer_sequence =
    boost::container::small_vector<net::const_buffer, 1>;

namespace detail {

class device_writer;

// ---- TCP 引擎常用类型 ----
// 时间统一采用单调时钟（steady_clock），不受系统时间调整影响
using tcp_clock = std::chrono::steady_clock;
using tcp_time_point = tcp_clock::time_point;
// TCP 读/写完成处理器（与 Boost.Asio 异步操作完成签名一致）
using tcp_read_handler =
    net::any_completion_handler<void(boost::system::error_code, size_t)>;
using tcp_write_handler = tcp_read_handler;

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
    SYN_RCVD,     // 收到客户端 SYN, 尚未回复 SYN+ACK (等待 accept/reject)
    SYN_ACK_SENT, // 已回复 SYN+ACK, 等待客户端 ACK
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
    // 接收窗口（未缩放字节数）：1MB，与 gVisor 默认接收缓冲对齐。大窗口
    // 使内核发送方不被 rwnd 限制在 64KB，上传吞吐不再受 窗口/RTT 约束.
    static constexpr uint32_t fixed_rcv_wnd = 1048576;
    // 本端通告的 Window Scale（RFC 7323）：1MB 需 scale=7（1<<7 对齐）
    static constexpr uint8_t k_rcv_wnd_scale = 7;

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
    uint32_t peer_wnd = 0; // 客户端通告的接收窗口（按对端 scale 放大后的实际值）
    // RFC 7323 §2.2 两方向缩放独立：解释对端窗口字段时左移对端通告原值
    uint8_t snd_wnd_scale = 0;
    // 本端通告的窗口缩放：广告窗口字段时右移本端通告值（对端通告 WS 时为 7）
    uint8_t rcv_wnd_scale = 0;
    bool wscale_ok = false; // 对端 SYN 通告了有效 WS 选项（值 0-14）
    uint32_t last_wnd_advertised = 0; // 最近通告的接收窗口（未缩放），窗口恢复检测
    bool fin_sent = false;
    bool fin_received = false;
    bool fin_pending = false; // 应用已请求关闭发送侧，但仍有未发送/未确认
                              // 数据：FIN 推迟到数据全部确认后再发送.
    bool rst = false;         // 收到 RST 或主动 RST
    bool app_closed = false;  // 应用层已关闭
    bool rx_shutdown = false; // 应用层已关闭接收侧
    bool accepted = false;    // 已交付给 accept
    bool probe_in_flight = false; // 零窗口探测已发出、尚未被对端确认
    tcp_time_point created_at;
    tcp_time_point destroy_at;
    // 关闭流程开始时间（发送 FIN 时记录），用于 FIN_WAIT/LAST_ACK 强制清理
    // 超时计时；避免以连接创建时间为基准导致长连接关闭时立即被清理。
    tcp_time_point close_started_at;

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

    // ---- 乱序重排缓存（按 seq 升序的段表）----
    // 并发读（多 slot 读 TUN）或链路抖动会引入乱序段：超前 seq 的段
    // 先缓存，缺失段到达后按序交付，避免依赖对端重传造成吞吐损失。
    struct ooo_segment
    {
        std::vector<uint8_t> data;
        bool fin = false; // 段携带 FIN（FIN 占一个序列号，交付后处理）
    };
    std::map<uint32_t, ooo_segment> ooo_cache;
    size_t ooo_bytes = 0; // 缓存数据字节数（限额与记账）
    size_t ooo_count = 0; // 缓存段数

    // ---- 挂起读操作（单读模型：同一时刻至多一个未完成读）----
    struct read_op
    {
        // 小缓冲优化：单缓冲区（最常见调用形式）在栈上存储，避免堆分配
        mutable_buffer_sequence buffers;
        size_t total = 0;
        tcp_read_handler handler;
    };
    std::optional<read_op> active_read;

    // ---- 挂起写操作（单写模型：同一时刻至多一个未完成写）----
    struct write_op
    {
        const_buffer_sequence buffers; // 用户数据引用，回调 handler 前由调用方保证有效
        size_t total = 0;              // 待发送总字节数（buffers 求和）
        size_t offset = 0;
        size_t buf_index = 0; // 当前发送位置所在缓冲区下标（增量推进，避免每段重扫）
        size_t buf_off = 0;   // 当前发送位置在 buf_index 缓冲区内的偏移
        uint32_t start_seq = 0; // 本次写操作首字节的发送序号（snd_nxt 快照）,
                                // 用于将未确认序列号范围映射回缓冲偏移以重传
        tcp_write_handler handler;
    };
    std::optional<write_op> active_write;
    // 窗口可写信号：发送协程在窗口耗尽时挂起等待，ACK 更新窗口后由引擎唤醒.
    std::optional<net::experimental::channel<void(boost::system::error_code)>>
        write_ch;

    net::ip::tcp::endpoint original_destination() const;
    bool is_open() const;
};

// 流指针 / accept 完成处理器 / 流表（依赖 tcp_flow 定义）
using tcp_flow_ptr = std::shared_ptr<tcp_flow>;
using tcp_accept_handler =
    net::any_completion_handler<void(boost::system::error_code,
        tcp_flow_ptr)>;
using tcp_flow_map =
    boost::unordered_flat_map<five_tuple, tcp_flow_ptr,
        std::hash<five_tuple>>;

template <typename Handler>
void tcp_flow_start_read(tcp_flow_ptr flow, mutable_buffer_sequence buffers,
    size_t total, Handler handler);
template <typename Handler>
void tcp_flow_start_write(tcp_flow_ptr flow, const_buffer_sequence buffers,
    size_t total, Handler handler);

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
    // ---- 由 tcp_flow 调用的发送辅助 ----
    packet_buffer build_segment(tcp_flow &f, uint32_t seq, uint8_t flags,
        const uint8_t *payload, size_t len, bool with_mss);
    void send_segment(tcp_flow &f, uint32_t seq, uint8_t flags,
        const uint8_t *payload, size_t len, bool with_mss);
    void send_fin(tcp_flow &f);
    void abort_flow(tcp_flow &f);
    void close_flow(tcp_flow &f, const boost::system::error_code &err);
    // 重传未确认数据 [snd_una, snd_nxt) 至多 max_bytes 字节（零窗口探测
    // 与 RTO 超时共用）；数据仍在 active_write 的用户缓冲中，无需拷贝.
    void retransmit_unacked(tcp_flow &f, size_t max_bytes);
    // 发送协程：持有单个写操作，按窗口/MSS 循环分片，
    // 设备写完成回调驱动背压；窗口耗尽时经 write_ch 挂起等待 ACK.
    net::awaitable<void> write_loop(tcp_flow_ptr f);
    void signal_write(tcp_flow &f);

    // 批准握手: 向客户端回复 SYN+ACK (幂等, 已回复过则忽略).
    void accept_flow(tcp_flow &f);
    // 拒绝握手: 立即向客户端发送 RST (幂等).
    void reject_flow(tcp_flow &f);

private:
    friend struct tcp_flow;
    template <typename Handler>
    friend void tcp_flow_start_read(tcp_flow_ptr, mutable_buffer_sequence,
        size_t, Handler);
    template <typename Handler>
    friend void tcp_flow_start_write(tcp_flow_ptr, const_buffer_sequence,
        size_t, Handler);

    void handle_segment(const tcp_flow_ptr &f, const tcp_header &th,
        const uint8_t *data, size_t data_len);
    void send_ack(tcp_flow &f);
    uint32_t current_wnd(const tcp_flow &f) const;
    void notify_window_updated(tcp_flow &f);
    void deliver_data(tcp_flow &f, const uint8_t *data, size_t len);
    void flush_reads(tcp_flow &f);
    void flush_ooo(tcp_flow &f);
    bool ooo_append(tcp_flow &f, uint32_t seq, const uint8_t *data,
        size_t len, bool fin);
    void handle_fin(tcp_flow &f, uint32_t fin_seq);
    void notify_accept(tcp_flow &f);
    void on_sweep(const boost::system::error_code &ec);

    net::any_io_executor strand_;
    device_writer &writer_;
    tun_config cfg_;
    engine_stats &stats_;
    std::shared_ptr<buffer_accountant> account_;
    size_t mss4_ = 536;  // IPv4 MSS = MTU - 20(IP) - 20(TCP)
    size_t mss6_ = 1220; // IPv6 MSS = MTU - 40(IP) - 20(TCP)

    tcp_flow_map flows_;
    std::deque<tcp_accept_handler> pending_accepts_;
    std::deque<tcp_flow_ptr> pending_flows_;
    net::steady_timer sweep_timer_;
};

// ---- 供 tun_tcp_socket 调用的入口（内部自动派发到 Strand）----
template <typename Handler>
void tcp_flow_start_read(tcp_flow_ptr flow, mutable_buffer_sequence buffers,
    size_t total, Handler handler)
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
        if (flow.state == tcp_state::SYN_RCVD) {
            // 握手尚未批准: 首次读视为隐式 accept, 回复 SYN+ACK.
            eng->accept_flow(flow);
        }
        if (total == 0) {
            handler(boost::system::error_code{}, 0);
            return;
        }
        if (flow.active_read) {
            // 单读模型：上一读操作尚未完成，拒绝重叠读以施加背压
            handler(boost::system::error_code(net::error::no_buffer_space), 0);
            return;
        }
        flow.active_read = tcp_flow::read_op{std::move(buffers), total,
            std::move(handler)};
        eng->flush_reads(flow);
    });
}

template <typename Handler>
void tcp_flow_start_write(tcp_flow_ptr flow, const_buffer_sequence buffers,
    size_t total, Handler handler)
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
        handler = std::move(handler), strand]() mutable {
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
        if (flow.state == tcp_state::SYN_RCVD) {
            // 握手尚未批准: 首次写视为隐式 accept, 回复 SYN+ACK.
            eng->accept_flow(flow);
        }
        if (total == 0) {
            handler(boost::system::error_code{}, 0);
            return;
        }
        if (flow.active_write) {
            // 单写模型：上一写操作尚未完成，拒绝重叠写以施加背压
            handler(boost::system::error_code(net::error::no_buffer_space), 0);
            return;
        }
        flow.active_write = tcp_flow::write_op{
            std::move(buffers), total, 0, 0, 0, 0, std::move(handler)};
        net::co_spawn(strand, eng->write_loop(f), net::detached);
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

void tcp_flow_shutdown_send(tcp_flow_ptr flow);
void tcp_flow_shutdown_receive(tcp_flow_ptr flow);
void tcp_flow_close(tcp_flow_ptr flow);
void tcp_flow_reset(tcp_flow_ptr flow);
void tcp_flow_accept(tcp_flow_ptr flow);
void tcp_flow_reject(tcp_flow_ptr flow);
bool tcp_flow_is_open(const tcp_flow_ptr &flow);

} // namespace detail
} // namespace tunio
