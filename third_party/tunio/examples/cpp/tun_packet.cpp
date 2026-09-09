//
// tun_packet.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// tun_packet：原始 IP 包中继/打印示例 —— 直接使用 tun_device + ip_packet。
//
// 读取 TUN 设备上的原始 IP 报文，解析并打印 IP 头信息与 TCP/UDP/ICMP 等
// 传输层协议详情；--echo 时把报文原样写回设备（包回环中继，对端可收到
// 自己的报文）。该示例展示了不依赖 tunio 引擎、直接操作设备层的用法：
//
// 用法示例：
//   sudo ./tun_packet --tun tun0 --ip 10.0.0.1 --netmask 255.255.255.0
//   sudo ./tun_packet --echo                    # 打印并回环中继
//   sudo ./tun_packet --hex                     # 打印载荷十六进制转储
//   sudo ./tun_packet --count 10                # 处理 10 个报文后退出
#include "tunio/ip_packet.hpp"
#include "tunio/tun_device.hpp"

#include <boost/asio.hpp>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
namespace net = boost::asio;

struct options
{
    std::string dev_name = "tun0";
    std::string ipv4_addr = "10.0.0.1";
    std::string netmask = "255.255.255.0";
    std::string ipv6_addr;
    uint8_t ipv6_prefix_len = 64;
    size_t mtu = 1500;
    int inject_fd = -1;
    bool utun_prefix = false;
    bool echo = false;      // 打印后将报文原样写回设备
    bool hex_dump = false;  // 打印传输层载荷十六进制转储
    uint64_t count = 0;     // 处理 n 个报文后退出（0 = 不限）
};

void usage(const char *prog)
{
    std::cout <<
        "usage: " << prog << " [options]\n"
        "  读取 TUN 设备上的原始 IP 报文，打印解析后的协议信息（IP 头 /\n"
        "  TCP / UDP / ICMP）；--echo 时把报文原样写回设备（包回环中继）。\n"
        "  --tun <name>          TUN 设备名（默认 tun0）\n"
        "  --ip <addr>           本地虚拟 IPv4 地址（默认 10.0.0.1）\n"
        "  --netmask <mask>      子网掩码（默认 255.255.255.0）\n"
        "  --ip6 <addr>          本地虚拟 IPv6 地址（可选，如 fd00::1）\n"
        "  --ip6-prefix <len>    IPv6 前缀长度（默认 64）\n"
        "  --mtu <bytes>         设备 MTU（默认 1500）\n"
        "  --inject-fd <fd>      注入外部已打开的 TUN 文件描述符\n"
        "  --utun-prefix         注入的 fd 为 macOS utun（读写带 4 字节家族前缀）\n"
        "  --echo                打印后将报文原样写回设备（回环中继）\n"
        "  --hex                 打印传输层载荷的十六进制转储（最多 64 字节）\n"
        "  --count <n>           处理 n 个报文后退出（0 = 不限，默认 0）\n"
        "  -h, --help            显示本帮助\n";
}

options parse_args(int argc, char **argv)
{
    options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing value for " + arg);
            }
            return argv[++i];
        };
        if (arg == "--tun") {
            opt.dev_name = next();
        } else if (arg == "--ip") {
            opt.ipv4_addr = next();
        } else if (arg == "--netmask") {
            opt.netmask = next();
        } else if (arg == "--ip6") {
            opt.ipv6_addr = next();
        } else if (arg == "--ip6-prefix") {
            opt.ipv6_prefix_len = static_cast<uint8_t>(std::stoul(next()));
        } else if (arg == "--mtu") {
            opt.mtu = static_cast<size_t>(std::stoul(next()));
        } else if (arg == "--inject-fd") {
            opt.inject_fd = std::stoi(next());
        } else if (arg == "--utun-prefix") {
            opt.utun_prefix = true;
        } else if (arg == "--echo") {
            opt.echo = true;
        } else if (arg == "--hex") {
            opt.hex_dump = true;
        } else if (arg == "--count") {
            opt.count = std::stoull(next());
        } else if (arg == "-h" || arg == "--help") {
            usage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("unknown option: " + arg);
        }
    }
    return opt;
}

std::string tcp_flags_string(uint8_t flags)
{
    std::string s;
    if (flags & 0x01) {
        s += "FIN|";
    }
    if (flags & 0x02) {
        s += "SYN|";
    }
    if (flags & 0x04) {
        s += "RST|";
    }
    if (flags & 0x08) {
        s += "PSH|";
    }
    if (flags & 0x10) {
        s += "ACK|";
    }
    if (flags & 0x20) {
        s += "URG|";
    }
    if (s.empty()) {
        return "-";
    }
    s.pop_back();
    return s;
}

void hex_dump(const uint8_t *data, size_t len)
{
    constexpr size_t per = 16;
    for (size_t off = 0; off < len; off += per) {
        std::printf("%04zx  ", off);
        const size_t n = std::min(per, len - off);
        for (size_t i = 0; i < per; ++i) {
            if (i < n) {
                std::printf("%02x ", data[off + i]);
            } else {
                std::printf("   ");
            }
        }
        std::printf(" |");
        for (size_t i = 0; i < n; ++i) {
            const uint8_t c = data[off + i];
            std::printf("%c",
                (c >= 0x20 && c < 0x7f) ? static_cast<char>(c) : '.');
        }
        std::printf("|\n");
    }
}

void print_packet(const tunio::ip_packet &pkt, bool hex)
{
    std::cout << '[' << (pkt.version() == 6 ? "IPv6" : "IPv4") << ']';
    std::cout << ' ' << pkt.source_address() << ':' << pkt.source_port();
    std::cout << " -> " << pkt.destination_address() << ':'
        << pkt.destination_port();
    if (pkt.is_tcp()) {
        std::cout << " TCP";
        if (const auto *t = pkt.tcp()) {
            std::cout << " flags=" << tcp_flags_string(t->flags)
                << " seq=" << ntohl(t->seq) << " ack=" << ntohl(t->ack)
                << " win=" << ntohs(t->window);
        }
    } else if (pkt.is_udp()) {
        std::cout << " UDP";
    } else if (pkt.is_icmp() || pkt.is_icmpv6()) {
        std::cout << (pkt.is_icmpv6() ? " ICMPv6" : " ICMP")
            << " type=" << static_cast<unsigned>(pkt.icmp_type())
            << " code=" << static_cast<unsigned>(pkt.icmp_code());
        const bool echo =
            pkt.version() == 6
            ? (pkt.icmp_type() == 128 || pkt.icmp_type() == 129)
            : (pkt.icmp_type() == 8 || pkt.icmp_type() == 0);
        if (echo) {
            std::cout << " id=" << pkt.icmp_echo_id()
                << " seq=" << pkt.icmp_echo_seq();
        }
    } else {
        std::cout << " proto=" << static_cast<unsigned>(pkt.ip_protocol());
    }
    if (pkt.fragmented()) {
        std::cout << " frag@offset=" << pkt.fragment_offset();
    }
    std::cout << " total=" << pkt.total_length()
        << " payload=" << pkt.transport_data_size() << 'B' << std::endl;
    if (hex && pkt.transport_data() != nullptr) {
        hex_dump(pkt.transport_data(),
            std::min<size_t>(pkt.transport_data_size(), 64));
    }
}

// 顺序处理循环：单 ip_packet 复用，每次读入 -> 打印 ->（可选）原样写回。
net::awaitable<void> run(tunio::tun_device &dev, const options &opt)
{
    tunio::ip_packet pkt;
    uint64_t processed = 0;
    for (;;) {
        boost::system::error_code ec;
        const size_t n = co_await dev.async_read_ip(pkt,
            net::redirect_error(net::use_awaitable, ec));
        if (ec) {
            if (ec != net::error::operation_aborted) {
                std::cerr << "read failed: " << ec.message() << std::endl;
            }
            co_return; // close() 触发的取消或设备故障
        }
        if (!pkt.valid()) {
            std::cout << "[invalid packet] " << n << " bytes: "
                << tunio::ip_packet::error_message(pkt.error()) << std::endl;
            continue;
        }
        print_packet(pkt, opt.hex_dump);
        ++processed;
        if (opt.echo) {
            // 原样写回设备：对端将收到自己发出的报文（包回环中继）。
            co_await dev.async_write_ip(pkt,
                net::redirect_error(net::use_awaitable, ec));
            if (ec) {
                std::cerr << "write failed: " << ec.message() << std::endl;
                co_return;
            }
        }
        if (opt.count != 0 && processed >= opt.count) {
            std::cout << "processed " << processed << " packets, exiting."
                << std::endl;
            co_return;
        }
    }
}

} // namespace

int main(int argc, char **argv)
{
    options opt;
    try {
        opt = parse_args(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << std::endl;
        usage(argv[0]);
        return 1;
    }

    net::io_context io(1);
    tunio::tun_device dev(io);
    boost::system::error_code ec;

    if (opt.inject_fd >= 0) {
        if (!dev.assign(tunio::native_handle_from_int(opt.inject_fd), opt.mtu,
                opt.utun_prefix, ec)) {
            std::cerr << "assign TUN fd failed: " << ec.message() << std::endl;
            return 1;
        }
    } else {
        tunio::device_config cfg;
        cfg.name = opt.dev_name;
        cfg.ipv4 = opt.ipv4_addr;
        cfg.netmask = opt.netmask;
        cfg.ipv6 = opt.ipv6_addr;
        cfg.ipv6_prefix_len = opt.ipv6_prefix_len;
        cfg.mtu = opt.mtu;
        if (!dev.open(cfg, ec)) {
            std::cerr << "open TUN failed: " << ec.message() << std::endl;
            return 1;
        }
    }
    std::cout << "tun_packet: " << opt.dev_name << " mtu=" << dev.mtu()
        << (opt.echo ? " echo" : "") << (opt.hex_dump ? " hex" : "")
        << std::endl;

    // 处理循环结束（含 --count 达到上限）后停止 io_context，
    // 否则挂起的 signal_set 等待会使 io.run() 永不返回。
    net::co_spawn(
        io,
        [&]() -> net::awaitable<void> {
            try {
                co_await run(dev, opt);
            } catch (const std::exception &e) {
                std::cerr << "error: " << e.what() << std::endl;
            }
            io.stop();
        },
        net::detached);

    net::signal_set signals(io, SIGINT, SIGTERM);
    signals.async_wait(
        [&](const boost::system::error_code &, int) { dev.close(); });

    io.run();
    return 0;
}
