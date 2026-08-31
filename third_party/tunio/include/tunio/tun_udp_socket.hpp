//
// tun_udp_socket.hpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tunio/tun_config.hpp"

#include <boost/asio.hpp>

#include <chrono>
#include <memory>

namespace tunio {
namespace net = boost::asio;

namespace detail {
struct udp_session;
} // namespace detail

// UDP 数据报套接字
//
// 表示引擎 UDP 会话表中绑定一个客户端套接字的会话（由客户端三元组唯一
// 标识，1 对 N：可向任意远端收发）。async_receive_from / async_send_to
// 严格遵循一次收发对应一个完整数据报的语义。
class tun_udp_socket
{
public:
    using executor_type = net::any_io_executor;

    explicit tun_udp_socket(executor_type ex);
    ~tun_udp_socket();

    tun_udp_socket(tun_udp_socket &&) noexcept;
    tun_udp_socket &operator=(tun_udp_socket &&) noexcept;

    executor_type get_executor() const noexcept;

    // 异步接收一个完整数据报；sender 输出该数据报的目标远端端点（发送者
    // 恒为会话绑定的客户端）。调用方必须保证 sender 在完成 handler 被回调
    // 前保持有效，失败路径不保证填充（与 Boost.Asio 的 async_receive_from
    // 语义一致）。
    template <typename MutableBufferSequence, typename CompletionToken>
    auto async_receive_from(MutableBufferSequence &&buffers,
        net::ip::udp::endpoint &sender, CompletionToken &&token)
    {
        return net::async_initiate<CompletionToken,
            void(boost::system::error_code, size_t)>(
            [this, &sender](auto handler, auto buffers) mutable {
                do_receive_from(std::move(buffers), sender,
                    net::bind_executor(ex_, std::move(handler)));
            },
            token, std::forward<MutableBufferSequence>(buffers));
    }

    // 异步发送一个完整数据报：构造并注入 src=remote → dst=客户端 的响应
    // 数据报。调用方必须保证缓冲区在完成 handler 被回调前保持有效（与
    // Boost.Asio 的 async_send 语义一致），引擎在发送期间只引用而不拷贝
    // 用户数据。
    template <typename ConstBufferSequence, typename CompletionToken>
    auto async_send_to(const net::ip::udp::endpoint &remote,
        ConstBufferSequence &&buffers, CompletionToken &&token)
    {
        return net::async_initiate<CompletionToken,
            void(boost::system::error_code, size_t)>(
            [this, remote](auto handler, auto buffers) mutable {
                do_send_to(remote, std::move(buffers),
                    net::bind_executor(ex_, std::move(handler)));
            },
            token, std::forward<ConstBufferSequence>(buffers));
    }

    // 客户端（虚拟网内）端点：源地址与源端口
    net::ip::udp::endpoint client_endpoint() const;

    // 设置会话空闲超时
    void set_timeout(std::chrono::seconds timeout);

    void close();
    // 会话是否仍打开。线程安全：仅限在 io 线程（或与引擎串行执行器同步
    // 的上下文）调用；多线程模式下与 Strand 上的状态更新并发访问构成数据
    // 竞争（与 Boost.Asio 对共享 socket 对象"并发访问不安全"的约定一致）.
    bool is_open() const noexcept;

private:
    template <typename MutableBufferSequence, typename Handler>
    void do_receive_from(MutableBufferSequence &&buffers,
                         net::ip::udp::endpoint &sender, Handler handler);
    template <typename ConstBufferSequence, typename Handler>
    void do_send_to(const net::ip::udp::endpoint &remote,
                    ConstBufferSequence &&buffers, Handler handler);

    executor_type ex_;
    std::shared_ptr<detail::udp_session> session_;

    friend class tun_udp_acceptor;
};

} // namespace tunio

#include "tunio/detail/tun_udp_ops.hpp"
