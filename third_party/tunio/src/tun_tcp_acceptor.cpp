//
// tun_tcp_acceptor.cpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tunio/tun_tcp_acceptor.hpp"

#include "tunio_impl.hpp"

#include <utility>

namespace tunio {

void tun_tcp_acceptor::cancel()
{
    engine_.impl_->cancel_tcp_accepts();
}

} // namespace tunio
