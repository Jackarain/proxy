//
// tun_device_unsupported.hpp
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

// 无平台实现时的兜底类型：所有操作立即失败。
class unsupported_tun_device
{
public:
    explicit unsupported_tun_device(net::io_context &)
    {
    }

    bool open(const device_config &, boost::system::error_code &ec)
    {
        ec = make_error_code(boost::system::errc::operation_not_supported);
        return false;
    }

    bool assign(native_handle_type, size_t, bool,
        boost::system::error_code &ec)
    {
        ec = make_error_code(boost::system::errc::operation_not_supported);
        return false;
    }

    void close()
    {
    }
    size_t mtu() const
    {
        return 0;
    }
    bool is_open() const
    {
        return false;
    }

    template <typename Handler>
    void async_read(packet_buffer &, Handler &&handler)
    {
        std::forward<Handler>(handler)(net::error::bad_descriptor, 0);
    }

    template <typename Handler>
    void async_write(packet_buffer &, Handler &&handler)
    {
        std::forward<Handler>(handler)(net::error::bad_descriptor, 0);
    }
};

} // namespace detail
} // namespace tunio
