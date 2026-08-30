//
// test_checksum.cpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "../src/ip_headers.hpp"

#include "test_harness.hpp"
#include "tunio/tun_config.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

// 验证引擎校验和实现与独立实现一致
int main()
{
    using tunio::detail::ip_checksum;
    using tunio::detail::ipv4_checksum;
    using tunio::detail::tcp_udp_checksum;
    using tunio::detail::verify_ipv4_checksum;

    // IP 头（checksum 字段由 ipv4_checksum 自动跳过）
    const uint32_t src = test::ip("10.0.0.2");
    const uint32_t dst = test::ip("8.8.8.8");
    std::vector<uint8_t> pkt = test::make_tcp(src, dst, 12345, 80, 0x10, 100,
        200, 65535, {'a', 'b', 'c'});

    // 生成器：ipv4_checksum 应等于报文中的校验和字段
    uint16_t stored = static_cast<uint16_t>((pkt[10] << 8) | pkt[11]);
    assert(ipv4_checksum(pkt.data(), 20) == stored);

    // 验证器：合法头部校验和为 0
    assert(verify_ipv4_checksum(pkt.data(), 20) == 0);

    // 损坏校验和
    std::vector<uint8_t> bad = pkt;
    bad[10] ^= 0xff;
    assert(verify_ipv4_checksum(bad.data(), 20) != 0);

    // TCP 校验
    const uint8_t *seg = pkt.data() + 20;
    const size_t seg_len = pkt.size() - 20;
    // 引擎从报文头部 memcpy 得到网络序地址，这里同样直接从包内取出
    uint32_t snet = 0, dnet = 0;
    std::memcpy(&snet, pkt.data() + 12, 4);
    std::memcpy(&dnet, pkt.data() + 16, 4);
    uint16_t tcp_csum = tcp_udp_checksum(snet, dnet, 6, seg, seg_len);
    assert(tcp_csum == 0);

    // 独立计算参考值
    // 地址按 host 序拆成 16 位字参与伪头求和
    uint32_t pseudo = (src >> 16) + (src & 0xffff) + (dst >> 16) +
        (dst & 0xffff) + 6 + seg_len;
    uint16_t ref = test::csum16(seg, seg_len, pseudo);
    assert(ref == 0);

    // UDP 校验
    std::vector<uint8_t> udp =
        test::make_udp(src, dst, 53000, 53, {1, 2, 3, 4});
    const uint8_t *useg = udp.data() + 20;
    const size_t ulen = udp.size() - 20;
    assert(tcp_udp_checksum(snet, dnet, 17, useg, ulen) == 0);

    // ---- IPv6 伪头部校验（128 位地址）----
    const auto s6 = test::v6("fd00::2");
    const auto d6 = test::v6("2001:4860:4860::8888");

    // TCP
    std::vector<uint8_t> pkt6 = test::make_tcp6(s6, d6, 12345, 80, 0x10, 100,
        200, 65535, {'a', 'b', 'c'});
    const uint8_t *seg6 = pkt6.data() + 40;
    const size_t seg6_len = pkt6.size() - 40;
    assert(tcp_udp_checksum(6, s6.data(), d6.data(), 6, seg6, seg6_len) == 0);

    // UDP
    std::vector<uint8_t> udp6 =
        test::make_udp6(s6, d6, 53000, 53, {1, 2, 3, 4});
    const uint8_t *useg6 = udp6.data() + 40;
    const size_t ulen6 = udp6.size() - 40;
    assert(tcp_udp_checksum(6, s6.data(), d6.data(), 17, useg6, ulen6) == 0);

    // ICMPv6
    std::vector<uint8_t> icmp6 =
        test::make_icmp6_echo(s6, d6, 0x1234, 1, {1, 2, 3});
    const uint8_t *ic6 = icmp6.data() + 40;
    const size_t ic6_len = icmp6.size() - 40;
    assert(tcp_udp_checksum(6, s6.data(), d6.data(), 58, ic6, ic6_len) == 0);

    // 单字节边界（奇数长度）
    uint8_t odd[] = {0x01, 0x02, 0x03};
    uint16_t c1 = test::csum16(odd, sizeof(odd));
    uint16_t c2 = ip_checksum(odd, sizeof(odd));
    assert(c1 == c2);

    return 0;
}
