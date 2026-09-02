//
// tun_tcp_socket.hpp
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

#include <memory>
#include <vector>

namespace tunio {
namespace net = boost::asio;

namespace detail {
struct tcp_flow;
} // namespace detail

// TCP 虚拟流套接字
//
// 表示一条由引擎识别的虚拟连接（客户端 SYN 已到达）。应用层可像使用
// net::ip::tcp::socket 一样进行异步读写，数据经引擎封装为 IP/TCP
// 报文后写入 TUN 设备，发往虚拟网内的客户端。
//
// 握手：收到 SYN 后引擎不立即回复，由 accept()/reject() 或首次读写
// （隐式批准）决定握手结果；三次握手完成前读写操作会缓冲，完成后交付。
class tun_tcp_socket
{
public:
    using executor_type = net::any_io_executor;

    explicit tun_tcp_socket(executor_type ex);
    ~tun_tcp_socket();

    tun_tcp_socket(tun_tcp_socket&&) noexcept;
    tun_tcp_socket& operator=(tun_tcp_socket&&) noexcept;

    executor_type get_executor() const noexcept;

    // 客户端原始请求的目标地址
    net::ip::tcp::endpoint original_destination() const noexcept;

    // 客户端（虚拟网内）端点：源地址与源端口
    net::ip::tcp::endpoint remote_endpoint() const noexcept;

    // 异步读取：返回已按序确认的字节流
    template <typename MutableBufferSequence, typename CompletionToken>
    auto async_read_some(
        MutableBufferSequence&& buffers, CompletionToken&& token)
    {
        return net::async_initiate<CompletionToken,
            void(boost::system::error_code, size_t)>(
            [this](auto handler, auto buffers) mutable
            {
                do_read_some(std::move(buffers),
                    net::bind_executor(ex_, std::move(handler)));
            },
            token,
            std::forward<MutableBufferSequence>(buffers));
    }

    // 异步写入：数据被封装为 TCP 段发送，完成时表示数据已被引擎接收。
    // 调用方必须保证缓冲区在完成 handler 被回调前保持有效（与 Boost.Asio
    // 的 async_write_some 语义一致），引擎在发送期间只引用而不拷贝用户数据。
    template <typename ConstBufferSequence, typename CompletionToken>
    auto async_write_some(
        ConstBufferSequence&& buffers, CompletionToken&& token)
    {
        return net::async_initiate<CompletionToken,
            void(boost::system::error_code, size_t)>(
            [this](auto handler, auto buffers) mutable
            {
                do_write_some(std::move(buffers),
                    net::bind_executor(ex_, std::move(handler)));
            },
            token,
            std::forward<ConstBufferSequence>(buffers));
    }

    // 优雅关闭：发送 FIN 完成四次挥手
    void shutdown(net::ip::tcp::socket::shutdown_type what,
        boost::system::error_code& ec);

    // 优雅关闭（与 shutdown(send) 等价）
    void close();

    // 中止连接：立即向客户端发送 RST（后端连接失败等场景）
    void reset();

    // 批准握手：向客户端回复 SYN+ACK（幂等，已回复过则忽略）。
    // 未调用时，首次 async_read_some/async_write_some 也会隐式批准。
    void accept();

    // 拒绝握手：立即向客户端发送 RST（幂等，与 reset 语义一致）。
    void reject();

    // 连接是否仍打开。
    // 线程安全：仅限在 io 线程（或与引擎串行执行器同步的上下文）调用。
    // 本方法直接读取流状态，多线程模式下与 Strand 上的状态更新并发访问
    // 构成数据竞争（与 Boost.Asio 对共享 socket 对象"并发访问不安全"的
    // 约定一致）；单线程模式（默认）无此限制。
    bool is_open() const noexcept;

private:
    template <typename MutableBufferSequence, typename Handler>
    void do_read_some(MutableBufferSequence&& buffers, Handler handler);
    template <typename ConstBufferSequence, typename Handler>
    void do_write_some(ConstBufferSequence&& buffers, Handler handler);

    executor_type ex_;
    std::shared_ptr<detail::tcp_flow> flow_;

    friend class tun_tcp_acceptor;
};

} // namespace tunio

#include "tunio/detail/tun_tcp_socket_ops.hpp"
