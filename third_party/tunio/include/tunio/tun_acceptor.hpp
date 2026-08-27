//
// tun_acceptor.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tunio/tun_stream.hpp"

#include <boost/asio.hpp>

namespace tunio {
namespace net = boost::asio;

class tunio;

// TCP 连接监听器
//
// async_accept 在三次握手完成（收到客户端 ACK）时触发完成回调，
// 此时连接处于 ESTABLISHED 状态。
//
// 生命周期约束：与 Boost.Asio 的 acceptor::async_accept(socket) 一致，
// 挂起 accept 期间传入的 tun_stream 必须存活至完成回调触发；提前销毁
// peer 将导致未定义行为。
class tun_acceptor
{
public:
    explicit tun_acceptor(tunio &engine)
        : engine_(engine)
    {
    }

    template <typename CompletionToken>
    auto async_accept(tun_stream &peer, CompletionToken &&token)
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
    void do_accept(tun_stream &peer, Handler handler);

    tunio &engine_;
};

} // namespace tunio

#include "tunio/detail/tun_acceptor_ops.hpp"
