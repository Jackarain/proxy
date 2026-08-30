//
// tun_udp_ops.hpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tunio/tun_udp_socket.hpp"
#include "udp_engine.hpp"

#include <memory>
#include <vector>

namespace tunio {
namespace net = boost::asio;

template <typename MutableBufferSequence, typename Handler>
void tun_udp_socket::do_receive_from(MutableBufferSequence &&buffers,
    net::ip::udp::endpoint &sender, Handler handler)
{
    // 单缓冲区（最常见调用形式）由 small_vector 栈上存储，避免堆分配
    mutable_buffer_sequence seq;
    size_t total = 0;
    // 兼容单缓冲区（net::buffer(char[]) 返回 mutable_buffer）与
    // 缓冲区序列两种调用形式，语义与 Boost.Asio async_receive_from 一致.
    for (auto it = net::buffer_sequence_begin(buffers);
        it != net::buffer_sequence_end(buffers); ++it) {
        seq.push_back(*it);
        total += it->size();
    }

    auto session = session_;
    if (!session) {
        handler(boost::system::error_code(net::error::bad_descriptor), 0);
        return;
    }
    detail::udp_session_start_receive(std::move(session), std::move(seq),
        total, sender, std::move(handler));
}

template <typename ConstBufferSequence, typename Handler>
void tun_udp_socket::do_send_to(const net::ip::udp::endpoint &remote,
    ConstBufferSequence &&buffers, Handler handler)
{
    const_buffer_sequence seq;
    size_t total = 0;
    // 兼容单缓冲区与缓冲区序列两种调用形式，语义与 Boost.Asio
    // async_send_to 一致.
    for (auto it = net::buffer_sequence_begin(buffers);
        it != net::buffer_sequence_end(buffers); ++it) {
        seq.push_back(*it);
        total += it->size();
    }

    auto session = session_;
    if (!session) {
        handler(boost::system::error_code(net::error::bad_descriptor), 0);
        return;
    }
    detail::udp_session_start_send(std::move(session), remote,
        std::move(seq), total, std::move(handler));
}

} // namespace tunio
