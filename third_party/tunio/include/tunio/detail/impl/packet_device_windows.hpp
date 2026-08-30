//
// packet_device_windows.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
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

namespace tunio {
namespace net = boost::asio;
namespace detail {

// Windows 实现 (Overlapped)：基于 windows::random_access_handle。
// 平台相关打开逻辑见 src/packet_device_windows.cpp。
class windows_packet_device_impl
{
public:
    explicit windows_packet_device_impl(net::io_context &ctx)
        : handle_(ctx)
    {
    }

    bool open(const device_config &cfg, boost::system::error_code &ec);

    bool assign(native_handle_type handle, size_t mtu, bool,
        boost::system::error_code &ec)
    {
        handle_.assign(handle, ec);
        if (!ec) {
            open_ = true;
            mtu_ = mtu;
        }
        return !ec;
    }

    void close()
    {
        if (open_) {
            handle_.close();
            open_ = false;
        }
    }

    size_t mtu() const
    {
        return mtu_;
    }
    bool is_open() const
    {
        return open_;
    }

    template <typename Handler>
    void async_read(packet_buffer &buf, Handler &&handler)
    {
        handle_.async_read_some_at(
            0, net::buffer(buf.writable_data(), buf.writable_size()),
            std::forward<Handler>(handler));
    }

    template <typename Handler>
    void async_write(packet_buffer &buf, Handler &&handler)
    {
        handle_.async_write_some_at(0, net::buffer(buf.data(), buf.size()),
            std::forward<Handler>(handler));
    }

    net::windows::random_access_handle handle_;
    size_t mtu_ = 1500;
    bool open_ = false;
};

} // namespace detail
} // namespace tunio
