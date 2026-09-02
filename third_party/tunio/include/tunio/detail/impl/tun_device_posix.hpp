//
// tun_device_posix.hpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tunio/packet_buffer.hpp"
#include "tunio/tun_config.hpp"

#include <boost/asio.hpp>

#include <sys/socket.h>

#include <functional>
#include <vector>

namespace tunio {
namespace net = boost::asio;
namespace detail {

// POSIX 实现 (Linux TUN / macOS utun)：基于 posix::stream_descriptor。
// Linux 下支持 IFF_MULTI_QUEUE 多队列：每个队列一个独立 fd（stream_descriptor），
// 读按队列并发、写按队列分发。平台相关打开逻辑见 src/tun_device_posix.cpp。
class posix_tun_device_impl
{
public:
    explicit posix_tun_device_impl(net::io_context& ctx)
        : ctx_(ctx)
    {
    }

    bool open(const device_config& cfg, boost::system::error_code& ec);

    // 对注入的 TUN fd 应用默认发送队列长度（Linux 下经 TUNGETIFF 取接口名
    // 后设置；非 TUN 句柄或无权限时静默跳过，失败不视为错误）.
    void apply_default_tx_queue_len(native_handle_type handle);

    bool assign(native_handle_type handle,
        size_t mtu,
        bool utun_prefix,
        boost::system::error_code& ec)
    {
        return assign_queues({handle}, mtu, utun_prefix, ec);
    }

    // 多句柄注入：每个句柄对应一个队列 fd（Linux TUN 多队列）。
    // 注入方需保证句柄为同一设备的独立队列 fd；单队列场景传单个句柄即可。
    // 失败时不关闭任何注入句柄（所有权始终归调用方）.
    bool assign_queues(const std::vector<native_handle_type>& handles,
        size_t mtu,
        bool utun_prefix,
        boost::system::error_code& ec);

    // 自主打开路径：把一组 fd 逐个接管进 descs_；中途失败时关闭所有
    // 未接管的 fd（这些 fd 由本类打开，所有权属于本类）.
    bool adopt_fds(
        std::vector<int>&& fds, size_t mtu, boost::system::error_code& ec);

    void close();

    size_t mtu() const noexcept
    {
        return mtu_;
    }
    // 单次读取可能需要的最大字节数（utun 含 4 字节家族前缀）
    size_t read_size_hint() const
    {
        return mtu_ + (utun_prefix_ ? 4 : 0);
    }
    // 队列数：Linux TUN 多队列下为打开的队列 fd 数，其余恒为 1
    size_t queue_count() const noexcept
    {
        return descs_.size();
    }
    bool is_open() const noexcept
    {
        return open_;
    }

    template <typename Handler>
    void async_read(packet_buffer& buf, Handler&& handler)
    {
        async_read(buf, 0, std::forward<Handler>(handler));
    }

    template <typename Handler>
    void async_read(packet_buffer& buf, size_t queue, Handler&& handler)
    {
        if (queue >= descs_.size())
        {
            std::forward<Handler>(handler)(
                make_error_code(net::error::bad_descriptor), 0);
            return;
        }
        auto& desc = descs_[queue];
        if (utun_prefix_)
        {
            // macOS utun 每次读取携带 4 字节家族前缀（大端 AF_INET/AF_INET6）：
            // 读入后跳过前缀，引擎仍按"完整 IP 报文"解析。
            desc.async_read_some(
                net::buffer(buf.writable_data(), buf.writable_size()),
                [h = std::forward<Handler>(handler), &buf](
                    const boost::system::error_code& ec, size_t n) mutable
                {
                    size_t pkt_n = n;
                    if (!ec && n > 4)
                    {
                        // 从实际读入位置（writable_data）剥离前缀，兼容
                        // 调用方未 reset 时 data() != 读入位置的场景。
                        uint8_t* base = buf.writable_data();
                        std::memmove(base, base + 4, n - 4);
                        pkt_n = n - 4;
                    }
                    std::move(h)(ec, pkt_n);
                });
        }
        else
            desc.async_read_some(
                net::buffer(buf.writable_data(), buf.writable_size()),
                std::forward<Handler>(handler));
    }

    template <typename Handler>
    void async_write(packet_buffer& buf, Handler&& handler)
    {
        async_write(buf, 0, std::forward<Handler>(handler));
    }

    template <typename Handler>
    void async_write(packet_buffer& buf, size_t queue, Handler&& handler)
    {
        if (queue >= descs_.size())
        {
            std::forward<Handler>(handler)(
                make_error_code(net::error::bad_descriptor), 0);
            return;
        }
        auto& desc = descs_[queue];
        if (utun_prefix_)
        {
            // macOS utun 写入必须前置 4 字节家族前缀（大端 AF_INET/AF_INET6），
            // 前缀写入 packet_buffer 的 headroom 区域（引擎出包 headroom >= 64）.
            // headroom 不足 4 字节时写入会越过堆分配起始地址（下溢），拒绝。
            if (buf.headroom() < 4)
            {
                std::forward<Handler>(handler)(
                    make_error_code(net::error::invalid_argument), 0);
                return;
            }
            uint8_t* prefix = buf.data() - 4;
            const uint32_t family =
                (buf.data()[0] >> 4) == 6 ? AF_INET6 : AF_INET;
            prefix[0] = static_cast<uint8_t>(family >> 24);
            prefix[1] = static_cast<uint8_t>(family >> 16);
            prefix[2] = static_cast<uint8_t>(family >> 8);
            prefix[3] = static_cast<uint8_t>(family);
            desc.async_write_some(net::buffer(prefix, buf.size() + 4),
                std::forward<Handler>(handler));
        }
        else
            desc.async_write_some(net::buffer(buf.data(), buf.size()),
                std::forward<Handler>(handler));
    }

    net::io_context& ctx_;
    std::vector<net::posix::stream_descriptor> descs_;
    size_t mtu_ = 1500;
    bool open_ = false;
    bool utun_prefix_ = false;
};

} // namespace detail
} // namespace tunio
