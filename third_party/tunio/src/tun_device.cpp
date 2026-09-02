//
// tun_device.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tunio/tun_device.hpp"

namespace tunio {

tun_device::tun_device(net::io_context& ctx)
    : impl_(ctx)
{
}

bool tun_device::open(const device_config& cfg, boost::system::error_code& ec)
{
    return impl_.open(cfg, ec);
}

bool tun_device::assign(native_handle_type handle,
    size_t mtu,
    bool utun_prefix,
    boost::system::error_code& ec)
{
    return impl_.assign(handle, mtu, utun_prefix, ec);
}

bool tun_device::assign_queues(const std::vector<native_handle_type>& handles,
    size_t mtu,
    bool utun_prefix,
    boost::system::error_code& ec)
{
    return impl_.assign_queues(handles, mtu, utun_prefix, ec);
}

void tun_device::close()
{
    impl_.close();
}

size_t tun_device::mtu() const noexcept
{
    return impl_.mtu();
}

size_t tun_device::queue_count() const noexcept
{
    return impl_.queue_count();
}

bool tun_device::is_open() const noexcept
{
    return impl_.is_open();
}

} // namespace tunio
