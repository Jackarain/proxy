//
// udp_engine.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tunio/detail/udp_engine.hpp"

#include "tunio/detail/tun_queue_writer.hpp"
#include "tunio/detail/tcp_engine.hpp"

#include <boost/asio.hpp>

#include <algorithm>
#include <cstring>
#include <utility>

namespace tunio {
namespace detail {

namespace {

// 会话超时堆推入节流间隔：持续流量下限制每条会话的入堆频率，
// 避免 expiry_heap_ 随包速率无界增长（见 refresh_expiry）.
constexpr auto k_expiry_push_interval = std::chrono::milliseconds(500);

// 由网络字节序地址字节与端口构造 Asio endpoint（family: 4 或 6）
net::ip::udp::endpoint make_udp_endpoint(
    uint8_t family, const uint8_t* addr, uint16_t port) noexcept
{
    if (family == 6)
    {
        net::ip::address_v6::bytes_type b{};
        std::memcpy(b.data(), addr, 16);
        return {net::ip::address_v6(b), ntohs(port)};
    }
    net::ip::address_v4::bytes_type b{};
    std::memcpy(b.data(), addr, 4);
    return {net::ip::address_v4(b), ntohs(port)};
}

} // namespace

// ---- udp_session ----

net::ip::udp::endpoint udp_session::client_endpoint() const noexcept
{
    if (key.family == 6)
    {
        net::ip::address_v6::bytes_type b{};
        std::copy(key.src_ip.begin(), key.src_ip.end(), b.begin());
        return {net::ip::address_v6(b), ntohs(key.src_port)};
    }
    net::ip::address_v4::bytes_type b{};
    std::copy(key.src_ip.begin(), key.src_ip.begin() + 4, b.begin());
    return {net::ip::address_v4(b), ntohs(key.src_port)};
}

// ---- 会话过期堆比较器 ----

bool udp_engine::expiry_entry_cmp::operator()(
    const expiry_entry& lhs, const expiry_entry& rhs) const
{
    return lhs.at > rhs.at;
}

// ---- 生命周期 ----

udp_engine::udp_engine(net::any_io_executor strand,
    std::shared_ptr<tun_queue_writer> writer,
    const tun_config& cfg,
    engine_stats& stats,
    std::shared_ptr<buffer_accountant> account)
    : strand_(std::move(strand))
    , writer_(std::move(writer))
    , cfg_(cfg)
    , stats_(stats)
    , account_(std::move(account))
    , mtu_(cfg.mtu)
    , expiry_timer_(strand_)
{
}

udp_engine::~udp_engine()
{
    try
    {
        expiry_timer_.cancel();
    }
    catch (...)
    {
    }
}

// ---- 收包与数据交付 ----

void udp_engine::on_packet(
    const ip_packet_info& ip, const uint8_t* payload, size_t len)
{
    if (len < sizeof(udp_header))
    {
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    udp_header uh;
    std::memcpy(&uh, payload, sizeof(uh));

    const size_t udp_len = ntohs(uh.length);
    if (udp_len < sizeof(udp_header) || udp_len > len)
    {
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    len = udp_len; // 截断可能的填充字节

    // 校验 UDP 校验和（IPv4 下 0 表示发送方未计算，允许；IPv6 下校验和强制）
    const uint16_t csum = ntohs(uh.checksum);
    const bool v6_missing_checksum = ip.family == 6 && csum == 0;
    if (v6_missing_checksum ||
        (csum != 0 &&
            tcp_udp_checksum(
                ip.family, ip.src_ip, ip.dst_ip, IPPROTO_UDP_V, payload, len) !=
                0))
    {
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const udp_session_key key =
        make_udp_session_key(ip.src_ip, uh.src_port, ip.family);
    // 目标远端端点：客户端发出的数据报需送达的对端
    const auto sender = make_udp_endpoint(ip.family, ip.dst_ip, uh.dst_port);
    const uint8_t* data = payload + sizeof(udp_header);
    const size_t data_len = len - sizeof(udp_header);

    auto it = sessions_.find(key);
    if (it != sessions_.end())
    {
        // 强引用：完成回调可能与会话关闭交错排队，强引用保证收尾期间
        // 会话对象仍然有效。
        udp_session_ptr s = it->second;
        if (!s->closed)
        {
            deliver_datagram(s, data, data_len, sender);
            return;
        }
        sessions_.erase(it);
    }

    if (sessions_.size() >= cfg_.max_udp_flows)
    {
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // 无等待者时新会话必须进入待领取队列；队列达到独立上限后直接丢弃，
    // 避免应用长期不调用 async_accept 时无界积累不可达会话。
    if (pending_accepts_.empty() &&
        pending_new_sessions_.size() >= max_pending_new_sessions)
    {
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // 新建会话并通知上层
    auto s = std::make_shared<udp_session>();
    s->key = key;
    s->eng = shared_from_this();
    s->timeout = cfg_.udp_idle_timeout;
    s->expiry = udp_clock::now() + s->timeout;
    sessions_.emplace(key, s);
    stats_.udp_sessions.fetch_add(1, std::memory_order_relaxed);

    deliver_datagram(s, data, data_len, sender);
    refresh_expiry(s);

    if (!pending_accepts_.empty())
    {
        auto h = std::move(pending_accepts_.front());
        pending_accepts_.pop_front();
        post_completion_on(
            strand_, std::move(h), boost::system::error_code{}, s);
    }
    else
    {
        s->pending_accept = true;
        pending_new_sessions_.push_back(s);
        s->pending_it = std::prev(pending_new_sessions_.end());
    }
}

void udp_engine::deliver_datagram(const udp_session_ptr& s,
    const uint8_t* data,
    size_t len,
    const net::ip::udp::endpoint& sender)
{
    if (s->closed)
        return;

    refresh_expiry(s);

    if (!s->pending_reads.empty())
    {
        auto op = std::move(s->pending_reads.front());
        s->pending_reads.pop_front();

        if (op.sender)
            *op.sender = sender;

        const size_t copied_len = std::min(len, op.total);
        size_t copied = 0;
        for (auto& buf : op.buffers)
        {
            if (copied >= copied_len)
                break;

            const size_t take = std::min(buf.size(), copied_len - copied);
            std::memcpy(buf.data(), data + copied, take);
            copied += take;
        }
        post_completion_on(strand_,
            std::move(op.handler), boost::system::error_code{}, copied);
        return;
    }

    if (s->rx_bytes + len > cfg_.max_rx_queue_per_flow ||
        !account_->reserve(len))
    {
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    udp_session::datagram dg;
    dg.data.assign(data, data + len);
    dg.sender = sender;
    s->rx_datagrams.push_back(std::move(dg));
    s->rx_bytes += len;
}

// ---- 会话过期清理 ----

void udp_engine::refresh_expiry(const udp_session_ptr& s)
{
    if (s->closed)
        return;

    s->expiry = udp_clock::now() + s->timeout;

    // 堆条目节流：持续流量下每会话最多每 500ms 推入一条新条目，旧条目
    // 到期时经代次懒失效弹出，避免堆大小随包速率无界增长。节流期间不
    // 递增 expiry_gen、不清 heap_live：已推入的条目保持有效，保证会话
    // 仍会在（至多早一个节流周期的）当前 expiry 被清理。
    const auto now = udp_clock::now();
    if (s->heap_live && now - s->last_heap_push < k_expiry_push_interval)
        return;

    ++s->expiry_gen;
    s->heap_live = true;
    s->last_heap_push = now;
    expiry_heap_.push_back({s->expiry, s->expiry_gen, s});
    std::push_heap(
        expiry_heap_.begin(), expiry_heap_.end(), expiry_entry_cmp{});
    arm_expiry_timer();
}

void udp_engine::arm_expiry_timer()
{
    // 弹出失效的堆顶
    while (!expiry_heap_.empty())
    {
        auto& top = expiry_heap_.front();
        auto sp = top.session.lock();
        if (sp && !sp->closed && sp->expiry_gen == top.gen)
            break;

        std::pop_heap(
            expiry_heap_.begin(), expiry_heap_.end(), expiry_entry_cmp{});
        expiry_heap_.pop_back();
    }

    if (expiry_heap_.empty())
        return;

    const auto target = expiry_heap_.front().at;
    // 现有等待已足够早，无需重排
    if (timer_waiting_ && target >= armed_target_)
        return;

    // 取消旧等待并安排新等待；被取消的旧回调通过代次号丢弃
    timer_waiting_ = false;
    try
    {
        expiry_timer_.cancel();
    }
    catch (...)
    {
    }
    ++wait_gen_;
    const uint64_t gen = wait_gen_;
    armed_target_ = target;
    timer_waiting_ = true;
    expiry_timer_.expires_at(target);

    // 定时器以引擎 Strand 构造，完成回调已在 Strand 上，无需再派发
    expiry_timer_.async_wait(
        [self = weak_from_this(), gen](const boost::system::error_code& ec)
        {
            auto s = self.lock();
            if (!s)
                return;
            if (gen != s->wait_gen_)
                return;
            s->on_expiry_timer(ec);
        });
}

void udp_engine::on_expiry_timer(const boost::system::error_code& ec)
{
    timer_waiting_ = false;

    if (ec)
    {
        // 等待被取消（expires_at 重排）：重新按堆顶安排
        arm_expiry_timer();
        return;
    }

    const auto now = udp_clock::now();

    // 条目到期但会话已被刷新延长（节流期间未重新入堆）：收集后按当前
    // expiry 重新入堆，保证空闲会话仍会在其当前 expiry 被清理
    std::vector<udp_session_ptr> extended;
    while (!expiry_heap_.empty())
    {
        auto& top = expiry_heap_.front();
        if (top.at > now)
            break;

        auto sp = top.session.lock();
        const bool stale = !sp || sp->closed || sp->expiry_gen != top.gen;
        if (!stale && sp->expiry <= now)
            remove_session(sp);
        else if (!stale)
        {
            sp->heap_live = false;
            extended.push_back(sp);
        }

        std::pop_heap(
            expiry_heap_.begin(), expiry_heap_.end(), expiry_entry_cmp{});
        expiry_heap_.pop_back();
    }

    for (auto& sp : extended)
    {
        refresh_expiry(sp); // heap_live == false -> 立即重新入堆
    }

    arm_expiry_timer();
}

void udp_engine::remove_session(udp_session_ptr s)
{
    if (s->closed)
        return;

    auto pending_reads = std::move(s->pending_reads);
    s->pending_reads.clear();

    s->closed = true;
    sessions_.erase(s->key);
    stats_.udp_sessions.fetch_sub(1, std::memory_order_relaxed);

    if (s->pending_accept)
    {
        pending_new_sessions_.erase(s->pending_it);
        s->pending_accept = false;
    }

    if (s->rx_bytes > 0)
    {
        account_->release(s->rx_bytes);
        s->rx_bytes = 0;
    }
    s->rx_datagrams.clear();

    // 会话状态与资源已全部清理，最后再投递读完成；回调异常不会中断
    // remove_session 或定时器的后续调度。
    for (auto& op : pending_reads)
    {
        post_completion_on(strand_,
            std::move(op.handler),
            boost::system::error_code(net::error::operation_aborted),
            0);
    }
}

// ---- 会话批量控制 ----

void udp_engine::cancel_accepts()
{
    while (!pending_accepts_.empty())
    {
        auto h = std::move(pending_accepts_.front());
        pending_accepts_.pop_front();
        post_completion_on(strand_,
            std::move(h),
            boost::system::error_code(net::error::operation_aborted),
            nullptr);
    }
}

void udp_engine::close_all()
{
    try
    {
        expiry_timer_.cancel();
    }
    catch (...)
    {
    }
    timer_waiting_ = false;

    std::vector<udp_session_ptr> all;
    all.reserve(sessions_.size());
    for (auto& [key, s] : sessions_)
    {
        (void)key;
        all.push_back(s);
    }

    for (auto& s : all)
    {
        remove_session(s);
    }

    pending_new_sessions_.clear();
    cancel_accepts();
    expiry_heap_.clear();
}

// ---- udp_session 外部回调（tun_udp_socket 接口）----

void udp_session_close(udp_session_ptr session)
{
    if (!session)
        return;

    auto eng = session->eng.lock();
    if (!eng)
        return;

    net::dispatch(eng->strand(),
        [session, eng]()
        {
            eng->remove_session(session);
        });
}

void udp_session_set_timeout(
    udp_session_ptr session, std::chrono::seconds timeout)
{
    if (!session)
        return;
    auto eng = session->eng.lock();
    if (!eng)
        return;
    net::dispatch(eng->strand(),
        [session, eng, timeout]()
        {
            if (session->closed)
                return;
            session->timeout = timeout;
            eng->refresh_expiry(session);
        });
}

bool udp_session_is_open(const udp_session_ptr& session)
{
    return session && !session->closed && !session->eng.expired();
}

} // namespace detail
} // namespace tunio
