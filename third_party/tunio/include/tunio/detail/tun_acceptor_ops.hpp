//
// tun_acceptor_ops.hpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tunio/tun_acceptor.hpp"
#include "tunio_impl.hpp"

namespace tunio {

template <typename Handler>
void tun_acceptor::do_accept(tun_stream &peer, Handler handler)
{
    auto ex = peer.get_executor();
    engine_.impl_->async_accept_tcp(
        [ex, &peer, handler = std::move(handler)](
            boost::system::error_code ec,
            std::shared_ptr<detail::tcp_flow> f) mutable {
            if (!ec && f) {
                peer.flow_ = std::move(f);
            }
            net::dispatch(ex, [handler = std::move(handler), ec]() mutable {
                handler(ec);
            });
        });
}

} // namespace tunio
