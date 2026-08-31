//
// tun_device_posix.hpp
// ~~~~~~~~~~~~~~~~~~~~
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

#include <sys/socket.h>

#include <functional>

namespace tunio {
namespace net = boost::asio;
namespace detail {

// POSIX 实现 (Linux TUN / macOS utun)：基于 posix::stream_descriptor。
// 平台相关打开逻辑见 src/tun_device_posix.cpp。
class posix_tun_device_impl
{
public:
    explicit posix_tun_device_impl(net::io_context &ctx)
        : desc_(ctx)
    {
    }

    bool open(const device_config &cfg, boost::system::error_code &ec);

    bool assign(native_handle_type handle, size_t mtu, bool utun_prefix,
        boost::system::error_code &ec)
    {
        desc_.assign(static_cast<native_handle_type>(handle), ec);
        if (!ec) {
            open_ = true;
            mtu_ = mtu;
            utun_prefix_ = utun_prefix;
        }
        return !ec;
    }

    void close()
    {
        if (open_) {
            desc_.close();
            open_ = false;
        }
    }

    size_t mtu() const
    {
        return mtu_;
    }
    bool is_open() const
    {
        return open_;
    }

    template <typename Handler>
    void async_read(packet_buffer &buf, Handler &&handler)
    {
        if (utun_prefix_) {
            // macOS utun 每次读取携带 4 字节家族前缀（大端 AF_INET/AF_INET6）：
            // 读入后跳过前缀，引擎仍按"完整 IP 报文"解析.
            desc_.async_read_some(
                net::buffer(buf.writable_data(), buf.writable_size()),
                [h = std::forward<Handler>(handler), &buf](
                    const boost::system::error_code &ec, size_t n) mutable {
                    size_t pkt_n = n;
                    if (!ec && n > 4) {
                        std::memmove(buf.data(), buf.data() + 4, n - 4);
                        pkt_n = n - 4;
                    }
                    std::move(h)(ec, pkt_n);
                });
        } else {
            desc_.async_read_some(
                net::buffer(buf.writable_data(), buf.writable_size()),
                std::forward<Handler>(handler));
        }
    }

    template <typename Handler>
    void async_write(packet_buffer &buf, Handler &&handler)
    {
        if (utun_prefix_) {
            // macOS utun 写入必须前置 4 字节家族前缀（大端 AF_INET/AF_INET6），
            // 前缀写入 packet_buffer 的 headroom 区域（引擎出包 headroom >= 64）.
            uint8_t *prefix = buf.data() - 4;
            const uint32_t family =
                (buf.data()[0] >> 4) == 6 ? AF_INET6 : AF_INET;
            prefix[0] = static_cast<uint8_t>(family >> 24);
            prefix[1] = static_cast<uint8_t>(family >> 16);
            prefix[2] = static_cast<uint8_t>(family >> 8);
            prefix[3] = static_cast<uint8_t>(family);
            desc_.async_write_some(net::buffer(prefix, buf.size() + 4),
                                   std::forward<Handler>(handler));
        } else {
            desc_.async_write_some(net::buffer(buf.data(), buf.size()),
                                   std::forward<Handler>(handler));
        }
    }

    net::posix::stream_descriptor desc_;
    size_t mtu_ = 1500;
    bool open_ = false;
    bool utun_prefix_ = false;
};

} // namespace detail
} // namespace tunio
