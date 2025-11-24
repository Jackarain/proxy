
//
// url_info.hpp
// ~~~~~~~~~~~~
//
// Copyright (c) 2025 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2025_11_23__URL_INFO_HPP
#define INCLUDE__2025_11_23__URL_INFO_HPP

#include <boost/url.hpp>
#include <tuple>
#include <string_view>

namespace proxy {

	constexpr auto alpha = boost::urls::grammar::alpha_chars;
	constexpr auto digit = boost::urls::grammar::digit_chars;
	constexpr auto scheme_chars = boost::urls::grammar::lut_chars("+-.") + alpha + digit;

	// scheme, user, passwd, host, port, resource
	using url_info = std::tuple<
		std::string_view, // scheme
		std::string_view, // user
		std::string_view, // passwd
		std::string_view, // host
		uint16_t,         // port
		std::string_view  // resource (including the leading '/')
	>;

	struct urlinfo_rule_t
	{
		using value_type = url_info;

		boost::system::result<value_type> parse(char const*& it, char const* const end) const noexcept
		{
			std::string_view scheme, user, passwd, host, port, resource;
			char const* start = it;

			// 匹配 scheme: 字母开头，后跟字母、数字、+、-、.
			auto scheme_res = boost::urls::grammar::parse(it, end,
				boost::urls::grammar::token_rule(scheme_chars));
			if (!scheme_res)
				return boost::urls::grammar::error::invalid;

			scheme = *scheme_res;

			// 匹配 "://"
			auto r = boost::urls::grammar::parse(it, end,
				boost::urls::grammar::literal_rule("://"));
			if (!r)
				return boost::urls::grammar::error::invalid;

			// 尝试解析 authority 部分，包括可选的用户信息和host和port信息.
			// authority = [ userinfo "@" ] host [ ":" port ]
			// authority 部分结束于第一个 '/' 或字符串结尾, 因此我们需要首先找到这个位置.
			// 计算 authority 部分的结束位置.
			char const* authority_start = it;
			char const* authority_end = it;
			while (authority_end < end && *authority_end != '/')
				++authority_end;

			// 确保 authority 部分不为空.
			if (authority_end == authority_start)
				return boost::urls::grammar::error::invalid;

			std::string_view authority(authority_start, authority_end - authority_start);

			// 解析 authority 部分, 如果有用户信息(包含’@‘则表示有用户信息), 则提取用户和密码.
			char const* host_start = authority_start;
			auto found = authority.find_first_of("@");
			if (found != std::string_view::npos)
			{
				host_start = authority_start + found + 1;

				char const* user_start = authority_start;
				char const* user_end = authority_start + found;
				user = std::string_view(user_start, user_end - user_start);

				found = user.find_first_of(":");
				if (found != std::string_view::npos)
				{
					char const* passwd_start = user_start + found + 1;
					char const* passwd_end = user_end;

					passwd = std::string_view(passwd_start, passwd_end - passwd_start);
					user_end = user_start + found;

					user = std::string_view(user_start, user_end - user_start);
				}
			}

			// 解析 host 和 port
			char const* host_end = authority_end;
			host = std::string_view(host_start, host_end - host_start);

			found = host.find_first_of(":");
			if (found != std::string_view::npos)
			{
				char const* port_start = host_start + found + 1;
				char const* port_end = host_end;

				port = std::string_view(port_start, port_end - port_start);
				host_end = host_start + found;

				host = std::string_view(host_start, host_end - host_start);
			}

			resource = std::string_view(authority_end, end - authority_end);

			// 移动迭代器到末尾.
			it = end;

			uint16_t port_num = boost::urls::default_port(boost::urls::string_to_scheme(scheme));
			if (!port.empty())
			{
				try {
					port_num = static_cast<uint16_t>(std::stoul(std::string(port)));
				}
				catch (...) {
					return boost::urls::grammar::error::invalid;
				}
			}

			return url_info{ scheme, user, passwd, host, port_num, resource };
		}
	};

	// 规则实例
	constexpr urlinfo_rule_t urlinfo_rule{};

	// 外部解析函数，匹配您的示例
	boost::system::result<url_info> parse_urlinfo(std::string_view s) noexcept
	{
		return boost::urls::grammar::parse(s, urlinfo_rule);
	};

}

#endif // INCLUDE__2025_11_23__URL_INFO_HPP
