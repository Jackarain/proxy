//
// authority.hpp
// ~~~~~~~~~~~~~
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_10_27__AUTHORITY_HPP
#define INCLUDE__2026_10_27__AUTHORITY_HPP

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace proxy::authority {

	inline constexpr std::size_t max_authority_size = 1024;

	inline bool valid_host_char(char c) noexcept
	{
		auto uc = static_cast<unsigned char>(c);
		return uc > 0x20 && uc < 0x7f && c != '/' && c != '@' &&
			c != '?' && c != '#' && c != '[' && c != ']';
	}

	// 解析 host:port. IPv6 必须使用 [addr]:port 形式.
	inline bool parse_authority(std::string_view value,
		std::string& host, uint16_t& port) noexcept
	{
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
			value.remove_prefix(1);
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
			value.remove_suffix(1);

		if (value.empty() || value.size() > max_authority_size)
			return false;

		std::string_view host_part;
		std::string_view port_part;

		if (value.front() == '[')
		{
			auto close = value.find(']');
			if (close == std::string_view::npos || close <= 1 ||
				close + 2 > value.size() || value[close + 1] != ':')
				return false;

			host_part = value.substr(1, close - 1);
			port_part = value.substr(close + 2);
			if (host_part.find(':') == std::string_view::npos)
				return false;
		}
		else
		{
			auto pos = value.rfind(':');
			if (pos == std::string_view::npos || pos == 0 ||
				value.find(':') != pos)
				return false;

			host_part = value.substr(0, pos);
			port_part = value.substr(pos + 1);
		}

		if (host_part.empty() || port_part.empty())
			return false;
		if (!std::all_of(host_part.begin(), host_part.end(), valid_host_char))
			return false;

		uint32_t port_value = 0;
		auto [ptr, ec] = std::from_chars(
			port_part.data(), port_part.data() + port_part.size(), port_value);
		if (ec != std::errc{} || ptr != port_part.data() + port_part.size() ||
			port_value == 0 || port_value > 65535)
			return false;

		host.assign(host_part);
		port = static_cast<uint16_t>(port_value);
		return true;
	}

	inline std::string format_authority(std::string_view host, uint16_t port)
	{
		std::string result;
		result.reserve(host.size() + 16);
		if (host.find(':') != std::string_view::npos)
		{
			result.push_back('[');
			result.append(host);
			result.push_back(']');
		}
		else
		{
			result.append(host);
		}
		result.push_back(':');
		result.append(std::to_string(port));
		return result;
	}

} // namespace proxy::authority

#endif // INCLUDE__2026_10_27__AUTHORITY_HPP
