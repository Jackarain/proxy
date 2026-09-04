//
// test_mac_utun_open.cpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// macOS utun 自主打开回归测试：经内核控制套接字（com.apple.net.utun_control）
// 创建 utun 设备并配置地址/MTU。需要 root（创建与配置接口均需特权）；
// CI 以非 root 运行时创建会以 EPERM 失败，测试直接跳过。
#define BOOST_TEST_MODULE mac_utun_open
#include <boost/test/included/unit_test.hpp>

#include "tunio/tun_device.hpp"

#include <unistd.h>

using namespace tunio;

BOOST_AUTO_TEST_CASE(test_mac_utun_autonomous_open)
{
    // 非 root 无权限创建 utun：跳过（CI 场景）。
    if (::geteuid() != 0)
        return;

    net::io_context ctx;
    tun_device dev(ctx);

    // 空设备名：由内核自动分配最低可用 utun 单元；同时配置 IPv4/IPv6 与
    // MTU，覆盖地址配置路径（SIOCSIFADDR/SIOCSIFNETMASK/SIOCAIFADDR_IN6）。
    device_config cfg;
    cfg.mtu = 1400;
    cfg.ipv4 = "10.88.88.1";
    cfg.netmask = "255.255.255.0";
    cfg.ipv6 = "fd88::1";
    cfg.ipv6_prefix_len = 64;

    boost::system::error_code ec;
    const bool ok = dev.open(cfg, ec);
    if (!ok && (ec == boost::system::errc::operation_not_permitted ||
                ec == boost::system::errc::permission_denied))
    {
        return; // 无权限，跳过断言。
    }
    BOOST_TEST(ok);
    if (!ok)
        return;

    // 打开成功：单队列、MTU 生效、utun 读含 4 字节家族前缀。
    BOOST_TEST(dev.is_open());
    BOOST_TEST(dev.queue_count() == 1);
    BOOST_TEST(dev.mtu() == 1400);
    BOOST_TEST(dev.read_size_hint() == 1400 + 4);

    dev.close();
    BOOST_TEST(!dev.is_open());
}