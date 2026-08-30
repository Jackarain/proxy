//
// udp_engine.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "device_writer.hpp"
#include "ip_headers.hpp"
#include "tcp_engine.hpp"
#include "tunio/tun_config.hpp"

#include <boost/asio.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tunio {
namespace net = boost::asio;

namespace detail {

class device_writer;
struct buffer_accountant;
class udp_engine;

// ---- UDP 引擎常用类型 ----
// 时间统一采用单调时钟（steady_clock），不受系统时间调整影响
using udp_clock = std::chrono::steady_clock;
using udp_time_point = udp_clock::time_point;
// UDP 数据报读完成处理器（与 Boost.Asio 异步操作完成签名一致）
using udp_read_handler =
    net::any_completion_handler<void(boost::system::error_code, size_t)>;

// UDP 会话：由客户端三元组唯一标识（1 对 N，可向任意远端收发），承载
// 数据报收发与空闲超时
struct udp_session : public std::enable_shared_from_this<udp_session>
{
    udp_session_key key;
    // 弱引用避免与引擎（sessions_ 持有会话强引用）构成循环引用；访问前须
    // lock()，引擎已销毁时以 bad_descriptor / 空操作优雅失败。
    std::weak_ptr<udp_engine> eng;

    bool closed = false;
    bool accepted = false;
    std::chrono::seconds timeout{30};
    udp_time_point expiry;
    uint64_t expiry_gen = 0; // 与堆条目配对，用于懒失效

    // 收到的数据报及其目标远端端点（客户端发出的数据报需送达的对端）
    struct datagram
    {
        std::vector<uint8_t> data;
        net::ip::udp::endpoint sender;
    };
    std::deque<datagram> rx_datagrams;
    size_t rx_bytes = 0;

    struct read_op
    {
        mutable_buffer_sequence buffers;
        size_t total = 0;
        net::ip::udp::endpoint *sender = nullptr; // 出参，完成时填充目标远端
        udp_read_handler handler;
    };
    std::deque<read_op> pending_reads;

    bool is_open() const
    {
        return !closed;
    }

    // 客户端（虚拟网内）端点：会话键中的源地址与源端口
    net::ip::udp::endpoint client_endpoint() const
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
};

// 会话指针 / accept 完成处理器 / 会话表（依赖 udp_session 定义）
using udp_session_ptr = std::shared_ptr<udp_session>;
using udp_accept_handler =
    net::any_completion_handler<void(boost::system::error_code,
        udp_session_ptr)>;
using udp_session_map =
    boost::unordered_flat_map<udp_session_key, udp_session_ptr,
        std::hash<udp_session_key>>;

template <typename Handler>
void udp_session_start_receive(udp_session_ptr session,
    mutable_buffer_sequence buffers, size_t total, size_t,
    net::ip::udp::endpoint &, Handler);

template <typename Handler>
void udp_session_start_send(udp_session_ptr session,
    const net::ip::udp::endpoint &, const_buffer_sequence, size_t, Handler);

class udp_engine : public std::enable_shared_from_this<udp_engine>
{
public:
    udp_engine(net::any_io_executor strand, device_writer &writer,
        const tun_config &cfg, engine_stats &stats,
        std::shared_ptr<buffer_accountant> account);
    ~udp_engine();

    // 处理一个 UDP 数据报（Strand 上调用）
    void on_packet(const ip_packet_info &ip, const uint8_t *payload,
        size_t len);

    // 等待新会话；完成回调签名 void(error_code, shared_ptr<udp_session>)
    template <typename Handler> void async_accept(Handler handler);
    void cancel_accepts();
    void close_all();
    size_t session_count() const
    {
        return sessions_.size();
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
    size_t mtu() const
    {
        return mtu_;
    }

private:
    friend struct udp_session;

    template <typename Handler>
    friend void udp_session_start_receive(udp_session_ptr,
        mutable_buffer_sequence, size_t, net::ip::udp::endpoint &, Handler);
    template <typename Handler>
    friend void udp_session_start_send(udp_session_ptr,
        const net::ip::udp::endpoint &, const_buffer_sequence, size_t,
        Handler);

    friend void udp_session_close(udp_session_ptr);
    friend void udp_session_set_timeout(udp_session_ptr,
        std::chrono::seconds);

    void deliver_datagram(const udp_session_ptr &s, const uint8_t *data,
        size_t len, const net::ip::udp::endpoint &sender);
    void refresh_expiry(const udp_session_ptr &s);
    void arm_expiry_timer();
    void on_expiry_timer(const boost::system::error_code &ec);
    void remove_session(udp_session_ptr s);

    net::any_io_executor strand_;
    device_writer &writer_;
    tun_config cfg_;
    engine_stats &stats_;
    std::shared_ptr<buffer_accountant> account_;
    size_t mtu_ = 1500;

    udp_session_map sessions_;
    std::deque<udp_accept_handler> pending_accepts_;
    std::deque<udp_session_ptr> pending_new_sessions_;

    // 最小堆：堆顶为最早即将过期的会话
    struct expiry_entry
    {
        udp_time_point at;
        uint64_t gen;
        std::weak_ptr<udp_session> session;
    };
    struct expiry_entry_cmp
    {
        bool operator()(const expiry_entry &lhs,
            const expiry_entry &rhs) const
        {
            return lhs.at > rhs.at;
        }
    };
    std::vector<expiry_entry> expiry_heap_;
    net::steady_timer expiry_timer_;
    bool timer_waiting_ = false;
    udp_time_point armed_target_{};
    uint64_t wait_gen_ = 0;
};

// ---- 供 tun_udp_socket 调用的入口（内部自动派发到 Strand）----
template <typename Handler>
void udp_session_start_receive(udp_session_ptr session,
    mutable_buffer_sequence buffers, size_t total,
    net::ip::udp::endpoint &sender, Handler handler)
{
    if (!session) {
        handler(boost::system::error_code(net::error::bad_descriptor), 0);
        return;
    }
    auto eng = session->eng.lock();
    if (!eng) {
        handler(boost::system::error_code(net::error::bad_descriptor), 0);
        return;
    }
    auto strand = eng->strand();
    net::dispatch(strand, [s = std::move(session), eng,
        buffers = std::move(buffers), total, &sender,
        handler = std::move(handler)]() mutable {
        auto &session = *s;
        if (session.closed) {
            handler(boost::system::error_code(net::error::bad_descriptor), 0);
            return;
        }
        if (total == 0) {
            handler(boost::system::error_code{}, 0);
            return;
        }
        if (!session.rx_datagrams.empty()) {
            auto dg = std::move(session.rx_datagrams.front());
            session.rx_datagrams.pop_front();
            session.rx_bytes -= dg.data.size();
            eng->account().release(dg.data.size());
            if (dg.data.size() > total) {
                handler(boost::system::error_code(net::error::message_size), 0);
                return;
            }
            sender = dg.sender;
            size_t copied = 0;
            for (auto &buf : buffers) {
                if (copied >= dg.data.size()) {
                    break;
                }
                const size_t take =
                    std::min(buf.size(), dg.data.size() - copied);
                std::memcpy(buf.data(), dg.data.data() + copied, take);
                copied += take;
            }
            handler(boost::system::error_code{}, dg.data.size());
            return;
        }
        session.pending_reads.push_back(
            {std::move(buffers), total, &sender, std::move(handler)});
    });
}

template <typename Handler>
void udp_session_start_send(udp_session_ptr session,
    const net::ip::udp::endpoint &remote, const_buffer_sequence buffers,
    size_t total, Handler handler)
{
    if (!session) {
        handler(boost::system::error_code(net::error::bad_descriptor), 0);
        return;
    }
    auto eng = session->eng.lock();
    if (!eng) {
        handler(boost::system::error_code(net::error::bad_descriptor), 0);
        return;
    }
    auto strand = eng->strand();
    net::dispatch(strand, [s = std::move(session), eng, remote,
        buffers = std::move(buffers), total,
        handler = std::move(handler)]() mutable {
        auto &session = *s;
        if (session.closed) {
            handler(boost::system::error_code(net::error::bad_descriptor), 0);
            return;
        }
        const int family = session.key.family;
        const bool remote_v6 = remote.address().is_v6() &&
                               !remote.address().to_v6().is_v4_mapped();
        if ((family == 6) != remote_v6) {
            handler(
                boost::system::error_code(
                    net::error::address_family_not_supported),
                0);
            return;
        }
        const size_t ip_hdr_len = ip_header_size(family);
        const size_t mtu = eng->mtu();

        uint8_t src_addr[16] = {};
        if (remote_v6) {
            const auto b = remote.address().to_v6().to_bytes();
            std::memcpy(src_addr, b.data(), 16);
        } else {
            const auto b = remote.address().to_v4().to_bytes();
            std::memcpy(src_addr, b.data(), 4);
        }

        if (total > mtu - ip_hdr_len - sizeof(udp_header)) {
            if (family != 4) {
                // IPv6 无扩展头分片支持：拒绝超出 MTU 的报文
                handler(boost::system::error_code(
                    net::error::message_size),
                    0);
                return;
            }
            // ---- IPv4 大报文分片（RFC 791）----
            // 数据报超出 MTU 时按 8 字节对齐拆分为多个 IP 分片依次写入设备：
            // 首片携带 UDP 头与完整校验和，后续片仅携带载荷，DF 位清零。
            eng->refresh_expiry(s);

            const size_t max_frag_payload =
                (mtu - sizeof(ipv4_header)) & ~size_t(7);
            // 首片 IP 载荷 = UDP 头 + 数据，需保证第二片偏移仍为 8 的倍数
            const size_t first_data =
                max_frag_payload > sizeof(udp_header)
                    ? (max_frag_payload - sizeof(udp_header)) & ~size_t(7)
                    : 0;

            std::vector<uint8_t> full(sizeof(udp_header) + total);
            auto *uh = reinterpret_cast<udp_header *>(full.data());
            uh->src_port = htons(remote.port());
            uh->dst_port = session.key.src_port;
            uh->length = htons(static_cast<uint16_t>(full.size()));
            uh->checksum = 0;
            uint8_t *dst = full.data() + sizeof(udp_header);
            for (const auto &buf : buffers) {
                std::memcpy(dst, buf.data(), buf.size());
                dst += buf.size();
            }
            uint16_t csum = tcp_udp_checksum(
                4, src_addr, session.key.src_ip.data(), IPPROTO_UDP_V,
                full.data(), full.size());
            if (csum == 0) {
                csum = 0xffff;
            }
            uh->checksum = htons(csum);

            const uint16_t ip_id = eng->writer().alloc_ip_id();
            // off 为相对完整 UDP 数据报（含 UDP 头）的偏移，即分片偏移字段
            // 的字节基准：首片消费 UDP 头 + first_data，后续片消费载荷。
            size_t off = 0;
            while (off < full.size()) {
                const size_t remain = full.size() - off;
                const size_t frag_data = off == 0
                    ? (std::min)(sizeof(udp_header) + first_data, remain)
                    : (std::min)(max_frag_payload, remain);
                const size_t frag_total = sizeof(ipv4_header) + frag_data;
                packet_buffer frag = eng->writer().acquire(mtu + 64, 64);
                frag.resize(frag_total);
                uint8_t *fb = frag.data();
                auto *ip = reinterpret_cast<ipv4_header *>(fb);
                ip->version_ihl = 0x45;
                ip->tos = 0;
                ip->total_len = htons(static_cast<uint16_t>(frag_total));
                ip->id = htons(ip_id);
                const bool last = off + frag_data >= full.size();
                ip->frag_off = htons(static_cast<uint16_t>(
                    (last ? 0 : 0x2000) |
                    static_cast<uint16_t>(off / 8)));
                ip->ttl = 64;
                ip->protocol = IPPROTO_UDP_V;
                ip->checksum = 0;
                std::memcpy(&ip->src_ip, src_addr, 4);
                std::memcpy(&ip->dst_ip, session.key.src_ip.data(), 4);
                ip->checksum =
                    htons(ipv4_checksum(fb, sizeof(ipv4_header)));
                std::memcpy(fb + sizeof(ipv4_header), full.data() + off,
                    frag_data);
                off += frag_data;
                eng->writer().async_write_and_forget(std::move(frag));
            }
            // 分片全部入队即视为发送完成：数据已由引擎持有，直接调用 handler
            std::move(handler)(boost::system::error_code{}, total);
            return;
        }
        eng->refresh_expiry(s);

        // 构造 IP + UDP 报文（源地址为客户端请求的目标地址）
        const size_t total_len = ip_hdr_len + sizeof(udp_header) + total;
        packet_buffer pkt = eng->writer().acquire(mtu + 64, 64);
        pkt.resize(total_len);
        uint8_t *base = pkt.data();

        build_ip_header(base, family, src_addr,
            session.key.src_ip.data(), IPPROTO_UDP_V, total_len,
            eng->writer().alloc_ip_id());

        auto *uh = reinterpret_cast<udp_header *>(base + ip_hdr_len);
        uh->src_port = htons(remote.port());
        uh->dst_port = session.key.src_port;
        uh->length = htons(static_cast<uint16_t>(sizeof(udp_header) + total));
        uh->checksum = 0;
        uint8_t *dst = base + ip_hdr_len + sizeof(udp_header);
        for (const auto &buf : buffers) {
            std::memcpy(dst, buf.data(), buf.size());
            dst += buf.size();
        }
        uint16_t csum = tcp_udp_checksum(
            family, src_addr, session.key.src_ip.data(),
            IPPROTO_UDP_V, base + ip_hdr_len, sizeof(udp_header) + total);
        // IPv6 下 UDP 校验和不可为 0；按 RFC 768/8200 以 0xffff 替代
        if (csum == 0) {
            csum = 0xffff;
        }
        uh->checksum = htons(csum);

        const size_t sent = total;
        // device_writer 保证完成回调在引擎 Strand 上触发，无需再派发
        eng->writer().async_write(
            std::move(pkt),
            [h = std::move(handler), sent](
                const boost::system::error_code &ec, size_t) mutable {
                // 设备写失败时透传错误码，避免向调用方误报成功
                std::move(h)(ec, ec ? 0 : sent);
            });
    });
}

template <typename Handler> void udp_engine::async_accept(Handler handler)
{
    while (!pending_new_sessions_.empty()) {
        auto s = std::move(pending_new_sessions_.front());
        pending_new_sessions_.pop_front();
        if (s->closed) {
            continue;
        }
        s->accepted = true;
        handler(boost::system::error_code{}, std::move(s));
        return;
    }
    pending_accepts_.push_back(std::move(handler));
}

void udp_session_close(udp_session_ptr session);
void udp_session_set_timeout(udp_session_ptr session,
    std::chrono::seconds timeout);
bool udp_session_is_open(const udp_session_ptr &session);

} // namespace detail
} // namespace tunio
