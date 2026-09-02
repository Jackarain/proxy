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

// ---- accept 取消（自动派发到 Strand）----

void tunio_impl::cancel_tcp_accepts()
{
    net::dispatch(strand_ex_,
        [tcp = tcp_]()
        {
            if (tcp)
                tcp->cancel_accepts();
        });
}

void tunio_impl::cancel_udp_accepts()
{
    net::dispatch(strand_ex_,
        [udp = udp_]()
        {
            if (udp)
                udp->cancel_accepts();
        });
}

// ---- 生命周期 ----

tunio_impl::tunio_impl(net::io_context& ctx, bool single_thread)
    : strand_ex_(single_thread ? net::any_io_executor(ctx.get_executor())
                               : net::any_io_executor(net::make_strand(ctx)))
    , device_(std::make_shared<tun_device>(ctx))
    , stats_(std::make_shared<engine_stats>())
{
}

tunio_impl::~tunio_impl()
{
    // 关闭引擎以打破定时器回调（self 捕获）形成的引用环：sweep/ack/expiry
    // 定时器用 shared_from_this 自持有，未显式调用 close() 直接析构时若不
    // 在此取消，引擎及流表将无法释放，io_context 也不会退出。
    if (tcp_)
        tcp_->close_all();
    if (udp_)
        udp_->close_all();
    if (writer_)
        writer_->cancel_all();
    if (device_)
        device_->close();
}

bool tunio_impl::open(const tun_config& cfg, boost::system::error_code& ec)
{
    // 重建前先同步收尾上一代实例（含 close() 后立即 reopen 的场景）：
    // close() 的异步清理会因 epoch 失效而放弃，旧引擎的定时器与读循环
    // 必须在此处于 Strand 上同步停止，否则 io_context 将残留任务无法退出。
    const bool had_engine = open_.exchange(false, std::memory_order_acq_rel) ||
        tcp_ || udp_ || writer_ || device_->is_open();
    if (had_engine)
    {
        std::promise<void> done;
        net::dispatch(strand_ex_,
            [this, &done]()
            {
                if (tcp_)
                    tcp_->close_all();
                if (udp_)
                    udp_->close_all();
                if (writer_)
                    writer_->cancel_all();
                if (device_)
                    device_->close();
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
    if (!cfg.ipv4_addr.empty())
    {
        boost::system::error_code parse_ec;
        const auto v4 = net::ip::make_address_v4(cfg.ipv4_addr, parse_ec);
        if (parse_ec)
        {
            ec = parse_ec;
            return false;
        }
        local_ip_ = v4;
        const auto b = v4.to_bytes();
        std::copy(b.begin(), b.end(), local_ip4_);
        have_ip4_ = true;
    }
    if (!cfg.ipv6_addr.empty())
    {
        boost::system::error_code parse_ec;
        const auto v6 = net::ip::make_address_v6(cfg.ipv6_addr, parse_ec);
        if (parse_ec)
        {
            ec = parse_ec;
            return false;
        }
        if (!have_ip4_)
            local_ip_ = v6;
        const auto b = v6.to_bytes();
        std::copy(b.begin(), b.end(), local_ip6_);
        have_ip6_ = true;
    }

    // 设备初始化：优先多句柄注入（Linux TUN 多队列），其次单句柄注入，
    // 最后自主打开。
    if (!cfg.external_handles.empty())
    {
        if (!device_->assign_queues(
                cfg.external_handles, cfg.external_mtu, cfg.utun_prefix, ec))
        {
            return false;
        }
    }
    else if (cfg.external_handle != invalid_native_handle)
    {
        if (!device_->assign(
                cfg.external_handle, cfg.external_mtu, cfg.utun_prefix, ec))
        {
            return false;
        }
    }
    else
    {
        device_config dc;
        dc.name = cfg.dev_name;
        dc.ipv4 = cfg.ipv4_addr;
        dc.netmask = cfg.netmask;
        dc.ipv6 = cfg.ipv6_addr;
        dc.ipv6_prefix_len = cfg.ipv6_prefix_len;
        dc.mtu = cfg.mtu;
        dc.num_queues = cfg.num_queues;
        if (!device_->open(dc, ec))
            return false;
    }

    mtu_ = device_->mtu();
    // 并发读槽按队列数分配：每队列均分 k_read_slots（至少 1 个），单队列
    // 保持原有 32 槽行为；旧代际残留的 in-flight 标记一律清空，确保重建后
    // start_read() 能为全部槽重新发起读取（旧设备迟到回调由代际检查拦截）.
    num_queues_ = std::clamp(device_->queue_count(), size_t{1}, k_max_queues);
    slots_per_queue_ = std::max<size_t>(1, k_read_slots / num_queues_);
    read_bufs_.clear();
    read_bufs_.reserve(num_queues_ * slots_per_queue_);
    for (size_t i = 0; i < num_queues_ * slots_per_queue_; ++i)
    {
        read_bufs_.emplace_back(mtu_ + 64, 64);
    }
    read_inflight_.assign(num_queues_ * slots_per_queue_, false);

    account_ = std::make_shared<buffer_accountant>();
    account_->limit = cfg.max_total_buffer;
    writer_ = std::make_shared<tun_queue_writer>(strand_ex_, device_, stats_);
    tcp_ = std::make_shared<tcp_engine>(
        strand_ex_, writer_, cfg_, *stats_, account_);
    udp_ = std::make_shared<udp_engine>(
        strand_ex_, writer_, cfg_, *stats_, account_);
    tcp_->start_sweep();

    epoch_.store(
        epoch_.load(std::memory_order_relaxed) + 1, std::memory_order_release);
    open_.store(true, std::memory_order_release);
    start_read();
    return true;
}

// ---- 打开与关闭 ----

void tunio_impl::close()
{
    if (!open_.exchange(false, std::memory_order_acq_rel))
        return;

    const uint64_t epoch = epoch_.load(std::memory_order_acquire);

    // 捕获当前（旧）引擎：即使随后被重新 open 替换，旧引擎也可能被自身的
    // 定时器回调（self 捕获）保活，必须通过捕获的 shared_ptr 在 Strand 上
    // 取消其定时器并清理，否则 io_context 将因挂起定时器永不退出。
    auto tcp = tcp_;
    auto udp = udp_;

    net::dispatch(strand_ex_,
        [self = shared_from_this(), epoch, tcp, udp]()
        {
            // 引擎会话清理（close_all 触发完成回调恢复上层协程）可能抛异常
            //（如协程访问已释放对象），必须捕获，确保设备与写队列一定被
            // 关闭/取消：否则旧引擎的读循环继续运行，注入的 tun fd 无法
            // 撤销，反复启停后 io_context 残留任务忙跑。
            try
            {
                if (tcp)
                    tcp->close_all();
            }
            catch (...)
            {
                // 忽略会话清理异常，保证设备与写队列一定被关闭/取消。
            }

            try
            {
                if (udp)
                    udp->close_all();
            }
            catch (...)
            {
                // 忽略会话清理异常，保证设备与写队列一定被关闭/取消。
            }

            // 设备与写队列指向共享成员（reopen 后可能已指向新实例），
            // 仅当未被重新 open 时才允许关闭，避免误关新引擎。
            if (epoch == self->epoch_)
            {
                try
                {
                    if (self->writer_)
                        self->writer_->cancel_all();
                }
                catch (...)
                {
                    // 忽略取消异常，设备仍须关闭。
                }

                if (self->device_)
                    self->device_->close();
            }
        });
}

// ---- 设备读泵 ----

void tunio_impl::start_read()
{
    if (!open_.load(std::memory_order_acquire))
        return;

    // 槽总数 = num_queues_ * slots_per_queue_（队列数 > 32 时每队列 1 槽，
    // 总数可能大于 k_read_slots），必须按实际槽数启动，保证每个队列
    // 至少挂起一个读取，否则后半队列的入站包无人读取。
    const size_t total = read_inflight_.size();

    for (size_t i = 0; i < total; ++i)
    {
        start_read_slot(i);
    }
}

void tunio_impl::start_read_slot(size_t index)
{
    if (!open_.load(std::memory_order_acquire) ||
        index >= read_inflight_.size() || read_inflight_[index])
    {
        return;
    }

    read_inflight_[index] = true;

    // 槽号按队列连续分配：槽 index 归属队列 index / slots_per_queue_，
    // 多队列下各队列的读互不阻塞（独立 fd 上的异步读）.
    const size_t queue = index / slots_per_queue_;

    // 弱引用避免读回调自持有形成引用环：引擎释放后迟到读回调直接跳过
    auto self = weak_from_this();
    const uint64_t epoch = epoch_.load(std::memory_order_acquire);

    device_->async_read_packet(read_bufs_[index],
        queue,
        net::bind_executor(strand_ex_,
            [self, index, epoch](const boost::system::error_code& ec, size_t n)
            {
                if (auto s = self.lock())
                    s->on_read(ec, n, index, epoch);
            }));
}

void tunio_impl::on_read(
    const boost::system::error_code& ec, size_t n, size_t index, uint64_t epoch)
{
    // 迟到回调（引擎已关闭、或设备已 close 后重新 open 的旧代际读）：
    // 不触碰任何读状态，槽位标记由 open() 的 fill(false) 与当前代际回调
    // 复位，避免误清重建后已重新发起读取的槽。
    // 先读 open_（acquire 与 open() 的 release 存储同步），再读 epoch_：
    // open_ 为 true 时 epoch_ 的递增必然可见，旧代际回调（epoch 不匹配）
    // 一定被拦截，杜绝"两次独立装载"的 TOCTOU 穿过守卫。
    if (!open_.load(std::memory_order_acquire) ||
        epoch != epoch_.load(std::memory_order_acquire))
    {
        return;
    }
    read_inflight_[index] = false;
    if (ec)
    {
        // 非取消错误视为设备故障：关闭引擎，避免读循环空转。
        if (ec != net::error::operation_aborted)
            open_.store(false, std::memory_order_release);
        if (open_.load(std::memory_order_acquire))
            start_read_slot(index); // 设备被 close 后重新 open 的场景：续读
        return;
    }
    packet_buffer& buf = read_bufs_[index];
    buf.commit(n);
    // TUN 设备为包语义：每次读取恰为一个完整 IP 报文，无需按流拆包拼接。
    // 仅保留对非法/残缺报文的防御性检查（正常 TUN 包不会触发）.
    const uint8_t* base = buf.data();
    const size_t avail = buf.size();
    if (avail < sizeof(ipv4_header))
    {
        stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
        buf.reset();
        start_read_slot(index);
        return;
    }
    size_t total_len = 0;
    const uint8_t version = base[0] >> 4;
    if (version == 4)
    {
        total_len = static_cast<size_t>((base[2] << 8) | base[3]);
        if (total_len < sizeof(ipv4_header))
        {
            stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
            buf.reset();
            start_read_slot(index);
            return;
        }
    }
    else if (version == 6)
    {
        total_len =
            sizeof(ipv6_header) + static_cast<size_t>((base[4] << 8) | base[5]);
        if (total_len < sizeof(ipv6_header))
        {
            stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
            buf.reset();
            start_read_slot(index);
            return;
        }
    }
    else
    {
        stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
        buf.reset();
        start_read_slot(index);
        return;
    }
    if (total_len > avail)
    {
        // 声明长度超限（读到的字节不足一个完整报文）：按包语义丢弃
        // （真实 TUN 包设备不会出现半包；防御非法/残缺报文）。
        stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
        buf.reset();
        start_read_slot(index);
        return;
    }
    stats_->rx_packets.fetch_add(1, std::memory_order_relaxed);
    // 用户完成回调（如 TCP 直投路径）可能抛异常：捕获后仍复位读槽并
    // 继续读泵，避免单个回调异常使 32 槽读路径整体停摆；异常被吞掉
    //（不再经 io.run() 传播），以换读取循环的持续可用。
    try
    {
        handle_packet(base, total_len);
    }
    catch (...)
    {
    }
    buf.reset();
    start_read_slot(index);
}

// ---- 报文分发 ----

void tunio_impl::handle_packet(const uint8_t* pkt, size_t len)
{
    if (len < 20)
    {
        stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    ip_packet_info ip;
    const uint8_t* payload = nullptr;
    size_t payload_len = 0;

    const uint8_t version = pkt[0] >> 4;
    if (version == 4)
    {
        if (len < sizeof(ipv4_header))
        {
            stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        ipv4_header h;
        std::memcpy(&h, pkt, sizeof(h));
        const size_t ihl = h.header_len();
        if (ihl < sizeof(ipv4_header) || ihl > len)
        {
            stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const size_t total_len = ntohs(h.total_len);
        if (total_len < ihl || total_len > len)
        {
            stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        // 丢弃分片包（带分片偏移或 MF 标志）：引擎不做 IP 重组
        const uint16_t frag = ntohs(h.frag_off);
        if ((frag & 0x1fff) != 0 || (frag & 0x2000) != 0)
        {
            stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        // 校验 IP 头部校验和
        if (verify_ipv4_checksum(pkt, ihl) != 0)
        {
            stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        ip.family = 4;
        ip.protocol = h.protocol;
        std::memcpy(ip.src_ip, pkt + 12, 4);
        std::memcpy(ip.dst_ip, pkt + 16, 4);
        payload = pkt + ihl;
        payload_len = total_len - ihl;
    }
    else if (version == 6)
    {
        if (len < sizeof(ipv6_header))
        {
            stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        ipv6_header h;
        std::memcpy(&h, pkt, sizeof(h));
        const size_t payload_len_field = ntohs(h.payload_len);
        if (sizeof(ipv6_header) + payload_len_field > len)
        {
            stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        // 丢弃带扩展头的报文（Hop-by-Hop=0、Routing=43、Fragment=44、
        // AH=51、Dest-Options=60）：引擎不做 IP 重组且不解析扩展头链
        if (h.next_header == 0 || h.next_header == 43 || h.next_header == 44 ||
            h.next_header == 51 || h.next_header == 60)
        {
            stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        ip.family = 6;
        ip.protocol = h.next_header;
        std::memcpy(ip.src_ip, h.src_ip, 16);
        std::memcpy(ip.dst_ip, h.dst_ip, 16);
        payload = pkt + sizeof(ipv6_header);
        payload_len = payload_len_field;
    }
    else
    {
        stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
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
    const auto is_reserved_local = [](const uint8_t* a, int family) noexcept
    {
        if (family == 4)
            return a[0] == 0x7f || a[0] == 0x00; // 127.0.0.0/8, 0.0.0.0/8
        const bool zero15 = std::all_of(a,
            a + 15,
            [](uint8_t b)
            {
                return b == 0;
            });
        if (zero15)
            return a[15] == 0 || a[15] == 1;          // ::/128, ::1/128
        return a[0] == 0xfe && (a[1] & 0xc0) == 0x80; // fe80::/10
    };
    const bool src_local = (have_ip4_ && ip.family == 4 &&
                               std::memcmp(ip.src_ip, local_ip4_, 4) == 0) ||
        (have_ip6_ && ip.family == 6 &&
            std::memcmp(ip.src_ip, local_ip6_, 16) == 0);
    if (src_local || is_reserved_local(ip.src_ip, ip.family) ||
        is_reserved_local(ip.dst_ip, ip.family))
    {
        stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }
#endif

    switch (ip.protocol)
    {
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
        stats_->rx_dropped.fetch_add(1, std::memory_order_relaxed);
        break;
    }
}

// ---- ICMP 应答 ----

void tunio_impl::handle_icmp(
    const ip_packet_info& ip, const uint8_t* icmp, size_t icmp_len)
{
    if (!have_ip4_ || icmp_len < 8)
        return;
    // 仅响应发往本地虚拟 IP 的 Echo Request
    if (std::memcmp(ip.dst_ip, local_ip4_, 4) != 0 || icmp[0] != 8)
        return;
    // 校验 ICMP 校验和
    if (ip_checksum(icmp, icmp_len) != 0)
        return;

    packet_buffer reply = writer_->acquire(20 + icmp_len + 64, 64);
    reply.resize(20 + icmp_len);
    uint8_t* out = reply.data();

    build_ip_header(out,
        4,
        ip.dst_ip,
        ip.src_ip,
        IPPROTO_ICMP_V,
        20 + icmp_len,
        writer_->alloc_ip_id());

    uint8_t* oicmp = out + 20;
    std::memcpy(oicmp, icmp, icmp_len);
    oicmp[0] = 0; // Type = Echo Reply
    oicmp[2] = 0; // 清零校验和字段后再计算
    oicmp[3] = 0;
    const uint16_t csum = ip_checksum(oicmp, icmp_len);
    oicmp[2] = static_cast<uint8_t>(csum >> 8);
    oicmp[3] = static_cast<uint8_t>(csum & 0xff);

    writer_->async_write_and_forget(std::move(reply));
    stats_->icmp_replies.fetch_add(1, std::memory_order_relaxed);
}

void tunio_impl::handle_icmpv6(
    const ip_packet_info& ip, const uint8_t* icmp, size_t icmp_len)
{
    if (!have_ip6_ || icmp_len < 8)
        return;
    // 仅响应发往本地虚拟 IP 的 Echo Request
    if (std::memcmp(ip.dst_ip, local_ip6_, 16) != 0 || icmp[0] != 128)
        return;
    // ICMPv6 校验和必须包含 IPv6 伪头部
    if (tcp_udp_checksum(
            6, ip.src_ip, ip.dst_ip, IPPROTO_ICMPV6_V, icmp, icmp_len) != 0)
    {
        return;
    }

    packet_buffer reply = writer_->acquire(40 + icmp_len + 64, 64);
    reply.resize(40 + icmp_len);
    uint8_t* out = reply.data();

    build_ip_header(
        out, 6, ip.dst_ip, ip.src_ip, IPPROTO_ICMPV6_V, 40 + icmp_len, 0);

    uint8_t* oicmp = out + 40;
    std::memcpy(oicmp, icmp, icmp_len);
    oicmp[0] = 129; // Type = Echo Reply
    oicmp[2] = 0;   // 清零校验和字段后再计算
    oicmp[3] = 0;
    const uint16_t csum = tcp_udp_checksum(
        6, ip.dst_ip, ip.src_ip, IPPROTO_ICMPV6_V, oicmp, icmp_len);
    oicmp[2] = static_cast<uint8_t>(csum >> 8);
    oicmp[3] = static_cast<uint8_t>(csum & 0xff);

    writer_->async_write_and_forget(std::move(reply));
    stats_->icmp_replies.fetch_add(1, std::memory_order_relaxed);
}

} // namespace detail

// ---- tunio 门面 ----

tunio::tunio(net::io_context& ctx, bool single_thread)
    : impl_(std::make_shared<detail::tunio_impl>(ctx, single_thread))
{
}

tunio::~tunio() = default;

bool tunio::open(const tun_config& config, boost::system::error_code& ec)
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

size_t tunio::queue_count() const noexcept
{
    return impl_->queue_count();
}

net::ip::address tunio::local_address() const noexcept
{
    return impl_->local_address();
}

const engine_stats& tunio::stats() const noexcept
{
    return impl_->stats();
}

tunio::executor_type tunio::get_executor() const noexcept
{
    return impl_->strand();
}

} // namespace tunio
