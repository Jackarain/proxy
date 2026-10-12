//
// net_socket_option.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_10_13__NET_SOCKET_OPTION_HPP
#define INCLUDE__2026_10_13__NET_SOCKET_OPTION_HPP


#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>

#include <cstdint>
#include <optional>

#if defined(__linux__)
# include <sys/socket.h>
#elif defined(__APPLE__)
# include <sys/socket.h>
# include <netinet/tcp.h>
#elif defined(BOOST_WINDOWS)
# include <mstcpip.h>
#endif


namespace proxy {

	namespace net = boost::asio;

#if defined(__linux__)
#  if !defined(IP_TRANSPARENT)
#    define IP_TRANSPARENT 19
#  endif
#  if !defined(IPV6_TRANSPARENT)
#    define IPV6_TRANSPARENT 75
#  endif
#  if !defined(IP_RECVORIGDSTADDR)
#    define IP_RECVORIGDSTADDR 20
#  endif
#  if !defined(IP_ORIGDSTADDR)
#    define IP_ORIGDSTADDR 20
#  endif
#  if !defined(IPV6_RECVORIGDSTADDR)
#    define IPV6_RECVORIGDSTADDR 74
#  endif
#  if !defined(IPV6_ORIGDSTADDR)
#    define IPV6_ORIGDSTADDR 74
#  endif

	// 设置 IP_TRANSPARENT 和 IPV6_TRANSPARENT 选项.
	using transparent_opt = net::detail::socket_option::boolean<
		IPPROTO_IP, IP_TRANSPARENT>;
	using transparent6_opt = net::detail::socket_option::boolean<
		IPPROTO_IPV6, IPV6_TRANSPARENT>;

#  ifdef ENABLE_REUSEPORT
#    if !defined(SO_REUSEPORT)
#      define SO_REUSEPORT 15
#    endif
	// 设置 SO_REUSEPORT 选项.
	using reuse_port = net::detail::socket_option::boolean<
		SOL_SOCKET, SO_REUSEPORT>;
#endif

#endif // defined(__linux__)

	// 将错误代码设置为系统错误.
	inline void make_error_code(boost::system::error_code& ec, bool err) noexcept
	{
		if (!err)
		{
			ec = {};
		}
		else
		{
	#if defined(BOOST_WINDOWS)
			ec = boost::system::error_code(WSAGetLastError(), net::error::get_system_category());
	#else
			ec = boost::system::error_code(errno, net::error::get_system_category());
	#endif
		}
	}

#if defined(BOOST_WINDOWS)
	// 启用 TCP Keep-Alive 选项.
	inline boost::system::result<bool> set_tcp_keepalive(SOCKET fd) noexcept
	{
		boost::system::error_code ec;
		int ret = 0;

		struct tcp_keepalive alive;
		alive.onoff = 1;
		alive.keepalivetime = 30000;  		// 30 seconds
		alive.keepaliveinterval = 15000; 	// 15 seconds
		ret = ::WSAIoctl(fd, SIO_KEEPALIVE_VALS, &alive, sizeof(alive), NULL, 0, NULL, NULL, NULL);
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;
		return true;
	}
#elif defined(__APPLE__)
	// macOS 使用 TCP_KEEPALIVE 替代 TCP_KEEPIDLE.
	inline boost::system::result<bool> set_tcp_keepalive(int fd) noexcept
	{
		boost::system::error_code ec;
		int ret = 0;
		int keepalive = 1;

		ret = ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		int idle_time = 30;     // 30 seconds
		ret = ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &idle_time, sizeof(idle_time));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		int interval = 15;      // 15 seconds
		ret = ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		int maxpkt = 3;         // 3 probes
		ret = ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(maxpkt));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		return true;
	}
#else
	// 启用 TCP Keep-Alive 选项.
	inline boost::system::result<bool> set_tcp_keepalive(int fd) noexcept
	{
		boost::system::error_code ec;
		int ret = 0;
		int keepalive = 1;

		ret = ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		int idle_time = 30;     // 30 seconds
		ret = ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle_time, sizeof(idle_time));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		int interval = 15;      // 15 seconds
		ret = ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		int maxpkt = 3;         // 3 probes
		ret = ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(maxpkt));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		return true;
	}
#endif

	// 设置 socket 的 mark 标志.
	inline boost::system::result<bool> set_socket_mark(int fd, uint32_t mark) noexcept
	{
		boost::system::error_code ec;
#if defined(__linux__)
		int ret = ::setsockopt(fd, SOL_SOCKET, SO_MARK, &mark, sizeof(uint32_t));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;
#endif
		return true;
	}

	// 在配置了 SO_MARK 时, 对指定 fd 应用 mark 标记.
	// 返回设置结果, 调用方负责记录日志.
	inline boost::system::result<void>
	apply_so_mark(int fd, const std::optional<uint32_t>& so_mark) noexcept
	{
#if defined(__linux__)
		if (so_mark)
		{
			auto r = set_socket_mark(fd, so_mark.value());
			if (r.has_error())
				return r.error();
		}
#endif
		return {};
	}

	// 禁用 Nagle 算法 (TCP_NODELAY), 使小数据包立即发送, 降低延迟.
	inline boost::system::result<bool> set_tcp_nodelay(int fd, bool on = true) noexcept
	{
		boost::system::error_code ec;
		int opt = on ? 1 : 0;
		int ret = ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
			reinterpret_cast<const char*>(&opt), sizeof(opt));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;
		return true;
	}

#if defined(__linux__)
	// 启用 TCP_QUICKACK, 立即发送 ACK 确认收到的数据, 减少延迟.
	// 注意: TCP_QUICKACK 是瞬态选项, Linux 内核在收到数据后可能重置.
	// 适合代理类场景, 因为双向转发会频繁交互.
	inline boost::system::result<bool> set_tcp_quickack(int fd, bool on = true) noexcept
	{
		boost::system::error_code ec;
		int opt = on ? 1 : 0;
		int ret = ::setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK,
			reinterpret_cast<const char*>(&opt), sizeof(opt));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;
		return true;
	}
#endif // defined(__linux__)

	// 增大 TCP 收发缓冲区以提升吞吐量.
	// 注意: 系统全局限制 (/proc/sys/net/core/rmem_max, wmem_max) 有上限.
	inline boost::system::result<bool> set_tcp_buffer_sizes(
		int fd, int recv_size, int send_size) noexcept
	{
		boost::system::error_code ec;
		int ret = 0;

		if (recv_size > 0)
		{
			ret = ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
				reinterpret_cast<const char*>(&recv_size), sizeof(recv_size));
			make_error_code(ec, ret != 0);
			if (ret != 0)
				return ec;
		}

		if (send_size > 0)
		{
			ret = ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
				reinterpret_cast<const char*>(&send_size), sizeof(send_size));
			make_error_code(ec, ret != 0);
			if (ret != 0)
				return ec;
		}

		return true;
	}

}


#endif // INCLUDE__2026_10_13__NET_SOCKET_OPTION_HPP
