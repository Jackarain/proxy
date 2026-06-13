//
// proxy_util.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_06_08__PROXY_UTIL_HPP
#define INCLUDE__2026_06_08__PROXY_UTIL_HPP

#include <random>
#include <string>
#include <string_view>

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/error.hpp>

#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>
#include <boost/config.hpp>

#if defined(__linux__) || defined(__APPLE__)
# include <sys/socket.h>
#elif defined(BOOST_WINDOWS)
# include <mstcpip.h>
#endif

namespace proxy {

    namespace net = boost::asio;


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

	// 检测 host 是否是域名或主机名, 如果是域名则返回 true, 否则返回 false.
	inline bool is_hostname(std::string_view host) noexcept
	{
		boost::system::error_code ec;
		net::ip::make_address(host, ec);
		if (ec)
			return true;
		return false;
	}

	// 全局随机数生成器, 用于生成随机数据.
	inline std::random_device& global_random_device() noexcept
	{
		static std::random_device rd;
		return rd;
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
}

#endif // INCLUDE__2026_06_08__PROXY_UTIL_HPP
