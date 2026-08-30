//
// packet_device_windows.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tunio/packet_device.hpp"

#if defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)

namespace tunio {

namespace detail {

bool windows_packet_device_impl::open(const device_config &,
    boost::system::error_code &ec)
{
    // Overlapped 设备自主打开尚未实现（Phase 3），句柄注入 assign() 已可用。
    ec = boost::system::error_code(
        boost::system::errc::operation_not_supported,
        boost::system::generic_category());
    return false;
}

} // namespace detail

} // namespace tunio

#endif // BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR
