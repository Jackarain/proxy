//
// tun_stream.hpp
// ~~~~~~~~~~~~~~
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
// 表示一条已由引擎完成 TCP 三次握手的虚拟连接。应用层可像使用
// net::ip::tcp::socket 一样进行异步读写，数据经引擎封装为 IP/TCP
// 报文后写入 TUN 设备，发往虚拟网内的客户端。
class tun_stream
{
public:
    using executor_type = net::any_io_executor;

    explicit tun_stream(executor_type ex);
    ~tun_stream();

    tun_stream(tun_stream &&) noexcept;
    tun_stream &operator=(tun_stream &&) noexcept;

    executor_type get_executor() const noexcept;

    // 客户端原始请求的目标地址
    net::ip::tcp::endpoint original_destination() const;

    // 客户端（虚拟网内）端点：源地址与源端口
    net::ip::tcp::endpoint remote_endpoint() const;

    // 异步读取：返回已按序确认的字节流
    template <typename MutableBufferSequence, typename CompletionToken>
    auto async_read_some(MutableBufferSequence &&buffers,
                         CompletionToken &&token)
    {
        return net::async_initiate<CompletionToken,
                                   void(boost::system::error_code, size_t)>(
            [this](auto handler, auto buffers) mutable {
                do_read_some(std::move(buffers),
                             net::bind_executor(ex_, std::move(handler)));
            },
            token, std::forward<MutableBufferSequence>(buffers));
    }

    // 异步写入：数据被封装为 TCP 段发送，完成时表示数据已被引擎接收。
    // 调用方必须保证缓冲区在完成 handler 被回调前保持有效（与 Boost.Asio
    // 的 async_write_some 语义一致），引擎在发送期间只引用而不拷贝用户数据。
    template <typename ConstBufferSequence, typename CompletionToken>
    auto async_write_some(ConstBufferSequence &&buffers,
                          CompletionToken &&token)
    {
        return net::async_initiate<CompletionToken,
                                   void(boost::system::error_code, size_t)>(
            [this](auto handler, auto buffers) mutable {
                do_write_some(std::move(buffers),
                              net::bind_executor(ex_, std::move(handler)));
            },
            token, std::forward<ConstBufferSequence>(buffers));
    }

    // 优雅关闭：发送 FIN 完成四次挥手
    void shutdown(net::ip::tcp::socket::shutdown_type what,
                  boost::system::error_code &ec);

    // 优雅关闭（与 shutdown(send) 等价）
    void close();

    // 中止连接：立即向客户端发送 RST（后端连接失败等场景）
    void reset();

    bool is_open() const noexcept;

private:
    template <typename MutableBufferSequence, typename Handler>
    void do_read_some(MutableBufferSequence &&buffers, Handler handler);
    template <typename ConstBufferSequence, typename Handler>
    void do_write_some(ConstBufferSequence &&buffers, Handler handler);

    executor_type ex_;
    std::shared_ptr<detail::tcp_flow> flow_;

    friend class tun_acceptor;
};

} // namespace tunio

#include "tunio/detail/tun_stream_ops.hpp"
