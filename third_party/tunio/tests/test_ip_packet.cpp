//
// test_ip_packet.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// ip_packet 与 tun_device::async_read_ip / async_write_ip 单元测试。
#define BOOST_TEST_MODULE ip_packet
#include <boost/test/included/unit_test.hpp>
#include "test_harness.hpp"

#include "tunio/ip_packet.hpp"
#include "tunio/tun_device.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace test;

namespace {
namespace net = boost::asio;
using tunio::ip_packet;
using tunio::tun_device;

// ---- 纯解析测试（不经过设备）----

BOOST_AUTO_TEST_CASE(test_parse_ipv4_tcp)
{
    const std::vector<uint8_t> data = {'a', 'b', 'c'};
    auto vec = make_tcp(0x0a000002, 0x08080808, 20000, 443, 0x12, 1000, 2000,
        65535, data);
    ip_packet p;
    p.parse(vec.data(), vec.size());
    TEST_ASSERT(p.valid());
    TEST_ASSERT(p.version() == 4);
    TEST_ASSERT(p.ip_protocol() == 6);
    TEST_ASSERT(p.is_tcp() && !p.is_udp() && !p.is_icmp());
    TEST_ASSERT(p.source_port() == 20000);
    TEST_ASSERT(p.destination_port() == 443);
    TEST_ASSERT(p.source_address() == net::ip::make_address("10.0.0.2"));
    TEST_ASSERT(p.destination_address() == net::ip::make_address("8.8.8.8"));
    TEST_ASSERT(p.total_length() == vec.size());
    TEST_ASSERT(p.tcp() != nullptr);
    TEST_ASSERT(p.tcp()->header_len() == 20);
    TEST_ASSERT((p.tcp()->flags & 0x12) == 0x12); // SYN|ACK
    TEST_ASSERT(ntohl(p.tcp()->seq) == 1000);
    TEST_ASSERT(p.payload_size() == vec.size() - 20);
    TEST_ASSERT(p.transport_data_size() == 3);
    TEST_ASSERT(std::memcmp(p.transport_data(), data.data(), 3) == 0);
    TEST_ASSERT(p.ipv4() != nullptr && p.ipv6() == nullptr);
    TEST_ASSERT(!p.fragmented() && p.fragment_offset() == 0);
}

BOOST_AUTO_TEST_CASE(test_parse_ipv4_udp)
{
    const std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto vec = make_udp(0x0a000002, 0x08080808, 30000, 53, data);
    ip_packet p;
    p.parse(vec.data(), vec.size());
    TEST_ASSERT(p.valid());
    TEST_ASSERT(p.is_udp());
    TEST_ASSERT(p.source_port() == 30000 && p.destination_port() == 53);
    TEST_ASSERT(p.udp() != nullptr);
    TEST_ASSERT(ntohs(p.udp()->length) == 8 + 5);
    TEST_ASSERT(p.transport_data_size() == 5);
    TEST_ASSERT(std::memcmp(p.transport_data(), data.data(), 5) == 0);
}

BOOST_AUTO_TEST_CASE(test_parse_ipv4_icmp)
{
    const std::vector<uint8_t> data = {0xde, 0xad, 0xbe, 0xef};
    auto vec = make_icmp_echo(0x0a000002, 0x0a000001, 0x1234, 7, data);
    ip_packet p;
    p.parse(vec.data(), vec.size());
    TEST_ASSERT(p.valid());
    TEST_ASSERT(p.is_icmp() && p.ip_protocol() == 1);
    TEST_ASSERT(p.icmp_type() == 8);
    TEST_ASSERT(p.icmp_code() == 0);
    TEST_ASSERT(p.icmp_checksum() != 0);
    TEST_ASSERT(p.icmp_echo_id() == 0x1234);
    TEST_ASSERT(p.icmp_echo_seq() == 7);
    TEST_ASSERT(p.transport_data_size() == 4);
    TEST_ASSERT(std::memcmp(p.transport_data(), data.data(), 4) == 0);
}

BOOST_AUTO_TEST_CASE(test_parse_ipv6_tcp)
{
    const std::vector<uint8_t> data = {1, 2, 3};
    auto vec =
        make_tcp6(v6("fd00::2"), v6("fd00::1"), 20000, 8080, 0x02, 1, 0,
            65535, data);
    ip_packet p;
    p.parse(vec.data(), vec.size());
    TEST_ASSERT(p.valid());
    TEST_ASSERT(p.version() == 6);
    TEST_ASSERT(p.is_tcp());
    TEST_ASSERT(p.source_port() == 20000 && p.destination_port() == 8080);
    TEST_ASSERT(p.source_address() == net::ip::make_address("fd00::2"));
    TEST_ASSERT(p.destination_address() == net::ip::make_address("fd00::1"));
    TEST_ASSERT(p.total_length() == vec.size());
    TEST_ASSERT(p.tcp() != nullptr);
    TEST_ASSERT(p.transport_data_size() == 3);
    TEST_ASSERT(p.ipv6() != nullptr && p.ipv4() == nullptr);
}

BOOST_AUTO_TEST_CASE(test_parse_ipv6_udp)
{
    const std::vector<uint8_t> data = {6, 5, 4, 3, 2, 1};
    auto vec = make_udp6(v6("fd00::2"), v6("fd00::1"), 40000, 5353, data);
    ip_packet p;
    p.parse(vec.data(), vec.size());
    TEST_ASSERT(p.valid());
    TEST_ASSERT(p.is_udp());
    TEST_ASSERT(p.source_port() == 40000 && p.destination_port() == 5353);
    TEST_ASSERT(p.transport_data_size() == 6);
}

BOOST_AUTO_TEST_CASE(test_parse_ipv6_icmp6_echo)
{
    const std::vector<uint8_t> data = {0xaa, 0xbb};
    auto vec = make_icmp6_echo(v6("fd00::2"), v6("fd00::1"), 0x7777, 3, data);
    ip_packet p;
    p.parse(vec.data(), vec.size());
    TEST_ASSERT(p.valid());
    TEST_ASSERT(p.is_icmpv6() && p.ip_protocol() == 58);
    TEST_ASSERT(p.icmp_type() == 128);
    TEST_ASSERT(p.icmp_echo_id() == 0x7777);
    TEST_ASSERT(p.icmp_echo_seq() == 3);
    TEST_ASSERT(p.transport_data_size() == 2);
}

BOOST_AUTO_TEST_CASE(test_parse_zero_copy)
{
    auto vec = make_tcp(0x0a000002, 0x08080808, 20000, 443, 0x02, 1, 0,
        65535, {1, 2, 3});
    ip_packet p;
    p.parse(vec.data(), vec.size());
    TEST_ASSERT(p.valid());
    // 传输层报文段零拷贝指向内部缓冲（IP 头之后）
    TEST_ASSERT(p.payload() == p.buffer().data() + 20);
    TEST_ASSERT(p.payload_size() == vec.size() - 20);
    // 应用数据在 TCP 头之后
    TEST_ASSERT(p.transport_data() == p.buffer().data() + 20 + 20);
    TEST_ASSERT(p.transport_data_size() == 3);
}

BOOST_AUTO_TEST_CASE(test_parse_malformed)
{
    // 版本非法
    {
        std::vector<uint8_t> v(40, 0);
        v[0] = 0x30; // version 3
        ip_packet p;
        p.parse(v.data(), v.size());
        TEST_ASSERT(!p.valid());
        TEST_ASSERT(p.error() == ip_packet::parse_error::invalid_version);
    }
    // 过短
    {
        std::vector<uint8_t> v(10, 0x45);
        ip_packet p;
        p.parse(v.data(), v.size());
        TEST_ASSERT(!p.valid());
        TEST_ASSERT(p.error() == ip_packet::parse_error::packet_too_short);
    }
    // IHL < 20
    {
        std::vector<uint8_t> v(40, 0);
        v[0] = 0x41; // version 4, ihl 1 -> header_len 4
        ip_packet p;
        p.parse(v.data(), v.size());
        TEST_ASSERT(!p.valid());
        TEST_ASSERT(p.error() == ip_packet::parse_error::invalid_ip_header_length);
    }
    // total_len 超过实际长度
    {
        auto vec = make_udp(0x0a000002, 0x08080808, 30000, 53, {1, 2, 3});
        vec[2] = 0xff;
        vec[3] = 0xff;
        ip_packet p;
        p.parse(vec.data(), vec.size());
        TEST_ASSERT(!p.valid());
        TEST_ASSERT(p.error() == ip_packet::parse_error::invalid_total_length);
    }
    // total_len < IHL
    {
        auto vec = make_udp(0x0a000002, 0x08080808, 30000, 53, {1, 2, 3});
        vec[2] = 0;
        vec[3] = 10; // total_len=10 < 20
        ip_packet p;
        p.parse(vec.data(), vec.size());
        TEST_ASSERT(!p.valid());
        TEST_ASSERT(p.error() == ip_packet::parse_error::invalid_total_length);
    }
    // TCP 段过短（载荷 < 20 字节）
    {
        std::vector<uint8_t> seg(4, 0);
        auto vec = make_ipv4(0x0a000002, 0x08080808, 6, seg);
        ip_packet p;
        p.parse(vec.data(), vec.size());
        TEST_ASSERT(!p.valid());
        TEST_ASSERT(p.error() == ip_packet::parse_error::invalid_transport_header);
    }
    // UDP length 字段 < 8
    {
        auto vec = make_udp(0x0a000002, 0x08080808, 30000, 53, {1, 2, 3});
        vec[24] = 0;
        vec[25] = 4; // UDP length=4 < 8（偏移 24 为 UDP 头内 length 字段）
        ip_packet p;
        p.parse(vec.data(), vec.size());
        TEST_ASSERT(!p.valid());
        TEST_ASSERT(p.error() == ip_packet::parse_error::invalid_transport_header);
    }
    // IPv6 payload_len 超过实际长度
    {
        auto vec =
            make_udp6(v6("fd00::2"), v6("fd00::1"), 30000, 53, {1, 2, 3});
        vec[4] = 0xff;
        vec[5] = 0xff;
        ip_packet p;
        p.parse(vec.data(), vec.size());
        TEST_ASSERT(!p.valid());
        TEST_ASSERT(p.error() == ip_packet::parse_error::invalid_total_length);
    }
}

BOOST_AUTO_TEST_CASE(test_parse_fragments)
{
    // 非首片（偏移 > 0）：解析成功、暴露分片字段、不解析传输层
    {
        auto vec =
            make_udp(0x0a000002, 0x08080808, 30000, 53, {1, 2, 3, 4, 5, 6, 7, 8});
        vec[6] = 0x00;
        vec[7] = 0x01; // frag_off: 偏移字段值 1（= 8 字节）
        ip_packet p;
        p.parse(vec.data(), vec.size());
        TEST_ASSERT(p.valid());
        TEST_ASSERT(p.fragmented());
        TEST_ASSERT(p.fragment_offset() == 8);
        TEST_ASSERT(p.is_udp());         // 协议号仍可见
        TEST_ASSERT(p.udp() == nullptr); // 非首片不解析传输层
        TEST_ASSERT(p.payload() != nullptr);
        TEST_ASSERT(p.payload_size() == vec.size() - 20);
    }
    // 首片（偏移 0，MF 置位）：含完整传输层头
    {
        auto vec =
            make_udp(0x0a000002, 0x08080808, 30000, 53, {1, 2, 3, 4, 5, 6, 7, 8});
        vec[6] = 0x20;
        vec[7] = 0x00; // MF
        ip_packet p;
        p.parse(vec.data(), vec.size());
        TEST_ASSERT(p.valid());
        TEST_ASSERT(p.fragmented());
        TEST_ASSERT(p.fragment_offset() == 0);
        TEST_ASSERT(p.udp() != nullptr);
    }
}

BOOST_AUTO_TEST_CASE(test_parse_v6_extension_header)
{
    // next_header = 44（Fragment 扩展头）：不遍历扩展头链
    std::vector<uint8_t> ext(8, 0);
    auto vec = make_ipv6(v6("fd00::2"), v6("fd00::1"), 44, ext);
    ip_packet p;
    p.parse(vec.data(), vec.size());
    TEST_ASSERT(p.valid());
    TEST_ASSERT(p.version() == 6);
    TEST_ASSERT(p.ip_protocol() == 44); // 原始扩展头号
    TEST_ASSERT(!p.is_tcp() && !p.is_udp() && !p.is_icmpv6());
    TEST_ASSERT(p.tcp() == nullptr && p.udp() == nullptr);
    TEST_ASSERT(p.payload() != nullptr);
    TEST_ASSERT(p.payload_size() == 8);
}

BOOST_AUTO_TEST_CASE(test_parse_buffer_too_small)
{
    ip_packet p(64, 16); // 可用 48 字节
    std::vector<uint8_t> payload(28, 7);
    auto vec = make_udp(0x0a000002, 0x08080808, 30000, 53, payload);
    // vec.size() = 20 + 8 + 28 = 56 > 48
    p.parse(vec.data(), vec.size());
    TEST_ASSERT(!p.valid());
    TEST_ASSERT(p.error() == ip_packet::parse_error::buffer_too_small);
}

// ---- 字段构造（builder）测试 ----

BOOST_AUTO_TEST_CASE(test_build_ipv4_tcp)
{
    ip_packet p;
    p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
        net::ip::make_address_v4("8.8.8.8"));
    const uint8_t mss[4] = {2, 4, 0x05, 0xb4};
    p.begin_tcp(12345, 443, 1000, 0, 0x02 /* SYN */, 65535, mss, 4);
    const std::vector<uint8_t> data = {'h', 'i'};
    p.append_payload(data.data(), data.size());
    p.finalize();
    TEST_ASSERT(p.valid());
    TEST_ASSERT(p.is_tcp());
    TEST_ASSERT(p.source_port() == 12345 && p.destination_port() == 443);
    TEST_ASSERT(p.tcp() != nullptr);
    TEST_ASSERT(p.tcp()->header_len() == 24);
    TEST_ASSERT(p.transport_data_size() == 2);
    TEST_ASSERT(std::memcmp(p.transport_data(), "hi", 2) == 0);

    // 独立校验和验证 + 二次解析
    std::vector<uint8_t> raw(
        p.buffer().data(), p.buffer().data() + p.buffer().size());
    TEST_ASSERT(verify_packet(raw));
    ip_packet p2;
    p2.parse(raw.data(), raw.size());
    TEST_ASSERT(p2.valid() && p2.is_tcp());
    TEST_ASSERT(p2.source_port() == 12345 && p2.destination_port() == 443);
    TEST_ASSERT(p2.source_address() == net::ip::make_address("10.0.0.1"));
    TEST_ASSERT(p2.destination_address() == net::ip::make_address("8.8.8.8"));
    TEST_ASSERT(ntohl(p2.tcp()->seq) == 1000);
}

BOOST_AUTO_TEST_CASE(test_build_ipv4_udp)
{
    ip_packet p;
    p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
        net::ip::make_address_v4("8.8.8.8"));
    p.begin_udp(12345, 53);
    const char payload[] = "hello udp";
    p.append_payload(payload, sizeof(payload) - 1);
    p.finalize();
    TEST_ASSERT(p.valid() && p.is_udp());
    TEST_ASSERT(p.source_port() == 12345 && p.destination_port() == 53);
    TEST_ASSERT(p.transport_data_size() == sizeof(payload) - 1);
    std::vector<uint8_t> raw(
        p.buffer().data(), p.buffer().data() + p.buffer().size());
    TEST_ASSERT(verify_packet(raw));
}

BOOST_AUTO_TEST_CASE(test_build_ipv4_icmp_echo)
{
    ip_packet p;
    p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
        net::ip::make_address_v4("10.0.0.2"));
    p.begin_icmp(8, 0);
    p.set_icmp_echo(0xbeef, 42);
    const char payload[] = "ping";
    p.append_payload(payload, 4);
    p.finalize();
    TEST_ASSERT(p.valid() && p.is_icmp());
    TEST_ASSERT(p.icmp_type() == 8);
    TEST_ASSERT(p.icmp_echo_id() == 0xbeef);
    TEST_ASSERT(p.icmp_echo_seq() == 42);
    std::vector<uint8_t> raw(
        p.buffer().data(), p.buffer().data() + p.buffer().size());
    TEST_ASSERT(verify_packet(raw));
}

BOOST_AUTO_TEST_CASE(test_build_ipv6_tcp)
{
    ip_packet p;
    p.begin_ipv6(net::ip::make_address_v6("fd00::1"),
        net::ip::make_address_v6("fd00::2"));
    p.begin_tcp(20000, 8080, 5, 6, 0x10 /* ACK */, 4096);
    const std::vector<uint8_t> data = {9, 8, 7};
    p.append_payload(data.data(), data.size());
    p.finalize();
    TEST_ASSERT(p.valid() && p.version() == 6 && p.is_tcp());
    TEST_ASSERT(p.source_port() == 20000 && p.destination_port() == 8080);
    TEST_ASSERT(p.source_address() == net::ip::make_address("fd00::1"));
    TEST_ASSERT(p.transport_data_size() == 3);
    std::vector<uint8_t> raw(
        p.buffer().data(), p.buffer().data() + p.buffer().size());
    TEST_ASSERT(verify_packet6(raw));
}

BOOST_AUTO_TEST_CASE(test_build_ipv6_udp)
{
    ip_packet p;
    p.begin_ipv6(net::ip::make_address_v6("fd00::1"),
        net::ip::make_address_v6("fd00::2"));
    p.begin_udp(30000, 5353);
    const char payload[] = "v6 udp";
    p.append_payload(payload, sizeof(payload) - 1);
    p.finalize();
    TEST_ASSERT(p.valid() && p.is_udp());
    TEST_ASSERT(p.transport_data_size() == sizeof(payload) - 1);
    std::vector<uint8_t> raw(
        p.buffer().data(), p.buffer().data() + p.buffer().size());
    TEST_ASSERT(verify_packet6(raw));
}

BOOST_AUTO_TEST_CASE(test_build_ipv6_udp_checksum_zero)
{
    // RFC 768/8200：IPv6 UDP 校验和强制存在且不可为 0（0 在 IPv4 中表示
    // 未计算），计算得 0 时必须传输 0xffff，否则生成非法报文被对端丢弃.
    const auto src6 = net::ip::make_address_v6("fd00::1").to_bytes();
    const auto dst6 = net::ip::make_address_v6("fd00::2").to_bytes();
    uint8_t seg[8];
    seg[0] = 0x75;             // sport 30000
    seg[1] = 0x30;
    seg[4] = 0;                // udp length = 8（无载荷）
    seg[5] = 8;
    seg[6] = 0;                // checksum 占位
    seg[7] = 0;

    // 遍历目标端口（16 位全空间，按模 65535 必命中）定位校验和为 0 的组合.
    uint16_t zero_port = 0;
    bool found = false;
    for (uint32_t port = 0; port <= 0xffff; ++port) {
        seg[2] = static_cast<uint8_t>(port >> 8);
        seg[3] = static_cast<uint8_t>(port & 0xff);
        if (tunio::tcp_udp_checksum(6, src6.data(), dst6.data(),
                tunio::ip_protocol_udp, seg, sizeof(seg)) == 0) {
            zero_port = static_cast<uint16_t>(port);
            found = true;
            break;
        }
    }
    TEST_ASSERT(found);

    ip_packet p;
    p.begin_ipv6(net::ip::make_address_v6("fd00::1"),
        net::ip::make_address_v6("fd00::2"));
    p.begin_udp(30000, zero_port);
    p.finalize();
    TEST_ASSERT(p.valid() && p.is_udp());
    TEST_ASSERT(p.udp() != nullptr);
    TEST_ASSERT(p.udp()->checksum == 0xffff);
}

BOOST_AUTO_TEST_CASE(test_build_ipv6_icmp6_echo)
{
    ip_packet p;
    p.begin_ipv6(net::ip::make_address_v6("fd00::1"),
        net::ip::make_address_v6("fd00::2"));
    p.begin_icmp(128, 0);
    p.set_icmp_echo(7, 8);
    p.finalize();
    TEST_ASSERT(p.valid() && p.is_icmpv6());
    TEST_ASSERT(p.icmp_type() == 128);
    TEST_ASSERT(p.icmp_echo_id() == 7);
    TEST_ASSERT(p.icmp_echo_seq() == 8);
    std::vector<uint8_t> raw(
        p.buffer().data(), p.buffer().data() + p.buffer().size());
    TEST_ASSERT(verify_packet6(raw));
}

BOOST_AUTO_TEST_CASE(test_build_ip_id)
{
    ip_packet p;
    p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
        net::ip::make_address_v4("10.0.0.2"));
    p.begin_udp(1, 2);
    p.set_ip_id(0x1234);
    p.finalize();
    TEST_ASSERT(p.ipv4() != nullptr);
    TEST_ASSERT(ntohs(p.ipv4()->id) == 0x1234);
}

BOOST_AUTO_TEST_CASE(test_builder_precondition)
{
    ip_packet p;
    bool threw = false;
    try {
        p.begin_tcp(1, 2, 0, 0, 0, 0);
    } catch (const std::logic_error &) {
        threw = true;
    }
    TEST_ASSERT(threw);
}

BOOST_AUTO_TEST_CASE(test_builder_double_begin)
{
    ip_packet p;
    p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
        net::ip::make_address_v4("10.0.0.2"));
    p.begin_udp(1, 2);
    // 传输层已开始后再 begin_* 应抛 std::logic_error
    bool threw = false;
    try {
        p.begin_tcp(3, 4, 0, 0, 0, 0);
    } catch (const std::logic_error &) {
        threw = true;
    }
    TEST_ASSERT(threw);
    threw = false;
    try {
        p.begin_udp(5, 6);
    } catch (const std::logic_error &) {
        threw = true;
    }
    TEST_ASSERT(threw);
}

BOOST_AUTO_TEST_CASE(test_builder_capacity_guard)
{
    // IP 头放不下（writable 16 < 20）
    {
        ip_packet p(16, 0);
        bool threw = false;
        try {
            p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
                net::ip::make_address_v4("10.0.0.2"));
        } catch (const std::length_error &) {
            threw = true;
        }
        TEST_ASSERT(threw);
    }
    // 传输层头放不下：可用 48 字节，IP 头 20 后剩 28 < 60（20 + 40 选项）
    {
        ip_packet p(64, 16);
        p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
            net::ip::make_address_v4("10.0.0.2"));
        uint8_t opts[40] = {};
        bool threw = false;
        try {
            p.begin_tcp(1, 2, 0, 0, 0, 0, opts, 40);
        } catch (const std::length_error &) {
            threw = true;
        }
        TEST_ASSERT(threw);
    }
    // 正常容量下不抛
    {
        ip_packet p;
        p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
            net::ip::make_address_v4("10.0.0.2"));
        p.begin_udp(1, 2);
        TEST_ASSERT(p.valid());
    }
}

BOOST_AUTO_TEST_CASE(test_parse_clears_builder_state)
{
    // 构造一个包
    ip_packet p;
    p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
        net::ip::make_address_v4("10.0.0.2"));
    p.begin_udp(12345, 53);
    const char hello[] = "hello";
    p.append_payload(hello, 5);
    p.finalize();
    TEST_ASSERT(p.valid() && p.is_udp());

    // 解析另一报文后，builder 状态已被清除，finalize 应为 no-op
    auto vec = make_udp(0x0a000002, 0x08080808, 30000, 53, {1, 2, 3});
    p.parse(vec.data(), vec.size());
    TEST_ASSERT(p.valid() && p.is_udp());
    TEST_ASSERT(p.source_port() == 30000);
    const size_t parsed_size = p.buffer().size();
    const std::vector<uint8_t> parsed(
        p.buffer().data(), p.buffer().data() + parsed_size);

    p.finalize(); // bld_.version == 0 -> 直接返回，不改动缓冲
    TEST_ASSERT(p.buffer().size() == parsed_size);
    TEST_ASSERT(std::memcmp(p.buffer().data(), parsed.data(), parsed_size) == 0);
    TEST_ASSERT(p.valid() && p.is_udp() && p.source_port() == 30000);
}

// ---- 设备级测试（socketpair 注入 tun_device）----

struct dev_env
{
    net::io_context io;
    tun_device dev;
    int peer = -1;
    std::thread thread;
    net::executor_work_guard<net::io_context::executor_type> guard;

    dev_env()
        : dev(io)
        , guard(net::make_work_guard(io))
    {
        int sv[2];
        if (::socketpair(AF_UNIX, SOCK_DGRAM, 0, sv) != 0) {
            TEST_THROW("socketpair failed");
        }
        peer = sv[1];
        boost::system::error_code ec;
        if (!dev.assign(sv[0], 1500, false, ec)) {
            TEST_THROW(
                "tun_device assign failed: " + ec.message());
        }
        thread = std::thread([this] { io.run(); });
    }

    ~dev_env()
    {
        dev.close();
        guard.reset();
        if (thread.joinable()) {
            thread.join();
        }
        ::close(peer);
    }
};

// 从原始 fd 读一个完整数据报（SOCK_DGRAM 语义），超时返回 false
static bool read_fd_packet(int fd, std::vector<uint8_t> &out, int timeout_ms = 3000)
{
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    const int remaining = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now())
            .count());
    if (remaining <= 0) {
        return false;
    }
    struct pollfd pfd{fd, POLLIN, 0};
    const int r = ::poll(&pfd, 1, remaining);
    if (r <= 0) {
        return false;
    }
    uint8_t buf[65536];
    const ssize_t n = ::read(fd, buf, sizeof(buf));
    if (n <= 0) {
        return false;
    }
    out.assign(buf, buf + n);
    return true;
}

BOOST_AUTO_TEST_CASE(test_device_read_ip)
{
    dev_env env;
    ip_packet pkt;
    auto fut = env.dev.async_read_ip(pkt, net::use_future);
    auto vec = make_tcp(0x0a000002, 0x08080808, 20000, 443, 0x12, 100, 0,
        65535, {1, 2, 3, 4});
    (void)::write(env.peer, vec.data(), vec.size());
    // use_future 对 void(ec, size_t) 签名：成功返回 size_t，设备错误抛 system_error
    const size_t n = future_get(std::move(fut));
    TEST_ASSERT(n == vec.size());
    TEST_ASSERT(pkt.valid());
    TEST_ASSERT(pkt.version() == 4 && pkt.is_tcp());
    TEST_ASSERT(pkt.source_port() == 20000 && pkt.destination_port() == 443);
    TEST_ASSERT(pkt.source_address() == net::ip::make_address("10.0.0.2"));
    TEST_ASSERT(pkt.destination_address() == net::ip::make_address("8.8.8.8"));
    TEST_ASSERT(pkt.transport_data_size() == 4);
    TEST_ASSERT(std::memcmp(pkt.transport_data(), vec.data() + 40, 4) == 0);
}

BOOST_AUTO_TEST_CASE(test_device_write_ip)
{
    dev_env env;
    ip_packet pkt;
    pkt.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
        net::ip::make_address_v4("10.0.0.2"));
    pkt.begin_udp(12345, 53);
    const char hello[] = "hello";
    pkt.append_payload(hello, 5);
    pkt.finalize();
    TEST_ASSERT(pkt.valid());
    TEST_ASSERT(pkt.is_udp() && pkt.source_port() == 12345);

    auto fut = env.dev.async_write_ip(pkt, net::use_future);
    const size_t n = future_get(std::move(fut));
    TEST_ASSERT(n == pkt.buffer().size());

    std::vector<uint8_t> out;
    TEST_ASSERT(read_fd_packet(env.peer, out));
    TEST_ASSERT(verify_packet(out)); // 独立校验和验证
    ip_packet p2;
    p2.parse(out.data(), out.size());
    TEST_ASSERT(p2.valid() && p2.is_udp());
    TEST_ASSERT(p2.source_port() == 12345 && p2.destination_port() == 53);
    TEST_ASSERT(p2.source_address() == net::ip::make_address("10.0.0.1"));
    TEST_ASSERT(p2.payload_size() == 8 + 5); // 传输层报文段（UDP 头 + 数据）
    TEST_ASSERT(p2.transport_data_size() == 5);
    TEST_ASSERT(std::memcmp(p2.transport_data(), hello, 5) == 0);
}

BOOST_AUTO_TEST_CASE(test_device_concurrent_reads)
{
    dev_env env;
    std::array<ip_packet, 4> pkts;
    std::array<std::future<size_t>, 4> futs;
    for (size_t i = 0; i < 4; ++i) {
        futs[i] = env.dev.async_read_ip(pkts[i], net::use_future);
    }
    for (size_t i = 0; i < 4; ++i) {
        auto vec = make_udp(0x0a000002, 0x08080808,
            10000 + static_cast<uint16_t>(i), 53,
            {static_cast<uint8_t>(i)});
        (void)::write(env.peer, vec.data(), vec.size());
    }
    for (size_t i = 0; i < 4; ++i) {
        (void)future_get(std::move(futs[i]));
        TEST_ASSERT(pkts[i].valid() && pkts[i].is_udp());
        TEST_ASSERT(pkts[i].source_port() == 10000 + i);
    }
}

BOOST_AUTO_TEST_CASE(test_device_read_invalid)
{
    dev_env env;
    ip_packet pkt;
    auto fut = env.dev.async_read_ip(pkt, net::use_future);
    std::vector<uint8_t> garbage(64, 0xab);
    garbage[0] = 0x35; // version 3
    (void)::write(env.peer, garbage.data(), garbage.size());
    // 设备读成功（非 I/O 错误），use_future 直接返回字节数
    const size_t n = future_get(std::move(fut));
    TEST_ASSERT(n == garbage.size());
    TEST_ASSERT(!pkt.valid());
    TEST_ASSERT(pkt.error() == ip_packet::parse_error::invalid_version);
}

BOOST_AUTO_TEST_CASE(test_device_read_capacity_guard)
{
    dev_env env; // mtu = 1500
    // 缓冲可用容量 1024 - 128 = 896 < 1500：不注入任何数据也应立即失败
    ip_packet pkt(1024, 128);
    auto fut = env.dev.async_read_ip(pkt, net::use_future);
    bool threw = false;
    boost::system::error_code ec;
    try {
        (void)future_get(std::move(fut));
    } catch (const boost::system::system_error &e) {
        ec = e.code();
        threw = true;
    }
    TEST_ASSERT(threw);
    TEST_ASSERT(ec == net::error::message_size);
}

} // namespace

