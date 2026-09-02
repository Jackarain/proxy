//
// tun_tcp_acceptor_ops.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tunio/tun_tcp_acceptor.hpp"
#include "tunio_impl.hpp"

namespace tunio {

template <typename Handler>
void tun_tcp_acceptor::do_accept(tun_tcp_socket& peer, Handler handler)
{
    auto ex = peer.get_executor();
    engine_.impl_->async_accept_tcp(
        [ex, &peer, handler = std::move(handler)](
            boost::system::error_code ec, detail::tcp_flow_ptr f) mutable
        {
            // 在调用方执行器（ex）上赋值 flow_，与用户线程对 peer 的
            // 读写处于同一执行上下文，避免跨 Strand 写共享状态
            //（多线程模式下构成数据竞争）.
            net::dispatch(ex,
                [&peer,
                    handler = std::move(handler),
                    ec,
                    f = std::move(f)]() mutable
                {
                    if (!ec && f)
                        peer.flow_ = std::move(f);
                    handler(ec);
                });
        });
}

} // namespace tunio
