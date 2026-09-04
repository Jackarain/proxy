//
// tun_device_posix.cpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tunio/tun_device.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#if defined(__linux__)
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#endif

#if defined(__APPLE__)
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_utun.h>
#include <netinet6/in6_var.h>
#include <sys/kern_control.h>
#include <sys/sys_domain.h>
#endif

#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)

namespace tunio {

namespace detail {

#if defined(__linux__)
namespace {

void addattr_l(
    struct nlmsghdr* n, size_t maxlen, int type, const void* data, size_t alen)
{
    const size_t len = RTA_LENGTH(alen);
    if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > maxlen)
        return; // 属性溢出消息缓冲：丢弃，避免越界写入
    struct rtattr* rta = reinterpret_cast<struct rtattr*>(
        reinterpret_cast<char*>(n) + NLMSG_ALIGN(n->nlmsg_len));
    rta->rta_type = type;
    rta->rta_len = static_cast<uint16_t>(len);
    std::memcpy(RTA_DATA(rta), data, alen);
    n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);
}

// 通过 netlink 为接口添加 IPv6 地址（RTM_NEWADDR），等待内核 ACK
bool add_ipv6_address(int ioctl_sock,
    const std::string& ifname,
    const std::string& addr,
    uint8_t prefix_len,
    boost::system::error_code& ec)
{
    struct in6_addr a6;
    if (::inet_pton(AF_INET6, addr.c_str(), &a6) != 1)
    {
        ec = boost::system::error_code(
            EINVAL, boost::system::generic_category());
        return false;
    }

    struct ifreq idx;
    std::memset(&idx, 0, sizeof(idx));
    std::snprintf(idx.ifr_name, IFNAMSIZ, "%s", ifname.c_str());
    if (::ioctl(ioctl_sock, SIOCGIFINDEX, &idx) < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        return false;
    }

    const int nl = ::socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (nl < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        return false;
    }

    char buf[512] = {};
    auto* n = reinterpret_cast<struct nlmsghdr*>(buf);
    n->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    n->nlmsg_type = RTM_NEWADDR;
    n->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;
    n->nlmsg_seq = 1;
    auto* ifa = reinterpret_cast<struct ifaddrmsg*>(NLMSG_DATA(n));
    ifa->ifa_family = AF_INET6;
    ifa->ifa_prefixlen = prefix_len;
    ifa->ifa_scope = 0;
    ifa->ifa_index = idx.ifr_ifindex;
    addattr_l(n, sizeof(buf), IFA_LOCAL, &a6, sizeof(a6));
    addattr_l(n, sizeof(buf), IFA_ADDRESS, &a6, sizeof(a6));

    if (::send(nl, buf, n->nlmsg_len, 0) < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(nl);
        return false;
    }

    // 等待 ACK（带超时，避免内核无响应时挂死）
    for (;;)
    {
        struct pollfd pfd{nl, POLLIN, 0};
        const int r = ::poll(&pfd, 1, 2000);
        if (r <= 0)
        {
            ec = boost::system::error_code(
                ETIMEDOUT, boost::system::generic_category());
            ::close(nl);
            return false;
        }
        char rbuf[4096];
        const ssize_t rd = ::recv(nl, rbuf, sizeof(rbuf), 0);
        if (rd < 0)
        {
            if (errno == EINTR)
                continue;
            ec = boost::system::error_code(
                errno, boost::system::generic_category());
            ::close(nl);
            return false;
        }
        int len = static_cast<int>(rd);
        for (struct nlmsghdr* h = reinterpret_cast<struct nlmsghdr*>(rbuf);
            NLMSG_OK(h, len);
            h = NLMSG_NEXT(h, len))
        {
            if (h->nlmsg_type != NLMSG_ERROR)
                continue;
            const auto* err = reinterpret_cast<struct nlmsgerr*>(NLMSG_DATA(h));
            if (err->error == 0 || err->error == -EEXIST)
            {
                // 成功，或地址已存在（幂等）
                ::close(nl);
                return true;
            }
            ec = boost::system::error_code(
                -err->error, boost::system::generic_category());
            ::close(nl);
            return false;
        }
    }
}

// 通过 ioctl socket 设置接口发送队列长度；非致命——失败时保留内核默认值。
void set_tx_queue_len(int sock, const char* ifname, int qlen)
{
    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    ifr.ifr_qlen = qlen;
    if (::ioctl(sock, SIOCSIFTXQLEN, &ifr) < 0)
    {
        // 非致命：保留内核默认队列长度。
    }
}

} // namespace
#elif defined(__APPLE__)
namespace {

// UTUN_OPT_IFNAME 定义于 macOS <net/if_utun.h>；低版本 SDK 缺省时兜底。
#ifndef UTUN_OPT_IFNAME
#define UTUN_OPT_IFNAME 2
#endif

// 经内核控制套接字打开 utun 设备（WireGuard / OpenVPN 同款流程）：
// socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL) -> CTLIOCGINFO ->
// connect(sockaddr_ctl) -> getsockopt(UTUN_OPT_IFNAME) 取回接口名。
//
// name 为空或 "utun" 时 sc_unit = 0，由内核分配最低可用单元；否则须为
// "utunN"（sc_unit = N + 1，内核据此创建 utunN，单元占用时 connect 失败）。
bool open_utun(const std::string& name, int& out_fd, std::string& out_ifname,
    boost::system::error_code& ec)
{
    // 解析单元号：空 / "utun" -> 自动分配（unit = -1，sc_unit = 0）.
    int unit = -1;
    if (!name.empty() && name != "utun")
    {
        if (name.compare(0, 4, "utun") != 0 ||
            !std::isdigit(static_cast<unsigned char>(name[4])))
        {
            ec = boost::system::error_code(
                EINVAL, boost::system::generic_category());
            return false;
        }
        char* end = nullptr;
        const long n = std::strtol(name.c_str() + 4, &end, 10);
        if (end == name.c_str() + 4 || *end != '\0' || n < 0)
        {
            ec = boost::system::error_code(
                EINVAL, boost::system::generic_category());
            return false;
        }
        unit = static_cast<int>(n);
    }

    const int fd = ::socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
    if (fd < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        return false;
    }

    struct ctl_info info;
    std::memset(&info, 0, sizeof(info));
    std::snprintf(info.ctl_name, sizeof(info.ctl_name), "%s",
        "com.apple.net.utun_control");
    if (::ioctl(fd, CTLIOCGINFO, &info) < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(fd);
        return false;
    }

    struct sockaddr_ctl sc;
    std::memset(&sc, 0, sizeof(sc));
    sc.sc_id = info.ctl_id;
    sc.sc_len = sizeof(sc);
    sc.sc_family = AF_SYSTEM;
    sc.ss_sysaddr = AF_SYS_CONTROL;
    // 单元号 + 1：connect 成功后内核创建 utun(单元号)；unit = -1 时
    // sc_unit = 0 表示自动分配最低可用单元。
    sc.sc_unit = static_cast<unsigned int>(unit + 1);
    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&sc), sizeof(sc)) < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(fd);
        return false;
    }

    // 取回内核分配的接口名（如 "utun3"），供后续 ioctl 配置使用。
    char ifname[IF_NAMESIZE] = {};
    socklen_t ifname_len = static_cast<socklen_t>(sizeof(ifname));
    if (::getsockopt(
            fd, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, ifname, &ifname_len) < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(fd);
        return false;
    }

    // utun fd 须为非阻塞（Asio stream_descriptor 需求）与 close-on-exec。
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ||
        ::fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(fd);
        return false;
    }

    out_fd = fd;
    out_ifname = ifname;
    return true;
}

// 通过 AF_INET 配置 socket 对 ifname 执行取值型 ioctl（地址/掩码/对端）.
bool set_ioctl_addr(int s, const char* ifname, unsigned long cmd,
    const std::string& addr, boost::system::error_code& ec)
{
    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    auto* sin = reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
    sin->sin_family = AF_INET;
    if (::inet_pton(AF_INET, addr.c_str(), &sin->sin_addr) != 1)
    {
        ec = boost::system::error_code(
            EINVAL, boost::system::generic_category());
        return false;
    }
    if (::ioctl(s, cmd, &ifr) < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        return false;
    }
    return true;
}

// 设置接口 MTU 并读回内核生效值（对齐 Linux 分支的 ifr.ifr_mtu 语义）.
bool set_ioctl_mtu(int s, const char* ifname, size_t mtu,
    size_t& out_mtu, boost::system::error_code& ec)
{
    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    ifr.ifr_mtu = static_cast<int>(std::max<size_t>(mtu, 576));
    if (::ioctl(s, SIOCSIFMTU, &ifr) < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        return false;
    }
    std::memset(&ifr, 0, sizeof(ifr));
    std::snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    if (::ioctl(s, SIOCGIFMTU, &ifr) == 0)
        out_mtu = static_cast<size_t>(ifr.ifr_mtu);
    else
        out_mtu = mtu;
    return true;
}

// 启用接口（IFF_UP；IFF_RUNNING 由内核根据链路状态自动维护，与 ifconfig
// 语义一致）.
bool set_if_up(int s, const char* ifname, boost::system::error_code& ec)
{
    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
    if (::ioctl(s, SIOCGIFFLAGS, &ifr) < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        return false;
    }
    ifr.ifr_flags |= IFF_UP;
    if (::ioctl(s, SIOCSIFFLAGS, &ifr) < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        return false;
    }
    return true;
}

// 通过 SIOCAIFADDR_IN6 为接口添加 IPv6 地址（ifconfig inet6 add 同款
// 机制，需 root）；地址已存在视为幂等成功（对齐 Linux netlink 的
// EEXIST 处理）.
bool add_ipv6_address(const char* ifname, const std::string& addr,
    uint8_t prefix_len, boost::system::error_code& ec)
{
    struct in6_addr a6;
    if (::inet_pton(AF_INET6, addr.c_str(), &a6) != 1)
    {
        ec = boost::system::error_code(
            EINVAL, boost::system::generic_category());
        return false;
    }

    const int s6 = ::socket(AF_INET6, SOCK_DGRAM, 0);
    if (s6 < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        return false;
    }

    struct in6_aliasreq ifra;
    std::memset(&ifra, 0, sizeof(ifra));
    std::snprintf(ifra.ifra_name, IFNAMSIZ, "%s", ifname);

    ifra.ifra_addr.sin6_family = AF_INET6;
    ifra.ifra_addr.sin6_len = static_cast<uint8_t>(sizeof(ifra.ifra_addr));
    ifra.ifra_addr.sin6_addr = a6;

    // 前缀掩码：前缀长度位全置 1（BSD sockaddr_in6 的 sin6_addr 存掩码）.
    uint8_t mask[16] = {};
    for (uint8_t i = 0; i < prefix_len; ++i)
        mask[i / 8] |= static_cast<uint8_t>(0x80 >> (i % 8));
    ifra.ifra_prefixmask.sin6_family = AF_INET6;
    ifra.ifra_prefixmask.sin6_len =
        static_cast<uint8_t>(sizeof(ifra.ifra_prefixmask));
    std::memcpy(&ifra.ifra_prefixmask.sin6_addr, mask, sizeof(mask));

    if (::ioctl(s6, SIOCAIFADDR_IN6, &ifra) < 0 && errno != EEXIST)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(s6);
        return false;
    }
    ::close(s6);
    return true;
}

} // namespace
#endif

bool posix_tun_device_impl::open(
    const device_config& cfg, boost::system::error_code& ec)
{
#if defined(__linux__)
    // 重复 open 前先关闭已打开的设备，避免新 fd 泄漏与旧设备残留
    close();

    // 多队列校验：0 或超上限（内核 MAX_TAP_QUEUES）视为非法配置；
    // 单队列（默认）保持不带 IFF_MULTI_QUEUE 的旧行为，兼容老内核。
    if (cfg.num_queues < 1 || cfg.num_queues > max_multi_queues)
    {
        ec = boost::system::error_code(
            EINVAL, boost::system::generic_category());
        return false;
    }
    const bool multi = cfg.num_queues > 1;

    // 打开第一个队列 fd（多队列模式下 TUNSETIFF 携带 IFF_MULTI_QUEUE）
    const int fd = ::open("/dev/net/tun", O_RDWR | O_CLOEXEC);
    if (fd < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        return false;
    }

    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::snprintf(ifr.ifr_name, IFNAMSIZ, "%s", cfg.name.c_str());
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI | (multi ? IFF_MULTI_QUEUE : 0);
    if (::ioctl(fd, TUNSETIFF, &ifr) < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(fd);
        return false;
    }
    // 空设备名自动命名时内核会把分配的名字写回 ifr.ifr_name：
    // 后续所有 ioctl 必须使用该有效名，而非 cfg.name（空串）.
    const std::string dev_name =
        ifr.ifr_name[0] != '\0' ? ifr.ifr_name : cfg.name;

    // 打开其余队列 fd（同一设备名，必须再次携带 IFF_MULTI_QUEUE）.
    // 任一队列打开失败时回滚关闭全部 fd，内核随之销毁该 tun 设备。
    std::vector<int> fds;
    fds.reserve(cfg.num_queues);
    fds.push_back(fd);
    for (size_t q = 1; q < cfg.num_queues; ++q)
    {
        const int qfd = ::open("/dev/net/tun", O_RDWR | O_CLOEXEC);
        if (qfd < 0)
        {
            ec = boost::system::error_code(
                errno, boost::system::generic_category());
            for (const int f : fds)
            {
                ::close(f);
            }
            return false;
        }
        struct ifreq qifr;
        std::memset(&qifr, 0, sizeof(qifr));
        std::snprintf(qifr.ifr_name, IFNAMSIZ, "%s", dev_name.c_str());
        qifr.ifr_flags = IFF_TUN | IFF_NO_PI | IFF_MULTI_QUEUE;
        if (::ioctl(qfd, TUNSETIFF, &qifr) < 0)
        {
            ec = boost::system::error_code(
                errno, boost::system::generic_category());
            ::close(qfd);
            for (const int f : fds)
            {
                ::close(f);
            }
            return false;
        }
        fds.push_back(qfd);
    }

    // 失败回滚：关闭全部已打开队列 fd，内核随之销毁该 tun 设备
    auto close_all = [&]()
    {
        for (const int f : fds)
        {
            ::close(f);
        }
    };

    // 配置 IP / 掩码（需要 CAP_NET_ADMIN）
    const int s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        close_all();
        return false;
    }

    // 默认调整发送队列长度（txqueuelen）为 4096，避免设备写拥塞时内核
    // 队列过短导致突发丢包；非致命——失败时保留内核默认值，不阻断打开。
    set_tx_queue_len(s, dev_name.c_str(), 4096);

    // 未指定 IPv4 地址时（如外部脚本负责配置地址/路由/UP），仅设置 MTU，
    // 不与外部配置冲突；与“只创建设备”的旧语义保持一致。
    if (cfg.ipv4.empty())
    {
        std::memset(&ifr, 0, sizeof(ifr));
        std::snprintf(ifr.ifr_name, IFNAMSIZ, "%s", dev_name.c_str());
        ifr.ifr_mtu = static_cast<int>(std::max<size_t>(cfg.mtu, 576));
        if (::ioctl(s, SIOCSIFMTU, &ifr) < 0)
        {
            ec = boost::system::error_code(
                errno, boost::system::generic_category());
            ::close(s);
            close_all();
            return false;
        }
        ::close(s);

        return adopt_fds(
            std::move(fds), static_cast<size_t>(ifr.ifr_mtu), false, ec);
    }

    auto set_ifr = [&](int cmd, const char* addr) -> bool
    {
        struct ifreq aifr;
        std::memset(&aifr, 0, sizeof(aifr));
        std::snprintf(aifr.ifr_name, IFNAMSIZ, "%s", dev_name.c_str());
        auto* sin = reinterpret_cast<struct sockaddr_in*>(&aifr.ifr_addr);
        sin->sin_family = AF_INET;
        if (::inet_pton(AF_INET, addr, &sin->sin_addr) != 1)
        {
            errno = EINVAL; // 地址解析失败：给出明确 errno，避免陈旧值误导
            return false;
        }
        return ::ioctl(s, cmd, &aifr) == 0;
    };

    if (!set_ifr(SIOCSIFADDR, cfg.ipv4.c_str()))
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(s);
        close_all();
        return false;
    }
    if (!set_ifr(SIOCSIFNETMASK, cfg.netmask.c_str()))
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(s);
        close_all();
        return false;
    }

    // 配置 IPv6 地址（可选）
    if (!cfg.ipv6.empty() &&
        !add_ipv6_address(s, dev_name, cfg.ipv6, cfg.ipv6_prefix_len, ec))
    {
        ::close(s);
        close_all();
        return false;
    }

    // 设置 MTU 并启用接口
    std::memset(&ifr, 0, sizeof(ifr));
    std::snprintf(ifr.ifr_name, IFNAMSIZ, "%s", dev_name.c_str());
    ifr.ifr_mtu = static_cast<int>(std::max<size_t>(cfg.mtu, 576));
    if (::ioctl(s, SIOCSIFMTU, &ifr) < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(s);
        close_all();
        return false;
    }
    if (::ioctl(s, SIOCGIFFLAGS, &ifr) < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(s);
        close_all();
        return false;
    }
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    if (::ioctl(s, SIOCSIFFLAGS, &ifr) < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(s);
        close_all();
        return false;
    }
    ::close(s);

    return adopt_fds(std::move(fds), static_cast<size_t>(ifr.ifr_mtu), false, ec);
#elif defined(__APPLE__)
    // macOS utun 自主打开：经内核控制套接字创建 utun 设备（WireGuard /
    // OpenVPN 同款流程），创建后按 device_config 配置 MTU / 地址并启用。
    // utun 读写携带 4 字节家族前缀，adopt 时置 utun_prefix = true。
    // 设备名须为 utunN 或空（自动分配最低可用单元）。
    close();

    int fd = -1;
    std::string ifname;
    if (!open_utun(cfg.name, fd, ifname, ec))
        return false;

    // 配置 socket（IPv4 地址 / 掩码 / 对端 / MTU / 接口 flags）.
    const int s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(fd);
        return false;
    }

    size_t mtu = cfg.mtu;
    if (!set_ioctl_mtu(s, ifname.c_str(), cfg.mtu, mtu, ec))
    {
        ::close(s);
        ::close(fd);
        return false;
    }

    const bool need_config = !cfg.ipv4.empty() || !cfg.ipv6.empty();

    if (!cfg.ipv4.empty())
    {
        // 点对点接口：本地地址、对端地址（置为自身，ifconfig utunX <ip>
        // <ip> 语义）与掩码。
        if (!set_ioctl_addr(s, ifname.c_str(), SIOCSIFADDR, cfg.ipv4, ec) ||
            !set_ioctl_addr(
                s, ifname.c_str(), SIOCSIFDSTADDR, cfg.ipv4, ec) ||
            !set_ioctl_addr(s, ifname.c_str(), SIOCSIFNETMASK,
                cfg.netmask.empty() ? "255.255.255.0" : cfg.netmask, ec))
        {
            ::close(s);
            ::close(fd);
            return false;
        }
    }

    if (!cfg.ipv6.empty() &&
        !add_ipv6_address(ifname.c_str(), cfg.ipv6, cfg.ipv6_prefix_len, ec))
    {
        ::close(s);
        ::close(fd);
        return false;
    }

    if (need_config && !set_if_up(s, ifname.c_str(), ec))
    {
        ::close(s);
        ::close(fd);
        return false;
    }

    ::close(s);

    return adopt_fds(std::vector<int>{fd}, mtu, true, ec);
#else
    // 其他 POSIX 平台的自主打开尚未实现，句柄注入模式 assign() 在所有
    // 平台可用。
    (void)cfg;
    ec = boost::system::error_code(boost::system::errc::operation_not_supported,
        boost::system::generic_category());
    return false;
#endif
}

bool posix_tun_device_impl::assign_queues(
    const std::vector<native_handle_type>& handles,
    size_t mtu,
    bool utun_prefix,
    boost::system::error_code& ec)
{
    if (handles.empty())
    {
        ec = boost::system::error_code(
            EINVAL, boost::system::generic_category());
        return false;
    }

    // 与自主打开一致：句柄数（队列数）上限为内核 MAX_TAP_QUEUES.
    if (handles.size() > max_multi_queues)
    {
        ec = boost::system::error_code(
            EINVAL, boost::system::generic_category());
        return false;
    }

    // 与自主打开一致：对注入的设备 fd 同样调整默认发送队列长度。
    std::vector<net::posix::stream_descriptor> new_descs;
    new_descs.reserve(handles.size());

    for (const auto h : handles)
    {
        apply_default_tx_queue_len(h);
        net::posix::stream_descriptor d(ctx_);
        d.assign(static_cast<native_handle_type>(h), ec);
        if (ec)
        {
            // Asio assign 失败时不接管句柄；把已接管的部分归还
            //（release 放弃所有权），保证失败路径不关闭任何注入句柄。
            for (auto& dd : new_descs)
            {
                dd.release();
            }
            new_descs.clear();
            return false;
        }
        new_descs.push_back(std::move(d));
    }

    // 成功接管：替换成员，旧描述符析构关闭旧句柄。
    descs_ = std::move(new_descs);
    open_ = true;
    mtu_ = mtu;
    utun_prefix_ = utun_prefix;
    return true;
}

void posix_tun_device_impl::close()
{
    if (open_)
    {
        for (auto& d : descs_)
        {
            d.close();
        }
        descs_.clear();
        open_ = false;
    }
}

bool posix_tun_device_impl::adopt_fds(std::vector<int>&& fds, size_t mtu,
    bool utun_prefix, boost::system::error_code& ec)
{
    // 逐个 assign 到 stream_descriptor：全部成功后才替换成员，任何一步
    // 失败即回滚——已接管的由 new_descs 析构关闭，未接管的显式关闭
    //（自主打开路径下这些 fd 由本类打开，所有权属于本类）.
    std::vector<net::posix::stream_descriptor> new_descs;
    new_descs.reserve(fds.size());
    for (size_t i = 0; i < fds.size(); ++i)
    {
        net::posix::stream_descriptor d(ctx_);
        d.assign(fds[i], ec);
        if (ec)
        {
            new_descs.clear();
            for (size_t j = i; j < fds.size(); ++j)
            {
                ::close(fds[j]);
            }
            return false;
        }
        new_descs.push_back(std::move(d));
    }
    descs_ = std::move(new_descs);
    open_ = true;
    mtu_ = mtu;
    utun_prefix_ = utun_prefix;
    return true;
}

void posix_tun_device_impl::apply_default_tx_queue_len(
    native_handle_type handle)
{
#if defined(__linux__)
    // 注入的 TUN fd 经 TUNGETIFF 取回接口名，再经 ioctl socket 应用默认
    // 发送队列长度；非 TUN 句柄（如测试用 socketpair）或缺少权限时静默跳过。
    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    if (::ioctl(handle, TUNGETIFF, &ifr) < 0)
        return;
    const int s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
        return;
    set_tx_queue_len(s, ifr.ifr_name, 4096);
    ::close(s);
#else
    (void)handle;
#endif
}

} // namespace detail

} // namespace tunio

#endif // BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR
