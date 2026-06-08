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

namespace proxy {

    namespace net = boost::asio;

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
}

#endif // INCLUDE__2026_06_08__PROXY_UTIL_HPP
