//
// tun_tcp_socket.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tunio/tun_tcp_socket.hpp"

#include "tcp_engine.hpp"
#include "tunio/detail/tun_tcp_socket_ops.hpp"

#include <utility>

namespace tunio {

tun_tcp_socket::tun_tcp_socket(executor_type ex)
    : ex_(std::move(ex))
{
}

tun_tcp_socket::~tun_tcp_socket()
{
    close();
}

tun_tcp_socket::tun_tcp_socket(tun_tcp_socket &&) noexcept = default;

tun_tcp_socket &tun_tcp_socket::operator=(tun_tcp_socket &&other) noexcept
{
    if (this != &other) {
        close();
        flow_ = std::move(other.flow_);
        ex_ = std::move(other.ex_);
    }
    return *this;
}

tun_tcp_socket::executor_type tun_tcp_socket::get_executor() const noexcept
{
    return ex_;
}

net::ip::tcp::endpoint tun_tcp_socket::original_destination() const
{
    if (!flow_) {
        return {};
    }
    return flow_->original_destination();
}

net::ip::tcp::endpoint tun_tcp_socket::remote_endpoint() const
{
    if (!flow_) {
        return {};
    }
    return flow_->remote_endpoint();
}

void tun_tcp_socket::shutdown(net::ip::tcp::socket::shutdown_type what,
    boost::system::error_code &ec)
{
    ec = {};
    if (!flow_) {
        ec = net::error::bad_descriptor;
        return;
    }
    if (what == net::ip::tcp::socket::shutdown_send ||
        what == net::ip::tcp::socket::shutdown_both) {
        detail::tcp_flow_shutdown_send(flow_);
    }
    if (what == net::ip::tcp::socket::shutdown_receive ||
        what == net::ip::tcp::socket::shutdown_both) {
        detail::tcp_flow_shutdown_receive(flow_);
    }
}

void tun_tcp_socket::close()
{
    if (flow_) {
        detail::tcp_flow_close(flow_);
    }
}

void tun_tcp_socket::reset()
{
    if (flow_) {
        detail::tcp_flow_reset(flow_);
    }
}

void tun_tcp_socket::accept()
{
    if (flow_) {
        detail::tcp_flow_accept(flow_);
    }
}

void tun_tcp_socket::reject()
{
    if (flow_) {
        detail::tcp_flow_reject(flow_);
    }
}

bool tun_tcp_socket::is_open() const noexcept
{
    return detail::tcp_flow_is_open(flow_);
}

} // namespace tunio
