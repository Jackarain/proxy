//
// consumer.cpp
// ~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// 安装包消费方冒烟用例：包含全部公开头并实例化其中的模板接口。模板定义
// 位于公开头内联包含的 detail/ 实现头（tun_tcp_socket_ops.hpp 等），其
// 函数体引用引擎实现头，因此本文件既能覆盖"头文件是否随包导出"（预处理
// 阶段），也能覆盖模板实例化是否可完整编译（否则缺失的实现头只会在
// 实例化时报错）.

#include "tunio/ip_packet.hpp"
#include "tunio/tun_device.hpp"
#include "tunio/tun_tcp_acceptor.hpp"
#include "tunio/tun_tcp_socket.hpp"
#include "tunio/tun_udp_acceptor.hpp"
#include "tunio/tun_udp_socket.hpp"
#include "tunio/tunio.hpp"

#include <boost/asio.hpp>

#include <cstddef>

namespace net = boost::asio;

namespace {

void instantiate_socket_ops(tunio::tunio& engine)
{
    tunio::tun_tcp_acceptor tcp_acceptor(engine);
    tunio::tun_tcp_socket tcp_peer(engine.get_executor());
    tcp_acceptor.async_accept(tcp_peer, [](boost::system::error_code) {});
    tcp_acceptor.cancel();

    char tcp_buf[64];
    tcp_peer.async_read_some(net::buffer(tcp_buf),
        [](boost::system::error_code, std::size_t) {});
    tcp_peer.async_write_some(net::buffer(tcp_buf, 4),
        [](boost::system::error_code, std::size_t) {});

    tunio::tun_udp_acceptor udp_acceptor(engine);
    tunio::tun_udp_socket udp_peer(engine.get_executor());
    udp_acceptor.async_accept(udp_peer, [](boost::system::error_code) {});
    udp_acceptor.cancel();

    net::ip::udp::endpoint remote;
    char udp_buf[64];
    udp_peer.async_receive_from(net::buffer(udp_buf), remote,
        [](boost::system::error_code, std::size_t) {});
    udp_peer.async_send_to(remote, net::buffer(udp_buf, 4),
        [](boost::system::error_code, std::size_t) {});
}

} // namespace

int main()
{
    net::io_context ctx;
    tunio::tunio engine(ctx);

    // 设备与报文解析接口（编译期覆盖，不打开真实 TUN 设备）
    tunio::ip_packet packet;
    (void)engine.mtu();
    (void)packet.valid();

    instantiate_socket_ops(engine);

    return 0;
}
