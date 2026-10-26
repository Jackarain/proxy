
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
#include <algorithm>
#include <charconv>
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

			// 匹配 scheme: 字母开头，后跟字母、数字、+、-、.
			auto scheme_res = boost::urls::grammar::parse(it, end,
				boost::urls::grammar::token_rule(scheme_chars));
			if (!scheme_res)
				return boost::urls::grammar::error::invalid;

			scheme = *scheme_res;
			uint16_t port_num = boost::urls::default_port(boost::urls::string_to_scheme(scheme));

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

			while (authority_end < end &&
				*authority_end != '/' && *authority_end != '?' && *authority_end != '#')
			{
				++authority_end;
			}

			// 确保 authority 部分不为空.
			if (authority_start == authority_end)
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

			// 解析 host 和 port，IPv6 主机必须使用 [...] 形式.
			auto parse_port = [&port, &port_num](std::string_view value) -> bool
			{
				if (value.empty())
					return true;

				uint32_t port_value = 0;
				auto [ptr, ec] = std::from_chars(
					value.data(), value.data() + value.size(), port_value);
				if (ec != std::errc{} ||
					ptr != value.data() + value.size() ||
					port_value == 0 || port_value > 65535)
					return false;

				port_num = static_cast<uint16_t>(port_value);
				return true;
			};

			std::string_view host_port(host_start, authority_end - host_start);
			if (!host_port.empty() && host_port.front() == '[')
			{
				auto close = std::find(host_start, authority_end, ']');
				if (close == authority_end || close == host_start + 1)
					return boost::urls::grammar::error::invalid;

				host = std::string_view(
					host_start + 1, static_cast<std::size_t>(close - host_start - 1));

				auto after_host = close + 1;
				if (after_host != authority_end)
				{
					if (*after_host != ':')
						return boost::urls::grammar::error::invalid;

					port = std::string_view(
						after_host + 1,
						static_cast<std::size_t>(authority_end - after_host - 1));
					if (!parse_port(port))
						return boost::urls::grammar::error::invalid;
				}
			}
			else
			{
				found = host_port.find(':');
				if (found != std::string_view::npos)
				{
					auto colon = host_start + found;
					if (std::find(colon + 1, authority_end, ':') != authority_end)
						return boost::urls::grammar::error::invalid;

					host = host_port.substr(0, found);
					port = host_port.substr(found + 1);
					if (!parse_port(port))
						return boost::urls::grammar::error::invalid;
				}
				else
				{
					host = host_port;
				}
			}

			if (host.empty())
				return boost::urls::grammar::error::invalid;

			// 解析 resource 部分, 包括路径和查询参数, 除了 fragment.
			char const* fragment_delim = std::find(authority_end, end, '#');
			resource = std::string_view(authority_end, fragment_delim - authority_end);

			// 移动迭代器到末尾.
			it = end;

			return url_info{ scheme, user, passwd, host, port_num, resource };
		}
	};

	// 规则实例
	constexpr urlinfo_rule_t urlinfo_rule{};

	// 外部解析函数，匹配您的示例
	inline boost::system::result<url_info> parse_urlinfo(std::string_view s) noexcept
	{
		return boost::urls::grammar::parse(s, urlinfo_rule);
	}

}

#endif // INCLUDE__2025_11_23__URL_INFO_HPP
