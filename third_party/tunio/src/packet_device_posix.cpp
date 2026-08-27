//
// packet_device_posix.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tunio/packet_device.hpp"

#include <algorithm>
#include <cerrno>
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

#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)

namespace tunio {

namespace detail {

#if defined(__linux__)
namespace {

void addattr_l(struct nlmsghdr *n, size_t maxlen, int type, const void *data,
               size_t alen)
{
    const size_t len = RTA_LENGTH(alen);
    if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > maxlen) {
        return; // 属性溢出消息缓冲：丢弃，避免越界写入
    }
    struct rtattr *rta = reinterpret_cast<struct rtattr *>(
        reinterpret_cast<char *>(n) + NLMSG_ALIGN(n->nlmsg_len));
    rta->rta_type = type;
    rta->rta_len = static_cast<uint16_t>(len);
    std::memcpy(RTA_DATA(rta), data, alen);
    n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);
}

// 通过 netlink 为接口添加 IPv6 地址（RTM_NEWADDR），等待内核 ACK
bool add_ipv6_address(int ioctl_sock, const std::string &ifname,
                      const std::string &addr, uint8_t prefix_len,
                      boost::system::error_code &ec)
{
    struct in6_addr a6;
    if (::inet_pton(AF_INET6, addr.c_str(), &a6) != 1) {
        ec = boost::system::error_code(EINVAL,
                                       boost::system::generic_category());
        return false;
    }

    struct ifreq idx;
    std::memset(&idx, 0, sizeof(idx));
    std::strncpy(idx.ifr_name, ifname.c_str(), IFNAMSIZ - 1);
    if (::ioctl(ioctl_sock, SIOCGIFINDEX, &idx) < 0) {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        return false;
    }

    const int nl = ::socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (nl < 0) {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        return false;
    }

    char buf[512] = {};
    auto *n = reinterpret_cast<struct nlmsghdr *>(buf);
    n->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    n->nlmsg_type = RTM_NEWADDR;
    n->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL | NLM_F_ACK;
    n->nlmsg_seq = 1;
    auto *ifa = reinterpret_cast<struct ifaddrmsg *>(NLMSG_DATA(n));
    ifa->ifa_family = AF_INET6;
    ifa->ifa_prefixlen = prefix_len;
    ifa->ifa_scope = 0;
    ifa->ifa_index = idx.ifr_ifindex;
    addattr_l(n, sizeof(buf), IFA_LOCAL, &a6, sizeof(a6));
    addattr_l(n, sizeof(buf), IFA_ADDRESS, &a6, sizeof(a6));

    if (::send(nl, buf, n->nlmsg_len, 0) < 0) {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(nl);
        return false;
    }

    // 等待 ACK（带超时，避免内核无响应时挂死）
    for (;;) {
        struct pollfd pfd{nl, POLLIN, 0};
        const int r = ::poll(&pfd, 1, 2000);
        if (r <= 0) {
            ec = boost::system::error_code(ETIMEDOUT,
                                           boost::system::generic_category());
            ::close(nl);
            return false;
        }
        char rbuf[4096];
        const ssize_t rd = ::recv(nl, rbuf, sizeof(rbuf), 0);
        if (rd < 0) {
            if (errno == EINTR) {
                continue;
            }
            ec = boost::system::error_code(errno,
                                           boost::system::generic_category());
            ::close(nl);
            return false;
        }
        int len = static_cast<int>(rd);
        for (struct nlmsghdr *h = reinterpret_cast<struct nlmsghdr *>(rbuf);
             NLMSG_OK(h, len); h = NLMSG_NEXT(h, len)) {
            if (h->nlmsg_type != NLMSG_ERROR) {
                continue;
            }
            const auto *err =
                reinterpret_cast<struct nlmsgerr *>(NLMSG_DATA(h));
            if (err->error == 0 || err->error == -EEXIST) {
                // 成功，或地址已存在（幂等）
                ::close(nl);
                return true;
            }
            ec = boost::system::error_code(-err->error,
                                           boost::system::generic_category());
            ::close(nl);
            return false;
        }
    }
}

} // namespace
#endif

bool posix_packet_device_impl::open(const device_config &cfg,
                                    boost::system::error_code &ec)
{
#if defined(__linux__)
    const int fd = ::open("/dev/net/tun", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        return false;
    }

    struct ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, cfg.name.c_str(), IFNAMSIZ - 1);
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    if (::ioctl(fd, TUNSETIFF, &ifr) < 0) {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(fd);
        return false;
    }

    // 配置 IP / 掩码（需要 CAP_NET_ADMIN）
    const int s = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(fd);
        return false;
    }

    // 未指定 IPv4 地址时（如外部脚本负责配置地址/路由/UP），仅设置 MTU，
    // 不与外部配置冲突；与“只创建设备”的旧语义保持一致。
    if (cfg.ipv4.empty()) {
        std::memset(&ifr, 0, sizeof(ifr));
        std::strncpy(ifr.ifr_name, cfg.name.c_str(), IFNAMSIZ - 1);
        ifr.ifr_mtu = static_cast<int>(std::max<size_t>(cfg.mtu, 576));
        if (::ioctl(s, SIOCSIFMTU, &ifr) < 0) {
            ec = boost::system::error_code(
                errno, boost::system::generic_category());
            ::close(s);
            ::close(fd);
            return false;
        }
        ::close(s);

        desc_.assign(fd, ec);
        if (!ec) {
            open_ = true;
            mtu_ = static_cast<size_t>(ifr.ifr_mtu);
        }
        return !ec;
    }

    auto set_ifr = [&](int cmd, const char *addr) -> bool {
        struct ifreq aifr;
        std::memset(&aifr, 0, sizeof(aifr));
        std::strncpy(aifr.ifr_name, cfg.name.c_str(), IFNAMSIZ - 1);
        auto *sin = reinterpret_cast<struct sockaddr_in *>(&aifr.ifr_addr);
        sin->sin_family = AF_INET;
        if (::inet_pton(AF_INET, addr, &sin->sin_addr) != 1) {
            return false;
        }
        return ::ioctl(s, cmd, &aifr) == 0;
    };

    if (!set_ifr(SIOCSIFADDR, cfg.ipv4.c_str())) {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(s);
        ::close(fd);
        return false;
    }
    if (!set_ifr(SIOCSIFNETMASK, cfg.netmask.c_str())) {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(s);
        ::close(fd);
        return false;
    }

    // 配置 IPv6 地址（可选）
    if (!cfg.ipv6.empty() &&
        !add_ipv6_address(s, cfg.name, cfg.ipv6, cfg.ipv6_prefix_len, ec)) {
        ::close(s);
        ::close(fd);
        return false;
    }

    // 设置 MTU 并启用接口
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, cfg.name.c_str(), IFNAMSIZ - 1);
    ifr.ifr_mtu = static_cast<int>(std::max<size_t>(cfg.mtu, 576));
    if (::ioctl(s, SIOCSIFMTU, &ifr) < 0) {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(s);
        ::close(fd);
        return false;
    }
    if (::ioctl(s, SIOCGIFFLAGS, &ifr) < 0) {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(s);
        ::close(fd);
        return false;
    }
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    if (::ioctl(s, SIOCSIFFLAGS, &ifr) < 0) {
        ec =
            boost::system::error_code(errno, boost::system::generic_category());
        ::close(s);
        ::close(fd);
        return false;
    }
    ::close(s);

    desc_.assign(fd, ec);
    if (!ec) {
        open_ = true;
        mtu_ = static_cast<size_t>(ifr.ifr_mtu);
    }
    return !ec;
#else
    // macOS utun / 其他 POSIX 平台的自主打开尚未实现（Phase 3），
    // 句柄注入模式 assign() 在所有平台可用。
    (void)cfg;
    ec = boost::system::error_code(boost::system::errc::operation_not_supported,
                                   boost::system::generic_category());
    return false;
#endif
}

} // namespace detail

} // namespace tunio

#endif // BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR
