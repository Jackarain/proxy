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

// 公开的报文头部结构体与校验和工具已提升至 tunio/ip_packet.hpp；
// 本头保留引擎内部专用件（ip_packet_info、序列号比较等），并经由
// using 声明把公开类型引入 detail 命名空间，保证引擎代码零改动。
#include "tunio/ip_packet.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace tunio {
namespace detail {

// 公开类型与工具（位于 tunio 命名空间）在 detail 中的别名
using ::tunio::build_ip_header;
using ::tunio::ip_checksum;
using ::tunio::ip_header_size;
using ::tunio::ipv4_checksum;
using ::tunio::ipv4_header;
using ::tunio::ipv6_header;
using ::tunio::tcp_header;
using ::tunio::tcp_udp_checksum;
using ::tunio::udp_header;
using ::tunio::verify_ipv4_checksum;

// 上层协议号常量（引擎内部命名）
constexpr uint8_t IPPROTO_ICMP_V = ip_protocol_icmp;
constexpr uint8_t IPPROTO_ICMPV6_V = ip_protocol_icmpv6;
constexpr uint8_t IPPROTO_TCP_V = ip_protocol_tcp;
constexpr uint8_t IPPROTO_UDP_V = ip_protocol_udp;

// TCP 标志位
enum tcp_flag : uint8_t
{
    TCP_FIN = 0x01,
    TCP_SYN = 0x02,
    TCP_RST = 0x04,
    TCP_PSH = 0x08,
    TCP_ACK = 0x10,
    TCP_URG = 0x20,
};

// 引擎内统一 IP 层信息：地址族 + 网络字节序地址字节（IPv4 仅前 4 字节有效）
struct ip_packet_info
{
    uint8_t family = 0;
    uint8_t protocol = 0;
    uint8_t src_ip[16] = {};
    uint8_t dst_ip[16] = {};
};

// 序列号比较（RFC 1982 环形比较）
inline bool seq_gt(uint32_t a, uint32_t b) noexcept
{
    return static_cast<int32_t>(a - b) > 0;
}

inline bool seq_ge(uint32_t a, uint32_t b) noexcept
{
    return a == b || seq_gt(a, b);
}

} // namespace detail
} // namespace tunio
