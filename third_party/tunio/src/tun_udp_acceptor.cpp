//
// tun_udp_acceptor.cpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tunio/tun_udp_acceptor.hpp"

#include "tunio_impl.hpp"

#include <utility>

namespace tunio {

tun_udp_acceptor::tun_udp_acceptor(tunio& engine)
    : engine_(engine)
{
}

void tun_udp_acceptor::cancel()
{
    engine_.impl_->cancel_udp_accepts();
}

} // namespace tunio
