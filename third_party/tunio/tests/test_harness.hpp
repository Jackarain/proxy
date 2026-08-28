//
// test_harness.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tunio/packet_buffer.hpp"
#include "tunio/tun_tcp_acceptor.hpp"
#include "tunio/tun_config.hpp"
#include "tunio/tun_tcp_socket.hpp"
#include "tunio/tun_udp_acceptor.hpp"
#include "tunio/tun_udp_socket.hpp"
#include "tunio/tunio.hpp"

#include <boost/asio.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace test {
namespace net = boost::asio;

using engine_type = tunio::tunio;
using tunio::device_config;
using tunio::engine_stats;
using tunio::five_tuple;
using tunio::invalid_native_handle;
using tunio::native_handle_type;
using tunio::packet_buffer;
using tunio::tun_tcp_acceptor;
using tunio::tun_config;
using tunio::tun_tcp_socket;
using tunio::tun_udp_acceptor;
using tunio::tun_udp_socket;

// ---- IP 地址辅助：host order 表示 ----
inline uint32_t ip(const char *s)
{
    return ntohl(::inet_addr(s));
}

// ---- IPv6 地址辅助：字符串 -> 网络字节序字节 ----
inline std::array<uint8_t, 16> v6(const char *s)
{
    std::array<uint8_t, 16> b{};
    if (::inet_pton(AF_INET6, s, b.data()) != 1) {
        throw std::runtime_error(std::string("bad ipv6 address: ") + s);
    }
    return b;
}

// ---- 独立校验和实现（与引擎实现相互验证）----
inline uint32_t raw_sum(const uint8_t *d, size_t n)
{
    uint32_t sum = 0;
    size_t i = 0;
    for (; i + 1 < n; i += 2) {
        sum += static_cast<uint16_t>((d[i] << 8) | d[i + 1]);
    }
    if (i < n) {
        sum += static_cast<uint16_t>(d[i] << 8);
    }
    return sum;
}

inline uint32_t fold_sum(uint32_t sum)
{
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return sum;
}

inline uint16_t csum16(const uint8_t *d, size_t n, uint32_t init = 0)
{
    return static_cast<uint16_t>(~fold_sum(raw_sum(d, n) + init));
}

// 独立校验一个完整 IPv6 报文的 TCP/UDP/ICMPv6 校验和（含伪头部）
inline bool verify_packet6(const std::vector<uint8_t> &pkt)
{
    if (pkt.size() < 40 || (pkt[0] >> 4) != 6) {
        return false;
    }
    const size_t plen = static_cast<size_t>((pkt[4] << 8) | pkt[5]);
    if (40 + plen > pkt.size()) {
        return false;
    }
    const uint8_t proto = pkt[6];
    const uint8_t *seg = pkt.data() + 40;
    uint32_t pseudo = 0;
    for (size_t i = 0; i + 1 < 16; i += 2) {
        pseudo += static_cast<uint16_t>((pkt[8 + i] << 8) | pkt[9 + i]);
        pseudo += static_cast<uint16_t>((pkt[24 + i] << 8) | pkt[25 + i]);
    }
    if (proto == 6) {
        if (plen < 20) {
            return false;
        }
        return csum16(seg, plen, pseudo + 6 + plen) == 0;
    }
    if (proto == 17) {
        if (plen < 8) {
            return false;
        }
        const size_t ulen = static_cast<size_t>((seg[4] << 8) | seg[5]);
        if (ulen < 8 || ulen > plen) {
            return false;
        }
        return csum16(seg, ulen, pseudo + 17 + ulen) == 0;
    }
    if (proto == 58) {
        return csum16(seg, plen, pseudo + 58 + plen) == 0;
    }
    return true;
}

// 独立校验一个完整 IPv4 报文的 IP 头校验和，以及 TCP/UDP/ICMP 校验和。
// 与引擎实现相互独立，用于验证引擎发送方向的报文合法性。
inline bool verify_packet(const std::vector<uint8_t> &pkt)
{
    if (pkt.size() < 20) {
        return false;
    }
    if (csum16(pkt.data(), 20) != 0) {
        return false; // IP 头校验和
    }
    const uint8_t proto = pkt[9];
    const size_t ihl = static_cast<size_t>(pkt[0] & 0x0f) * 4;
    const size_t total = static_cast<size_t>((pkt[2] << 8) | pkt[3]);
    if (ihl < 20 || total < ihl || pkt.size() < total) {
        return false;
    }
    const uint8_t *seg = pkt.data() + ihl;
    const size_t seg_len = total - ihl;
    const uint32_t pseudo_ip =
        ((pkt[12] << 8) | pkt[13]) + ((pkt[14] << 8) | pkt[15]) +
        ((pkt[16] << 8) | pkt[17]) + ((pkt[18] << 8) | pkt[19]);
    if (proto == 6) {
        if (seg_len < 20) {
            return false;
        }
        return csum16(seg, seg_len, pseudo_ip + 6 + seg_len) == 0;
    }
    if (proto == 17) {
        if (seg_len < 8) {
            return false;
        }
        const size_t ulen = static_cast<size_t>((seg[4] << 8) | seg[5]);
        if (ulen < 8 || ulen > seg_len) {
            return false;
        }
        return csum16(seg, ulen, pseudo_ip + 17 + ulen) == 0;
    }
    if (proto == 1) {
        return csum16(seg, seg_len) == 0; // ICMP 无伪头
    }
    return true;
}

// ---- 报文构造（host order 地址入参）----
inline std::vector<uint8_t> make_ipv4(uint32_t src, uint32_t dst, uint8_t proto,
                                      const std::vector<uint8_t> &payload)
{
    std::vector<uint8_t> pkt;
    pkt.reserve(20 + payload.size());
    pkt.resize(20, 0);
    pkt[0] = 0x45;
    const uint16_t total = static_cast<uint16_t>(20 + payload.size());
    pkt[2] = static_cast<uint8_t>((total >> 8) & 0xff);
    pkt[3] = static_cast<uint8_t>(total & 0xff);
    pkt[8] = 64; // ttl
    pkt[9] = proto;
    uint32_t s = htonl(src);
    uint32_t d = htonl(dst);
    std::memcpy(&pkt[12], &s, 4);
    std::memcpy(&pkt[16], &d, 4);
    uint16_t c = csum16(pkt.data(), 20);
    pkt[10] = static_cast<uint8_t>(c >> 8);
    pkt[11] = static_cast<uint8_t>(c & 0xff);
    pkt.insert(pkt.end(), payload.begin(), payload.end());
    return pkt;
}

// ---- IPv6 报文构造（网络字节序地址字节入参）----
inline std::vector<uint8_t> make_ipv6(const std::array<uint8_t, 16> &src,
                                      const std::array<uint8_t, 16> &dst,
                                      uint8_t proto,
                                      const std::vector<uint8_t> &payload)
{
    std::vector<uint8_t> pkt(40, 0);
    pkt[0] = 0x60;
    const uint16_t plen = static_cast<uint16_t>(payload.size());
    pkt[4] = static_cast<uint8_t>(plen >> 8);
    pkt[5] = static_cast<uint8_t>(plen & 0xff);
    pkt[6] = proto;
    pkt[7] = 64; // hop limit
    std::copy(src.begin(), src.end(), pkt.begin() + 8);
    std::copy(dst.begin(), dst.end(), pkt.begin() + 24);
    pkt.insert(pkt.end(), payload.begin(), payload.end());
    return pkt;
}

inline std::vector<uint8_t>
make_tcp6(const std::array<uint8_t, 16> &src,
          const std::array<uint8_t, 16> &dst, uint16_t sport, uint16_t dport,
          uint8_t flags, uint32_t seq, uint32_t ack, uint16_t win,
          const std::vector<uint8_t> &data, bool mss = false)
{
    const size_t hlen = mss ? 24 : 20;
    std::vector<uint8_t> seg(hlen + data.size(), 0);
    seg[0] = static_cast<uint8_t>(sport >> 8);
    seg[1] = static_cast<uint8_t>(sport & 0xff);
    seg[2] = static_cast<uint8_t>(dport >> 8);
    seg[3] = static_cast<uint8_t>(dport & 0xff);
    uint32_t s = htonl(seq), a = htonl(ack);
    std::memcpy(&seg[4], &s, 4);
    std::memcpy(&seg[8], &a, 4);
    seg[12] = static_cast<uint8_t>((mss ? 6 : 5) << 4);
    seg[13] = flags;
    seg[14] = static_cast<uint8_t>(win >> 8);
    seg[15] = static_cast<uint8_t>(win & 0xff);
    if (mss) {
        seg[20] = 2; // MSS 选项
        seg[21] = 4;
        seg[22] = 0x05;
        seg[23] = 0xb4;
    }
    if (!data.empty()) {
        std::memcpy(seg.data() + hlen, data.data(), data.size());
    }
    uint32_t pseudo = 0;
    for (size_t i = 0; i + 1 < 16; i += 2) {
        pseudo += static_cast<uint16_t>((src[i] << 8) | src[i + 1]);
        pseudo += static_cast<uint16_t>((dst[i] << 8) | dst[i + 1]);
    }
    uint16_t c = csum16(seg.data(), seg.size(), pseudo + 6 + seg.size());
    seg[16] = static_cast<uint8_t>(c >> 8);
    seg[17] = static_cast<uint8_t>(c & 0xff);
    return make_ipv6(src, dst, 6, seg);
}

inline std::vector<uint8_t> make_udp6(const std::array<uint8_t, 16> &src,
                                      const std::array<uint8_t, 16> &dst,
                                      uint16_t sport, uint16_t dport,
                                      const std::vector<uint8_t> &data)
{
    std::vector<uint8_t> seg(8 + data.size(), 0);
    seg[0] = static_cast<uint8_t>(sport >> 8);
    seg[1] = static_cast<uint8_t>(sport & 0xff);
    seg[2] = static_cast<uint8_t>(dport >> 8);
    seg[3] = static_cast<uint8_t>(dport & 0xff);
    const uint16_t ulen = static_cast<uint16_t>(seg.size());
    seg[4] = static_cast<uint8_t>(ulen >> 8);
    seg[5] = static_cast<uint8_t>(ulen & 0xff);
    if (!data.empty()) {
        std::memcpy(seg.data() + 8, data.data(), data.size());
    }
    uint32_t pseudo = 0;
    for (size_t i = 0; i + 1 < 16; i += 2) {
        pseudo += static_cast<uint16_t>((src[i] << 8) | src[i + 1]);
        pseudo += static_cast<uint16_t>((dst[i] << 8) | dst[i + 1]);
    }
    uint16_t c = csum16(seg.data(), seg.size(), pseudo + 17 + seg.size());
    seg[6] = static_cast<uint8_t>(c >> 8);
    seg[7] = static_cast<uint8_t>(c & 0xff);
    return make_ipv6(src, dst, 17, seg);
}

inline std::vector<uint8_t> make_icmp6_echo(const std::array<uint8_t, 16> &src,
                                            const std::array<uint8_t, 16> &dst,
                                            uint16_t id, uint16_t seqno,
                                            const std::vector<uint8_t> &data)
{
    std::vector<uint8_t> icmp(8 + data.size(), 0);
    icmp[0] = 128; // Echo Request
    icmp[4] = static_cast<uint8_t>(id >> 8);
    icmp[5] = static_cast<uint8_t>(id & 0xff);
    icmp[6] = static_cast<uint8_t>(seqno >> 8);
    icmp[7] = static_cast<uint8_t>(seqno & 0xff);
    if (!data.empty()) {
        std::memcpy(icmp.data() + 8, data.data(), data.size());
    }
    uint32_t pseudo = 0;
    for (size_t i = 0; i + 1 < 16; i += 2) {
        pseudo += static_cast<uint16_t>((src[i] << 8) | src[i + 1]);
        pseudo += static_cast<uint16_t>((dst[i] << 8) | dst[i + 1]);
    }
    uint16_t c = csum16(icmp.data(), icmp.size(), pseudo + 58 + icmp.size());
    icmp[2] = static_cast<uint8_t>(c >> 8);
    icmp[3] = static_cast<uint8_t>(c & 0xff);
    return make_ipv6(src, dst, 58, icmp);
}

inline std::vector<uint8_t> make_tcp(uint32_t src, uint32_t dst, uint16_t sport,
                                     uint16_t dport, uint8_t flags,
                                     uint32_t seq, uint32_t ack, uint16_t win,
                                     const std::vector<uint8_t> &data,
                                     bool mss = false)
{
    const size_t hlen = mss ? 24 : 20;
    std::vector<uint8_t> seg(hlen + data.size(), 0);
    seg[0] = static_cast<uint8_t>(sport >> 8);
    seg[1] = static_cast<uint8_t>(sport & 0xff);
    seg[2] = static_cast<uint8_t>(dport >> 8);
    seg[3] = static_cast<uint8_t>(dport & 0xff);
    uint32_t s = htonl(seq), a = htonl(ack);
    std::memcpy(&seg[4], &s, 4);
    std::memcpy(&seg[8], &a, 4);
    seg[12] = static_cast<uint8_t>((mss ? 6 : 5) << 4);
    seg[13] = flags;
    seg[14] = static_cast<uint8_t>(win >> 8);
    seg[15] = static_cast<uint8_t>(win & 0xff);
    if (mss) {
        seg[20] = 2; // MSS 选项
        seg[21] = 4;
        seg[22] = 0x05;
        seg[23] = 0xb4;
    }
    if (!data.empty()) {
        std::memcpy(seg.data() + hlen, data.data(), data.size());
    }
    const uint32_t pseudo = (src >> 16) + (src & 0xffff) + (dst >> 16) +
                            (dst & 0xffff) + 6 + seg.size();
    uint16_t c = csum16(seg.data(), seg.size(), pseudo);
    seg[16] = static_cast<uint8_t>(c >> 8);
    seg[17] = static_cast<uint8_t>(c & 0xff);
    return make_ipv4(src, dst, 6, seg);
}

inline std::vector<uint8_t> make_udp(uint32_t src, uint32_t dst, uint16_t sport,
                                     uint16_t dport,
                                     const std::vector<uint8_t> &data)
{
    std::vector<uint8_t> seg(8 + data.size(), 0);
    seg[0] = static_cast<uint8_t>(sport >> 8);
    seg[1] = static_cast<uint8_t>(sport & 0xff);
    seg[2] = static_cast<uint8_t>(dport >> 8);
    seg[3] = static_cast<uint8_t>(dport & 0xff);
    const uint16_t ulen = static_cast<uint16_t>(seg.size());
    seg[4] = static_cast<uint8_t>(ulen >> 8);
    seg[5] = static_cast<uint8_t>(ulen & 0xff);
    if (!data.empty()) {
        std::memcpy(seg.data() + 8, data.data(), data.size());
    }
    const uint32_t pseudo = (src >> 16) + (src & 0xffff) + (dst >> 16) +
                            (dst & 0xffff) + 17 + seg.size();
    uint16_t c = csum16(seg.data(), seg.size(), pseudo);
    seg[6] = static_cast<uint8_t>(c >> 8);
    seg[7] = static_cast<uint8_t>(c & 0xff);
    return make_ipv4(src, dst, 17, seg);
}

inline std::vector<uint8_t> make_icmp_echo(uint32_t src, uint32_t dst,
                                           uint16_t id, uint16_t seqno,
                                           const std::vector<uint8_t> &data)
{
    std::vector<uint8_t> icmp(8 + data.size(), 0);
    icmp[0] = 8; // Echo Request
    icmp[1] = 0;
    icmp[4] = static_cast<uint8_t>(id >> 8);
    icmp[5] = static_cast<uint8_t>(id & 0xff);
    icmp[6] = static_cast<uint8_t>(seqno >> 8);
    icmp[7] = static_cast<uint8_t>(seqno & 0xff);
    if (!data.empty()) {
        std::memcpy(icmp.data() + 8, data.data(), data.size());
    }
    uint16_t c = csum16(icmp.data(), icmp.size());
    icmp[2] = static_cast<uint8_t>(c >> 8);
    icmp[3] = static_cast<uint8_t>(c & 0xff);
    return make_ipv4(src, dst, 1, icmp);
}

// ---- 报文解析 ----
struct ip_hdr_info
{
    uint8_t proto = 0;
    uint8_t ihl = 0;
    uint32_t src = 0, dst = 0; // host order
    uint16_t total_len = 0;
    const uint8_t *payload = nullptr;
    size_t payload_len = 0;
};

struct ip6_hdr_info
{
    uint8_t proto = 0;
    std::array<uint8_t, 16> src{};
    std::array<uint8_t, 16> dst{};
    size_t payload_len = 0;
    const uint8_t *payload = nullptr;
};

inline bool parse_ip6(const std::vector<uint8_t> &pkt, ip6_hdr_info &out)
{
    if (pkt.size() < 40 || (pkt[0] >> 4) != 6) {
        return false;
    }
    const size_t plen = static_cast<size_t>((pkt[4] << 8) | pkt[5]);
    if (40 + plen > pkt.size()) {
        return false;
    }
    out.proto = pkt[6];
    std::copy(pkt.begin() + 8, pkt.begin() + 24, out.src.begin());
    std::copy(pkt.begin() + 24, pkt.begin() + 40, out.dst.begin());
    out.payload = pkt.data() + 40;
    out.payload_len = plen;
    return true;
}

inline bool parse_ip(const std::vector<uint8_t> &pkt, ip_hdr_info &out)
{
    if (pkt.size() < 20 || (pkt[0] >> 4) != 4) {
        return false;
    }
    out.ihl = static_cast<uint8_t>((pkt[0] & 0x0f) * 4);
    out.total_len = static_cast<uint16_t>((pkt[2] << 8) | pkt[3]);
    if (out.total_len < out.ihl || out.total_len > pkt.size()) {
        return false;
    }
    out.proto = pkt[9];
    uint32_t s, d;
    std::memcpy(&s, &pkt[12], 4);
    std::memcpy(&d, &pkt[16], 4);
    out.src = ntohl(s);
    out.dst = ntohl(d);
    out.payload = pkt.data() + out.ihl;
    out.payload_len = out.total_len - out.ihl;
    return true;
}

struct tcp_hdr_info
{
    uint16_t sport = 0, dport = 0;
    uint8_t flags = 0;
    uint32_t seq = 0, ack = 0;
    uint16_t win = 0;
    const uint8_t *data = nullptr;
    size_t len = 0;
};

inline bool parse_tcp(const uint8_t *p, size_t n, tcp_hdr_info &out)
{
    if (n < 20) {
        return false;
    }
    out.sport = static_cast<uint16_t>((p[0] << 8) | p[1]);
    out.dport = static_cast<uint16_t>((p[2] << 8) | p[3]);
    uint32_t s, a;
    std::memcpy(&s, &p[4], 4);
    std::memcpy(&a, &p[8], 4);
    out.seq = ntohl(s);
    out.ack = ntohl(a);
    const size_t hlen = static_cast<size_t>((p[12] >> 4) * 4);
    out.flags = p[13];
    out.win = static_cast<uint16_t>((p[14] << 8) | p[15]);
    if (hlen > n) {
        return false;
    }
    out.data = p + hlen;
    out.len = n - hlen;
    return true;
}

struct udp_hdr_info
{
    uint16_t sport = 0, dport = 0;
    uint16_t len = 0;
    const uint8_t *data = nullptr;
    size_t n = 0;
};

inline bool parse_udp(const uint8_t *p, size_t n, udp_hdr_info &out)
{
    if (n < 8) {
        return false;
    }
    out.sport = static_cast<uint16_t>((p[0] << 8) | p[1]);
    out.dport = static_cast<uint16_t>((p[2] << 8) | p[3]);
    out.len = static_cast<uint16_t>((p[4] << 8) | p[5]);
    out.data = p + 8;
    out.n = out.len > 8 ? out.len - 8 : 0;
    return true;
}

// ---- 虚拟 TUN 设备（socketpair 注入）----
class fake_device
{
public:
    fake_device()
    {
        int sv[2];
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
            throw std::runtime_error("socketpair failed");
        }
        fd_ = sv[0];
        inject_fd_ = sv[1];
    }

    ~fake_device()
    {
        ::close(fd_);
    }

    int inject_fd() const
    {
        return inject_fd_;
    }

    void send(const std::vector<uint8_t> &pkt)
    {
        size_t off = 0;
        while (off < pkt.size()) {
            const ssize_t n = ::write(fd_, pkt.data() + off, pkt.size() - off);
            if (n <= 0) {
                throw std::runtime_error("fake_device send failed");
            }
            off += static_cast<size_t>(n);
        }
    }

    // 读取一个完整 IP 包；支持粘包拆包；超时返回 false
    bool read_packet(std::vector<uint8_t> &out, int timeout_ms = 3000)
    {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(timeout_ms);
        for (;;) {
            while (stash_.size() >= 4) {
                size_t total = 0;
                switch (stash_[0] >> 4) {
                case 4:
                    if (stash_.size() < 20) {
                        break;
                    }
                    total = static_cast<size_t>((stash_[2] << 8) | stash_[3]);
                    break;
                case 6:
                    if (stash_.size() < 40) {
                        break;
                    }
                    total =
                        40 + static_cast<size_t>((stash_[4] << 8) | stash_[5]);
                    break;
                default:
                    break;
                }
                if (total < 20 || stash_.size() < total) {
                    break;
                }
                out.assign(stash_.begin(), stash_.begin() + total);
                stash_.erase(stash_.begin(), stash_.begin() + total);
                return true;
            }
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                return false;
            }
            const int remaining = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                                      now)
                    .count());
            struct pollfd pfd{fd_, POLLIN, 0};
            const int r = ::poll(&pfd, 1, remaining);
            if (r <= 0) {
                return false;
            }
            uint8_t buf[65536];
            const ssize_t n = ::read(fd_, buf, sizeof(buf));
            if (n <= 0) {
                return false;
            }
            stash_.insert(stash_.end(), buf, buf + n);
        }
    }

private:
    int fd_ = -1;
    int inject_fd_ = -1;
    std::vector<uint8_t> stash_;
};

// ---- 引擎测试环境：io_context 线程 + 注入设备 ----
// 使用 executor_work_guard 保持 io 持续运行：close() 会清空引擎挂起工作，
// 若无 guard，io.run() 将返回导致线程退出（真实应用同样需要持续 run）。
struct engine_env
{
    net::io_context io;
    engine_type engine;
    fake_device dev;
    std::thread thread;
    net::executor_work_guard<net::io_context::executor_type> guard;

    explicit engine_env(
        size_t mtu = 1500,
        std::chrono::seconds udp_timeout = std::chrono::seconds(1),
        std::chrono::seconds tcp_accept_timeout = std::chrono::seconds(30),
        std::chrono::seconds tcp_syn_timeout = std::chrono::seconds(30),
        size_t max_rx_queue = 1024 * 1024, size_t max_tx_queue = 1024 * 1024)
        : engine(io)
        , guard(net::make_work_guard(io))
    {
        tun_config cfg;
        cfg.external_handle = dev.inject_fd();
        cfg.external_mtu = mtu;
        cfg.ipv4_addr = "10.0.0.1";
        cfg.netmask = "255.255.255.0";
        cfg.ipv6_addr = "fd00::1";
        cfg.ipv6_prefix_len = 64;
        cfg.udp_idle_timeout = udp_timeout;
        cfg.tcp_accept_timeout = tcp_accept_timeout;
        cfg.tcp_syn_timeout = tcp_syn_timeout;
        cfg.max_rx_queue_per_flow = max_rx_queue;
        cfg.max_tx_queue_per_flow = max_tx_queue;
        boost::system::error_code ec;
        if (!engine.open(cfg, ec)) {
            throw std::runtime_error("engine open failed: " + ec.message());
        }
        thread = std::thread([this] { io.run(); });
    }

    ~engine_env()
    {
        engine.close();
        guard.reset();
        if (thread.joinable()) {
            thread.join();
        }
    }
};

// 带超时的 future 等待
template <typename T>
inline T future_get(std::future<T> fut, int timeout_ms = 5000)
{
    if (fut.wait_for(std::chrono::milliseconds(timeout_ms)) !=
        std::future_status::ready) {
        throw std::runtime_error("future timeout");
    }
    return fut.get();
}

} // namespace test
