//
// tun_udp_acceptor_ops.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tunio/tun_udp_acceptor.hpp"
#include "tunio_impl.hpp"

namespace tunio {

template <typename Handler>
void tun_udp_acceptor::do_accept(tun_udp_socket &peer, Handler handler)
{
    auto ex = peer.get_executor();
    engine_.impl_->async_accept_udp(
        [ex, &peer, handler = std::move(handler)](
            boost::system::error_code ec,
            detail::udp_session_ptr s) mutable {
            if (!ec && s) {
                peer.session_ = std::move(s);
            }
            net::dispatch(ex, [handler = std::move(handler), ec]() mutable {
                handler(ec);
            });
        });
}

} // namespace tunio
