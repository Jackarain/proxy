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

// Windows 下 windows.h 会定义 min/max 函数宏，破坏 std::min/std::max;
// 在包含任何 Windows 头之前统一禁用，代码中全部使用 std::min/std::max.
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace tunio {

// 平台原生句柄类型，支持外部句柄注入
#ifdef _WIN32
using native_handle_type = void*;
#else
using native_handle_type = int;
#endif

inline constexpr native_handle_type invalid_native_handle =
#ifdef _WIN32
    nullptr;
#else
    -1;
#endif

// Linux TUN 多队列（IFF_MULTI_QUEUE）的队列数上限，与内核
// drivers/net/tun.c 的 MAX_TAP_QUEUES 一致；超出视为非法配置。
inline constexpr size_t max_multi_queues = 256;

// 从整型构造外部句柄：POSIX 下句柄即文件描述符（int）；Windows 下句柄为
// 指针类型，按位模式转换（intptr_t 保证整型宽度足够），语义由注入方保证。
// 供 --inject-fd 之类的 CLI 注入路径使用，避免平台差异类型无法直接赋值。
inline native_handle_type native_handle_from_int(int handle) noexcept
{
#ifdef _WIN32
    return reinterpret_cast<native_handle_type>(static_cast<intptr_t>(handle));
#else
    return handle;
#endif
}

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
inline five_tuple make_five_tuple(const uint8_t* src_ip, const uint8_t* dst_ip,
    uint16_t src_port, uint16_t dst_port, uint8_t protocol, uint8_t family) noexcept
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

inline bool operator==(const five_tuple& lhs, const five_tuple& rhs) noexcept
{
    return lhs.family == rhs.family && lhs.src_ip == rhs.src_ip &&
        lhs.dst_ip == rhs.dst_ip && lhs.src_port == rhs.src_port &&
        lhs.dst_port == rhs.dst_port && lhs.protocol == rhs.protocol;
}

inline bool operator!=(const five_tuple& lhs, const five_tuple& rhs) noexcept
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
inline udp_session_key make_udp_session_key(
    const uint8_t* src_ip, uint16_t src_port, uint8_t family) noexcept
{
    udp_session_key k{};
    k.family = family;
    k.src_port = src_port;
    const size_t n = family == 6 ? 16 : 4;
    std::memcpy(k.src_ip.data(), src_ip, n);
    return k;
}

inline bool operator==(
    const udp_session_key& lhs, const udp_session_key& rhs) noexcept
{
    return lhs.family == rhs.family && lhs.src_ip == rhs.src_ip &&
        lhs.src_port == rhs.src_port;
}

inline bool operator!=(
    const udp_session_key& lhs, const udp_session_key& rhs) noexcept
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
    // Linux TUN 多队列（IFF_MULTI_QUEUE）队列数：>1 时按多队列模式打开，
    // 每个队列一个独立 fd，读按队列并发、写按五元组哈希分发到队列；
    // 其他平台忽略（恒为单队列）.
    size_t num_queues = 1;
    // macOS utun 设备读写携带 4 字节家族前缀（大端 AF_INET/AF_INET6）；
    // 自主打开 utun 时自动启用。socketpair 等纯 IP 注入保持 false.
    bool utun_prefix = false;
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
    // Linux TUN 多队列（IFF_MULTI_QUEUE）队列数（自主打开时生效）；
    // 其他平台忽略。
    size_t num_queues = 1;

    // ---- 外部句柄注入 ----
    native_handle_type external_handle = invalid_native_handle;
    size_t external_mtu = 1500;
    // 多句柄注入：非空时优先于 external_handle，按元素顺序注入为各队列
    // fd（仅 Linux TUN 多队列有意义，队列数 = 句柄数）；其他平台不支持。
    std::vector<native_handle_type> external_handles;
    // 注入的句柄是否为 macOS utun 设备（读写带 4 字节家族前缀）.
    // 真实 utun fd 注入时置 true；socketpair 注入（测试/模拟）保持 false.
    bool utun_prefix = false;

    // ---- 资源上限 ----
    size_t max_tcp_flows = 65536;
    size_t max_udp_flows = 65536;
    size_t max_rx_queue_per_flow = 8 * 1024 * 1024;
    // 乱序重排缓存：单流最多缓存的乱序段数（超出后丢弃并发 Dup-ACK，
    // 防止恶意对端用海量乱序段耗尽内存）。需覆盖大窗口（1MB/MSS≈757 段）
    // 下并发读产生的整窗乱序，否则乱序段被拒导致重传风暴。
    size_t tcp_ooo_max_segments = 4096;
    size_t max_total_buffer = 512 * 1024 * 1024;

    // ---- 超时策略 ----
    std::chrono::seconds udp_idle_timeout{30};
    std::chrono::seconds tcp_time_wait_timeout{10};
    std::chrono::seconds tcp_accept_timeout{
        30}; // 已建立但未被 async_accept 领取的连接超时
    std::chrono::seconds tcp_syn_timeout{30}; // 未完成握手的半开连接超时
    std::chrono::seconds tcp_close_timeout{
        30}; // 关闭流程（FIN 挥手）未完成时的强制清理超时
    // 零窗口持久计时器初始间隔：对端通告窗口 0 时周期性发送窗口探测，
    // 探测被确认后按指数退避加倍，上限 60s.
    std::chrono::milliseconds tcp_persist_timeout{5000};
    // 零窗口持久探测最大次数：对端持续通告窗口 0 且不恢复（窗口更新/ACK
    // 缺失）时，探测按上述间隔退避周期性发送，超过该次数判定对端无窗口
    // 恢复能力，以 RST 关闭连接并以 connection_reset 完成挂起写，避免
    // 流表条目与挂起写永久滞留；收到窗口更新或有效 ACK 时复位计数。
    int tcp_persist_max_probes = 15;
    // 数据段重传（RTO）：初始超时与最大重传次数。发送侧对未确认数据按
    // 指数退避（上限 60s）重传，超过最大次数判定发送超时并以 RST 关闭。
    std::chrono::milliseconds tcp_rto_timeout{200};
    int tcp_rto_max_retransmits = 8;
};

// 引擎统计信息
struct engine_stats
{
    std::atomic<uint64_t> rx_packets{0};
    std::atomic<uint64_t> tx_packets{0};
    std::atomic<uint64_t> rx_dropped{0};
    std::atomic<uint64_t> rx_ooo{0}; // 乱序缓存段数
    std::atomic<uint64_t> tcp_connections{0};
    std::atomic<uint64_t> udp_sessions{0};
    std::atomic<uint64_t> icmp_replies{0};
};

} // namespace tunio

namespace std {
template <> struct hash<tunio::five_tuple>
{
    size_t operator()(const tunio::five_tuple& k) const noexcept
    {
        // 64 位字混合哈希：一次 memcpy 加载 8 字节（未用字节恒为
        // 0，不引入碰撞），替代逐字节 FNV 循环，减少每包查找的指令数。
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
        for (uint64_t x : v)
        {
            h ^= x;
            h *= 1099511628211ULL; // FNV prime
        }
        return static_cast<size_t>(h);
    }
};

template <> struct hash<tunio::udp_session_key>
{
    size_t operator()(const tunio::udp_session_key& k) const noexcept
    {
        // 64 位字混合哈希：一次 memcpy 加载 8 字节（未用字节恒为
        // 0，不引入碰撞），替代逐字节 FNV 循环，减少每包查找的指令数。
        uint64_t h = 1469598103934665603ULL; // FNV offset basis
        uint64_t v[3];
        std::memcpy(v, k.src_ip.data(), 8);
        std::memcpy(v + 1, k.src_ip.data() + 8, 8);
        v[2] = static_cast<uint64_t>(k.src_port) |
            (static_cast<uint64_t>(k.family) << 16);
        for (uint64_t x : v)
        {
            h ^= x;
            h *= 1099511628211ULL; // FNV prime
        }
        return static_cast<size_t>(h);
    }
};
} // namespace std
