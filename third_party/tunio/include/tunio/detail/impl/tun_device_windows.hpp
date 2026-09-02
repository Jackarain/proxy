//
// tun_device_windows.hpp
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

#include <functional>
#include <vector>

namespace tunio {
namespace net = boost::asio;
namespace detail {

// Windows 实现 (TAP/Overlapped)：基于 windows::random_access_handle。
// 自主打开支持 TAP 驱动设备（tap0901、tapnordvpn、tap-tb-0901 等），
// 平台相关打开逻辑见 src/tun_device_windows.cpp。
class windows_tun_device_impl
{
public:
    explicit windows_tun_device_impl(net::io_context& ctx)
        : handle_(ctx)
    {
    }

    bool open(const device_config& cfg, boost::system::error_code& ec);

    bool assign(native_handle_type handle,
        size_t mtu,
        bool,
        boost::system::error_code& ec);

    // Windows TAP 驱动无多队列概念：注入多个句柄不支持。
    bool assign_queues(const std::vector<native_handle_type>&,
        size_t,
        bool,
        boost::system::error_code& ec);

    void close();

    size_t mtu() const noexcept
    {
        return mtu_;
    }
    // 单次读取可能需要的最大字节数
    size_t read_size_hint() const
    {
        return mtu_;
    }
    size_t queue_count() const noexcept
    {
        return 1;
    }
    bool is_open() const noexcept
    {
        return open_;
    }

    template <typename Handler>
    void async_read(packet_buffer& buf, Handler&& handler)
    {
        handle_.async_read_some_at(0,
            net::buffer(buf.writable_data(), buf.writable_size()),
            std::forward<Handler>(handler));
    }

    template <typename Handler>
    void async_read(packet_buffer& buf, size_t, Handler&& handler)
    {
        async_read(buf, std::forward<Handler>(handler));
    }

    template <typename Handler>
    void async_write(packet_buffer& buf, Handler&& handler)
    {
        handle_.async_write_some_at(0,
            net::buffer(buf.data(), buf.size()),
            std::forward<Handler>(handler));
    }

    template <typename Handler>
    void async_write(packet_buffer& buf, size_t, Handler&& handler)
    {
        async_write(buf, std::forward<Handler>(handler));
    }

    net::windows::random_access_handle handle_;
    size_t mtu_ = 1500;
    bool open_ = false;
};

} // namespace detail
} // namespace tunio
