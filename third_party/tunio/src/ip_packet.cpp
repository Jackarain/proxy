//
// ip_packet.cpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tunio/ip_packet.hpp"

#include <cstring>
#include <stdexcept>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

namespace tunio {
namespace {

// 校验和求和（RFC 1071 反码和；内部工具，不对外公开）
#if defined(__SSE2__)
// SSE2 向量化：每 32 字节展开为 8 路 32 位累加器（16 位字按网络序解释），
// 避免标量实现的逐字进位链；无 SIMD 平台回退到下方标量版本。
uint32_t checksum_sum(const uint8_t* data, size_t len, uint32_t sum = 0)
{
    __m128i acc0 = _mm_setzero_si128();
    __m128i acc1 = _mm_setzero_si128();
    const __m128i mask = _mm_set1_epi16(0x00ff);
    size_t i = 0;

    for (; i + 32 <= len; i += 32)
    {
        const __m128i v0 =
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i));
        const __m128i v1 =
            _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + i + 16));
        const __m128i w0 = _mm_or_si128(
            _mm_slli_epi16(_mm_and_si128(v0, mask), 8), _mm_srli_epi16(v0, 8));
        const __m128i w1 = _mm_or_si128(
            _mm_slli_epi16(_mm_and_si128(v1, mask), 8), _mm_srli_epi16(v1, 8));
        acc0 = _mm_add_epi32(acc0, _mm_unpacklo_epi16(w0, _mm_setzero_si128()));
        acc1 = _mm_add_epi32(acc1, _mm_unpackhi_epi16(w0, _mm_setzero_si128()));
        acc0 = _mm_add_epi32(acc0, _mm_unpacklo_epi16(w1, _mm_setzero_si128()));
        acc1 = _mm_add_epi32(acc1, _mm_unpackhi_epi16(w1, _mm_setzero_si128()));
    }

    uint32_t t[8];
    _mm_storeu_si128(reinterpret_cast<__m128i*>(t), acc0);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(t + 4), acc1);
    sum += t[0] + t[1] + t[2] + t[3] + t[4] + t[5] + t[6] + t[7];

    for (; i + 1 < len; i += 2)
    {
        sum += static_cast<uint16_t>((data[i] << 8) | data[i + 1]);
    }
    if (i < len)
        sum += static_cast<uint16_t>(data[i] << 8);

    return sum;
}

#else

uint32_t checksum_sum(const uint8_t* data, size_t len, uint32_t sum = 0)
{
    size_t i = 0;

    for (; i + 1 < len; i += 2)
    {
        sum += static_cast<uint16_t>((data[i] << 8) | data[i + 1]);
    }
    if (i < len)
        sum += static_cast<uint16_t>(data[i] << 8);

    return sum;
}

#endif

uint16_t checksum_fold(uint32_t sum) noexcept
{
    while (sum >> 16)
    {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}

} // namespace

// ---- 校验和工具 ----

uint16_t ip_checksum(const uint8_t* data, size_t len) noexcept
{
    return checksum_fold(checksum_sum(data, len));
}

uint16_t ipv4_checksum(const uint8_t* ip, size_t hlen) noexcept
{
    uint32_t sum = 0;
    size_t i = 0;

    for (; i + 1 < hlen; i += 2)
    {
        uint16_t word = static_cast<uint16_t>((ip[i] << 8) | ip[i + 1]);
        if (i == 10)
            word = 0;

        sum += word;
    }

    return checksum_fold(sum);
}

uint16_t verify_ipv4_checksum(const uint8_t* ip, size_t hlen) noexcept
{
    return checksum_fold(checksum_sum(ip, hlen));
}

uint16_t tcp_udp_checksum(int family,
    const uint8_t* src_ip,
    const uint8_t* dst_ip,
    uint8_t protocol,
    const uint8_t* segment,
    size_t seg_len) noexcept
{
    uint32_t sum = 0;
    const size_t addr_len = family == 6 ? 16 : 4;

    for (size_t i = 0; i + 1 < addr_len; i += 2)
    {
        sum += static_cast<uint16_t>((src_ip[i] << 8) | src_ip[i + 1]);
    }

    for (size_t i = 0; i + 1 < addr_len; i += 2)
    {
        sum += static_cast<uint16_t>((dst_ip[i] << 8) | dst_ip[i + 1]);
    }

    sum += static_cast<uint16_t>(protocol);
    sum += static_cast<uint16_t>(seg_len);
    sum += checksum_sum(segment, seg_len);

    return checksum_fold(sum);
}

uint16_t tcp_udp_checksum(uint32_t src_ip,
    uint32_t dst_ip,
    uint8_t protocol,
    const uint8_t* segment,
    size_t seg_len) noexcept
{
    // 地址按网络字节序存储：直接按内存字节拆成 16 位字，避免主机字节序干扰
    const uint8_t* s = reinterpret_cast<const uint8_t*>(&src_ip);
    const uint8_t* d = reinterpret_cast<const uint8_t*>(&dst_ip);
    return tcp_udp_checksum(4, s, d, protocol, segment, seg_len);
}

size_t ip_header_size(int family) noexcept
{
    return family == 6 ? 40 : 20;
}

size_t build_ip_header(uint8_t* buf,
    int family,
    const uint8_t* src_ip,
    const uint8_t* dst_ip,
    uint8_t protocol,
    size_t total_len,
    uint16_t ip_id) noexcept
{
    if (family == 4)
    {
        // 注：total_len 以 uint16_t 写入 IPv4 头，TUN 场景 MTU 不会触发
        // 截断（IPv4 报文最大 65535 字节）
        auto* ip = reinterpret_cast<ipv4_header*>(buf);
        ip->version_ihl = 0x45;
        ip->tos = 0;
        ip->total_len = htons(static_cast<uint16_t>(total_len));
        ip->id = htons(ip_id);
        ip->frag_off = htons(0x4000); // DF
        ip->ttl = 64;
        ip->protocol = protocol;
        ip->checksum = 0;
        std::memcpy(&ip->src_ip, src_ip, 4);
        std::memcpy(&ip->dst_ip, dst_ip, 4);

        ip->checksum = htons(ipv4_checksum(buf, 20));
        return 20;
    }

    auto* ip = reinterpret_cast<ipv6_header*>(buf);
    ip->vtc_flow = htonl(0x60000000u);

    // total_len < 40 时避免无符号下溢（写出错误 payload_len）
    ip->payload_len =
        htons(static_cast<uint16_t>(total_len > 40 ? total_len - 40 : 0));
    ip->next_header = protocol;
    ip->hop_limit = 64;
    std::memcpy(ip->src_ip, src_ip, 16);
    std::memcpy(ip->dst_ip, dst_ip, 16);

    return 40;
}

// ---- ip_packet ----

const char* ip_packet::error_message(parse_error e) noexcept
{
    switch (e)
    {
    case parse_error::none:
        return "no error";
    case parse_error::invalid_version:
        return "invalid IP version";
    case parse_error::packet_too_short:
        return "packet too short";
    case parse_error::buffer_too_small:
        return "buffer too small";
    case parse_error::invalid_ip_header_length:
        return "invalid IP header length";
    case parse_error::invalid_total_length:
        return "invalid total length";
    case parse_error::invalid_transport_header:
        return "invalid transport header";
    }
    return "unknown error";
}

void ip_packet::clear_state() noexcept
{
    parse_error_ = parse_error::none;
    version_ = 0;
    protocol_ = 0;
    std::memset(src_ip_, 0, sizeof(src_ip_));
    std::memset(dst_ip_, 0, sizeof(dst_ip_));
    total_len_ = 0;
    fragmented_ = false;
    frag_offset_ = 0;
    std::memset(&v4_, 0, sizeof(v4_));
    std::memset(&v6_, 0, sizeof(v6_));
    std::memset(&tcp_, 0, sizeof(tcp_));
    std::memset(&udp_, 0, sizeof(udp_));
    icmp_type_ = 0;
    icmp_code_ = 0;
    icmp_checksum_ = 0;
    icmp_echo_id_ = 0;
    icmp_echo_seq_ = 0;
    payload_ = nullptr;
    payload_len_ = 0;
    transport_parsed_ = false;
}

void ip_packet::reset() noexcept
{
    buf_.reset();
    clear_state();
    bld_ = builder_state{};
}

void ip_packet::copy_and_parse(const uint8_t* data, size_t len)
{
    buf_.reset();
    if (len > buf_.writable_size())
    {
        parse_error_ = parse_error::buffer_too_small;
        return;
    }

    // 用 memmove：data 可能指向自身缓冲（如设备读入后 pkt.parse(buf_, n)）
    std::memmove(buf_.data(), data, len);
    buf_.commit(len);

    // 解析与构造状态互斥：解析后残留的 builder 状态不再可用于 finalize
    bld_ = builder_state{};
    do_parse(buf_.data(), len);
}

void ip_packet::parse(const packet_buffer& src, size_t len)
{
    // 源缓冲越界防御：len 不得超过 src 的可写容量。以容量为界而非
    // src.size()：设备读入未 commit 的路径下 size() 可能为 0，而字节
    // 已位于 data() 处。
    if (len > src.capacity() - src.headroom())
    {
        parse_error_ = parse_error::buffer_too_small;
        return;
    }
    copy_and_parse(src.data(), len);
}

void ip_packet::parse(const uint8_t* data, size_t len)
{
    copy_and_parse(data, len);
}

net::ip::address ip_packet::source_address() const noexcept
{
    if (version_ == 4)
        return net::ip::address_v4(std::array<uint8_t, 4>{
            src_ip_[0], src_ip_[1], src_ip_[2], src_ip_[3]});
    if (version_ == 6)
    {
        std::array<uint8_t, 16> b{};
        std::memcpy(b.data(), src_ip_, 16);
        return net::ip::address_v6(b);
    }
    return net::ip::address();
}

net::ip::address ip_packet::destination_address() const noexcept
{
    if (version_ == 4)
        return net::ip::address_v4(std::array<uint8_t, 4>{
            dst_ip_[0], dst_ip_[1], dst_ip_[2], dst_ip_[3]});
    if (version_ == 6)
    {
        std::array<uint8_t, 16> b{};
        std::memcpy(b.data(), dst_ip_, 16);
        return net::ip::address_v6(b);
    }
    return net::ip::address();
}

uint16_t ip_packet::source_port() const noexcept
{
    if (is_tcp() && transport_parsed_)
        return ntohs(tcp_.src_port);
    if (is_udp() && transport_parsed_)
        return ntohs(udp_.src_port);
    return 0;
}

uint16_t ip_packet::destination_port() const noexcept
{
    if (is_tcp() && transport_parsed_)
        return ntohs(tcp_.dst_port);
    if (is_udp() && transport_parsed_)
        return ntohs(udp_.dst_port);
    return 0;
}

const uint8_t* ip_packet::transport_data() const noexcept
{
    if (payload_ == nullptr)
        return nullptr;
    if (is_tcp() && transport_parsed_)
        return payload_ + tcp_.header_len();
    if (is_udp() && transport_parsed_)
        return payload_ + sizeof(udp_header);
    if ((is_icmp() || is_icmpv6()) && transport_parsed_)
        return payload_ + 8;
    return payload_;
}

size_t ip_packet::transport_data_size() const noexcept
{
    if (payload_ == nullptr)
        return 0;
    if (is_tcp() && transport_parsed_)
        return payload_len_ > tcp_.header_len()
            ? payload_len_ - tcp_.header_len()
            : 0;
    if (is_udp() && transport_parsed_)
        return payload_len_ > sizeof(udp_header)
            ? payload_len_ - sizeof(udp_header)
            : 0;
    if ((is_icmp() || is_icmpv6()) && transport_parsed_)
        return payload_len_ > 8 ? payload_len_ - 8 : 0;
    return payload_len_;
}

void ip_packet::builder_begin(uint8_t version)
{
    buf_.reset();
    bld_ = builder_state{};
    bld_.version = version;
    bld_.ip_hlen = version == 4 ? sizeof(ipv4_header) : sizeof(ipv6_header);

    if (buf_.writable_size() < bld_.ip_hlen)
        throw std::length_error("ip_packet: buffer too small for IP header");

    // 预留 IP 头部空间（finalize 时回填）
    std::memset(buf_.data(), 0, bld_.ip_hlen);
    buf_.commit(bld_.ip_hlen);
}

void ip_packet::reserve_transport()
{
    if (buf_.writable_size() < bld_.transport_hlen)
        throw std::length_error(
            "ip_packet: buffer too small for transport header");
    // 预留传输层头部空间（finalize 时回填）
    std::memset(buf_.writable_data(), 0, bld_.transport_hlen);
    buf_.commit(bld_.transport_hlen);
}

void ip_packet::begin_ipv4(
    const net::ip::address_v4& src, const net::ip::address_v4& dst, uint8_t ttl)
{
    builder_begin(4);

    const auto s = src.to_bytes();
    const auto d = dst.to_bytes();
    std::memcpy(bld_.src, s.data(), 4);
    std::memcpy(bld_.dst, d.data(), 4);
    bld_.ttl_hop = ttl;
}

void ip_packet::begin_ipv6(const net::ip::address_v6& src,
    const net::ip::address_v6& dst,
    uint8_t hop_limit)
{
    builder_begin(6);

    const auto s = src.to_bytes();
    const auto d = dst.to_bytes();
    std::memcpy(bld_.src, s.data(), 16);
    std::memcpy(bld_.dst, d.data(), 16);
    bld_.ttl_hop = hop_limit;
}

void ip_packet::begin_tcp(uint16_t src_port,
    uint16_t dst_port,
    uint32_t seq,
    uint32_t ack,
    uint8_t flags,
    uint16_t window,
    const void* options,
    size_t options_len)
{
    if (bld_.version == 0)
        throw std::logic_error(
            "ip_packet::begin_tcp: begin_ipv4/begin_ipv6 must be called first");
    if (bld_.protocol != 0)
        throw std::logic_error(
            "ip_packet::begin_tcp: transport header already begun");
    if (options_len % 4 != 0 || options_len > 40)
        throw std::invalid_argument("ip_packet::begin_tcp: options_len must be "
                                    "a multiple of 4 and <= 40");

    bld_.protocol = ip_protocol_tcp;
    bld_.sport = src_port;
    bld_.dport = dst_port;
    bld_.seq = seq;
    bld_.ack = ack;
    bld_.flags = flags;
    bld_.window = window;
    bld_.transport_hlen = sizeof(tcp_header) + options_len;

    if (options_len != 0)
        std::memcpy(bld_.tcp_options, options, options_len);
    bld_.tcp_options_len = options_len;
    reserve_transport();
}

void ip_packet::begin_udp(uint16_t src_port, uint16_t dst_port)
{
    if (bld_.version == 0)
        throw std::logic_error(
            "ip_packet::begin_udp: begin_ipv4/begin_ipv6 must be called first");
    if (bld_.protocol != 0)
        throw std::logic_error(
            "ip_packet::begin_udp: transport header already begun");

    bld_.protocol = ip_protocol_udp;
    bld_.sport = src_port;
    bld_.dport = dst_port;
    bld_.transport_hlen = sizeof(udp_header);
    reserve_transport();
}

void ip_packet::begin_icmp(uint8_t type, uint8_t code)
{
    if (bld_.version == 0)
        throw std::logic_error("ip_packet::begin_icmp: begin_ipv4/begin_ipv6 "
                               "must be called first");
    if (bld_.protocol != 0)
        throw std::logic_error(
            "ip_packet::begin_icmp: transport header already begun");

    bld_.protocol = bld_.version == 4 ? ip_protocol_icmp : ip_protocol_icmpv6;
    bld_.icmp_type = type;
    bld_.icmp_code = code;
    bld_.icmp_echo_set = false;
    bld_.transport_hlen = 8;
    reserve_transport();
}

void ip_packet::append_payload(const void* data, size_t len)
{
    if (bld_.version == 0)
        throw std::logic_error("ip_packet::append_payload: "
                               "begin_ipv4/begin_ipv6 must be called first");
    if (buf_.writable_size() < len)
        throw std::length_error("ip_packet: payload exceeds buffer capacity");

    if (len != 0)
    {
        std::memcpy(buf_.writable_data(), data, len);
        buf_.commit(len);
    }
}

void ip_packet::finalize()
{
    if (bld_.version == 0)
        return;

    const size_t payload_len = buf_.size() - bld_.ip_hlen - bld_.transport_hlen;
    const size_t total = bld_.ip_hlen + bld_.transport_hlen + payload_len;
    uint8_t* out = buf_.data();
    const int family = bld_.version == 4 ? 4 : 6;

    if (family == 4)
    {
        build_ip_header(
            out, 4, bld_.src, bld_.dst, bld_.protocol, total, bld_.ip_id);
        out[8] = bld_.ttl_hop;
    }
    else
    {
        build_ip_header(out, 6, bld_.src, bld_.dst, bld_.protocol, total, 0);
        out[7] = bld_.ttl_hop;
    }

    uint8_t* seg = out + bld_.ip_hlen;
    const size_t seg_len = bld_.transport_hlen + payload_len;

    switch (bld_.protocol)
    {
    case ip_protocol_tcp:
    {
        tcp_header th{};
        th.src_port = htons(bld_.sport);
        th.dst_port = htons(bld_.dport);
        th.seq = htonl(bld_.seq);
        th.ack = htonl(bld_.ack);
        th.data_offset = static_cast<uint8_t>((bld_.transport_hlen / 4) << 4);
        th.flags = bld_.flags;
        th.window = htons(bld_.window);
        std::memcpy(seg, &th, sizeof(th));

        if (bld_.tcp_options_len != 0)
            std::memcpy(
                seg + sizeof(th), bld_.tcp_options, bld_.tcp_options_len);

        const uint16_t csum = tcp_udp_checksum(
            family, bld_.src, bld_.dst, ip_protocol_tcp, seg, seg_len);
        seg[16] = static_cast<uint8_t>(csum >> 8);
        seg[17] = static_cast<uint8_t>(csum & 0xff);
        break;
    }
    case ip_protocol_udp:
    {
        udp_header uh{};
        uh.src_port = htons(bld_.sport);
        uh.dst_port = htons(bld_.dport);
        uh.length = htons(static_cast<uint16_t>(seg_len));
        std::memcpy(seg, &uh, sizeof(uh));

        uint16_t csum = tcp_udp_checksum(
            family, bld_.src, bld_.dst, ip_protocol_udp, seg, seg_len);
        // 校验和为 0 时以 0xffff 替代（RFC 768/8200，与 udp_build_datagram
        // 一致）：IPv4 下 0 保留为"未计算"标志，IPv6 下 UDP 校验和强制
        // 存在，两者都不应输出 0.
        if (csum == 0)
            csum = 0xffff;
        seg[6] = static_cast<uint8_t>(csum >> 8);
        seg[7] = static_cast<uint8_t>(csum & 0xff);
        break;
    }
    case ip_protocol_icmp:
    case ip_protocol_icmpv6:
    {
        seg[0] = bld_.icmp_type;
        seg[1] = bld_.icmp_code;
        seg[2] = 0;
        seg[3] = 0;

        if (bld_.icmp_echo_set)
        {
            seg[4] = static_cast<uint8_t>(bld_.icmp_id >> 8);
            seg[5] = static_cast<uint8_t>(bld_.icmp_id & 0xff);
            seg[6] = static_cast<uint8_t>(bld_.icmp_seq >> 8);
            seg[7] = static_cast<uint8_t>(bld_.icmp_seq & 0xff);
        }

        uint16_t csum;
        if (family == 4)
            csum = ip_checksum(seg, seg_len);
        else
            csum = tcp_udp_checksum(
                6, bld_.src, bld_.dst, ip_protocol_icmpv6, seg, seg_len);

        seg[2] = static_cast<uint8_t>(csum >> 8);
        seg[3] = static_cast<uint8_t>(csum & 0xff);
        break;
    }
    default:
        break;
    }

    // 同步解析视图，使 finalize() 后访问器立即可用
    parse(buf_, buf_.size());
}

void ip_packet::parse_transport(
    uint8_t protocol, const uint8_t* seg, size_t seg_len)
{
    transport_parsed_ = false;
    switch (protocol)
    {
    case ip_protocol_tcp:
        if (seg_len < sizeof(tcp_header))
        {
            parse_error_ = parse_error::invalid_transport_header;
            return;
        }

        std::memcpy(&tcp_, seg, sizeof(tcp_header));
        {
            const size_t hlen = tcp_.header_len();
            if (hlen < sizeof(tcp_header) || hlen > seg_len)
            {
                parse_error_ = parse_error::invalid_transport_header;
                return;
            }
        }

        transport_parsed_ = true;
        break;
    case ip_protocol_udp:
        if (seg_len < sizeof(udp_header))
        {
            parse_error_ = parse_error::invalid_transport_header;
            return;
        }

        std::memcpy(&udp_, seg, sizeof(udp_header));
        {
            const size_t ulen = ntohs(udp_.length);
            if (ulen < sizeof(udp_header) || ulen > seg_len)
            {
                parse_error_ = parse_error::invalid_transport_header;
                return;
            }
        }

        transport_parsed_ = true;
        break;
    case ip_protocol_icmp:
    case ip_protocol_icmpv6:
        if (seg_len < 8)
        {
            parse_error_ = parse_error::invalid_transport_header;
            return;
        }

        icmp_type_ = seg[0];
        icmp_code_ = seg[1];
        icmp_checksum_ = static_cast<uint16_t>((seg[2] << 8) | seg[3]);
        {
            const bool echo = (protocol == ip_protocol_icmp &&
                                  (icmp_type_ == 8 || icmp_type_ == 0)) ||
                (protocol == ip_protocol_icmpv6 &&
                    (icmp_type_ == 128 || icmp_type_ == 129));
            if (echo)
            {
                icmp_echo_id_ = static_cast<uint16_t>((seg[4] << 8) | seg[5]);
                icmp_echo_seq_ = static_cast<uint16_t>((seg[6] << 8) | seg[7]);
            }
        }

        transport_parsed_ = true;
        break;
    default:
        // 未知协议：无类型化视图，载荷仍可访问
        break;
    }
}

void ip_packet::do_parse(const uint8_t* data, size_t len)
{
    clear_state();

    if (len < 20)
    {
        parse_error_ = parse_error::packet_too_short;
        return;
    }
    const uint8_t version = static_cast<uint8_t>(data[0] >> 4);
    if (version == 4)
    {
        std::memcpy(&v4_, data, sizeof(ipv4_header));
        const size_t ihl = v4_.header_len();
        if (ihl < sizeof(ipv4_header) || ihl > len)
        {
            parse_error_ = parse_error::invalid_ip_header_length;
            return;
        }
        const size_t total = ntohs(v4_.total_len);
        if (total < ihl || total > len)
        {
            parse_error_ = parse_error::invalid_total_length;
            return;
        }
        version_ = 4;
        protocol_ = v4_.protocol;
        std::memcpy(src_ip_, data + 12, 4);
        std::memcpy(dst_ip_, data + 16, 4);
        total_len_ = total;
        const uint16_t frag = ntohs(v4_.frag_off);
        fragmented_ = (frag & 0x3fff) != 0;
        frag_offset_ = static_cast<uint16_t>((frag & 0x1fff) * 8);
        payload_ = data + ihl;
        payload_len_ = total - ihl;
        // 分片非首片（偏移 > 0）的载荷从流中间开始，不解析传输层
        if (frag_offset_ == 0)
            parse_transport(protocol_, payload_, payload_len_);
    }
    else if (version == 6)
    {
        if (len < sizeof(ipv6_header))
        {
            parse_error_ = parse_error::packet_too_short;
            return;
        }
        std::memcpy(&v6_, data, sizeof(ipv6_header));
        const size_t plen = ntohs(v6_.payload_len);
        if (sizeof(ipv6_header) + plen > len)
        {
            parse_error_ = parse_error::invalid_total_length;
            return;
        }
        version_ = 6;
        protocol_ = v6_.next_header;
        std::memcpy(src_ip_, v6_.src_ip, 16);
        std::memcpy(dst_ip_, v6_.dst_ip, 16);
        total_len_ = sizeof(ipv6_header) + plen;
        payload_ = data + sizeof(ipv6_header);
        payload_len_ = plen;
        parse_transport(protocol_, payload_, payload_len_);
    }
    else
        parse_error_ = parse_error::invalid_version;
}

} // namespace tunio
