//
// tun_tcp_acceptor.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tunio/tun_tcp_socket.hpp"

#include <boost/asio.hpp>

namespace tunio {
namespace net = boost::asio;

class tunio;

// TCP 连接监听器
//
// async_accept 在收到客户端 SYN 时触发完成回调，三次握手由应用通过
// stream.accept()/stream.reject() 决定（未显式调用时，首次读写会隐式
// 批准握手）。完成回调触发时流处于 SYN_RCVD 状态，应用需在领取后决定
// 握手结果。
//
// 生命周期约束：与 Boost.Asio 的 acceptor::async_accept(socket) 一致，
// 挂起 accept 期间传入的 tun_tcp_socket 必须存活至完成回调触发；提前销毁
// peer 将导致未定义行为。
class tun_tcp_acceptor
{
public:
    explicit tun_tcp_acceptor(tunio &engine)
        : engine_(engine)
    {
    }

    template <typename CompletionToken>
    auto async_accept(tun_tcp_socket &peer, CompletionToken &&token)
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
    void do_accept(tun_tcp_socket &peer, Handler handler);

    tunio &engine_;
};

} // namespace tunio

#include "tunio/detail/tun_tcp_acceptor_ops.hpp"
