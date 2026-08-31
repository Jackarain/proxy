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

private:
    detail::tun_device_impl impl_;
};

} // namespace tunio
