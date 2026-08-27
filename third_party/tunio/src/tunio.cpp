//
// tunio.cpp
// ~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tunio/tunio.hpp"

#include "ip_headers.hpp"
#include "tunio_impl.hpp"

#include <boost/asio.hpp>

#include <algorithm>
#include <cstring>
#include <future>

namespace tunio {
namespace detail {

tunio_impl::tunio_impl(net::io_context &ctx)
    : strand_ex_(net::make_strand(ctx))
    , device_(std::make_unique<packet_device>(ctx))
    , read_buf_(2048, 64)
{
}

tunio_impl::~tunio_impl()
{
    // 关闭引擎以打破定时器回调（self 捕获）形成的引用环：sweep/ack/expiry
    // 定时器用 shared_from_this 自持有，未显式调用 close() 直接析构时若不
    // 在此取消，引擎及流表将无法释放，io_context 也不会退出。
    if (tcp_) {
        tcp_->close_all();
    }
    if (udp_) {
        udp_->close_all();
    }
    if (writer_) {
        writer_->cancel_all();
    }
    if (device_) {
        device_->close();
    }
}

bool tunio_impl::open(const tun_config &cfg, boost::system::error_code &ec)
{
    // 重建前先同步收尾上一代实例（含 close() 后立即 reopen 的场景）：
    // close() 的异步清理会因 epoch 失效而放弃，旧引擎的定时器与读循环
    // 必须在此处于 Strand 上同步停止，否则 io_context 将残留任务无法退出。
    const bool had_engine = open_.exchange(false, std::memory_order_acq_rel) ||
                            tcp_ || udp_ || writer_ || device_->is_open();
    if (had_engine) {
        std::promise<void> done;
        net::dispatch(strand_ex_, [this, &done]() {
            if (tcp_) {
                tcp_->close_all();
            }
            if (udp_) {
                udp_->close_all();
            }
            if (writer_) {
                writer_->cancel_all();
            }
            if (device_) {
                device_->close();
            }
            done.set_value();
        });
        done.get_future().wait();
    }
    ec = {};
    cfg_ = cfg;

    // 解析本地虚拟 IP（用于 ICMP/ICMPv6 回显等），允许为空
    have_ip4_ = false;
    have_ip6_ = false;
    local_ip_ = net::ip::address();
    if (!cfg.ipv4_addr.empty()) {
        boost::system::error_code parse_ec;
        const auto v4 = net::ip::make_address_v4(cfg.ipv4_addr, parse_ec);
        if (parse_ec) {
            ec = parse_ec;
            return false;
        }
        local_ip_ = v4;
        const auto b = v4.to_bytes();
        std::copy(b.begin(), b.end(), local_ip4_);
        have_ip4_ = true;
    }
    if (!cfg.ipv6_addr.empty()) {
        boost::system::error_code parse_ec;
        const auto v6 = net::ip::make_address_v6(cfg.ipv6_addr, parse_ec);
        if (parse_ec) {
            ec = parse_ec;
            return false;
        }
        if (!have_ip4_) {
            local_ip_ = v6;
        }
        const auto b = v6.to_bytes();
        std::copy(b.begin(), b.end(), local_ip6_);
        have_ip6_ = true;
    }

    // 设备初始化：优先使用外部句柄注入
    if (cfg.external_handle != invalid_native_handle) {
        if (!device_->assign(cfg.external_handle, cfg.external_mtu, ec)) {
            return false;
        }
    } else {
        device_config dc;
        dc.name = cfg.dev_name;
        dc.ipv4 = cfg.ipv4_addr;
        dc.netmask = cfg.netmask;
        dc.ipv6 = cfg.ipv6_addr;
        dc.ipv6_prefix_len = cfg.ipv6_prefix_len;
        dc.mtu = cfg.mtu;
        if (!device_->open(dc, ec)) {
            return false;
        }
    }

    mtu_ = device_->mtu();
    read_buf_ = packet_buffer(mtu_ + 64, 64);

    account_ = std::make_shared<buffer_accountant>();
    account_->limit = cfg.max_total_buffer;
    writer_ = std::make_unique<device_writer>(strand_ex_, *device_, stats_);
    tcp_ = std::make_shared<tcp_engine>(strand_ex_, *writer_, cfg_, stats_,
                                        account_);
    udp_ = std::make_shared<udp_engine>(strand_ex_, *writer_, cfg_, stats_,
                                        account_);
    tcp_->start_sweep();

    ++epoch_; // 递增代际：使任何在途的 close 清理任务失效，避免误关新实例
    open_.store(true, std::memory_order_release);
    start_read();
    return true;
}

void tunio_impl::close()
{
    if (!open_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    const uint64_t epoch = epoch_;
    // 捕获当前（旧）引擎：即使随后被重新 open 替换，旧引擎也可能被自身的
    // 定时器回调（self 捕获）保活，必须通过捕获的 shared_ptr 在 Strand 上
    // 取消其定时器并清理，否则 io_context 将因挂起定时器永不退出。
    auto tcp = tcp_;
    auto udp = udp_;
    net::dispatch(strand_ex_, [self = shared_from_this(), epoch, tcp, udp]() {
        if (tcp) {
            tcp->close_all();
        }
        if (udp) {
            udp->close_all();
        }
        // 设备与写队列指向共享成员（reopen 后可能已指向新实例），
        // 仅当未被重新 open 时才允许关闭，避免误关新引擎。
        if (epoch == self->epoch_) {
            if (self->writer_) {
                self->writer_->cancel_all();
            }
            if (self->device_) {
                self->device_->close();
            }
        }
    });
}

void tunio_impl::start_read()
{
    if (!open_.load(std::memory_order_acquire) || reading_) {
        return;
    }
    reading_ = true;
    read_epoch_ = epoch_;
    auto self = shared_from_this();
    device_->async_read_packet(
        read_buf_, net::bind_executor(
                       strand_ex_, [self](const boost::system::error_code &ec,
                                          size_t n) { self->on_read(ec, n); }));
}

void tunio_impl::on_read(const boost::system::error_code &ec, size_t n)
{
    reading_ = false;
    if (ec) {
        // 仅当错误来自当前代际的读操作时才视为当前引擎的设备故障；
        // 旧设备（重新 open 前）的迟到回调（如 bad_descriptor）不得关闭新引擎。
        if (read_epoch_ == epoch_ && ec != net::error::operation_aborted) {
            open_.store(false, std::memory_order_release);
        }
        if (open_.load(std::memory_order_acquire)) {
            start_read(); // 设备被 close 后重新 open 的场景：继续读取新设备
        }
        return;
    }
    // 丢弃迟到数据：引擎已关闭（close() 不递增 epoch，仅查 read_epoch_ 不够），
    // 或数据来自已被重新 open 替换的旧设备；两种情况都不得注入当前引擎。
    if (!open_.load(std::memory_order_acquire) || read_epoch_ != epoch_) {
        read_buf_.reset();
        if (open_.load(std::memory_order_acquire)) {
            start_read();
        }
        return;
    }
    read_buf_.commit(n);
    // 注：注入设备为字节流（如 socketpair）时，一次读取可能粘合/拆散多个报文；
    // 按 IP 头中的总长度逐包解析，完整处理缓冲区内全部报文（IPv4/IPv6
    // 均支持），
    // 未凑成完整报文的尾部字节保留在缓冲区内，与下一次读取拼接后继续解析。
    const uint8_t *base = read_buf_.data();
    const size_t avail = read_buf_.size();
    size_t offset = 0;
    while (offset + 4 <= avail) {
        size_t total_len = 0;
        const uint8_t version = base[offset] >> 4;
        if (version == 4) {
            if (offset + sizeof(ipv4_header) > avail) {
                break; // 头部不完整，等待续读
            }
            total_len =
                static_cast<size_t>((base[offset + 2] << 8) | base[offset + 3]);
            if (total_len < sizeof(ipv4_header)) {
                ++offset; // 非法长度：跳过该字节继续扫描
                continue;
            }
        } else if (version == 6) {
            if (offset + sizeof(ipv6_header) > avail) {
                break; // 头部不完整，等待续读
            }
            total_len =
                sizeof(ipv6_header) +
                static_cast<size_t>((base[offset + 4] << 8) | base[offset + 5]);
            if (total_len < sizeof(ipv6_header)) {
                ++offset; // 非法长度：跳过该字节继续扫描
                continue;
            }
        } else {
            ++offset; // 非 IP 报文：跳过该字节继续扫描
            continue;
        }
        if (total_len > read_buf_.capacity() - read_buf_.headroom()) {
            // 声明长度超出缓冲可容纳上限（正常 MTU 内报文不可能出现）：
            // 该报文永远无法凑齐，丢弃全部缓冲并重新开始，避免读循环停滞。
            stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
            read_buf_.reset();
            start_read();
            return;
        }
        if (offset + total_len > avail) {
            break; // 报文体不完整，等待续读
        }
        stats_.rx_packets.fetch_add(1, std::memory_order_relaxed);
        handle_packet(base + offset, total_len);
        offset += total_len;
    }
    const size_t kept = avail - offset;
    if (kept > 0) {
        read_buf_.rewind(kept);
    } else {
        read_buf_.reset();
    }
    start_read();
}

void tunio_impl::handle_packet(const uint8_t *pkt, size_t len)
{
    if (len < 20) {
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    ip_packet_info ip;
    const uint8_t *payload = nullptr;
    size_t payload_len = 0;

    const uint8_t version = pkt[0] >> 4;
    if (version == 4) {
        if (len < sizeof(ipv4_header)) {
            stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        ipv4_header h;
        std::memcpy(&h, pkt, sizeof(h));
        const size_t ihl = h.header_len();
        if (ihl < sizeof(ipv4_header) || ihl > len) {
            stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const size_t total_len = ntohs(h.total_len);
        if (total_len < ihl || total_len > len) {
            stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        // 丢弃分片包（带分片偏移或 MF 标志）：引擎不做 IP 重组
        const uint16_t frag = ntohs(h.frag_off);
        if ((frag & 0x1fff) != 0 || (frag & 0x2000) != 0) {
            stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        // 校验 IP 头部校验和
        if (verify_ipv4_checksum(pkt, ihl) != 0) {
            stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        ip.family = 4;
        ip.protocol = h.protocol;
        std::memcpy(ip.src_ip, pkt + 12, 4);
        std::memcpy(ip.dst_ip, pkt + 16, 4);
        payload = pkt + ihl;
        payload_len = total_len - ihl;
    } else if (version == 6) {
        if (len < sizeof(ipv6_header)) {
            stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        ipv6_header h;
        std::memcpy(&h, pkt, sizeof(h));
        const size_t payload_len_field = ntohs(h.payload_len);
        if (sizeof(ipv6_header) + payload_len_field > len) {
            stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        // 丢弃带扩展头的报文（Hop-by-Hop=0、Routing=43、Fragment=44、
        // AH=51、Dest-Options=60）：引擎不做 IP 重组且不解析扩展头链
        if (h.next_header == 0 || h.next_header == 43 || h.next_header == 44 ||
            h.next_header == 51 || h.next_header == 60) {
            stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        ip.family = 6;
        ip.protocol = h.next_header;
        std::memcpy(ip.src_ip, h.src_ip, 16);
        std::memcpy(ip.dst_ip, h.dst_ip, 16);
        payload = pkt + sizeof(ipv6_header);
        payload_len = payload_len_field;
    } else {
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // 环路与本地地址防护：丢弃源地址为本地虚拟 IP，或源/目标属于本机
    // 链路本地保留段（127.0.0.0/8、0.0.0.0/8、::/128、::1/128、fe80::/10）
    // 的入包。这类地址只会来自本机进程（未绕过 tun 的出站回环、mDNS 等
    // 链路本地流量），正常客户端流量不可能使用；若不拦截会被当作新连接
    // 无限放大，形成连接风暴。目标为本地虚拟 IP 的入包（如 ICMP 回显）
    // 仍保留处理。默认启用；编译时定义 TUNIO_DISABLE_LOOPBACK_GUARD
    // 可关闭该防护。
#ifndef TUNIO_DISABLE_LOOPBACK_GUARD
    const auto is_reserved_local = [](const uint8_t *a, int family) noexcept {
        if (family == 4) {
            return a[0] == 0x7f || a[0] == 0x00; // 127.0.0.0/8, 0.0.0.0/8
        }
        const bool zero15 =
            std::all_of(a, a + 15, [](uint8_t b) { return b == 0; });
        if (zero15) {
            return a[15] == 0 || a[15] == 1; // ::/128, ::1/128
        }
        return a[0] == 0xfe && (a[1] & 0xc0) == 0x80; // fe80::/10
    };
    const bool src_local = (have_ip4_ && ip.family == 4 &&
                            std::memcmp(ip.src_ip, local_ip4_, 4) == 0) ||
                           (have_ip6_ && ip.family == 6 &&
                            std::memcmp(ip.src_ip, local_ip6_, 16) == 0);
    if (src_local || is_reserved_local(ip.src_ip, ip.family) ||
        is_reserved_local(ip.dst_ip, ip.family)) {
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
#endif

    switch (ip.protocol) {
    case IPPROTO_ICMP_V:
        handle_icmp(ip, payload, payload_len);
        break;
    case IPPROTO_ICMPV6_V:
        handle_icmpv6(ip, payload, payload_len);
        break;
    case IPPROTO_TCP_V:
        tcp_->on_packet(ip, payload, payload_len);
        break;
    case IPPROTO_UDP_V:
        udp_->on_packet(ip, payload, payload_len);
        break;
    default:
        stats_.rx_dropped.fetch_add(1, std::memory_order_relaxed);
        break;
    }
}

void tunio_impl::handle_icmp(const ip_packet_info &ip, const uint8_t *icmp,
                             size_t icmp_len)
{
    if (!have_ip4_ || icmp_len < 8) {
        return;
    }
    // 仅响应发往本地虚拟 IP 的 Echo Request
    if (std::memcmp(ip.dst_ip, local_ip4_, 4) != 0 || icmp[0] != 8) {
        return;
    }
    // 校验 ICMP 校验和
    if (ip_checksum(icmp, icmp_len) != 0) {
        return;
    }

    packet_buffer reply = writer_->acquire(20 + icmp_len + 64, 64);
    reply.resize(20 + icmp_len);
    uint8_t *out = reply.data();

    build_ip_header(out, 4, ip.dst_ip, ip.src_ip, IPPROTO_ICMP_V, 20 + icmp_len,
                    writer_->alloc_ip_id());

    uint8_t *oicmp = out + 20;
    std::memcpy(oicmp, icmp, icmp_len);
    oicmp[0] = 0; // Type = Echo Reply
    oicmp[2] = 0; // 清零校验和字段后再计算
    oicmp[3] = 0;
    const uint16_t csum = ip_checksum(oicmp, icmp_len);
    oicmp[2] = static_cast<uint8_t>(csum >> 8);
    oicmp[3] = static_cast<uint8_t>(csum & 0xff);

    writer_->async_write_and_forget(std::move(reply));
    stats_.icmp_replies.fetch_add(1, std::memory_order_relaxed);
}

void tunio_impl::handle_icmpv6(const ip_packet_info &ip, const uint8_t *icmp,
                               size_t icmp_len)
{
    if (!have_ip6_ || icmp_len < 8) {
        return;
    }
    // 仅响应发往本地虚拟 IP 的 Echo Request
    if (std::memcmp(ip.dst_ip, local_ip6_, 16) != 0 || icmp[0] != 128) {
        return;
    }
    // ICMPv6 校验和必须包含 IPv6 伪头部
    if (tcp_udp_checksum(6, ip.src_ip, ip.dst_ip, IPPROTO_ICMPV6_V, icmp,
                         icmp_len) != 0) {
        return;
    }

    packet_buffer reply = writer_->acquire(40 + icmp_len + 64, 64);
    reply.resize(40 + icmp_len);
    uint8_t *out = reply.data();

    build_ip_header(out, 6, ip.dst_ip, ip.src_ip, IPPROTO_ICMPV6_V,
                    40 + icmp_len, 0);

    uint8_t *oicmp = out + 40;
    std::memcpy(oicmp, icmp, icmp_len);
    oicmp[0] = 129; // Type = Echo Reply
    oicmp[2] = 0;   // 清零校验和字段后再计算
    oicmp[3] = 0;
    const uint16_t csum = tcp_udp_checksum(6, ip.dst_ip, ip.src_ip,
                                           IPPROTO_ICMPV6_V, oicmp, icmp_len);
    oicmp[2] = static_cast<uint8_t>(csum >> 8);
    oicmp[3] = static_cast<uint8_t>(csum & 0xff);

    writer_->async_write_and_forget(std::move(reply));
    stats_.icmp_replies.fetch_add(1, std::memory_order_relaxed);
}

} // namespace detail

tunio::tunio(net::io_context &ctx)
    : impl_(std::make_shared<detail::tunio_impl>(ctx))
{
}

tunio::~tunio() = default;

bool tunio::open(const tun_config &config, boost::system::error_code &ec)
{
    return impl_->open(config, ec);
}

void tunio::close()
{
    impl_->close();
}

bool tunio::is_open() const noexcept
{
    return impl_->is_open();
}

size_t tunio::mtu() const noexcept
{
    return impl_->mtu();
}

net::ip::address tunio::local_address() const noexcept
{
    return impl_->local_address();
}

const engine_stats &tunio::stats() const noexcept
{
    return impl_->stats();
}

tunio::executor_type tunio::get_executor() const noexcept
{
    return impl_->strand();
}

} // namespace tunio
