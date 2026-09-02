//
// ip_packet.hpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tunio/packet_buffer.hpp"

#include <boost/asio.hpp>

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

namespace tunio {
namespace net = boost::asio;

// 上层协议号（避免与系统宏 IPPROTO_* 冲突，取独立命名）
inline constexpr uint8_t ip_protocol_icmp = 1;
inline constexpr uint8_t ip_protocol_tcp = 6;
inline constexpr uint8_t ip_protocol_udp = 17;
inline constexpr uint8_t ip_protocol_icmpv6 = 58;

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

    uint8_t version() const noexcept
    {
        return static_cast<uint8_t>(version_ihl >> 4);
    }
    uint8_t ihl() const noexcept
    {
        return static_cast<uint8_t>(version_ihl & 0x0f);
    }
    size_t header_len() const noexcept
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

    size_t header_len() const noexcept
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

// ---- 校验和工具（RFC 1071 反码和；实现见 src/ip_packet.cpp）----

// 对 data[0, len) 计算反码和校验
uint16_t ip_checksum(const uint8_t* data, size_t len) noexcept;

// IPv4 头部校验和（自动跳过 checksum 字段本身）
uint16_t ipv4_checksum(const uint8_t* ip, size_t hlen) noexcept;

// IPv4 头部校验和验证（包含 checksum 字段求和，合法头部结果为 0）
uint16_t verify_ipv4_checksum(const uint8_t* ip, size_t hlen) noexcept;

// TCP/UDP/ICMPv6 伪头部校验和（family: 4 或 6，地址均为网络字节序）
uint16_t tcp_udp_checksum(int family,
    const uint8_t* src_ip,
    const uint8_t* dst_ip,
    uint8_t protocol,
    const uint8_t* segment,
    size_t seg_len) noexcept;

// TCP/UDP 伪头部校验和（IPv4 便捷重载，参数为网络字节序地址）
uint16_t tcp_udp_checksum(uint32_t src_ip,
    uint32_t dst_ip,
    uint8_t protocol,
    const uint8_t* segment,
    size_t seg_len) noexcept;

// IP 头部长度（IPv4 20 字节 / IPv6 40 字节）
size_t ip_header_size(int family) noexcept;

// 构建 IP 头部（IPv4 自动计算头部校验和，IPv6 无校验和字段）；
// buf 需至少容纳 ip_header_size(family) 字节，返回头部长度。
size_t build_ip_header(uint8_t* buf,
    int family,
    const uint8_t* src_ip,
    const uint8_t* dst_ip,
    uint8_t protocol,
    size_t total_len,
    uint16_t ip_id) noexcept;

// 从 TUN 设备读取/向 TUN 设备写入的一个完整 IP 报文。
//
// 读路径（async_read_ip）：设备将报文直接读入内部 packet_buffer 后立即做
// 结构解析，暴露 IP 层与传输层（TCP/UDP/ICMP/ICMPv6）的类型化视图，载荷
// 以零拷贝指针形式访问。解析只做结构校验（不验证校验和），分片包解析并
// 暴露字段（是否丢弃由调用方决定）；IPv6 扩展头不遍历，next_header 为扩展
// 头号时传输层视图为空、ip_protocol() 返回原始号。解析失败不抛出异常，
// 通过 valid()/error() 查询。
//
// 写路径（async_write_ip）：支持原始写出（buffer() 中已有字节）与字段构造
// （begin_* -> append_payload -> finalize()），finalize() 自动回填长度并计算
// IP/TCP/UDP/ICMP 校验和（含伪头部）。
//
// 非平凡实现位于 src/ip_packet.cpp；本头仅保留类型、声明与平凡访问器，
// 以降低所有使用方 TU 的头文件解析开销。
class ip_packet
{
public:
    // 解析失败原因
    enum class parse_error : uint8_t
    {
        none = 0,
        invalid_version,          // 版本既不是 4 也不是 6
        packet_too_short,         // 数据不足最小头部
        buffer_too_small,         // 内部缓冲不足以容纳报文
        invalid_ip_header_length, // IPv4 IHL < 20 或超过报文长度
        invalid_total_length,     // total_len 小于头部长度或超过实际数据
        invalid_transport_header  // TCP/UDP/ICMP 头部结构非法
    };

    explicit ip_packet(size_t capacity = 2048, size_t headroom = 128)
        : buf_(capacity, headroom)
    {
    }

    // ip_packet 自持 packet_buffer（内含 unique_ptr 存储），仅可移动
    ip_packet(const ip_packet&) = delete;
    ip_packet& operator=(const ip_packet&) = delete;
    ip_packet(ip_packet&&) = default;
    ip_packet& operator=(ip_packet&&) = default;

    // 复位缓冲与解析状态，可复用对象进行下一次读取/构造
    void reset() noexcept;

    // ---- 解析（读路径）----

    // 解析外部缓冲区中的报文（拷贝进内部缓冲）；src 允许为自身 buffer()
    void parse(const packet_buffer& src, size_t len);

    // 解析裸字节报文（拷贝进内部缓冲）
    void parse(const uint8_t* data, size_t len);

    bool valid() const noexcept
    {
        return parse_error_ == parse_error::none;
    }
    parse_error error() const noexcept
    {
        return parse_error_;
    }
    static const char* error_message(parse_error e) noexcept;

    // ---- IP 层 ----

    // 4 / 6 / 0（解析失败时为 0）
    uint8_t version() const noexcept
    {
        return version_;
    }
    // 原始协议号（如 ip_protocol_tcp / ip_protocol_udp / ip_protocol_icmp /
    // ip_protocol_icmpv6，未知协议或 IPv6 扩展头号原样返回）
    uint8_t ip_protocol() const noexcept
    {
        return protocol_;
    }
    // 整个 IP 报文长度（IPv6 为 40 + payload_len）
    size_t total_length() const noexcept
    {
        return total_len_;
    }
    // 是否为分片报文（带分片偏移或 MF 标志）
    bool fragmented() const noexcept
    {
        return fragmented_;
    }
    // 分片偏移（字节，IPv4 首片/未分片为 0）
    uint16_t fragment_offset() const noexcept
    {
        return frag_offset_;
    }
    net::ip::address source_address() const noexcept;
    net::ip::address destination_address() const noexcept;
    // 网络字节序地址字节（IPv4 仅前 4 字节有效），零拷贝热路径
    const uint8_t* source_ip_bytes() const noexcept
    {
        return src_ip_;
    }
    const uint8_t* destination_ip_bytes() const noexcept
    {
        return dst_ip_;
    }
    // 原始头部视图（版本不匹配时为 nullptr）
    const ipv4_header* ipv4() const noexcept
    {
        return version_ == 4 ? &v4_ : nullptr;
    }
    const ipv6_header* ipv6() const noexcept
    {
        return version_ == 6 ? &v6_ : nullptr;
    }

    // ---- 传输层 ----

    bool is_tcp() const noexcept
    {
        return protocol_ == ip_protocol_tcp;
    }
    bool is_udp() const noexcept
    {
        return protocol_ == ip_protocol_udp;
    }
    bool is_icmp() const noexcept
    {
        return protocol_ == ip_protocol_icmp;
    }
    bool is_icmpv6() const noexcept
    {
        return protocol_ == ip_protocol_icmpv6;
    }
    // 主机字节序端口（非 TCP/UDP 或传输层未解析时为 0）
    uint16_t source_port() const noexcept;
    uint16_t destination_port() const noexcept;
    // 类型化传输层头部（协议不匹配、解析失败或分片非首片时为 nullptr）
    const tcp_header* tcp() const noexcept
    {
        return is_tcp() && transport_parsed_ ? &tcp_ : nullptr;
    }
    const udp_header* udp() const noexcept
    {
        return is_udp() && transport_parsed_ ? &udp_ : nullptr;
    }
    uint8_t icmp_type() const noexcept
    {
        return icmp_type_;
    }
    uint8_t icmp_code() const noexcept
    {
        return icmp_code_;
    }
    uint16_t icmp_checksum() const noexcept
    {
        return icmp_checksum_;
    }
    // Echo 类报文（v4 type 8/0，v6 type 128/129）的 id/seq，其余为 0
    uint16_t icmp_echo_id() const noexcept
    {
        return icmp_echo_id_;
    }
    uint16_t icmp_echo_seq() const noexcept
    {
        return icmp_echo_seq_;
    }

    // ---- 载荷 ----

    // 传输层报文段（IP 头之后的全部字节，含 TCP/UDP/ICMP 头部），指向内部
    // 缓冲，零拷贝；分片非首片为分片内容。仅在 valid() 时可靠。
    const uint8_t* payload() const noexcept
    {
        return payload_;
    }
    size_t payload_size() const noexcept
    {
        return payload_len_;
    }
    // 纯应用数据（传输层头部之后）；传输层未解析（未知协议或分片非首片）
    // 时与 payload() 相同（此时无传输层视图可裁剪）。
    const uint8_t* transport_data() const noexcept;
    size_t transport_data_size() const noexcept;

    // ---- 写路径（字段构造）----

    // 以下 begin_* 系列必须按 begin_ipv4/begin_ipv6 -> begin_tcp/udp/icmp ->
    // [append_payload] -> finalize() 的顺序使用；每个报文构造前自动复位缓冲，
    // 传输层 begin_* 仅允许调用一次（重复调用抛 std::logic_error）。
    void begin_ipv4(const net::ip::address_v4& src,
        const net::ip::address_v4& dst,
        uint8_t ttl = 64);
    void begin_ipv6(const net::ip::address_v6& src,
        const net::ip::address_v6& dst,
        uint8_t hop_limit = 64);

    // options/options_len 为可选的 TCP 选项区（MSS 等），长度须为 4 的倍数且
    // 不超过 40 字节；不传表示无选项（data offset 20）。
    void begin_tcp(uint16_t src_port,
        uint16_t dst_port,
        uint32_t seq,
        uint32_t ack,
        uint8_t flags,
        uint16_t window,
        const void* options = nullptr,
        size_t options_len = 0);
    void begin_udp(uint16_t src_port, uint16_t dst_port);

    // type/code 为 ICMP 头部字段；Echo 类报文（v4 8/0，v6 128/129）需再调用
    // set_icmp_echo() 设置 id/seq。
    void begin_icmp(uint8_t type, uint8_t code);

    // 仅用于 Echo 类 ICMP/ICMPv6 报文
    void set_icmp_echo(uint16_t id, uint16_t seq) noexcept
    {
        bld_.icmp_id = id;
        bld_.icmp_seq = seq;
        bld_.icmp_echo_set = true;
    }

    // 追加传输层载荷（可多次调用）
    void append_payload(const void* data, size_t len);

    // 回填 IP/传输层长度与全部校验和；完成后即可通过访问器读取各字段
    //（内部会对自身做一次解析以同步视图）。
    void finalize();

    // 设置 IPv4 Identification 字段（仅对 IPv4 生效；须在 begin_ipv4 之后、
    // finalize 之前调用，默认 0）
    void set_ip_id(uint16_t id) noexcept
    {
        bld_.ip_id = id;
    }

    // 底层缓冲：读路径设备直接读入其中；原始写路径 async_write_ip 写出其内容
    packet_buffer& buffer() noexcept
    {
        return buf_;
    }
    const packet_buffer& buffer() const noexcept
    {
        return buf_;
    }

private:
    void clear_state() noexcept;
    void copy_and_parse(const uint8_t* data, size_t len);
    void builder_begin(uint8_t version);
    void reserve_transport();
    void parse_transport(uint8_t protocol, const uint8_t* seg, size_t seg_len);
    void do_parse(const uint8_t* data, size_t len);

    packet_buffer buf_;
    parse_error parse_error_ = parse_error::none;
    uint8_t version_ = 0;
    uint8_t protocol_ = 0;
    uint8_t src_ip_[16] = {};
    uint8_t dst_ip_[16] = {};
    size_t total_len_ = 0;
    bool fragmented_ = false;
    uint16_t frag_offset_ = 0;
    ipv4_header v4_{};
    ipv6_header v6_{};
    tcp_header tcp_{};
    udp_header udp_{};
    uint8_t icmp_type_ = 0;
    uint8_t icmp_code_ = 0;
    uint16_t icmp_checksum_ = 0;
    uint16_t icmp_echo_id_ = 0;
    uint16_t icmp_echo_seq_ = 0;
    const uint8_t* payload_ = nullptr;
    size_t payload_len_ = 0;
    // 传输层头部是否已成功解析（协议号匹配但解析失败/分片非首片时为 false）
    bool transport_parsed_ = false;

    struct builder_state
    {
        uint8_t version = 0;
        uint8_t protocol = 0;
        uint8_t src[16] = {};
        uint8_t dst[16] = {};
        uint8_t ttl_hop = 64;
        uint16_t ip_id = 0;
        uint16_t sport = 0;
        uint16_t dport = 0;
        uint32_t seq = 0;
        uint32_t ack = 0;
        uint8_t flags = 0;
        uint16_t window = 0;
        uint8_t tcp_options[40] = {};
        size_t tcp_options_len = 0;
        uint8_t icmp_type = 0;
        uint8_t icmp_code = 0;
        uint16_t icmp_id = 0;
        uint16_t icmp_seq = 0;
        bool icmp_echo_set = false;
        size_t ip_hlen = 0;
        size_t transport_hlen = 0;
    } bld_{};
};

} // namespace tunio
