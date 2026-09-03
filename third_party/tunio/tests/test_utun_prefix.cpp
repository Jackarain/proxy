//
// test_utun_prefix.cpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// macOS utun 注入路径回归测试：utun 帧 = 4 字节家族前缀 + IP 报文。
// 引擎读槽容量须按 read_size_hint（mtu + 4）分配，否则满 MTU 帧读入
// 时被截断、按残缺报文丢弃（修复前 1500 字节 IP 包整包丢失）.
#define BOOST_TEST_MODULE utun_prefix
#include <boost/test/included/unit_test.hpp>
#include "test_harness.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using namespace test;

namespace {
namespace net = boost::asio;

// 构造 utun 读帧：4 字节家族前缀（大端 AF_INET）+ 1500 字节 IPv4 UDP
// 报文（UDP 校验和为 0，IPv4 下合法，无需计算）。
std::vector<uint8_t> make_utun_frame()
{
    std::vector<uint8_t> frame(4 + 1500, 0xaa);
    frame[0] = 0;
    frame[1] = 0;
    frame[2] = 0;
    frame[3] = 2; // AF_INET 大端

    uint8_t *p = frame.data() + 4;
    p[0] = 0x45;                 // ver=4, ihl=5
    p[2] = 0x05;                 // total_len = 1500
    p[3] = 0xdc;
    p[4] = 0x12;                 // id
    p[5] = 0x34;
    p[6] = 0x40;                 // DF
    p[7] = 0x00;
    p[8] = 64;                   // ttl
    p[9] = 17;                   // udp
    p[10] = 0;                   // checksum（下面回填）
    p[11] = 0;
    p[12] = 10; p[13] = 0; p[14] = 0; p[15] = 2;   // src 10.0.0.2
    p[16] = 8;  p[17] = 8; p[18] = 8; p[19] = 8;   // dst 8.8.8.8
    const uint16_t ip_csum = csum16(p, 20);
    p[10] = static_cast<uint8_t>(ip_csum >> 8);
    p[11] = static_cast<uint8_t>(ip_csum & 0xff);

    // UDP 头（载荷任意填充）：IPv4 下 checksum 0 表示未计算，合法
    p[20] = 0x30; p[21] = 0x39;   // sport 12345
    p[22] = 0x00; p[23] = 0x35;   // dport 53
    p[24] = 0x05; p[25] = 0xc8;   // udp len = 1480
    p[26] = 0;    p[27] = 0;      // checksum = 0
    return frame;
}

} // namespace

BOOST_AUTO_TEST_CASE(test_utun_prefix_full_mtu_read)
{
    // 注入 socketpair 模拟 utun 句柄并启用 4 字节前缀路径.
    engine_env env(1500, std::chrono::seconds(1), std::chrono::seconds(30),
        std::chrono::seconds(30), 1024 * 1024,
        std::chrono::milliseconds(5000), std::chrono::milliseconds(200), 8,
        1, true);

    env.dev.send(make_utun_frame());

    // 等待引擎完成处理：满 MTU 帧须被完整读取并进入协议引擎（修复前
    // 截断丢弃，rx_packets 恒为 0）.
    const auto deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(2000);
    const auto &st = env.engine.stats();
    while (st.rx_packets.load(std::memory_order_relaxed) == 0 &&
        std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TEST_ASSERT(st.rx_packets.load(std::memory_order_relaxed) == 1);
    TEST_ASSERT(st.rx_dropped.load(std::memory_order_relaxed) == 0);
    // 报文完整进入 UDP 引擎：客户端三元组会话已建立
    TEST_ASSERT(st.udp_sessions.load(std::memory_order_relaxed) == 1);
}
