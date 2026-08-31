//
// tun_device.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tunio/ip_packet.hpp"
#include "tunio/packet_buffer.hpp"
#include "tunio/tun_config.hpp"

#include <boost/asio.hpp>

// 平台实现按文件拆分，参考 Asio 的 impl 目录布局：
//   detail/impl/tun_device_posix.hpp
//   detail/impl/tun_device_windows.hpp
//   detail/impl/tun_device_wintun.hpp
//   detail/impl/tun_device_unsupported.hpp
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
#include "tunio/detail/impl/tun_device_posix.hpp"
#elif defined(_WIN32) && defined(USE_WINTUN_DRIVER)
#include "tunio/detail/impl/tun_device_wintun.hpp"
#elif defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)
#include "tunio/detail/impl/tun_device_windows.hpp"
#else
#include "tunio/detail/impl/tun_device_unsupported.hpp"
#endif

namespace tunio {
namespace net = boost::asio;
namespace detail {

#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
using tun_device_impl = posix_tun_device_impl;
#elif defined(_WIN32) && defined(USE_WINTUN_DRIVER)
using tun_device_impl = wintun_tun_device_impl;
#elif defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)
using tun_device_impl = windows_tun_device_impl;
#else
using tun_device_impl = unsupported_tun_device;
#endif

} // namespace detail

// 跨平台设备抽象层
//
// 支持两种初始化模式：
//   ① 自主打开模式：open(device_config) -> 由引擎创建并配置 TUN 设备；
//   ② 句柄注入模式：assign(handle, mtu) -> 接管外部应用已打开的平台句柄。
//
// 异步 I/O 完全对齐 Boost.Asio 范式：async_read_packet / async_write_packet
// 使用 CompletionToken 模板参数，通过 async_initiate 实现，可与 use_awaitable、
// use_future 及自定义 CompletionToken 无缝协作。
//
// 除原始字节包 I/O 外，还提供解析级接口 async_read_ip / async_write_ip：
// async_read_ip 读取并解析一个完整 IP 报文到 ip_packet（含 IP 头信息，TCP/
// UDP/ICMP 等传输层协议信息与载荷视图），async_write_ip 写出 ip_packet
// （支持字段构造并自动计算校验和）。macOS utun 的 4 字节家族前缀在平台
// 实现层透明剥离/附加，ip_packet 始终看到纯 IP 报文。
class tun_device
{
public:
    explicit tun_device(net::io_context &ctx)
        : impl_(ctx)
    {
    }

    // ---- 模式 1: 自主打开 ----
    bool open(const device_config &cfg, boost::system::error_code &ec)
    {
        return impl_.open(cfg, ec);
    }

    // ---- 模式 2: 句柄注入 ----
    bool assign(native_handle_type handle, size_t mtu, bool utun_prefix,
        boost::system::error_code &ec)
    {
        return impl_.assign(handle, mtu, utun_prefix, ec);
    }

    void close()
    {
        impl_.close();
    }

    size_t mtu() const
    {
        return impl_.mtu();
    }

    bool is_open() const
    {
        return impl_.is_open();
    }

    // ---- 异步读取一个完整数据包 ----
    template <typename CompletionToken>
    auto async_read_packet(packet_buffer &buf, CompletionToken &&token)
    {
        return net::async_initiate<CompletionToken,
            void(boost::system::error_code, size_t)>(
            [this, &buf](auto handler) {
                impl_.async_read(buf, std::move(handler));
            },
            token);
    }

    // ---- 异步写入一个完整数据包 ----
    template <typename CompletionToken>
    auto async_write_packet(packet_buffer &buf, CompletionToken &&token)
    {
        return net::async_initiate<CompletionToken,
            void(boost::system::error_code, size_t)>(
            [this, &buf](auto handler) {
                impl_.async_write(buf, std::move(handler));
            },
            token);
    }

    // ---- 异步读取并解析一个 IP 报文 ----
    //
    // 设备将报文直接读入 pkt 的内部缓冲并立即解析（零拷贝），完成后即可
    // 通过 pkt 的版本/地址/端口/传输层视图/载荷等访问器读取具体协议信息。
    // 完成签名与 async_read_packet 一致：void(error_code, size_t)，ec 仅反映
    // 设备 I/O 错误（含 pkt 缓冲容量不足以容纳一个 MTU 报文时的
    // net::error::message_size）；报文结构非法（解析失败）时 ec 为 no_error，
    // 通过 pkt.valid() / pkt.error() 判断。每个未完成的 async_read_ip 需要
    // 独立的 ip_packet 对象（自持缓冲），同一对象不可并发发起多次读取。
    template <typename CompletionToken>
    auto async_read_ip(ip_packet &pkt, CompletionToken &&token)
    {
        return net::async_initiate<CompletionToken,
            void(boost::system::error_code, size_t)>(
            [this, &pkt](auto handler) {
                pkt.buffer().reset();
                // 容量不足容纳一个完整报文（utun 读含 4 字节前缀）时立即以
                // message_size 完成，避免截断读入后解析报出令人困惑的
                // invalid_total_length.
                if (impl_.read_size_hint() != 0 &&
                    pkt.buffer().writable_size() < impl_.read_size_hint()) {
                    // 用 dispatch 在 handler 的关联执行器上完成（post 在
                    // Asio 1.38+ 的默认 inline_executor 上无法编译）.
                    net::dispatch(net::get_associated_executor(handler),
                        [h = std::move(handler)]() mutable {
                            h(make_error_code(net::error::message_size), 0);
                        });
                    return;
                }
                impl_.async_read(pkt.buffer(),
                    [h = std::move(handler),
                        &pkt](const boost::system::error_code &ec, size_t n) mutable {
                        if (!ec) {
                            pkt.parse(pkt.buffer(), n);
                        }
                        std::move(h)(ec, n);
                    });
            },
            token);
    }

    // ---- 异步写入一个 IP 报文 ----
    //
    // 写出 pkt.buffer() 中的完整报文。报文可通过 pkt 的字段构造接口
    // （begin_ipv4/begin_ipv6 -> begin_tcp/udp/icmp -> append_payload ->
    // finalize()）构建，或直接复用读入/改写的原始字节。
    template <typename CompletionToken>
    auto async_write_ip(ip_packet &pkt, CompletionToken &&token)
    {
        return net::async_initiate<CompletionToken,
            void(boost::system::error_code, size_t)>(
            [this, &pkt](auto handler) {
                impl_.async_write(pkt.buffer(), std::move(handler));
            },
            token);
    }

private:
    detail::tun_device_impl impl_;
};

} // namespace tunio
