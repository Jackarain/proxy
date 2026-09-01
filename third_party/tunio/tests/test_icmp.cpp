//
// test_icmp.cpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#define BOOST_TEST_MODULE icmp
#include <boost/test/included/unit_test.hpp>
#include "test_harness.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

using namespace test;

BOOST_AUTO_TEST_CASE(icmp)
{
    engine_env env;
    auto &io = env.io;
    (void)io;

    // 对引擎本地虚拟 IP 的 ICMP Echo Request
    const std::vector<uint8_t> payload = {0xde, 0xad, 0xbe, 0xef};
    env.dev.send(
        make_icmp_echo(0x0a000002, 0x0a000001, 0x1234, 0x0001, payload));

    std::vector<uint8_t> pkt;
    if (!env.dev.read_packet(pkt)) {
        TEST_THROW("no ICMP reply");
    }
    if (!verify_packet(pkt)) {
        TEST_THROW("verify_packet failed");
    }
    ip_hdr_info ipi;
    if (!parse_ip(pkt, ipi)) {
        TEST_THROW("parse_ip failed");
    }
    TEST_ASSERT(ipi.src == 0x0a000001 && ipi.dst == 0x0a000002 && ipi.proto == 1);

    // Echo Reply：type=0，ID/序号/数据保持
    const uint8_t *icmp = ipi.payload;
    TEST_ASSERT(icmp[0] == 0);
    TEST_ASSERT(icmp[1] == 0);
    TEST_ASSERT(icmp[4] == 0x12 && icmp[5] == 0x34); // identifier
    TEST_ASSERT(icmp[6] == 0x00 && icmp[7] == 0x01); // sequence
    TEST_ASSERT(ipi.payload_len == 8 + payload.size());
    TEST_ASSERT(std::memcmp(icmp + 8, payload.data(), payload.size()) == 0);

    // ICMP 校验和有效
    const uint16_t c = csum16(icmp, ipi.payload_len);
    TEST_ASSERT(c == 0);

    // 发给其他地址的 Echo 不应响应
    env.dev.send(make_icmp_echo(0x0a000002, 0x0a000003, 0x0001, 0x0002, {}));
    std::vector<uint8_t> ignored;
    if (env.dev.read_packet(ignored, 300)) {
        TEST_THROW("unexpected reply to non-local address");
    }
}
