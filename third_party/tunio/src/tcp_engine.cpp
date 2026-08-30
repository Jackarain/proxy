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
#include <boost/asio/experimental/awaitable_operators.hpp>

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
{
}

tcp_engine::~tcp_engine()
{
    sweep_timer_.cancel();
}

void tcp_engine::start_sweep()
{
    sweep_timer_.expires_after(std::chrono::seconds(1));
    // 定时器以引擎 Strand 构造，完成回调已在 Strand 上，无需再派发
    // 弱引用避免定时器回调自持有形成引用环，引擎释放后回调直接跳过
    sweep_timer_.async_wait(
        [self = weak_from_this()](const boost::system::error_code &ec) {
            if (auto s = self.lock()) {
                s->on_sweep(ec);
            }
        });
}

void tcp_engine::on_sweep(const boost::system::error_code &ec)
{
    if (ec) {
        return;
    }
    const auto now = tcp_clock::now();
    std::vector<tcp_flow_ptr> victims;
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
    const uint8_t *data = payload + hlen;
    const size_t data_len = len - hlen;

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
        // 解析对端 SYN 通告的 Window Scale（RFC 7323 选项 kind=3, len=3）。
        // 两方向缩放独立（RFC 7323 §2.2）：解释对端窗口字段时左移对端通告
        // 原值（snd）；本端广告窗口时右移本端通告值（rcv，固定 7）。对端未
        // 通告或值 >14（RFC 7323 规定忽略）时视为未启用缩放，本端也不缩放。
        uint8_t wscale = 0;
        bool wscale_ok = false;
        for (size_t o = sizeof(tcp_header);
            o + 2 <= hlen && payload[o] != 0;) {
            const uint8_t kind = payload[o];
            if (kind == 1) {
                ++o; // NOP
                continue;
            }
            const uint8_t olen = payload[o + 1];
            if (olen < 2 || o + olen > hlen) {
                break; // 非法选项：终止解析
            }
            if (kind == 3 && olen == 3) {
                if (payload[o + 2] <= 14) {
                    wscale = payload[o + 2];
                    wscale_ok = true;
                }
            }
            o += olen;
        }
        f->wscale_ok = wscale_ok;
        f->snd_wnd_scale = wscale_ok ? wscale : 0;
        f->rcv_wnd_scale =
            wscale_ok ? tcp_flow::k_rcv_wnd_scale : 0;
        // SYN 段窗口字段本身不缩放（RFC 7323 §2.2），按原值记录
        f->peer_wnd = ntohs(th.window);
        f->created_at = tcp_clock::now();
        flows_.emplace(key, f);
        if (data_len > 0 && ntohl(th.seq) + 1 == f->rcv_nxt) {
            // TFO：SYN 携带数据（seq = irs + 1）。缓存并按序推进 rcv_nxt，
            // 不单独发 ACK——由 accept_flow 的 SYN-ACK 捎带确认；队列超限时
            // 丢弃，客户端将在建立连接后重传该数据.
            if (f->rx_bytes + data_len <= cfg_.max_rx_queue_per_flow &&
                account_->reserve(data_len)) {
                f->rx_data.insert(f->rx_data.end(), data, data + data_len);
                f->rx_bytes += data_len;
                f->rcv_nxt += static_cast<uint32_t>(data_len);
            } else {
                stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
            }
        }
        // 延迟握手: 不立即回复 SYN+ACK, 交给 async_accept 领取后由
        // 应用 accept()/reject() 或首次读写（隐式批准）决定握手结果.
        notify_accept(*f);
        return;
    }
    // 持有强引用：handle_segment 内会内联调用用户完成回调（当流绑定引擎
    // Strand 时），回调中 reset() 后销毁流可能擦除 flows_ 并释放最后引用；
    // 强引用保证回调返回后 f 仍然有效，避免 use-after-free。
    tcp_flow_ptr f = it->second;
    handle_segment(f, th, data, data_len);
}

void tcp_engine::handle_segment(const tcp_flow_ptr &f,
    const tcp_header &th, const uint8_t *data, size_t data_len)
{
    const uint32_t seq = ntohl(th.seq);
    const uint32_t ack = ntohl(th.ack);
    const uint8_t flags = th.flags;
    const uint16_t wnd = ntohs(th.window);

    // ---- ACK 与窗口更新 ----
    if (flags & TCP_ACK) {
        if (seq_gt(ack, f->snd_una) && seq_ge(f->snd_nxt, ack)) {
            f->snd_una = ack;
        } else if (ack == f->snd_nxt + 1 && seq_gt(ack, f->snd_una) &&
                   (f->probe_in_flight ||
                       tail_covers(*f, f->snd_nxt))) {
            // 确认序号比已计入 snd_nxt 的数据末尾多 1：零窗口探测字节
            // （未计入 snd_nxt）被对端接收，或该字节正由流级尾部重传
            // 覆盖；同步推进序号与写偏移，避免 in_flight 回绕与数据错位.
            f->snd_una = ack;
            f->snd_nxt = ack;
            if (f->active_write &&
                f->active_write->offset < f->active_write->total) {
                ++f->active_write->offset;
                ++f->active_write->buf_off;
            }
        }
        // 任何 ACK 都意味着对端对探测做出了回应（确认或丢弃），探测状态结束
        f->probe_in_flight = false;
        // 尾部未确认范围随 ACK 推进：全部确认则释放拷贝缓冲，部分确认
        // 则复位退避，尽快重传剩余部分.
        tail_ack_progress(*f);
        // 数据全部确认且无未发送数据：补发被推迟的 FIN（ACK 是发送侧
        // 数据进展的可靠信号，避免 FIN 与在途数据段序列号重叠）
        if (f->fin_pending && f->snd_una == f->snd_nxt &&
            !tail_covers(*f, f->snd_nxt) &&
            (!f->active_write ||
             f->active_write->offset == f->active_write->total)) {
            f->fin_pending = false;
            send_fin(*f);
        }
        if (f->state == tcp_state::FIN_WAIT_1 && f->fin_sent &&
            seq_ge(ack, f->snd_nxt)) {
            // 客户端确认了我们的 FIN
            f->state =
                f->fin_received ? tcp_state::TIME_WAIT : tcp_state::FIN_WAIT_2;
            if (f->state == tcp_state::TIME_WAIT) {
                f->destroy_at =
                    tcp_clock::now() + cfg_.tcp_time_wait_timeout;
            }
        } else if (f->state == tcp_state::LAST_ACK && f->fin_sent &&
                   seq_ge(ack, f->snd_nxt)) {
            close_flow(*f, boost::system::error_code{});
            return;
        }
    }
    // 对端通告的窗口字段按协商 scale 放大后才是实际可用发送窗口（RFC 7323）
    const uint32_t old_wnd = f->peer_wnd;
    f->peer_wnd = static_cast<uint32_t>(wnd) << f->snd_wnd_scale;
    if (old_wnd == 0 && f->peer_wnd > 0 && !f->tail_buf.empty() &&
        f->snd_una < f->tail_end) {
        // 窗口从 0 恢复且仍有未确认尾部：复位退避，避免窗口恢复后仍需
        // 等待最长 60s 退避才重传.
        f->tail_rto = cfg_.tcp_rto_timeout;
        f->tail_retransmits = 0;
        arm_tail_timer(*f);
    }

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
        if (!(flags & TCP_ACK) || ack != f->iss + 1) {
            // 非预期 ACK：忽略
            return;
        }
        f->state = tcp_state::ESTABLISHED;
        stats_.tcp_connections.fetch_add(1, std::memory_order_relaxed);
        // 流已在收到 SYN 时交付给 async_accept, 不再重复通知.
        if (!f->accepted) {
            notify_accept(*f);
        }
        // 交付握手前已缓存的数据（TFO：SYN 携带的数据）
        flush_reads(*f);
        // 不 return：客户端可能在握手 ACK 中合并数据与 FIN（快速关闭），
        // 由下方统一的数据/FIN 处理逻辑接管；原实现直接 return 会漏掉
        // 同段 FIN，导致 fin_received 不置位、读侧永远等不到 EOF.
    }

    // ---- 已建立连接的数据处理 ----
    if (data_len > 0) {
        if (seq == f->rcv_nxt) {
            deliver_data(*f, data, data_len);
            // 缺失段补齐后，交付已缓存的连续乱序段
            flush_ooo(*f);
        } else if (seq_gt(seq, f->rcv_nxt)) {
            // 超前序列号：缓存乱序段，缺失段补齐后按序交付。缓存成功时
            // 静默等待（缺失段在并发读场景往往即将到达），不发 Dup-ACK，
            // 避免人为乱序触发对端快速重传与拥塞窗口减半；缓存拒绝
            // （超限/重复）时才发 Dup-ACK 促使对端重传缺失段.
            if (ooo_append(*f, seq, data, data_len,
                           (flags & TCP_FIN) != 0)) {
                stats_.rx_ooo.fetch_add(1, std::memory_order_relaxed);
            } else {
                send_ack(*f);
            }
        } else {
            // 重复或已接收段：静默丢弃
            stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // ---- FIN ----
    if (flags & TCP_FIN) {
        // FIN 序号 = seq + data_len（FIN 消耗一个序列号，可与数据同段）
        const uint32_t fin_seq = seq + static_cast<uint32_t>(data_len);
        handle_fin(*f, fin_seq);
    }

    signal_write(*f);
}

void tcp_engine::deliver_data(tcp_flow &f, const uint8_t *data, size_t len)
{
    if (len == 0) {
        return;
    }
    if (f.rx_shutdown) {
        // 应用已关闭接收侧：数据不再入队，丢弃并推进序号后正常确认，
        // 避免无消费方时 rx_data 持续积压占用缓冲记账.
        f.rcv_nxt += static_cast<uint32_t>(len);
        // 每段立即确认：避免 delayed ACK 40ms 兜底在低 cwnd 时把
        // 一问一答周期拉长到 40ms，拖慢内核发送节奏.
        send_ack(f);
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
    if (f.rx_data.empty() && f.active_read) {
        auto op = std::move(*f.active_read);
        f.active_read.reset();
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
        if (n < len) {
            // 剩余数据先入队并记账再回调：回调内 close()/reset() 时能正确
            // 释放记账，避免向已关闭流残留数据.
            f.rx_data.insert(f.rx_data.end(), data + n, data + len);
            f.rx_bytes += len - n;
        }
        op.handler(boost::system::error_code{}, n);
        if (n < len) {
            flush_reads(f);
        }
        send_ack(f);
        return;
    }
    f.rx_data.insert(f.rx_data.end(), data, data + len);
    f.rx_bytes += len;
    f.rcv_nxt += static_cast<uint32_t>(len);
    // 每段立即确认：ACK 及时性优先，避免 40ms 兜底拖慢内核发送节奏.
    send_ack(f);
    flush_reads(f);
}

void tcp_engine::flush_reads(tcp_flow &f)
{
    if (f.active_read && f.rx_bytes > 0) {
        auto op = std::move(*f.active_read);
        f.active_read.reset();
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
        notify_window_updated(f);
    }
    // 头部偏移过大时压缩连续缓冲，保持内存占用与 cache 友好
    if (f.rx_head > 0 &&
        (f.rx_head == f.rx_data.size() || f.rx_head >= 65536)) {
        f.rx_data.erase(f.rx_data.begin(), f.rx_data.begin() +
            static_cast<std::ptrdiff_t>(f.rx_head));
        f.rx_head = 0;
    }
    // 数据耗尽后处理 EOF：按 Asio 语义以 error::eof 完成读操作（对端 FIN），
    // 避免以 (success, 0) 交付导致调用方无法区分 EOF 与空读，进而死循环.
    if (f.rx_bytes == 0 && f.fin_received) {
        if (f.active_read) {
            auto op = std::move(*f.active_read);
            f.active_read.reset();
            op.handler(net::error::eof, 0);
        }
    }
}

void tcp_engine::flush_ooo(tcp_flow &f)
{
    bool delivered = false;
    for (;;) {
        auto it = f.ooo_cache.find(f.rcv_nxt);
        if (it == f.ooo_cache.end()) {
            break;
        }
        const uint32_t seg_seq = it->first;
        auto seg = std::move(it->second);
        const size_t len = seg.data.size();
        f.ooo_cache.erase(it);
        f.ooo_bytes -= len;
        --f.ooo_count;
        // 缓存占用先归还记账，交付路径（rx_data/直投）再重新记账，
        // 总量保持一致；deliver_data 内部的限额判定因此只面对 rx_data.
        account_->release(len);
        notify_window_updated(f);
        if (len > 0) {
            deliver_data(f, seg.data.data(), len);
        }
        if (seg.fin) {
            // FIN 序号 = 段首 seq + 数据长度（FIN 消耗一个序列号）
            handle_fin(f, seg_seq + static_cast<uint32_t>(len));
        }
        delivered = true;
    }
    // 批量补齐后立即确认全部缓存段：避免各段 delayed ACK（每 2 段/40ms）
    // 累积延迟，使内核尽快推进发送窗口，降低 RTO 概率.
    if (delivered) {
        send_ack(f);
    }
}

bool tcp_engine::ooo_append(tcp_flow &f, uint32_t seq, const uint8_t *data,
    size_t len, bool fin)
{
    if (f.ooo_cache.find(seq) != f.ooo_cache.end()) {
        return false; // 重复乱序段：忽略
    }
    if (f.ooo_count >= cfg_.tcp_ooo_max_segments ||
        f.rx_bytes + f.ooo_bytes + len > cfg_.max_rx_queue_per_flow ||
        !account_->reserve(len)) {
        // 缓存超限：丢弃并依赖对端重传（调用方随后发 Dup-ACK）
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    tcp_flow::ooo_segment seg;
    seg.data.assign(data, data + len);
    seg.fin = fin;
    f.ooo_cache.emplace(seq, std::move(seg));
    f.ooo_bytes += len;
    ++f.ooo_count;
    return true;
}

void tcp_engine::handle_fin(tcp_flow &f, uint32_t fin_seq)
{
    if (fin_seq == f.rcv_nxt) {
        if (!f.fin_received) {
            f.rcv_nxt += 1;
            f.fin_received = true;
        }
        send_ack(f);
        switch (f.state) {
        case tcp_state::ESTABLISHED:
            f.state = tcp_state::CLOSE_WAIT;
            break;
        case tcp_state::FIN_WAIT_1:
            // 等待客户端 ACK 我们的 FIN（ACK 分支推进到 FIN_WAIT_2 /
            // TIME_WAIT）
            break;
        case tcp_state::FIN_WAIT_2:
            f.state = tcp_state::TIME_WAIT;
            f.destroy_at =
                tcp_clock::now() + cfg_.tcp_time_wait_timeout;
            break;
        case tcp_state::TIME_WAIT:
            break;
        default:
            break;
        }
        flush_reads(f);
    } else if (f.fin_received && fin_seq == f.rcv_nxt - 1) {
        // 对端重传 FIN：重新确认（避免客户端长时间重复重传）
        send_ack(f);
    }
}

packet_buffer tcp_engine::build_segment(tcp_flow &f, uint32_t seq,
    uint8_t flags, const uint8_t *payload, size_t len, bool with_mss)
{
    const int family = f.key.family;
    const size_t ip_hdr_len = ip_header_size(family);
    // MSS(4) + Window Scale(3) + NOP(1) 对齐到 8 字节
    const size_t tcp_hdr_len = with_mss ? 28 : 20;
    const size_t total = ip_hdr_len + tcp_hdr_len + len;
    packet_buffer pkt = writer_.acquire(cfg_.mtu + 64, 64);
    pkt.resize(total);
    uint8_t *base = pkt.data();

    build_ip_header(base, family, f.key.dst_ip.data(),
        f.key.src_ip.data(), IPPROTO_TCP_V, total,
        writer_.alloc_ip_id());

    auto *th = reinterpret_cast<tcp_header *>(base + ip_hdr_len);
    th->src_port = f.key.dst_port;
    th->dst_port = f.key.src_port;
    th->seq = htonl(seq);
    th->ack = htonl(f.rcv_nxt);
    th->data_offset = static_cast<uint8_t>((with_mss ? 7 : 5) << 4);
    th->flags = flags;
    // 动态接收窗口：通告 min(固定上限, 剩余接收缓冲)，剩余缓冲耗尽时
    // 通告 0 施加背压，避免固定大窗口误导对端超发导致队列积压丢包
    // （对齐 gVisor selectWindow 的窗口-可用缓冲联动）。窗口缩放仅在
    // 连接建立后生效（RFC 7323）：SYN-ACK 阶段 window 未缩放，协商
    // 完成后按 rcv_wnd_scale 右移后写入字段（上限 65535 由对端解释）。
    const uint8_t scale =
        f.state == tcp_state::SYN_ACK_SENT ? 0 : f.rcv_wnd_scale;
    uint32_t wnd = current_wnd(f);
    f.last_wnd_advertised = wnd;
    const uint32_t scaled = wnd >> scale;
    th->window =
        htons(static_cast<uint16_t>(scaled > 65535 ? 65535 : scaled));
    th->checksum = 0;
    th->urgent = 0;

    if (with_mss) {
        uint8_t *opt = base + ip_hdr_len + 20;
        opt[0] = 2; // kind = MSS
        opt[1] = 4; // len = 4
        const size_t mss = this->mss(family);
        opt[2] = static_cast<uint8_t>(mss >> 8);
        opt[3] = static_cast<uint8_t>(mss & 0xff);
        // 对端未通告 WS 时 SYN-ACK 不携带 Window Scale 选项（RFC 7323：
        // 收到对端 WS 选项才在本端 SYN-ACK 中通告），避免老栈按未缩放
        // 解释本端窗口字段时被 7 倍放大导致超发.
        if (f.wscale_ok) {
            opt[4] = 3; // kind = Window Scale
            opt[5] = 3; // len = 3
            opt[6] = tcp_flow::k_rcv_wnd_scale;
            opt[7] = 1; // NOP 对齐
        } else {
            // 显式 NOP 填充：缓冲池复用可能残留上一连接的 WS 选项字节，
            // 未初始化字节会破坏"不携带 WS 选项"的语义（RFC 7323）
            opt[4] = 1;
            opt[5] = 1;
            opt[6] = 1;
            opt[7] = 1;
        }
    }
    if (len > 0) {
        std::memcpy(base + ip_hdr_len + tcp_hdr_len, payload, len);
    }
    th->checksum = htons(
        tcp_udp_checksum(family, f.key.dst_ip.data(),
            f.key.src_ip.data(), IPPROTO_TCP_V, base + ip_hdr_len,
            tcp_hdr_len + len));

    return pkt;
}

void tcp_engine::send_segment(tcp_flow &f, uint32_t seq, uint8_t flags,
    const uint8_t *payload, size_t len, bool with_mss)
{
    // 控制段（SYN/SYN+ACK/FIN/RST/ACK）直通：不参与数据背压，确保
    // 连接建立/关闭的关键段不被数据发送队列阻塞.
    writer_.async_write_and_forget(
        build_segment(f, seq, flags, payload, len, with_mss));
}

net::awaitable<void> tcp_engine::write_loop(tcp_flow_ptr f)
{
    // 强引用保活：协程可能在引擎/设备写器析构后仍挂起（等写完成回调），
    // 捕获 shared_ptr 避免协程恢复时访问已销毁对象.
    auto eng = shared_from_this();
    auto writer = writer_.shared_from_this();
    auto &flow = *f;

    if (!flow.active_write) {
        // 挂起期间连接被关闭：close_flow 已以错误完成写操作.
        co_return;
    }

    if (!flow.write_ch) {
        flow.write_ch.emplace(strand_);
    }
    // 记录本次写操作首个字节的发送序号：未确认范围 [snd_una, snd_nxt)
    // 减去该值即用户缓冲偏移，RTO 重传据此定位数据（无需拷贝缓冲）.
    flow.active_write->start_seq = flow.snd_nxt;

    // 零窗口持久计时器：对端通告窗口 0 时周期性发送窗口探测，避免依赖
    // 对端主动发送窗口更新（该 ACK 丢失时若无探测将永久挂起）；探测确认
    // 后按指数退避加倍，上限 60s.
    net::steady_timer persist_timer(strand_);
    auto persist_interval = cfg_.tcp_persist_timeout;
    const auto persist_max = std::chrono::milliseconds(60000);

    // RTO 重传计时器：对端窗口允许但 ACK 迟迟不推进时，周期性重传未确认
    // 数据（指数退避，上限 60s）；超过最大重传次数判定发送超时以 RST 关闭.
    net::steady_timer rto_timer(strand_);
    auto rto = cfg_.tcp_rto_timeout;
    const auto rto_max = std::chrono::milliseconds(60000);
    int retransmits = 0;
    uint32_t last_una = flow.snd_una;

    using namespace net::experimental::awaitable_operators;

    try {
        while (flow.active_write && flow.state != tcp_state::CLOSED &&
               !flow.rst) {
            // 写数据末尾序号：FIN 发送后 snd_nxt 会再前进 1（FIN 占序号），
            // 写完成只要求对端确认到数据末尾，不等待 FIN 的确认.
            const uint32_t write_end =
                flow.active_write->start_seq +
                static_cast<uint32_t>(flow.active_write->total);
            if (flow.active_write->offset == flow.active_write->total) {
                // 数据已全部发出即完成写操作：避免逐片等待对端 ACK 时，
                // 小段触发 delayed ACK(40ms) 拖慢大流量吞吐；未确认数据
                // 由流级尾部 RTO 重传兜底，与直连/代理端行为一致.
                capture_tail(flow);
                auto h = std::move(flow.active_write->handler);
                const size_t done = flow.active_write->total;
                flow.active_write.reset();
                h(boost::system::error_code{}, done);
                co_return;
            }
            if (flow.peer_wnd == 0) {
                // 零窗口：等待窗口更新信号与持久计时器竞速；定时器超时则
                // 发送窗口探测（对端即使窗口仍为 0 也会回复 ACK 通告窗口，
                // 从而刷新窗口状态并重置计时）.
                persist_timer.expires_after(persist_interval);
                auto result = co_await (
                    flow.write_ch->async_receive(
                        net::as_tuple(net::use_awaitable)) ||
                    persist_timer.async_wait(
                        net::as_tuple(net::use_awaitable)));
                if (result.index() == 1 &&
                    std::get<0>(std::get<1>(result)) ==
                        boost::system::error_code{}) {
                    if (flow.active_write->offset <
                        flow.active_write->total) {
                        // 常规探测：发送下一个未发送字节（不推进序号，确认
                        // 时由 handle_segment 的 ACK 处理推进写偏移）
                        const uint8_t *probe = nullptr;
                        {
                            auto &op = *flow.active_write;
                            while (op.buf_index < op.buffers.size() &&
                                   op.buf_off >=
                                       op.buffers[op.buf_index].size()) {
                                op.buf_off -= op.buffers[op.buf_index].size();
                                ++op.buf_index;
                            }
                            if (op.buf_index < op.buffers.size() &&
                                op.offset < op.total) {
                                probe =
                                    static_cast<const uint8_t *>(
                                        op.buffers[op.buf_index].data()) +
                                    op.buf_off;
                            }
                        }
                        if (probe) {
                            send_segment(flow, flow.snd_nxt, TCP_ACK, probe, 1,
                                         false);
                            flow.probe_in_flight = true;
                        }
                    } else if (flow.snd_una != write_end) {
                        // 数据已全部发出：重传未确认数据段首部作为探测，
                        // 迫使对端回复 ACK 通告最新窗口.
                        retransmit_unacked(flow, mss(f->key.family));
                    }
                    if (persist_interval < persist_max) {
                        persist_interval =
                            std::min(persist_interval * 2, persist_max);
                    }
                } else {
                    // 收到窗口信号（ACK 更新窗口）：重置退避后重新检查
                    persist_interval = cfg_.tcp_persist_timeout;
                }
                continue;
            }
            const uint32_t in_flight = flow.snd_nxt - flow.snd_una;
            if (flow.active_write->offset < flow.active_write->total &&
                in_flight < flow.peer_wnd) {
                // 窗口允许且仍有未发送数据：发送下一个分片
                size_t chunk = 0;
                packet_buffer pkt{};
                {
                    auto &op = *flow.active_write;
                    // 增量定位：offset 单调推进，buf_index/buf_off 持久化在
                    // write_op 中，避免每段从头扫描整个缓冲序列.
                    while (op.buf_index < op.buffers.size() &&
                           op.buf_off >= op.buffers[op.buf_index].size()) {
                        op.buf_off -= op.buffers[op.buf_index].size();
                        ++op.buf_index;
                    }
                    if (op.buf_index == op.buffers.size()) {
                        break;
                    }
                    const size_t remaining = op.total - op.offset;
                    const size_t avail =
                        op.buffers[op.buf_index].size() - op.buf_off;
                    chunk = std::min(
                        {remaining, avail, mss(f->key.family),
                            static_cast<size_t>(flow.peer_wnd - in_flight)});
                    if (chunk == 0) {
                        break;
                    }
                    const uint8_t *payload =
                        static_cast<const uint8_t *>(
                            op.buffers[op.buf_index].data()) +
                        op.buf_off;
                    pkt = build_segment(flow, flow.snd_nxt, TCP_ACK | TCP_PSH,
                                        payload, chunk, false);
                    // 提前推进序号与写偏移：在途分片同样占用序列号空间，
                    // 保证等待期间到达的 ACK/FIN 处理（snd_una 推进、FIN
                    // 序号）不依赖设备写完成时序.
                    flow.snd_nxt += static_cast<uint32_t>(chunk);
                    op.offset += chunk;
                    op.buf_off += chunk;
                }
                // 数据段经设备写完成回调驱动背压：设备写通道拥塞时协程挂起，
                // 内存占用受"每流单写 + 设备队列水位"约束，不再无界累积.
                auto [ec, n] = co_await writer->async_write(
                    std::move(pkt), net::as_tuple(net::use_awaitable));
                if (ec) {
                    break;
                }
                if (!flow.active_write) {
                    // 挂起期间连接被关闭：写操作已由关闭路径完成，停止推进.
                    break;
                }
                continue;
            }
            // 窗口耗尽或数据已全部发出：等待 ACK 推进窗口，或 RTO 超时
            // 重传未确认数据（链路丢段时 snd_una 永不推进，若无重传将
            // 永久挂起，大流量传输即死锁）.
            if (flow.snd_una != last_una) {
                // 上次检查后 ACK 已推进发送序号：无需等待（写入期间到达的
                // ACK 可能因写通道无等待者而错过信号，直接重查避免 RTO）.
                last_una = flow.snd_una;
                continue;
            }
            rto_timer.expires_after(rto);
            auto result = co_await (
                flow.write_ch->async_receive(
                    net::as_tuple(net::use_awaitable)) ||
                rto_timer.async_wait(net::as_tuple(net::use_awaitable)));
            if (result.index() == 1 &&
                std::get<0>(std::get<1>(result)) ==
                    boost::system::error_code{}) {
                if (flow.snd_una == last_una) {
                    // RTO 超时且无任何 ACK 进展：重传未确认数据并退避
                    if (++retransmits > cfg_.tcp_rto_max_retransmits) {
                        abort_flow(flow);
                        break;
                    }
                    if (flow.snd_una != write_end) {
                        // 重传量按对端当前窗口限幅，避免窗口收缩后超窗
                        // 加重网络负担；窗口足够时等效全量重传
                        retransmit_unacked(flow, flow.peer_wnd);
                    }
                    rto = std::min(rto * 2, rto_max);
                } else {
                    // 超时前已有 ACK 进展（竞态）：重置退避
                    rto = cfg_.tcp_rto_timeout;
                    retransmits = 0;
                }
            } else if (flow.snd_una != last_una) {
                // ACK 推进窗口：重置重传退避
                rto = cfg_.tcp_rto_timeout;
                retransmits = 0;
            }
            last_una = flow.snd_una;
        }
    } catch (...) {
        // 异常兜底：以错误完成挂起写操作，避免 detached 协程抛出
    }

    if (flow.active_write) {
        auto err = flow.rst ? net::error::connection_reset
                            : net::error::bad_descriptor;
        auto h = std::move(flow.active_write->handler);
        flow.active_write.reset();
        h(err, 0);
    }
    co_return;
}

void tcp_engine::retransmit_unacked(tcp_flow &f, size_t max_bytes)
{
    if (!f.active_write || f.snd_una == f.snd_nxt) {
        return;
    }
    auto &op = *f.active_write;
    // 未确认范围 [snd_una, snd_nxt) 映射为写缓冲偏移（offset 与
    // snd_nxt - start_seq 同步推进，探测字节确认时两者同步递增）.
    // 起点限制在本次写操作首字节：更早的未确认尾部由流级重传负责，
    // 用户缓冲此时可能已被应用复用，不能重读.
    const size_t begin = static_cast<size_t>(
        (f.snd_una > op.start_seq ? f.snd_una : op.start_seq) -
        op.start_seq);
    // 终点不得超过用户缓冲长度：FIN 发送后 snd_nxt 含 FIN 的序号空间.
    const size_t end =
        std::min(static_cast<size_t>(f.snd_nxt - op.start_seq), op.total);
    size_t off = begin;
    size_t budget = max_bytes;
    const size_t seg_mss = mss(f.key.family);
    size_t bi = 0;   // 当前缓冲下标
    size_t base = 0; // bi 缓冲的起始流偏移
    while (bi < op.buffers.size() &&
           base + op.buffers[bi].size() <= off) {
        base += op.buffers[bi].size();
        ++bi;
    }
    while (bi < op.buffers.size() && off < end && budget > 0) {
        const size_t bo = off - base;
        const size_t avail = op.buffers[bi].size() - bo;
        const size_t want = std::min({end - off, avail, budget, seg_mss});
        if (want == 0) {
            break;
        }
        const uint8_t *payload =
            static_cast<const uint8_t *>(op.buffers[bi].data()) + bo;
        send_segment(f, op.start_seq + static_cast<uint32_t>(off),
                     TCP_ACK | TCP_PSH, payload, want, false);
        off += want;
        budget -= want;
        if (off >= base + op.buffers[bi].size()) {
            base += op.buffers[bi].size();
            ++bi;
        }
    }
}

bool tcp_engine::tail_covers(const tcp_flow &f, uint32_t seq)
{
    return !f.tail_buf.empty() && f.tail_seq <= seq && seq < f.tail_end;
}

void tcp_engine::capture_tail(tcp_flow &f)
{
    auto &op = *f.active_write;
    const uint32_t end_seq =
        op.start_seq + static_cast<uint32_t>(op.total);
    if (f.snd_una >= end_seq) {
        // 数据已全部确认：无需保留尾部（残留 tail 由 ACK 路径清理）
        if (!f.tail_buf.empty()) {
            clear_tail(f);
        }
        return;
    }
    if (!f.tail_buf.empty() && f.snd_una >= f.tail_end) {
        // 防御：上一尾部已全部确认但尚未被清理
        clear_tail(f);
    }
    // 丢弃已确认前缀，保持 tail_buf 从未确认起点开始
    if (!f.tail_buf.empty() && f.snd_una > f.tail_seq) {
        const size_t drop = static_cast<size_t>(f.snd_una - f.tail_seq);
        f.tail_buf.erase(
            f.tail_buf.begin(),
            f.tail_buf.begin() + static_cast<std::ptrdiff_t>(drop));
        f.tail_seq = f.snd_una;
    }
    // 追加本次写操作全部字节：有尾时 tail_end == start_seq（序列空间
    // 连续）；无尾时从已确认起点拷贝（snd_una >= start_seq，防御性钳到 0）.
    const size_t begin_off = f.tail_buf.empty()
        ? (f.snd_una > op.start_seq
               ? static_cast<size_t>(f.snd_una - op.start_seq)
               : 0)
        : 0;
    if (f.tail_buf.empty()) {
        f.tail_seq = op.start_seq + static_cast<uint32_t>(begin_off);
    }
    const size_t want = op.total - begin_off;
    f.tail_buf.reserve(f.tail_buf.size() + want);
    size_t bi = 0;
    size_t base = 0;
    while (bi < op.buffers.size() &&
           base + op.buffers[bi].size() <= begin_off) {
        base += op.buffers[bi].size();
        ++bi;
    }
    size_t copied = 0;
    while (bi < op.buffers.size() && copied < want) {
        const size_t bo = begin_off + copied - base;
        const size_t avail = op.buffers[bi].size() - bo;
        const size_t take = std::min(avail, want - copied);
        if (take == 0) {
            break;
        }
        const uint8_t *src =
            static_cast<const uint8_t *>(op.buffers[bi].data()) + bo;
        f.tail_buf.insert(f.tail_buf.end(), src, src + take);
        copied += take;
        if (copied < want) {
            base += op.buffers[bi].size();
            ++bi;
        }
    }
    f.tail_end = end_seq;
    // 启动/复位流级 RTO：重传 [snd_una, tail_end) 直至确认或超限
    f.tail_rto = cfg_.tcp_rto_timeout;
    f.tail_retransmits = 0;
    arm_tail_timer(f);
}

void tcp_engine::retransmit_tail(tcp_flow &f, size_t max_bytes)
{
    if (f.tail_buf.empty() || f.snd_una >= f.tail_end) {
        return;
    }
    const size_t begin = static_cast<size_t>(f.snd_una - f.tail_seq);
    const size_t end = f.tail_buf.size();
    if (begin >= end) {
        return; // 防御：尾部已全部确认但尚未清理
    }
    size_t off = begin;
    size_t budget = max_bytes;
    const size_t seg_mss = mss(f.key.family);
    while (off < end && budget > 0) {
        const size_t want = std::min({end - off, budget, seg_mss});
        send_segment(f, f.tail_seq + static_cast<uint32_t>(off),
            TCP_ACK | TCP_PSH, f.tail_buf.data() + off, want, false);
        off += want;
        budget -= want;
    }
}

void tcp_engine::arm_tail_timer(tcp_flow &f)
{
    if (!f.tail_timer) {
        f.tail_timer = std::make_unique<net::steady_timer>(strand_);
    }
    f.tail_timer->expires_after(f.tail_rto);
    // 弱引用避免定时器回调自持有形成引用环；流/引擎销毁后回调直接跳过.
    f.tail_timer->async_wait(
        [self = weak_from_this(),
            flow = std::weak_ptr<tcp_flow>(f.shared_from_this())](
            const boost::system::error_code &ec) {
            if (ec) {
                return;
            }
            auto eng = self.lock();
            auto flow_ptr = flow.lock();
            if (eng && flow_ptr) {
                eng->on_tail_rto(flow_ptr);
            }
        });
}

void tcp_engine::clear_tail(tcp_flow &f)
{
    f.tail_buf.clear();
    f.tail_seq = 0;
    f.tail_end = 0;
    f.tail_rto = std::chrono::milliseconds(0);
    f.tail_retransmits = 0;
    if (f.tail_timer) {
        f.tail_timer->cancel();
    }
}

void tcp_engine::tail_ack_progress(tcp_flow &f)
{
    if (f.tail_buf.empty()) {
        return;
    }
    if (f.snd_una >= f.tail_end) {
        // 尾部数据全部确认：释放拷贝缓冲并停止重传
        clear_tail(f);
        return;
    }
    if (f.snd_una > f.tail_seq) {
        // 部分确认（累计推进）：复位退避并尽快重传剩余部分；仅推进时
        // 复位，避免对端持续重复 ACK 时无限激进重传.
        f.tail_rto = cfg_.tcp_rto_timeout;
        f.tail_retransmits = 0;
        arm_tail_timer(f);
    }
}

void tcp_engine::on_tail_rto(const tcp_flow_ptr &f)
{
    if (f->state == tcp_state::CLOSED || f->rst) {
        return;
    }
    if (f->tail_buf.empty() || f->snd_una >= f->tail_end) {
        clear_tail(*f);
        return;
    }
    if (++f->tail_retransmits > cfg_.tcp_rto_max_retransmits) {
        // 重传超限：与发送协程 RTO 行为一致，以 RST 快速释放连接
        abort_flow(*f);
        return;
    }
    // 窗口允许时重传未确认尾部；窗口为 0 时等待窗口更新（持续退避）
    if (f->peer_wnd > 0) {
        retransmit_tail(*f, f->peer_wnd);
    }
    f->tail_rto =
        std::min(f->tail_rto * 2, std::chrono::milliseconds(60000));
    arm_tail_timer(*f);
}

void tcp_engine::signal_write(tcp_flow &f)
{
    if (f.write_ch && f.write_ch->is_open()) {
        f.write_ch->try_send(boost::system::error_code{});
    }
}

void tcp_engine::send_ack(tcp_flow &f)
{
    send_segment(f, f.snd_nxt, TCP_ACK, nullptr, 0, false);
}

uint32_t tcp_engine::current_wnd(const tcp_flow &f) const
{
    const uint32_t queued =
        static_cast<uint32_t>(f.rx_bytes + f.ooo_bytes);
    uint32_t wnd = tcp_flow::fixed_rcv_wnd;
    if (cfg_.max_rx_queue_per_flow > queued) {
        wnd = std::min(wnd,
            static_cast<uint32_t>(cfg_.max_rx_queue_per_flow - queued));
    } else {
        wnd = 0;
    }
    const uint32_t mss_bytes = mss(f.key.family);
    if (wnd < mss_bytes) {
        wnd = 0; // 小于一个 MSS 通告 0，触发对端窗口探测
    }
    return wnd;
}

void tcp_engine::notify_window_updated(tcp_flow &f)
{
    if (f.state == tcp_state::CLOSED) {
        return;
    }
    const uint32_t wnd = current_wnd(f);
    const uint32_t mss_bytes = mss(f.key.family);
    const bool was_zero = f.last_wnd_advertised == 0;
    // 窗口从 0 恢复（>=MSS）或增长超过一个 MSS：主动发 ACK 通告新窗口，
    // 避免零窗口死锁下仅靠对端窗口探测（指数退避）缓慢恢复.
    if ((was_zero && wnd >= mss_bytes) ||
        (!was_zero && wnd > f.last_wnd_advertised + mss_bytes)) {
        send_ack(f);
    }
    f.last_wnd_advertised = wnd;
}

void tcp_engine::send_fin(tcp_flow &f)
{
    if (f.fin_sent || f.state == tcp_state::CLOSED) {
        return;
    }
    if ((f.active_write &&
         f.active_write->offset < f.active_write->total) ||
        f.snd_una != f.snd_nxt || tail_covers(f, f.snd_nxt)) {
        // 仍有未发送/未确认数据，或零窗口探测字节（未计入 snd_nxt）尚未
        // 确认：推迟 FIN，等数据全部确认后再发送，避免 FIN 与在途数据段
        // 序列号重叠导致对端丢弃 FIN 或后续数据.
        f.fin_pending = true;
        return;
    }
    f.fin_sent = true;
    switch (f.state) {
    case tcp_state::ESTABLISHED:
        f.state = tcp_state::FIN_WAIT_1;
        f.close_started_at = tcp_clock::now();
        send_segment(f, f.snd_nxt, TCP_ACK | TCP_FIN, nullptr, 0, false);
        f.snd_nxt += 1;
        break;
    case tcp_state::CLOSE_WAIT:
        f.state = tcp_state::LAST_ACK;
        f.close_started_at = tcp_clock::now();
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

void tcp_engine::close_flow(tcp_flow &f,
    const boost::system::error_code &err)
{
    if (f.state == tcp_state::CLOSED) {
        return;
    }
    clear_tail(f);
    if (f.active_read) {
        auto op = std::move(*f.active_read);
        f.active_read.reset();
        op.handler(err, 0);
    }
    if (f.active_write) {
        auto h = std::move(f.active_write->handler);
        f.active_write.reset();
        h(err, 0);
    }
    if (f.write_ch) {
        // 关闭信号通道：唤醒挂起等待窗口的发送协程，协程恢复后
        // 因 active_write 已清空而直接退出.
        f.write_ch->close();
    }
    if (f.rx_bytes > 0) {
        account_->release(f.rx_bytes);
        f.rx_bytes = 0;
    }
    f.rx_data.clear();
    f.rx_head = 0;
    if (f.ooo_bytes > 0) {
        account_->release(f.ooo_bytes);
        f.ooo_bytes = 0;
    }
    f.ooo_cache.clear();
    f.ooo_count = 0;
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
    std::vector<tcp_flow_ptr> all;
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

void tcp_flow_shutdown_send(tcp_flow_ptr flow)
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

void tcp_flow_shutdown_receive(tcp_flow_ptr flow)
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
        if (f.active_read) {
            auto op = std::move(*f.active_read);
            f.active_read.reset();
            op.handler(boost::system::error_code(net::error::operation_aborted),
                       0);
        }
        if (f.rx_bytes > 0) {
            eng->account().release(f.rx_bytes);
            f.rx_bytes = 0;
        }
        f.rx_data.clear();
        f.rx_head = 0;
        if (f.ooo_bytes > 0) {
            eng->account().release(f.ooo_bytes);
            f.ooo_bytes = 0;
        }
        f.ooo_cache.clear();
        f.ooo_count = 0;
    });
}

void tcp_flow_close(tcp_flow_ptr flow)
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
        if (f.active_read) {
            auto op = std::move(*f.active_read);
            f.active_read.reset();
            op.handler(boost::system::error_code(net::error::operation_aborted),
                       0);
        }
        if (f.active_write) {
            auto h = std::move(f.active_write->handler);
            f.active_write.reset();
            h(boost::system::error_code(net::error::operation_aborted), 0);
        }
        if (f.write_ch) {
            f.write_ch->close();
        }
        if (f.rx_bytes > 0) {
            eng->account().release(f.rx_bytes);
            f.rx_bytes = 0;
        }
        f.rx_data.clear();
        f.rx_head = 0;
        if (f.ooo_bytes > 0) {
            eng->account().release(f.ooo_bytes);
            f.ooo_bytes = 0;
        }
        f.ooo_cache.clear();
        f.ooo_count = 0;
        if (f.snd_una != f.snd_nxt) {
            // 有未确认数据（在途或已丢失）：此时 FIN 序号会超前于对端
            // 期望序号而被当作乱序丢弃，改发 RST 快速释放连接.
            eng->abort_flow(f);
        } else {
            eng->send_fin(f);
        }
    });
}

void tcp_flow_reset(tcp_flow_ptr flow)
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

void tcp_flow_accept(tcp_flow_ptr flow)
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

void tcp_flow_reject(tcp_flow_ptr flow)
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

bool tcp_flow_is_open(const tcp_flow_ptr &flow)
{
    return flow && !flow->eng.expired() &&
        flow->state != tcp_state::CLOSED && !flow->app_closed &&
        !flow->rst;
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
