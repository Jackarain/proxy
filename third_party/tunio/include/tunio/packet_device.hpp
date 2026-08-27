//
// packet_device.hpp
// ~~~~~~~~~~~~~~~~~
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

#include <memory>
#include <variant>

// 平台实现按文件拆分，参考 Asio 的 impl 目录布局：
//   detail/impl/packet_device_posix.hpp
//   detail/impl/packet_device_windows.hpp
//   detail/impl/packet_device_wintun.hpp
//   detail/impl/packet_device_unsupported.hpp
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
#include "tunio/detail/impl/packet_device_posix.hpp"
#elif defined(_WIN32) && defined(USE_WINTUN_DRIVER)
#include "tunio/detail/impl/packet_device_wintun.hpp"
#elif defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)
#include "tunio/detail/impl/packet_device_windows.hpp"
#else
#include "tunio/detail/impl/packet_device_unsupported.hpp"
#endif

namespace tunio {
namespace net = boost::asio;
namespace detail {

using device_impl_variant = std::variant<
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
    posix_packet_device_impl
#elif defined(_WIN32) && defined(USE_WINTUN_DRIVER)
    wintun_packet_device_impl
#elif defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)
    windows_packet_device_impl
#else
    unsupported_packet_device
#endif
    >;

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
class packet_device
{
public:
    explicit packet_device(net::io_context &ctx)
        : ctx_(ctx)
        , impl_(std::in_place_index<0>, ctx)
    {
    }

    // ---- 模式 1: 自主打开 ----
    bool open(const device_config &cfg, boost::system::error_code &ec)
    {
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
        impl_.emplace<detail::posix_packet_device_impl>(ctx_);
#elif defined(_WIN32) && defined(USE_WINTUN_DRIVER)
        impl_.emplace<detail::wintun_packet_device_impl>(ctx_);
#elif defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)
        impl_.emplace<detail::windows_packet_device_impl>(ctx_);
#else
        impl_.emplace<detail::unsupported_packet_device>(ctx_);
#endif
        return std::visit([&](auto &impl) { return impl.open(cfg, ec); },
                          impl_);
    }

    // ---- 模式 2: 句柄注入 ----
    bool assign(native_handle_type handle, size_t mtu,
                boost::system::error_code &ec)
    {
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
        impl_.emplace<detail::posix_packet_device_impl>(ctx_);
#elif defined(_WIN32) && defined(USE_WINTUN_DRIVER)
        impl_.emplace<detail::wintun_packet_device_impl>(ctx_);
#elif defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)
        impl_.emplace<detail::windows_packet_device_impl>(ctx_);
#else
        impl_.emplace<detail::unsupported_packet_device>(ctx_);
#endif
        return std::visit(
            [&](auto &impl) { return impl.assign(handle, mtu, ec); }, impl_);
    }

    void close()
    {
        std::visit([](auto &impl) { impl.close(); }, impl_);
    }

    size_t mtu() const
    {
        return std::visit([](const auto &impl) -> size_t { return impl.mtu(); },
                          impl_);
    }

    bool is_open() const
    {
        return std::visit(
            [](const auto &impl) -> bool { return impl.is_open(); }, impl_);
    }

    // ---- 异步读取一个完整数据包 ----
    template <typename CompletionToken>
    auto async_read_packet(packet_buffer &buf, CompletionToken &&token)
    {
        return net::async_initiate<CompletionToken,
                                   void(boost::system::error_code, size_t)>(
            [this, &buf](auto handler) {
                std::visit(
                    [&buf, h = std::move(handler)](auto &impl) mutable {
                        impl.async_read(buf, std::move(h));
                    },
                    impl_);
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
                std::visit(
                    [&buf, h = std::move(handler)](auto &impl) mutable {
                        impl.async_write(buf, std::move(h));
                    },
                    impl_);
            },
            token);
    }

private:
    net::io_context &ctx_;
    detail::device_impl_variant impl_;
};

} // namespace tunio
