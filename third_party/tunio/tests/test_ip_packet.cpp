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

static void test_parse_ipv4_tcp()
{
    const std::vector<uint8_t> data = {'a', 'b', 'c'};
    auto vec = make_tcp(0x0a000002, 0x08080808, 20000, 443, 0x12, 1000, 2000,
        65535, data);
    ip_packet p;
    p.parse(vec.data(), vec.size());
    assert(p.valid());
    assert(p.version() == 4);
    assert(p.ip_protocol() == 6);
    assert(p.is_tcp() && !p.is_udp() && !p.is_icmp());
    assert(p.source_port() == 20000);
    assert(p.destination_port() == 443);
    assert(p.source_address() == net::ip::make_address("10.0.0.2"));
    assert(p.destination_address() == net::ip::make_address("8.8.8.8"));
    assert(p.total_length() == vec.size());
    assert(p.tcp() != nullptr);
    assert(p.tcp()->header_len() == 20);
    assert((p.tcp()->flags & 0x12) == 0x12); // SYN|ACK
    assert(ntohl(p.tcp()->seq) == 1000);
    assert(p.payload_size() == vec.size() - 20);
    assert(p.transport_data_size() == 3);
    assert(std::memcmp(p.transport_data(), data.data(), 3) == 0);
    assert(p.ipv4() != nullptr && p.ipv6() == nullptr);
    assert(!p.fragmented() && p.fragment_offset() == 0);
}

static void test_parse_ipv4_udp()
{
    const std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto vec = make_udp(0x0a000002, 0x08080808, 30000, 53, data);
    ip_packet p;
    p.parse(vec.data(), vec.size());
    assert(p.valid());
    assert(p.is_udp());
    assert(p.source_port() == 30000 && p.destination_port() == 53);
    assert(p.udp() != nullptr);
    assert(ntohs(p.udp()->length) == 8 + 5);
    assert(p.transport_data_size() == 5);
    assert(std::memcmp(p.transport_data(), data.data(), 5) == 0);
}

static void test_parse_ipv4_icmp()
{
    const std::vector<uint8_t> data = {0xde, 0xad, 0xbe, 0xef};
    auto vec = make_icmp_echo(0x0a000002, 0x0a000001, 0x1234, 7, data);
    ip_packet p;
    p.parse(vec.data(), vec.size());
    assert(p.valid());
    assert(p.is_icmp() && p.ip_protocol() == 1);
    assert(p.icmp_type() == 8);
    assert(p.icmp_code() == 0);
    assert(p.icmp_checksum() != 0);
    assert(p.icmp_echo_id() == 0x1234);
    assert(p.icmp_echo_seq() == 7);
    assert(p.transport_data_size() == 4);
    assert(std::memcmp(p.transport_data(), data.data(), 4) == 0);
}

static void test_parse_ipv6_tcp()
{
    const std::vector<uint8_t> data = {1, 2, 3};
    auto vec =
        make_tcp6(v6("fd00::2"), v6("fd00::1"), 20000, 8080, 0x02, 1, 0,
            65535, data);
    ip_packet p;
    p.parse(vec.data(), vec.size());
    assert(p.valid());
    assert(p.version() == 6);
    assert(p.is_tcp());
    assert(p.source_port() == 20000 && p.destination_port() == 8080);
    assert(p.source_address() == net::ip::make_address("fd00::2"));
    assert(p.destination_address() == net::ip::make_address("fd00::1"));
    assert(p.total_length() == vec.size());
    assert(p.tcp() != nullptr);
    assert(p.transport_data_size() == 3);
    assert(p.ipv6() != nullptr && p.ipv4() == nullptr);
}

static void test_parse_ipv6_udp()
{
    const std::vector<uint8_t> data = {6, 5, 4, 3, 2, 1};
    auto vec = make_udp6(v6("fd00::2"), v6("fd00::1"), 40000, 5353, data);
    ip_packet p;
    p.parse(vec.data(), vec.size());
    assert(p.valid());
    assert(p.is_udp());
    assert(p.source_port() == 40000 && p.destination_port() == 5353);
    assert(p.transport_data_size() == 6);
}

static void test_parse_ipv6_icmp6_echo()
{
    const std::vector<uint8_t> data = {0xaa, 0xbb};
    auto vec = make_icmp6_echo(v6("fd00::2"), v6("fd00::1"), 0x7777, 3, data);
    ip_packet p;
    p.parse(vec.data(), vec.size());
    assert(p.valid());
    assert(p.is_icmpv6() && p.ip_protocol() == 58);
    assert(p.icmp_type() == 128);
    assert(p.icmp_echo_id() == 0x7777);
    assert(p.icmp_echo_seq() == 3);
    assert(p.transport_data_size() == 2);
}

static void test_parse_zero_copy()
{
    auto vec = make_tcp(0x0a000002, 0x08080808, 20000, 443, 0x02, 1, 0,
        65535, {1, 2, 3});
    ip_packet p;
    p.parse(vec.data(), vec.size());
    assert(p.valid());
    // 传输层报文段零拷贝指向内部缓冲（IP 头之后）
    assert(p.payload() == p.buffer().data() + 20);
    assert(p.payload_size() == vec.size() - 20);
    // 应用数据在 TCP 头之后
    assert(p.transport_data() == p.buffer().data() + 20 + 20);
    assert(p.transport_data_size() == 3);
}

static void test_parse_malformed()
{
    // 版本非法
    {
        std::vector<uint8_t> v(40, 0);
        v[0] = 0x30; // version 3
        ip_packet p;
        p.parse(v.data(), v.size());
        assert(!p.valid());
        assert(p.error() == ip_packet::parse_error::invalid_version);
    }
    // 过短
    {
        std::vector<uint8_t> v(10, 0x45);
        ip_packet p;
        p.parse(v.data(), v.size());
        assert(!p.valid());
        assert(p.error() == ip_packet::parse_error::packet_too_short);
    }
    // IHL < 20
    {
        std::vector<uint8_t> v(40, 0);
        v[0] = 0x41; // version 4, ihl 1 -> header_len 4
        ip_packet p;
        p.parse(v.data(), v.size());
        assert(!p.valid());
        assert(p.error() == ip_packet::parse_error::invalid_ip_header_length);
    }
    // total_len 超过实际长度
    {
        auto vec = make_udp(0x0a000002, 0x08080808, 30000, 53, {1, 2, 3});
        vec[2] = 0xff;
        vec[3] = 0xff;
        ip_packet p;
        p.parse(vec.data(), vec.size());
        assert(!p.valid());
        assert(p.error() == ip_packet::parse_error::invalid_total_length);
    }
    // total_len < IHL
    {
        auto vec = make_udp(0x0a000002, 0x08080808, 30000, 53, {1, 2, 3});
        vec[2] = 0;
        vec[3] = 10; // total_len=10 < 20
        ip_packet p;
        p.parse(vec.data(), vec.size());
        assert(!p.valid());
        assert(p.error() == ip_packet::parse_error::invalid_total_length);
    }
    // TCP 段过短（载荷 < 20 字节）
    {
        std::vector<uint8_t> seg(4, 0);
        auto vec = make_ipv4(0x0a000002, 0x08080808, 6, seg);
        ip_packet p;
        p.parse(vec.data(), vec.size());
        assert(!p.valid());
        assert(p.error() == ip_packet::parse_error::invalid_transport_header);
    }
    // UDP length 字段 < 8
    {
        auto vec = make_udp(0x0a000002, 0x08080808, 30000, 53, {1, 2, 3});
        vec[24] = 0;
        vec[25] = 4; // UDP length=4 < 8（偏移 24 为 UDP 头内 length 字段）
        ip_packet p;
        p.parse(vec.data(), vec.size());
        assert(!p.valid());
        assert(p.error() == ip_packet::parse_error::invalid_transport_header);
    }
    // IPv6 payload_len 超过实际长度
    {
        auto vec =
            make_udp6(v6("fd00::2"), v6("fd00::1"), 30000, 53, {1, 2, 3});
        vec[4] = 0xff;
        vec[5] = 0xff;
        ip_packet p;
        p.parse(vec.data(), vec.size());
        assert(!p.valid());
        assert(p.error() == ip_packet::parse_error::invalid_total_length);
    }
}

static void test_parse_fragments()
{
    // 非首片（偏移 > 0）：解析成功、暴露分片字段、不解析传输层
    {
        auto vec =
            make_udp(0x0a000002, 0x08080808, 30000, 53, {1, 2, 3, 4, 5, 6, 7, 8});
        vec[6] = 0x00;
        vec[7] = 0x01; // frag_off: 偏移字段值 1（= 8 字节）
        ip_packet p;
        p.parse(vec.data(), vec.size());
        assert(p.valid());
        assert(p.fragmented());
        assert(p.fragment_offset() == 8);
        assert(p.is_udp());         // 协议号仍可见
        assert(p.udp() == nullptr); // 非首片不解析传输层
        assert(p.payload() != nullptr);
        assert(p.payload_size() == vec.size() - 20);
    }
    // 首片（偏移 0，MF 置位）：含完整传输层头
    {
        auto vec =
            make_udp(0x0a000002, 0x08080808, 30000, 53, {1, 2, 3, 4, 5, 6, 7, 8});
        vec[6] = 0x20;
        vec[7] = 0x00; // MF
        ip_packet p;
        p.parse(vec.data(), vec.size());
        assert(p.valid());
        assert(p.fragmented());
        assert(p.fragment_offset() == 0);
        assert(p.udp() != nullptr);
    }
}

static void test_parse_v6_extension_header()
{
    // next_header = 44（Fragment 扩展头）：不遍历扩展头链
    std::vector<uint8_t> ext(8, 0);
    auto vec = make_ipv6(v6("fd00::2"), v6("fd00::1"), 44, ext);
    ip_packet p;
    p.parse(vec.data(), vec.size());
    assert(p.valid());
    assert(p.version() == 6);
    assert(p.ip_protocol() == 44); // 原始扩展头号
    assert(!p.is_tcp() && !p.is_udp() && !p.is_icmpv6());
    assert(p.tcp() == nullptr && p.udp() == nullptr);
    assert(p.payload() != nullptr);
    assert(p.payload_size() == 8);
}

static void test_parse_buffer_too_small()
{
    ip_packet p(64, 16); // 可用 48 字节
    std::vector<uint8_t> payload(28, 7);
    auto vec = make_udp(0x0a000002, 0x08080808, 30000, 53, payload);
    // vec.size() = 20 + 8 + 28 = 56 > 48
    p.parse(vec.data(), vec.size());
    assert(!p.valid());
    assert(p.error() == ip_packet::parse_error::buffer_too_small);
}

// ---- 字段构造（builder）测试 ----

static void test_build_ipv4_tcp()
{
    ip_packet p;
    p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
        net::ip::make_address_v4("8.8.8.8"));
    const uint8_t mss[4] = {2, 4, 0x05, 0xb4};
    p.begin_tcp(12345, 443, 1000, 0, 0x02 /* SYN */, 65535, mss, 4);
    const std::vector<uint8_t> data = {'h', 'i'};
    p.append_payload(data.data(), data.size());
    p.finalize();
    assert(p.valid());
    assert(p.is_tcp());
    assert(p.source_port() == 12345 && p.destination_port() == 443);
    assert(p.tcp() != nullptr);
    assert(p.tcp()->header_len() == 24);
    assert(p.transport_data_size() == 2);
    assert(std::memcmp(p.transport_data(), "hi", 2) == 0);

    // 独立校验和验证 + 二次解析
    std::vector<uint8_t> raw(
        p.buffer().data(), p.buffer().data() + p.buffer().size());
    assert(verify_packet(raw));
    ip_packet p2;
    p2.parse(raw.data(), raw.size());
    assert(p2.valid() && p2.is_tcp());
    assert(p2.source_port() == 12345 && p2.destination_port() == 443);
    assert(p2.source_address() == net::ip::make_address("10.0.0.1"));
    assert(p2.destination_address() == net::ip::make_address("8.8.8.8"));
    assert(ntohl(p2.tcp()->seq) == 1000);
}

static void test_build_ipv4_udp()
{
    ip_packet p;
    p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
        net::ip::make_address_v4("8.8.8.8"));
    p.begin_udp(12345, 53);
    const char payload[] = "hello udp";
    p.append_payload(payload, sizeof(payload) - 1);
    p.finalize();
    assert(p.valid() && p.is_udp());
    assert(p.source_port() == 12345 && p.destination_port() == 53);
    assert(p.transport_data_size() == sizeof(payload) - 1);
    std::vector<uint8_t> raw(
        p.buffer().data(), p.buffer().data() + p.buffer().size());
    assert(verify_packet(raw));
}

static void test_build_ipv4_icmp_echo()
{
    ip_packet p;
    p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
        net::ip::make_address_v4("10.0.0.2"));
    p.begin_icmp(8, 0);
    p.set_icmp_echo(0xbeef, 42);
    const char payload[] = "ping";
    p.append_payload(payload, 4);
    p.finalize();
    assert(p.valid() && p.is_icmp());
    assert(p.icmp_type() == 8);
    assert(p.icmp_echo_id() == 0xbeef);
    assert(p.icmp_echo_seq() == 42);
    std::vector<uint8_t> raw(
        p.buffer().data(), p.buffer().data() + p.buffer().size());
    assert(verify_packet(raw));
}

static void test_build_ipv6_tcp()
{
    ip_packet p;
    p.begin_ipv6(net::ip::make_address_v6("fd00::1"),
        net::ip::make_address_v6("fd00::2"));
    p.begin_tcp(20000, 8080, 5, 6, 0x10 /* ACK */, 4096);
    const std::vector<uint8_t> data = {9, 8, 7};
    p.append_payload(data.data(), data.size());
    p.finalize();
    assert(p.valid() && p.version() == 6 && p.is_tcp());
    assert(p.source_port() == 20000 && p.destination_port() == 8080);
    assert(p.source_address() == net::ip::make_address("fd00::1"));
    assert(p.transport_data_size() == 3);
    std::vector<uint8_t> raw(
        p.buffer().data(), p.buffer().data() + p.buffer().size());
    assert(verify_packet6(raw));
}

static void test_build_ipv6_udp()
{
    ip_packet p;
    p.begin_ipv6(net::ip::make_address_v6("fd00::1"),
        net::ip::make_address_v6("fd00::2"));
    p.begin_udp(30000, 5353);
    const char payload[] = "v6 udp";
    p.append_payload(payload, sizeof(payload) - 1);
    p.finalize();
    assert(p.valid() && p.is_udp());
    assert(p.transport_data_size() == sizeof(payload) - 1);
    std::vector<uint8_t> raw(
        p.buffer().data(), p.buffer().data() + p.buffer().size());
    assert(verify_packet6(raw));
}

static void test_build_ipv6_icmp6_echo()
{
    ip_packet p;
    p.begin_ipv6(net::ip::make_address_v6("fd00::1"),
        net::ip::make_address_v6("fd00::2"));
    p.begin_icmp(128, 0);
    p.set_icmp_echo(7, 8);
    p.finalize();
    assert(p.valid() && p.is_icmpv6());
    assert(p.icmp_type() == 128);
    assert(p.icmp_echo_id() == 7);
    assert(p.icmp_echo_seq() == 8);
    std::vector<uint8_t> raw(
        p.buffer().data(), p.buffer().data() + p.buffer().size());
    assert(verify_packet6(raw));
}

static void test_build_ip_id()
{
    ip_packet p;
    p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
        net::ip::make_address_v4("10.0.0.2"));
    p.begin_udp(1, 2);
    p.set_ip_id(0x1234);
    p.finalize();
    assert(p.ipv4() != nullptr);
    assert(ntohs(p.ipv4()->id) == 0x1234);
}

static void test_builder_precondition()
{
    ip_packet p;
    bool threw = false;
    try {
        p.begin_tcp(1, 2, 0, 0, 0, 0);
    } catch (const std::logic_error &) {
        threw = true;
    }
    assert(threw);
}

static void test_builder_double_begin()
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
    assert(threw);
    threw = false;
    try {
        p.begin_udp(5, 6);
    } catch (const std::logic_error &) {
        threw = true;
    }
    assert(threw);
}

static void test_builder_capacity_guard()
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
        assert(threw);
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
        assert(threw);
    }
    // 正常容量下不抛
    {
        ip_packet p;
        p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
            net::ip::make_address_v4("10.0.0.2"));
        p.begin_udp(1, 2);
        assert(p.valid());
    }
}

static void test_parse_clears_builder_state()
{
    // 构造一个包
    ip_packet p;
    p.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
        net::ip::make_address_v4("10.0.0.2"));
    p.begin_udp(12345, 53);
    const char hello[] = "hello";
    p.append_payload(hello, 5);
    p.finalize();
    assert(p.valid() && p.is_udp());

    // 解析另一报文后，builder 状态已被清除，finalize 应为 no-op
    auto vec = make_udp(0x0a000002, 0x08080808, 30000, 53, {1, 2, 3});
    p.parse(vec.data(), vec.size());
    assert(p.valid() && p.is_udp());
    assert(p.source_port() == 30000);
    const size_t parsed_size = p.buffer().size();
    const std::vector<uint8_t> parsed(
        p.buffer().data(), p.buffer().data() + parsed_size);

    p.finalize(); // bld_.version == 0 -> 直接返回，不改动缓冲
    assert(p.buffer().size() == parsed_size);
    assert(std::memcmp(p.buffer().data(), parsed.data(), parsed_size) == 0);
    assert(p.valid() && p.is_udp() && p.source_port() == 30000);
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
            throw std::runtime_error("socketpair failed");
        }
        peer = sv[1];
        boost::system::error_code ec;
        if (!dev.assign(sv[0], 1500, false, ec)) {
            throw std::runtime_error(
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

static void test_device_read_ip()
{
    dev_env env;
    ip_packet pkt;
    auto fut = env.dev.async_read_ip(pkt, net::use_future);
    auto vec = make_tcp(0x0a000002, 0x08080808, 20000, 443, 0x12, 100, 0,
        65535, {1, 2, 3, 4});
    (void)::write(env.peer, vec.data(), vec.size());
    // use_future 对 void(ec, size_t) 签名：成功返回 size_t，设备错误抛 system_error
    const size_t n = future_get(std::move(fut));
    assert(n == vec.size());
    assert(pkt.valid());
    assert(pkt.version() == 4 && pkt.is_tcp());
    assert(pkt.source_port() == 20000 && pkt.destination_port() == 443);
    assert(pkt.source_address() == net::ip::make_address("10.0.0.2"));
    assert(pkt.destination_address() == net::ip::make_address("8.8.8.8"));
    assert(pkt.transport_data_size() == 4);
    assert(std::memcmp(pkt.transport_data(), vec.data() + 40, 4) == 0);
}

static void test_device_write_ip()
{
    dev_env env;
    ip_packet pkt;
    pkt.begin_ipv4(net::ip::make_address_v4("10.0.0.1"),
        net::ip::make_address_v4("10.0.0.2"));
    pkt.begin_udp(12345, 53);
    const char hello[] = "hello";
    pkt.append_payload(hello, 5);
    pkt.finalize();
    assert(pkt.valid());
    assert(pkt.is_udp() && pkt.source_port() == 12345);

    auto fut = env.dev.async_write_ip(pkt, net::use_future);
    const size_t n = future_get(std::move(fut));
    assert(n == pkt.buffer().size());

    std::vector<uint8_t> out;
    assert(read_fd_packet(env.peer, out));
    assert(verify_packet(out)); // 独立校验和验证
    ip_packet p2;
    p2.parse(out.data(), out.size());
    assert(p2.valid() && p2.is_udp());
    assert(p2.source_port() == 12345 && p2.destination_port() == 53);
    assert(p2.source_address() == net::ip::make_address("10.0.0.1"));
    assert(p2.payload_size() == 8 + 5); // 传输层报文段（UDP 头 + 数据）
    assert(p2.transport_data_size() == 5);
    assert(std::memcmp(p2.transport_data(), hello, 5) == 0);
}

static void test_device_concurrent_reads()
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
        assert(pkts[i].valid() && pkts[i].is_udp());
        assert(pkts[i].source_port() == 10000 + i);
    }
}

static void test_device_read_invalid()
{
    dev_env env;
    ip_packet pkt;
    auto fut = env.dev.async_read_ip(pkt, net::use_future);
    std::vector<uint8_t> garbage(64, 0xab);
    garbage[0] = 0x35; // version 3
    (void)::write(env.peer, garbage.data(), garbage.size());
    // 设备读成功（非 I/O 错误），use_future 直接返回字节数
    const size_t n = future_get(std::move(fut));
    assert(n == garbage.size());
    assert(!pkt.valid());
    assert(pkt.error() == ip_packet::parse_error::invalid_version);
}

static void test_device_read_capacity_guard()
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
    assert(threw);
    assert(ec == net::error::message_size);
}

} // namespace

int main()
{
    test_parse_ipv4_tcp();
    test_parse_ipv4_udp();
    test_parse_ipv4_icmp();
    test_parse_ipv6_tcp();
    test_parse_ipv6_udp();
    test_parse_ipv6_icmp6_echo();
    test_parse_zero_copy();
    test_parse_malformed();
    test_parse_fragments();
    test_parse_v6_extension_header();
    test_parse_buffer_too_small();

    test_build_ipv4_tcp();
    test_build_ipv4_udp();
    test_build_ipv4_icmp_echo();
    test_build_ipv6_tcp();
    test_build_ipv6_udp();
    test_build_ipv6_icmp6_echo();
    test_build_ip_id();
    test_builder_precondition();
    test_builder_double_begin();
    test_builder_capacity_guard();
    test_parse_clears_builder_state();

    test_device_read_ip();
    test_device_write_ip();
    test_device_concurrent_reads();
    test_device_read_invalid();
    test_device_read_capacity_guard();

    std::cout << "test_ip_packet: all tests passed" << std::endl;
    return 0;
}
