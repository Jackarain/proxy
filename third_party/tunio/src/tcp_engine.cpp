//
// tcp_engine.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tcp_engine.hpp"

#include "device_writer.hpp"

#include <boost/asio.hpp>

#include <algorithm>
#include <cstring>
#include <random>
#include <utility>
#include <vector>

namespace tunio {
namespace detail {

namespace {

uint32_t random_iss()
{
    // thread_local：多个引擎（不同线程）并发时避免共享 RNG 的数据竞争
    static thread_local std::mt19937 rng{std::random_device{}()};
    return rng();
}

} // namespace

tcp_engine::tcp_engine(net::any_io_executor strand, device_writer &writer,
                       const tun_config &cfg, engine_stats &stats,
                       std::shared_ptr<buffer_accountant> account)
    : strand_(std::move(strand))
    , writer_(writer)
    , cfg_(cfg)
    , stats_(stats)
    , account_(std::move(account))
    , mss4_(cfg.mtu > 40 ? cfg.mtu - 40 : 536)
    , mss6_(cfg.mtu > 60 ? cfg.mtu - 60 : 1220)
    , sweep_timer_(strand_)
    , ack_timer_(strand_)
{
}

tcp_engine::~tcp_engine()
{
    sweep_timer_.cancel();
    ack_timer_.cancel();
}

void tcp_engine::start_sweep()
{
    sweep_timer_.expires_after(std::chrono::seconds(1));
    // 定时器以引擎 Strand 构造，完成回调已在 Strand 上，无需再派发
    sweep_timer_.async_wait(
        [self = shared_from_this()](const boost::system::error_code &ec) {
            self->on_sweep(ec);
        });
}

void tcp_engine::on_sweep(const boost::system::error_code &ec)
{
    if (ec) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    std::vector<std::shared_ptr<tcp_flow>> victims;
    for (const auto &[key, f] : flows_) {
        (void)key;
        if ((f->state == tcp_state::SYN_RCVD ||
             f->state == tcp_state::SYN_ACK_SENT) &&
            now - f->created_at > cfg_.tcp_syn_timeout) {
            // 未完成握手的半开连接，超时后清理
            victims.push_back(f);
        } else if (f->state == tcp_state::ESTABLISHED && !f->accepted &&
                   now - f->created_at > cfg_.tcp_accept_timeout) {
            // 应用层长期未通过 async_accept 领取的连接：发送 RST
            // 通知客户端后回收
            victims.push_back(f);
        } else if (f->state == tcp_state::TIME_WAIT && now >= f->destroy_at) {
            victims.push_back(f);
        } else if ((f->state == tcp_state::FIN_WAIT_1 ||
                    f->state == tcp_state::FIN_WAIT_2 ||
                    f->state == tcp_state::LAST_ACK) &&
                   now - f->close_started_at > cfg_.tcp_close_timeout) {
            // 关闭流程长期未完成（对端未确认 FIN / 未回复 FIN），超时后强制清理
            victims.push_back(f);
        }
    }
    for (auto &f : victims) {
        if (f->state == tcp_state::SYN_RCVD ||
            f->state == tcp_state::SYN_ACK_SENT ||
            (f->state == tcp_state::ESTABLISHED && !f->accepted)) {
            // 半开连接或未被领取的连接: 发 RST 通知客户端后回收.
            abort_flow(*f);
        } else {
            close_flow(*f, net::error::operation_aborted);
        }
    }
    start_sweep();
}

void tcp_engine::on_packet(const ip_packet_info &ip, const uint8_t *payload,
                           size_t len)
{
    if (len < sizeof(tcp_header)) {
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    tcp_header th;
    std::memcpy(&th, payload, sizeof(th));

    // 校验 TCP 校验和
    if (tcp_udp_checksum(ip.family, ip.src_ip, ip.dst_ip, IPPROTO_TCP_V,
                         payload, len) != 0) {
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // 校验 TCP 头长（新建流与既有流统一前置检查）
    const size_t hlen = th.header_len();
    if (hlen < sizeof(tcp_header) || hlen > len) {
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const bool is_syn = (th.flags & TCP_SYN) != 0 && (th.flags & TCP_ACK) == 0;
    const five_tuple key =
        make_five_tuple(ip.src_ip, ip.dst_ip, th.src_port, th.dst_port,
                        IPPROTO_TCP_V, ip.family);

    auto it = flows_.find(key);
    if (it == flows_.end()) {
        if (!is_syn) {
            // 未知流且非 SYN：丢弃
            return;
        }
        if (flows_.size() >= cfg_.max_tcp_flows) {
            stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        auto f = std::make_shared<tcp_flow>();
        f->key = key;
        f->eng = shared_from_this();
        f->irs = ntohl(th.seq);
        f->iss = random_iss();
        f->rcv_nxt = f->irs + 1;
        f->snd_nxt = f->iss;
        f->snd_una = f->iss;
        f->state = tcp_state::SYN_RCVD;
        f->peer_wnd = ntohs(th.window);
        f->created_at = std::chrono::steady_clock::now();
        flows_.emplace(key, f);
        // 延迟握手: 不立即回复 SYN+ACK, 交给 async_accept 领取后由
        // 应用 accept()/reject() 或首次读写（隐式批准）决定握手结果.
        notify_accept(*f);
        return;
    }
    // 持有强引用：handle_segment 内会内联调用用户完成回调（当流绑定引擎
    // Strand 时），回调中 reset() 后销毁流可能擦除 flows_ 并释放最后引用；
    // 强引用保证回调返回后 f 仍然有效，避免 use-after-free。
    std::shared_ptr<tcp_flow> f = it->second;

    const uint8_t *data = payload + hlen;
    size_t data_len = len - hlen;
    handle_segment(f, th, data, data_len);
}

void tcp_engine::handle_segment(const std::shared_ptr<tcp_flow> &f,
                                const tcp_header &th, const uint8_t *data,
                                size_t data_len)
{
    const uint32_t seq = ntohl(th.seq);
    const uint32_t ack = ntohl(th.ack);
    const uint8_t flags = th.flags;
    const uint16_t wnd = ntohs(th.window);

    // ---- ACK 与窗口更新 ----
    if (flags & TCP_ACK) {
        if (seq_gt(ack, f->snd_una) && seq_ge(f->snd_nxt, ack)) {
            f->snd_una = ack;
        }
        if (f->state == tcp_state::FIN_WAIT_1 && f->fin_sent &&
            seq_ge(ack, f->snd_nxt)) {
            // 客户端确认了我们的 FIN
            f->state =
                f->fin_received ? tcp_state::TIME_WAIT : tcp_state::FIN_WAIT_2;
            if (f->state == tcp_state::TIME_WAIT) {
                f->destroy_at = std::chrono::steady_clock::now() +
                                cfg_.tcp_time_wait_timeout;
            }
        } else if (f->state == tcp_state::LAST_ACK && f->fin_sent &&
                   seq_ge(ack, f->snd_nxt)) {
            close_flow(*f, boost::system::error_code{});
            return;
        }
    }
    f->peer_wnd = wnd;

    if (flags & TCP_RST) {
        f->rst = true;
        close_flow(*f, net::error::connection_reset);
        return;
    }

    if (f->state == tcp_state::CLOSED) {
        return;
    }

    // ---- 握手状态 ----
    if (f->state == tcp_state::SYN_RCVD) {
        // 尚未批准握手: 客户端重传 SYN 忽略, 等应用决定后回 SYN+ACK
        // (accept/隐式批准) 或 RST (reject).
        return;
    }
    if (f->state == tcp_state::SYN_ACK_SENT) {
        if ((flags & TCP_SYN) && seq == f->irs) {
            // 客户端重传 SYN：重新发送 SYN-ACK
            send_segment(*f, f->iss, TCP_SYN | TCP_ACK, nullptr, 0, true);
            return;
        }
        if ((flags & TCP_ACK) && ack == f->iss + 1) {
            f->state = tcp_state::ESTABLISHED;
            stats_.tcp_connections.fetch_add(1, std::memory_order_relaxed);
            // 流已在收到 SYN 时交付给 async_accept, 不再重复通知.
            if (!f->accepted) {
                notify_accept(*f);
            }
            if (data_len > 0 && seq == f->rcv_nxt) {
                deliver_data(*f, data, data_len);
            }
        }
        return;
    }

    // ---- 已建立连接的数据处理 ----
    if (data_len > 0) {
        if (seq == f->rcv_nxt) {
            deliver_data(*f, data, data_len);
        } else if (seq_gt(seq, f->rcv_nxt)) {
            // 超前序列号：不缓存，发送 Dup-ACK 触发对端快速重传
            send_ack(*f);
            stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        } else {
            // 重复或已接收段：静默丢弃
            stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // ---- FIN ----
    if (flags & TCP_FIN) {
        // FIN 序号 = seq + data_len（FIN 消耗一个序列号，可与数据同段）
        const uint32_t fin_seq = seq + static_cast<uint32_t>(data_len);
        if (fin_seq == f->rcv_nxt) {
            if (!f->fin_received) {
                f->rcv_nxt += 1;
                f->fin_received = true;
            }
            send_ack(*f);
            switch (f->state) {
            case tcp_state::ESTABLISHED:
                f->state = tcp_state::CLOSE_WAIT;
                break;
            case tcp_state::FIN_WAIT_1:
                // 等待客户端 ACK 我们的 FIN（ACK 分支推进到 FIN_WAIT_2 /
                // TIME_WAIT）
                break;
            case tcp_state::FIN_WAIT_2:
                f->state = tcp_state::TIME_WAIT;
                f->destroy_at = std::chrono::steady_clock::now() +
                                cfg_.tcp_time_wait_timeout;
                break;
            case tcp_state::TIME_WAIT:
                break;
            default:
                break;
            }
            flush_reads(*f);
        } else if (f->fin_received && fin_seq == f->rcv_nxt - 1) {
            // 对端重传 FIN：重新确认（避免客户端长时间重复重传）
            send_ack(*f);
        }
    }

    flush_writes(*f);
}

void tcp_engine::deliver_data(tcp_flow &f, const uint8_t *data, size_t len)
{
    if (len == 0) {
        return;
    }
    if (f.rx_bytes + len > cfg_.max_rx_queue_per_flow ||
        !account_->reserve(len)) {
        // 队列积压或总缓冲超限：静默丢弃，不回复 ACK 以施加背压
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    // 直投：接收缓冲无积压且用户读操作等待时，跳过 rx_data 中间缓冲，
    // 直接拷入用户缓冲，减少一次全量 memcpy。
    if (f.rx_data.empty() && !f.pending_reads.empty()) {
        auto op = std::move(f.pending_reads.front());
        f.pending_reads.pop_front();
        const size_t n = std::min(op.total, len);
        size_t copied = 0;
        for (auto &buf : op.buffers) {
            if (copied >= n) {
                break;
            }
            const size_t take = std::min(buf.size(), n - copied);
            std::memcpy(buf.data(), data + copied, take);
            copied += take;
        }
        f.rcv_nxt += static_cast<uint32_t>(len);
        account_->release(n);
        op.handler(boost::system::error_code{}, n);
        if (n < len) {
            // 剩余数据缓存，等待后续读操作消费
            f.rx_data.insert(f.rx_data.end(), data + n, data + len);
            f.rx_bytes += len - n;
            flush_reads(f);
        }
        if (++f.ack_pending >= 2) {
            send_ack(f);
            f.ack_pending = 0;
        } else {
            defer_ack(f);
        }
        return;
    }
    f.rx_data.insert(f.rx_data.end(), data, data + len);
    f.rx_bytes += len;
    f.rcv_nxt += static_cast<uint32_t>(len);
    // delayed ACK：每 2 个数据段确认一次；单段交由 40ms 定时器兜底，
    // 期间引擎发送的任何段都会捎带最新 rcv_nxt（见 send_segment）。
    if (++f.ack_pending >= 2) {
        send_ack(f);
        f.ack_pending = 0;
    } else {
        defer_ack(f);
    }
    flush_reads(f);
}

void tcp_engine::flush_reads(tcp_flow &f)
{
    while (!f.pending_reads.empty() && f.rx_bytes > 0) {
        auto op = std::move(f.pending_reads.front());
        f.pending_reads.pop_front();
        const size_t n = std::min(op.total, f.rx_bytes);
        size_t copied = 0;
        for (auto &buf : op.buffers) {
            if (copied >= n) {
                break;
            }
            const size_t take = std::min(buf.size(), n - copied);
            std::memcpy(buf.data(), f.rx_data.data() + f.rx_head + copied,
                        take);
            copied += take;
        }
        f.rx_head += copied;
        f.rx_bytes -= copied;
        account_->release(copied);
        op.handler(boost::system::error_code{}, copied);
    }
    // 头部偏移过大时压缩连续缓冲，保持内存占用与 cache 友好
    if (f.rx_head > 0 &&
        (f.rx_head == f.rx_data.size() || f.rx_head >= 65536)) {
        f.rx_data.erase(f.rx_data.begin(),
                        f.rx_data.begin() +
                            static_cast<std::ptrdiff_t>(f.rx_head));
        f.rx_head = 0;
    }
    // 数据耗尽后处理 EOF
    if (f.rx_bytes == 0 && f.fin_received) {
        while (!f.pending_reads.empty()) {
            auto op = std::move(f.pending_reads.front());
            f.pending_reads.pop_front();
            op.handler(boost::system::error_code{}, 0);
        }
    }
}

void tcp_engine::flush_writes(tcp_flow &f)
{
    while (!f.pending_writes.empty()) {
        auto &op = f.pending_writes.front();
        const uint32_t in_flight = f.snd_nxt - f.snd_una;
        if (in_flight >= f.peer_wnd) {
            // 窗口耗尽：等待客户端 ACK 更新窗口
            break;
        }
        // 定位 op.offset 对应的用户缓冲区及其区内偏移
        size_t buf_index = 0;
        size_t buf_off = op.offset;
        while (buf_index < op.buffers.size() &&
               buf_off >= op.buffers[buf_index].size()) {
            buf_off -= op.buffers[buf_index].size();
            ++buf_index;
        }
        if (buf_index == op.buffers.size()) {
            break;
        }
        const size_t remaining = op.total - op.offset;
        const size_t avail = op.buffers[buf_index].size() - buf_off;
        const size_t chunk =
            std::min({remaining, avail, mss(f.key.family),
                      static_cast<size_t>(f.peer_wnd - in_flight)});
        if (chunk == 0) {
            break;
        }
        const uint8_t *payload = static_cast<const uint8_t *>(
                                     op.buffers[buf_index].data()) +
                                 buf_off;
        send_segment(f, f.snd_nxt, TCP_ACK | TCP_PSH,
                     payload, chunk, false);
        f.snd_nxt += static_cast<uint32_t>(chunk);
        f.tx_bytes -= chunk;
        op.offset += chunk;
        if (op.offset == op.total) {
            auto h = std::move(op.handler);
            f.pending_writes.pop_front();
            h(boost::system::error_code{}, op.total);
        }
    }
}

void tcp_engine::send_segment(tcp_flow &f, uint32_t seq, uint8_t flags,
                              const uint8_t *payload, size_t len, bool with_mss)
{
    // 任何段都携带最新 rcv_nxt，视为已完成一次数据确认
    f.ack_pending = 0;
    const int family = f.key.family;
    const size_t ip_hdr_len = ip_header_size(family);
    const size_t tcp_hdr_len = with_mss ? 24 : 20;
    const size_t total = ip_hdr_len + tcp_hdr_len + len;
    packet_buffer pkt = writer_.acquire(cfg_.mtu + 64, 64);
    pkt.resize(total);
    uint8_t *base = pkt.data();

    build_ip_header(base, family, f.key.dst_ip.data(), f.key.src_ip.data(),
                    IPPROTO_TCP_V, total, writer_.alloc_ip_id());

    auto *th = reinterpret_cast<tcp_header *>(base + ip_hdr_len);
    th->src_port = f.key.dst_port;
    th->dst_port = f.key.src_port;
    th->seq = htonl(seq);
    th->ack = htonl(f.rcv_nxt);
    th->data_offset = static_cast<uint8_t>((with_mss ? 6 : 5) << 4);
    th->flags = flags;
    th->window = htons(tcp_flow::fixed_rcv_wnd);
    th->checksum = 0;
    th->urgent = 0;

    if (with_mss) {
        uint8_t *opt = base + ip_hdr_len + 20;
        opt[0] = 2; // kind = MSS
        opt[1] = 4; // len = 4
        const size_t mss = this->mss(family);
        opt[2] = static_cast<uint8_t>(mss >> 8);
        opt[3] = static_cast<uint8_t>(mss & 0xff);
    }
    if (len > 0) {
        std::memcpy(base + ip_hdr_len + tcp_hdr_len, payload, len);
    }
    th->checksum = htons(
        tcp_udp_checksum(family, f.key.dst_ip.data(), f.key.src_ip.data(),
                         IPPROTO_TCP_V, base + ip_hdr_len, tcp_hdr_len + len));

    writer_.async_write_and_forget(std::move(pkt));
}

void tcp_engine::send_ack(tcp_flow &f)
{
    send_segment(f, f.snd_nxt, TCP_ACK, nullptr, 0, false);
}

void tcp_engine::defer_ack(tcp_flow &f)
{
    if (f.ack_deferred) {
        return;
    }
    f.ack_deferred = true;
    ack_deferred_.push_back(f.shared_from_this());
    if (ack_timer_waiting_) {
        return;
    }
    ack_timer_waiting_ = true;
    ack_timer_.expires_after(std::chrono::milliseconds(40));
    // 定时器以引擎 Strand 构造，完成回调已在 Strand 上，无需再派发
    ack_timer_.async_wait(
        [self = shared_from_this()](const boost::system::error_code &ec) {
            self->on_ack_timer(ec);
        });
}

void tcp_engine::on_ack_timer(const boost::system::error_code &ec)
{
    ack_timer_waiting_ = false;
    if (ec) {
        return;
    }
    std::deque<std::shared_ptr<tcp_flow>> deferred;
    deferred.swap(ack_deferred_);
    for (auto &f : deferred) {
        f->ack_deferred = false;
        if (f->state == tcp_state::CLOSED || f->ack_pending == 0) {
            continue;
        }
        send_ack(*f);
        f->ack_pending = 0;
    }
}

void tcp_engine::send_fin(tcp_flow &f)
{
    if (f.fin_sent || f.state == tcp_state::CLOSED) {
        return;
    }
    f.fin_sent = true;
    switch (f.state) {
    case tcp_state::ESTABLISHED:
        f.state = tcp_state::FIN_WAIT_1;
        f.close_started_at = std::chrono::steady_clock::now();
        send_segment(f, f.snd_nxt, TCP_ACK | TCP_FIN, nullptr, 0, false);
        f.snd_nxt += 1;
        break;
    case tcp_state::CLOSE_WAIT:
        f.state = tcp_state::LAST_ACK;
        f.close_started_at = std::chrono::steady_clock::now();
        send_segment(f, f.snd_nxt, TCP_ACK | TCP_FIN, nullptr, 0, false);
        f.snd_nxt += 1;
        break;
    case tcp_state::SYN_RCVD:
        // 握手尚未批准：直接关闭，不发送 FIN
        close_flow(f, net::error::operation_aborted);
        break;
    case tcp_state::SYN_ACK_SENT:
        // 已回 SYN+ACK 但未完成握手：RST 告知客户端连接被放弃
        abort_flow(f);
        break;
    default:
        break;
    }
}

void tcp_engine::accept_flow(tcp_flow &f)
{
    if (f.state != tcp_state::SYN_RCVD) {
        // 幂等: 已回复过 SYN+ACK 或已关闭时忽略多余的 accept.
        return;
    }
    // 回复 SYN-ACK（携带 MSS 选项）
    send_segment(f, f.iss, TCP_SYN | TCP_ACK, nullptr, 0, true);
    f.snd_nxt = f.iss + 1; // SYN 消耗一个序号
    f.state = tcp_state::SYN_ACK_SENT;
}

void tcp_engine::reject_flow(tcp_flow &f)
{
    abort_flow(f); // 幂等: CLOSED 时忽略; 发 RST 并关闭
}

void tcp_engine::abort_flow(tcp_flow &f)
{
    if (f.state == tcp_state::CLOSED) {
        return;
    }
    // RST 段：seq = snd_nxt, ack = rcv_nxt
    send_segment(f, f.snd_nxt, TCP_RST | TCP_ACK, nullptr, 0, false);
    f.rst = true;
    close_flow(f, net::error::connection_reset);
}

void tcp_engine::close_flow(tcp_flow &f, const boost::system::error_code &err)
{
    if (f.state == tcp_state::CLOSED) {
        return;
    }
    for (auto &op : f.pending_reads) {
        op.handler(err, 0);
    }
    f.pending_reads.clear();
    for (auto &op : f.pending_writes) {
        op.handler(err, 0);
    }
    f.pending_writes.clear();
    f.tx_bytes = 0;
    if (f.rx_bytes > 0) {
        account_->release(f.rx_bytes);
        f.rx_bytes = 0;
    }
    f.rx_data.clear();
    f.rx_head = 0;
    if (f.state != tcp_state::SYN_RCVD &&
        f.state != tcp_state::SYN_ACK_SENT) {
        stats_.tcp_connections.fetch_sub(1, std::memory_order_relaxed);
    }
    f.state = tcp_state::CLOSED;
    flows_.erase(f.key);
}

void tcp_engine::notify_accept(tcp_flow &f)
{
    if (!pending_accepts_.empty()) {
        auto h = std::move(pending_accepts_.front());
        pending_accepts_.pop_front();
        f.accepted = true;
        h(boost::system::error_code{}, f.shared_from_this());
    } else {
        pending_flows_.push_back(f.shared_from_this());
    }
}

void tcp_engine::cancel_accepts()
{
    while (!pending_accepts_.empty()) {
        auto h = std::move(pending_accepts_.front());
        pending_accepts_.pop_front();
        h(boost::system::error_code(net::error::operation_aborted), nullptr);
    }
}

void tcp_engine::close_all()
{
    sweep_timer_.cancel();
    ack_timer_.cancel();
    ack_timer_waiting_ = false;
    ack_deferred_.clear();
    std::vector<std::shared_ptr<tcp_flow>> all;
    all.reserve(flows_.size());
    for (auto &[key, f] : flows_) {
        (void)key;
        all.push_back(f);
    }
    for (auto &f : all) {
        close_flow(*f, net::error::operation_aborted);
    }
    pending_flows_.clear();
    cancel_accepts();
}

void tcp_flow_shutdown_send(std::shared_ptr<tcp_flow> flow)
{
    if (!flow) {
        return;
    }
    auto eng = flow->eng.lock();
    if (!eng) {
        return;
    }
    net::dispatch(eng->strand(), [flow, eng]() {
        auto &f = *flow;
        if (f.state == tcp_state::CLOSED || f.app_closed) {
            return;
        }
        eng->send_fin(f);
    });
}

void tcp_flow_shutdown_receive(std::shared_ptr<tcp_flow> flow)
{
    if (!flow) {
        return;
    }
    auto eng = flow->eng.lock();
    if (!eng) {
        return;
    }
    net::dispatch(eng->strand(), [flow, eng]() {
        auto &f = *flow;
        if (f.state == tcp_state::CLOSED) {
            return;
        }
        f.rx_shutdown = true;
        for (auto &op : f.pending_reads) {
            op.handler(boost::system::error_code(net::error::operation_aborted),
                       0);
        }
        f.pending_reads.clear();
        if (f.rx_bytes > 0) {
            eng->account().release(f.rx_bytes);
            f.rx_bytes = 0;
        }
        f.rx_data.clear();
        f.rx_head = 0;
    });
}

void tcp_flow_close(std::shared_ptr<tcp_flow> flow)
{
    if (!flow) {
        return;
    }
    auto eng = flow->eng.lock();
    if (!eng) {
        return;
    }
    net::dispatch(eng->strand(), [flow, eng]() {
        auto &f = *flow;
        if (f.state == tcp_state::CLOSED || f.app_closed) {
            return;
        }
        f.app_closed = true;
        for (auto &op : f.pending_reads) {
            op.handler(boost::system::error_code(net::error::operation_aborted),
                       0);
        }
        f.pending_reads.clear();
        for (auto &op : f.pending_writes) {
            op.handler(boost::system::error_code(net::error::operation_aborted),
                       0);
        }
        f.pending_writes.clear();
        f.tx_bytes = 0;
        if (f.rx_bytes > 0) {
            eng->account().release(f.rx_bytes);
            f.rx_bytes = 0;
        }
        f.rx_data.clear();
        f.rx_head = 0;
        eng->send_fin(f);
    });
}

void tcp_flow_reset(std::shared_ptr<tcp_flow> flow)
{
    if (!flow) {
        return;
    }
    auto eng = flow->eng.lock();
    if (!eng) {
        return;
    }
    net::dispatch(eng->strand(), [flow, eng]() {
        auto &f = *flow;
        if (f.state == tcp_state::CLOSED || f.app_closed) {
            return;
        }
        eng->abort_flow(f);
    });
}

void tcp_flow_accept(std::shared_ptr<tcp_flow> flow)
{
    if (!flow) {
        return;
    }
    auto eng = flow->eng.lock();
    if (!eng) {
        return;
    }
    net::dispatch(eng->strand(), [flow, eng]() {
        eng->accept_flow(*flow);
    });
}

void tcp_flow_reject(std::shared_ptr<tcp_flow> flow)
{
    if (!flow) {
        return;
    }
    auto eng = flow->eng.lock();
    if (!eng) {
        return;
    }
    net::dispatch(eng->strand(), [flow, eng]() {
        eng->reject_flow(*flow);
    });
}

bool tcp_flow_is_open(const std::shared_ptr<tcp_flow> &flow)
{
    return flow && !flow->eng.expired() && flow->state != tcp_state::CLOSED &&
           !flow->app_closed && !flow->rst;
}

net::ip::tcp::endpoint tcp_flow::original_destination() const
{
    if (key.family == 6) {
        net::ip::address_v6::bytes_type b{};
        std::copy(key.dst_ip.begin(), key.dst_ip.end(), b.begin());
        return {net::ip::address_v6(b), ntohs(key.dst_port)};
    }
    net::ip::address_v4::bytes_type b{};
    std::copy(key.dst_ip.begin(), key.dst_ip.begin() + 4, b.begin());
    return {net::ip::address_v4(b), ntohs(key.dst_port)};
}

bool tcp_flow::is_open() const
{
    return state != tcp_state::CLOSED && !app_closed && !rst;
}

} // namespace detail
} // namespace tunio
