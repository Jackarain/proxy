//
// ip_headers.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

namespace tunio {
namespace detail {

constexpr uint8_t IPPROTO_ICMP_V = 1;
constexpr uint8_t IPPROTO_ICMPV6_V = 58;
constexpr uint8_t IPPROTO_TCP_V = 6;
constexpr uint8_t IPPROTO_UDP_V = 17;

// ---- 紧凑报文头部（均为网络字节序字段）----
#pragma pack(push, 1)
struct ipv4_header
{
    uint8_t version_ihl;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;

    uint8_t version() const
    {
        return static_cast<uint8_t>(version_ihl >> 4);
    }
    uint8_t ihl() const
    {
        return static_cast<uint8_t>(version_ihl & 0x0f);
    }
    size_t header_len() const
    {
        return static_cast<size_t>(ihl()) * 4;
    }
};

struct ipv6_header
{
    uint32_t vtc_flow;    // version(4) | traffic class(8) | flow label(20)
    uint16_t payload_len; // 不含 IPv6 头部的载荷长度
    uint8_t next_header;  // 上层协议号
    uint8_t hop_limit;
    uint8_t src_ip[16]; // 网络字节序
    uint8_t dst_ip[16]; // 网络字节序
};

struct tcp_header
{
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t data_offset;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;

    size_t header_len() const
    {
        return static_cast<size_t>(data_offset >> 4) * 4;
    }
};

struct udp_header
{
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
};
#pragma pack(pop)

// TCP 标志位
enum tcp_flag : uint8_t {
    TCP_FIN = 0x01,
    TCP_SYN = 0x02,
    TCP_RST = 0x04,
    TCP_PSH = 0x08,
    TCP_ACK = 0x10,
    TCP_URG = 0x20,
};

// ---- 校验和工具（RFC 1071 反码和）----
#if defined(__SSE2__)
// SSE2 向量化：每 32 字节展开为 8 路 32 位累加器（16 位字按网络序解释），
// 避免标量实现的逐字进位链；无 SIMD 平台回退到下方标量版本。
inline uint32_t checksum_sum(const uint8_t *data, size_t len, uint32_t sum = 0)
{
    __m128i acc0 = _mm_setzero_si128();
    __m128i acc1 = _mm_setzero_si128();
    const __m128i mask = _mm_set1_epi16(0x00ff);
    size_t i = 0;
    for (; i + 32 <= len; i += 32) {
        const __m128i v0 =
            _mm_loadu_si128(reinterpret_cast<const __m128i *>(data + i));
        const __m128i v1 =
            _mm_loadu_si128(reinterpret_cast<const __m128i *>(data + i + 16));
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
    _mm_storeu_si128(reinterpret_cast<__m128i *>(t), acc0);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(t + 4), acc1);
    sum += t[0] + t[1] + t[2] + t[3] + t[4] + t[5] + t[6] + t[7];
    for (; i + 1 < len; i += 2) {
        sum += static_cast<uint16_t>((data[i] << 8) | data[i + 1]);
    }
    if (i < len) {
        sum += static_cast<uint16_t>(data[i] << 8);
    }
    return sum;
}

#else

inline uint32_t checksum_sum(const uint8_t *data, size_t len, uint32_t sum = 0)
{
    size_t i = 0;
    for (; i + 1 < len; i += 2) {
        sum += static_cast<uint16_t>((data[i] << 8) | data[i + 1]);
    }
    if (i < len) {
        sum += static_cast<uint16_t>(data[i] << 8);
    }
    return sum;
}

#endif

inline uint16_t checksum_fold(uint32_t sum)
{
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}

// 对 data[0, len) 计算反码和校验
inline uint16_t ip_checksum(const uint8_t *data, size_t len)
{
    return checksum_fold(checksum_sum(data, len));
}

// IPv4 头部校验和（自动跳过 checksum 字段本身）
inline uint16_t ipv4_checksum(const uint8_t *ip, size_t hlen)
{
    uint32_t sum = 0;
    size_t i = 0;
    for (; i + 1 < hlen; i += 2) {
        uint16_t word = static_cast<uint16_t>((ip[i] << 8) | ip[i + 1]);
        if (i == 10) {
            word = 0;
        }
        sum += word;
    }
    return checksum_fold(sum);
}

// IPv4 头部校验和验证（包含 checksum 字段求和，合法头部结果为 0）
inline uint16_t verify_ipv4_checksum(const uint8_t *ip, size_t hlen)
{
    return checksum_fold(checksum_sum(ip, hlen));
}

// TCP/UDP/ICMPv6 伪头部校验和（family: 4 或 6，地址均为网络字节序）
inline uint16_t tcp_udp_checksum(int family, const uint8_t *src_ip,
                                 const uint8_t *dst_ip, uint8_t protocol,
                                 const uint8_t *segment, size_t seg_len)
{
    uint32_t sum = 0;
    const size_t addr_len = family == 6 ? 16 : 4;
    for (size_t i = 0; i + 1 < addr_len; i += 2) {
        sum += static_cast<uint16_t>((src_ip[i] << 8) | src_ip[i + 1]);
    }
    for (size_t i = 0; i + 1 < addr_len; i += 2) {
        sum += static_cast<uint16_t>((dst_ip[i] << 8) | dst_ip[i + 1]);
    }
    sum += static_cast<uint16_t>(protocol);
    sum += static_cast<uint16_t>(seg_len);
    sum += checksum_sum(segment, seg_len);
    return checksum_fold(sum);
}

// TCP/UDP 伪头部校验和（IPv4 便捷重载，参数为网络字节序地址）
inline uint16_t tcp_udp_checksum(uint32_t src_ip, uint32_t dst_ip,
                                 uint8_t protocol, const uint8_t *segment,
                                 size_t seg_len)
{
    // 地址按网络字节序存储：直接按内存字节拆成 16 位字，避免主机字节序干扰
    const uint8_t *s = reinterpret_cast<const uint8_t *>(&src_ip);
    const uint8_t *d = reinterpret_cast<const uint8_t *>(&dst_ip);
    return tcp_udp_checksum(4, s, d, protocol, segment, seg_len);
}

// IP 头部长度（IPv4 20 字节 / IPv6 40 字节）
inline size_t ip_header_size(int family) noexcept
{
    return family == 6 ? 40 : 20;
}

// 构建 IP 头部（IPv4 自动计算头部校验和，IPv6 无校验和字段）；
// buf 需至少容纳 ip_header_size(family) 字节，返回头部长度。
inline size_t build_ip_header(uint8_t *buf, int family, const uint8_t *src_ip,
                              const uint8_t *dst_ip, uint8_t protocol,
                              size_t total_len, uint16_t ip_id)
{
    if (family == 4) {
        auto *ip = reinterpret_cast<ipv4_header *>(buf);
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
    auto *ip = reinterpret_cast<ipv6_header *>(buf);
    ip->vtc_flow = htonl(0x60000000u);
    ip->payload_len = htons(static_cast<uint16_t>(total_len - 40));
    ip->next_header = protocol;
    ip->hop_limit = 64;
    std::memcpy(ip->src_ip, src_ip, 16);
    std::memcpy(ip->dst_ip, dst_ip, 16);
    return 40;
}

// 引擎内统一 IP 层信息：地址族 + 网络字节序地址字节（IPv4 仅前 4 字节有效）
struct ip_packet_info
{
    uint8_t family = 0;
    uint8_t protocol = 0;
    uint8_t src_ip[16] = {};
    uint8_t dst_ip[16] = {};
};

// 序列号比较（RFC 1982 环形比较）
inline bool seq_gt(uint32_t a, uint32_t b)
{
    return static_cast<int32_t>(a - b) > 0;
}

inline bool seq_ge(uint32_t a, uint32_t b)
{
    return a == b || seq_gt(a, b);
}

} // namespace detail
} // namespace tunio
