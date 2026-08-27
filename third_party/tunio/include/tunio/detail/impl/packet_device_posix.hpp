//
// packet_device_posix.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~
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

// POSIX 实现 (Linux TUN / macOS utun)：基于 posix::stream_descriptor。
// 平台相关打开逻辑见 src/packet_device_posix.cpp。
class posix_packet_device_impl
{
public:
    explicit posix_packet_device_impl(net::io_context &ctx)
        : desc_(ctx)
    {
    }

    bool open(const device_config &cfg, boost::system::error_code &ec);

    bool assign(native_handle_type handle, size_t mtu,
                boost::system::error_code &ec)
    {
        desc_.assign(static_cast<native_handle_type>(handle), ec);
        if (!ec) {
            open_ = true;
            mtu_ = mtu;
        }
        return !ec;
    }

    void close()
    {
        if (open_) {
            desc_.close();
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
        desc_.async_read_some(
            net::buffer(buf.writable_data(), buf.writable_size()),
            std::forward<Handler>(handler));
    }

    template <typename Handler>
    void async_write(packet_buffer &buf, Handler &&handler)
    {
        desc_.async_write_some(net::buffer(buf.data(), buf.size()),
                               std::forward<Handler>(handler));
    }

    net::posix::stream_descriptor desc_;
    size_t mtu_ = 1500;
    bool open_ = false;
};

} // namespace detail
} // namespace tunio
