//
// tun_config.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

namespace tunio {

// 平台原生句柄类型，支持外部句柄注入
#ifdef _WIN32
using native_handle_type = void *;
#else
using native_handle_type = int;
#endif

inline constexpr native_handle_type invalid_native_handle =
#ifdef _WIN32
    nullptr;
#else
    -1;
#endif

// 统一五元组，用于 NAT 查表与 Flow 索引
//
// IP 地址以网络字节序原始字节保存（IPv4 仅前 4 字节有效），family 区分
// 地址族（4 或 6），端口为网络字节序，可直接与报文头部字段比较。
#pragma pack(push, 1)
struct five_tuple
{
    std::array<uint8_t, 16> src_ip{}; // 网络字节序（IPv4 仅前 4 字节有效）
    std::array<uint8_t, 16> dst_ip{};
    uint16_t src_port = 0; // 网络字节序
    uint16_t dst_port = 0; // 网络字节序
    uint8_t protocol = 0;  // IPPROTO_TCP (6) 或 IPPROTO_UDP (17)
    uint8_t family = 0;    // 4 或 6
};
#pragma pack(pop)

// 构造五元组：IP 为网络字节序字节，IPv4 仅拷贝前 4 字节
inline five_tuple make_five_tuple(const uint8_t *src_ip, const uint8_t *dst_ip,
                                  uint16_t src_port, uint16_t dst_port,
                                  uint8_t protocol, uint8_t family) noexcept
{
    five_tuple k{};
    k.family = family;
    k.protocol = protocol;
    k.src_port = src_port;
    k.dst_port = dst_port;
    const size_t n = family == 6 ? 16 : 4;
    std::memcpy(k.src_ip.data(), src_ip, n);
    std::memcpy(k.dst_ip.data(), dst_ip, n);
    return k;
}

inline bool operator==(const five_tuple &lhs, const five_tuple &rhs) noexcept
{
    return lhs.family == rhs.family && lhs.src_ip == rhs.src_ip &&
           lhs.dst_ip == rhs.dst_ip && lhs.src_port == rhs.src_port &&
           lhs.dst_port == rhs.dst_port && lhs.protocol == rhs.protocol;
}

inline bool operator!=(const five_tuple &lhs, const five_tuple &rhs) noexcept
{
    return !(lhs == rhs);
}

// UDP 会话键：以客户端三元组标识（网络字节序），一个会话对应一个客户端
// 套接字，可向任意远端收发（1 对 N），远端端点随每个数据报单独携带。
struct udp_session_key
{
    std::array<uint8_t, 16> src_ip{}; // 网络字节序（IPv4 仅前 4 字节有效）
    uint16_t src_port = 0;            // 网络字节序
    uint8_t family = 0;               // 4 或 6
};

// 构造 UDP 会话键：IP 为网络字节序字节，IPv4 仅拷贝前 4 字节
inline udp_session_key make_udp_session_key(const uint8_t *src_ip,
                                            uint16_t src_port,
                                            uint8_t family) noexcept
{
    udp_session_key k{};
    k.family = family;
    k.src_port = src_port;
    const size_t n = family == 6 ? 16 : 4;
    std::memcpy(k.src_ip.data(), src_ip, n);
    return k;
}

inline bool operator==(const udp_session_key &lhs,
                       const udp_session_key &rhs) noexcept
{
    return lhs.family == rhs.family && lhs.src_ip == rhs.src_ip &&
           lhs.src_port == rhs.src_port;
}

inline bool operator!=(const udp_session_key &lhs,
                       const udp_session_key &rhs) noexcept
{
    return !(lhs == rhs);
}

// 设备配置：自主打开 TUN 设备时使用
struct device_config
{
    std::string name;
    std::string ipv4;
    std::string netmask;
    std::string ipv6;             // 可选 IPv6 地址，如 "fd00::1"
    uint8_t ipv6_prefix_len = 64; // IPv6 前缀长度
    size_t mtu = 1500;
};

// 引擎总配置
struct tun_config
{
    // ---- 网络配置 ----
    std::string dev_name;
    std::string ipv4_addr;
    std::string netmask;
    std::string ipv6_addr;        // 可选 IPv6 地址，如 "fd00::1"
    uint8_t ipv6_prefix_len = 64; // IPv6 前缀长度
    size_t mtu = 1500;

    // ---- 外部句柄注入 ----
    native_handle_type external_handle = invalid_native_handle;
    size_t external_mtu = 1500;

    // ---- 资源上限 ----
    size_t max_tcp_flows = 65536;
    size_t max_udp_flows = 65536;
    size_t max_rx_queue_per_flow = 1024 * 1024;
    size_t max_tx_queue_per_flow =
        1024 * 1024; // 每条 TCP 连接排队待发送的字节数上限
    size_t max_total_buffer = 512 * 1024 * 1024;

    // ---- 超时策略 ----
    std::chrono::seconds udp_idle_timeout{30};
    std::chrono::seconds tcp_time_wait_timeout{10};
    std::chrono::seconds tcp_accept_timeout{
        30}; // 已建立但未被 async_accept 领取的连接超时
    std::chrono::seconds tcp_syn_timeout{30}; // 未完成握手的半开连接超时
    std::chrono::seconds tcp_close_timeout{
        30}; // 关闭流程（FIN 挥手）未完成时的强制清理超时

    // ---- 可选 Checksum 控制 ----
    // 用户态 TUN 收发的报文必须由本引擎计算校验和，该开关保留用于兼容设计文档；
    // 引擎始终计算并校验 IP/TCP/UDP 校验和。当前实现不使用该字段（兼容占位）。
    [[maybe_unused]] bool enable_checksum_offload = true;
};

// 引擎统计信息
struct engine_stats
{
    std::atomic<uint64_t> rx_packets{0};
    std::atomic<uint64_t> tx_packets{0};
    std::atomic<uint64_t> rx_dropped{0};
    std::atomic<uint64_t> tcp_connections{0};
    std::atomic<uint64_t> udp_sessions{0};
    std::atomic<uint64_t> icmp_replies{0};
};

} // namespace tunio

namespace std {
template <> struct hash<tunio::five_tuple>
{
    size_t operator()(const tunio::five_tuple &k) const noexcept
    {
        // 64 位字混合哈希：一次 memcpy 加载 8 字节（未用字节恒为
        // 0，不引入碰撞）， 替代逐字节 FNV 循环，减少每包查找的指令数。
        uint64_t h = 1469598103934665603ULL; // FNV offset basis
        uint64_t v[6];
        std::memcpy(v, k.src_ip.data(), 8);
        std::memcpy(v + 1, k.src_ip.data() + 8, 8);
        std::memcpy(v + 2, k.dst_ip.data(), 8);
        std::memcpy(v + 3, k.dst_ip.data() + 8, 8);
        v[4] = static_cast<uint64_t>(k.src_port) |
               (static_cast<uint64_t>(k.dst_port) << 16);
        v[5] = static_cast<uint64_t>(k.protocol) |
               (static_cast<uint64_t>(k.family) << 8);
        for (uint64_t x : v) {
            h ^= x;
            h *= 1099511628211ULL; // FNV prime
        }
        return static_cast<size_t>(h);
    }
};

template <> struct hash<tunio::udp_session_key>
{
    size_t operator()(const tunio::udp_session_key &k) const noexcept
    {
        // 64 位字混合哈希：一次 memcpy 加载 8 字节（未用字节恒为
        // 0，不引入碰撞）， 替代逐字节 FNV 循环，减少每包查找的指令数。
        uint64_t h = 1469598103934665603ULL; // FNV offset basis
        uint64_t v[3];
        std::memcpy(v, k.src_ip.data(), 8);
        std::memcpy(v + 1, k.src_ip.data() + 8, 8);
        v[2] = static_cast<uint64_t>(k.src_port) |
               (static_cast<uint64_t>(k.family) << 16);
        for (uint64_t x : v) {
            h ^= x;
            h *= 1099511628211ULL; // FNV prime
        }
        return static_cast<size_t>(h);
    }
};
} // namespace std
