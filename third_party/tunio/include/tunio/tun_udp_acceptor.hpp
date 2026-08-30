//
// tun_udp_acceptor.hpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tunio/tun_udp_socket.hpp"

#include <boost/asio.hpp>

namespace tunio {
namespace net = boost::asio;

class tunio;

// UDP 新会话监听器
//
// 当引擎收到一个属于未知五元组的 UDP 数据报时，自动创建新的会话，
// async_accept 触发完成回调并将会话对应的 tun_udp_socket 交给调用者。
//
// 生命周期约束：与 Boost.Asio 的 acceptor::async_accept(socket) 一致，
// 挂起 accept 期间传入的 tun_udp_socket 必须存活至完成回调触发；提前
// 销毁 peer 将导致未定义行为。
class tun_udp_acceptor
{
public:
    explicit tun_udp_acceptor(tunio &engine)
        : engine_(engine)
    {
    }

    template <typename CompletionToken>
    auto async_accept(tun_udp_socket &peer, CompletionToken &&token)
    {
        return net::async_initiate<CompletionToken,
            void(boost::system::error_code)>(
            [this, &peer](auto handler) {
                do_accept(peer, std::move(handler));
            },
            token);
    }

    // 取消全部挂起的 accept 操作
    void cancel();

private:
    template <typename Handler>
    void do_accept(tun_udp_socket &peer, Handler handler);

    tunio &engine_;
};

} // namespace tunio

#include "tunio/detail/tun_udp_acceptor_ops.hpp"
