//
// tunio_impl.hpp
// ~~~~~~~~~~~~~~
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
#include "tunio/tunio.hpp"

#include "device_writer.hpp"
#include "tcp_engine.hpp"
#include "udp_engine.hpp"

#include <boost/asio.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <utility>

namespace tunio {
namespace net = boost::asio;
namespace detail {

class tunio_impl : public std::enable_shared_from_this<tunio_impl>
{
public:
    // single_thread=true（默认）时内部串行执行器直接使用 io_context 的
    // 执行器，无 Strand 派发开销，要求 io_context 单线程 run；false 时
    // 使用 Strand，支持多线程 run.
    explicit tunio_impl(net::io_context &ctx, bool single_thread = true);
    ~tunio_impl();

    bool open(const tun_config &cfg, boost::system::error_code &ec);
    void close();
    bool is_open() const noexcept
    {
        return open_.load(std::memory_order_acquire);
    }
    size_t mtu() const noexcept
    {
        return mtu_;
    }
    net::ip::address local_address() const noexcept
    {
        return local_ip_;
    }
    engine_stats &stats() noexcept
    {
        return *stats_;
    }
    net::any_io_executor strand() const noexcept
    {
        return strand_ex_;
    }

    // ---- accept 入口（自动派发到 Strand）----
    template <typename Handler> void async_accept_tcp(Handler handler)
    {
        net::dispatch(
            strand_ex_, [tcp = tcp_, h = std::move(handler)]() mutable {
                if (tcp) {
                    tcp->async_accept(std::move(h));
                } else {
                    h(boost::system::error_code(net::error::bad_descriptor),
                        nullptr);
                }
            });
    }

    template <typename Handler> void async_accept_udp(Handler handler)
    {
        net::dispatch(
            strand_ex_, [udp = udp_, h = std::move(handler)]() mutable {
                if (udp) {
                    udp->async_accept(std::move(h));
                } else {
                    h(boost::system::error_code(net::error::bad_descriptor),
                        nullptr);
                }
            });
    }

    void cancel_tcp_accepts()
    {
        net::dispatch(strand_ex_, [tcp = tcp_]() {
            if (tcp) {
                tcp->cancel_accepts();
            }
        });
    }

    void cancel_udp_accepts()
    {
        net::dispatch(strand_ex_, [udp = udp_]() {
            if (udp) {
                udp->cancel_accepts();
            }
        });
    }

private:
    void start_read();
    void start_read_slot(size_t index);
    void on_read(const boost::system::error_code &ec, size_t n, size_t index,
        uint64_t epoch);
    void handle_packet(const uint8_t *pkt, size_t len);
    void handle_icmp(const ip_packet_info &ip, const uint8_t *icmp,
        size_t icmp_len);
    void handle_icmpv6(const ip_packet_info &ip, const uint8_t *icmp,
        size_t icmp_len);

    net::any_io_executor strand_ex_;
    std::atomic<bool> open_{false};
    uint64_t epoch_ = 0; // 代际计数：close 的异步清理据此判断是否已被重新 open
    tun_config cfg_;
    net::ip::address local_ip_{}; // 用于 local_address()
    uint8_t local_ip4_[4] = {};   // 网络字节序
    uint8_t local_ip6_[16] = {};  // 网络字节序
    bool have_ip4_ = false;
    bool have_ip6_ = false;
    size_t mtu_ = 1500;

    std::shared_ptr<tun_device> device_;
    std::shared_ptr<device_writer> writer_;
    std::shared_ptr<tcp_engine> tcp_;
    std::shared_ptr<udp_engine> udp_;
    std::shared_ptr<buffer_accountant> account_;
    std::shared_ptr<engine_stats> stats_;

    // 并发读槽：同一时刻保持 k_read_slots 个未完成读取，TUN 包设备下
    // 每个读回调独立拿到完整报文，拆包处理与重发起互不依赖。
    static constexpr size_t k_read_slots = 32;
    std::array<packet_buffer, k_read_slots> read_bufs_;
    std::array<bool, k_read_slots> read_inflight_{};
};

} // namespace detail
} // namespace tunio
