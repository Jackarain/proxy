//
// tunio.hpp
// ~~~~~~~~~
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

namespace tunio {
namespace net = boost::asio;

namespace detail {
class tunio_impl;
class tcp_engine;
class udp_engine;
} // namespace detail

// 用户态 TUN 网络引擎
//
// 引擎封装 TUN 设备 I/O、TCP/UDP 协议引擎与 NAT 会话表，向应用层暴露
// tun_tcp_socket / tun_tcp_acceptor / tun_udp_socket / tun_udp_acceptor 等异步接口。
// 内部状态串行化有两种模式：单线程模式（默认）直接运行于 io_context 执行器
// 上，省去每包 Strand 派发开销，要求 io_context 单线程 run；多线程模式使用
// Strand，支持多线程运行 io_context。两种模式下所有内部状态变更均串行执行。
//
// 生命周期要求：引擎必须在所有 tun_tcp_socket / tun_udp_socket 销毁之后，
// 且 io_context 停止运行（所有 run() 已返回、io 线程已 join，无任何线程
// 正在执行完成回调）之后销毁。若在 io 线程仍在执行回调时销毁引擎，引擎
// 内部状态与在途异步操作将与销毁线程并发访问（数据竞争），且设备层读
// 回调引用的内部缓冲可能已释放（与 Boost.Asio "socket 须在 io_context
// 销毁前析构"的约束互补）。运行期间需要关闭请调用 close()，其内部会在
// 串行执行器上完成清理。
// open() 会同步重建引擎内部状态，必须在 io_context 开始运行（io.run()）之前
// 首次调用；若在运行期间调用 open()，请通过 get_executor() 派发屏障任务，
// 确保与引擎串行执行器上的任务串行，避免与数据通路回调产生数据竞争。
// 对已打开（或 close 后尚未完成异步清理）的引擎再次调用 open() 时，
// io_context 必须正在运行：open() 会在串行执行器上同步收尾上一代实例，
// io_context 未运行时该收尾任务无法执行，将导致调用线程阻塞等待。
class tunio
{
public:
    using executor_type = net::any_io_executor;

    // single_thread=true（默认）时引擎内部状态运行于 io_context 执行器上，
    // 要求 io_context 单线程 run；false 时使用 Strand，支持多线程 run.
    explicit tunio(net::io_context &ctx, bool single_thread = true);
    ~tunio();

    tunio(const tunio &) = delete;
    tunio &operator=(const tunio &) = delete;

    // 打开 TUN 设备并启动数据通路；外部句柄注入时 config.external_handle
    // 必须有效。
    bool open(const tun_config &config, boost::system::error_code &ec);

    // 关闭引擎：停止数据通路并清理全部会话与挂起操作。
    void close();

    bool is_open() const noexcept;
    size_t mtu() const noexcept;

    // 引擎本地虚拟 IP（用于 ICMP 回显响应等）。
    net::ip::address local_address() const noexcept;

    const engine_stats &stats() const noexcept;

    // 引擎内部串行执行器；应用层也可通过其提交任务保证与引擎状态串行化。
    executor_type get_executor() const noexcept;

private:
    friend class tun_tcp_acceptor;
    friend class tun_udp_acceptor;

    std::shared_ptr<detail::tunio_impl> impl_;
};

} // namespace tunio
