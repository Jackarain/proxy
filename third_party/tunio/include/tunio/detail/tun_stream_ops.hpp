//
// tun_stream_ops.hpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tcp_engine.hpp"
#include "tunio/tun_stream.hpp"

#include <memory>
#include <vector>

namespace tunio {
namespace net = boost::asio;

namespace detail {
} // namespace detail

template <typename MutableBufferSequence, typename Handler>
void tun_stream::do_read_some(MutableBufferSequence &&buffers, Handler handler)
{
    std::vector<net::mutable_buffer> seq;
    size_t total = 0;
    // 兼容单缓冲区（net::buffer(char[]) 返回 mutable_buffer）与
    // 缓冲区序列两种调用形式，语义与 Boost.Asio async_read_some 一致.
    for (auto it = net::buffer_sequence_begin(buffers);
         it != net::buffer_sequence_end(buffers); ++it) {
        seq.push_back(*it);
        total += it->size();
    }

    auto flow = flow_;
    if (!flow) {
        handler(boost::system::error_code(net::error::bad_descriptor), 0);
        return;
    }
    detail::tcp_flow_start_read(std::move(flow), std::move(seq), total,
                                std::move(handler));
}

template <typename ConstBufferSequence, typename Handler>
void tun_stream::do_write_some(ConstBufferSequence &&buffers, Handler handler)
{
    std::vector<net::const_buffer> seq;
    size_t total = 0;
    // 兼容单缓冲区与缓冲区序列两种调用形式，语义与 Boost.Asio
    // async_write_some 一致.
    for (auto it = net::buffer_sequence_begin(buffers);
         it != net::buffer_sequence_end(buffers); ++it) {
        seq.push_back(*it);
        total += it->size();
    }

    auto flow = flow_;
    if (!flow) {
        handler(boost::system::error_code(net::error::bad_descriptor), 0);
        return;
    }
    detail::tcp_flow_start_write(std::move(flow), std::move(seq), total,
                                 std::move(handler));
}

} // namespace tunio
