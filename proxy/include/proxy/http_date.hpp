//
// http_date.hpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_10_13__HTTP_DATE_HPP
#define INCLUDE__2026_10_13__HTTP_DATE_HPP


#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <optional>
#include <string>
#include <string_view>


namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// HTTP 缓存校验工具 (Last-Modified / ETag)

	// 将 Unix 时间戳格式化为 HTTP-date (IMF-fixdate), 例如:
	// "Sun, 06 Nov 1994 08:49:37 GMT".
	inline std::string http_date_string(std::time_t time_c) noexcept
	{
		struct tm tm_buf{};
#ifdef WIN32
		gmtime_s(&tm_buf, &time_c);
#else
		gmtime_r(&time_c, &tm_buf);
#endif

		char tmbuf[64] = { 0 };
		std::strftime(tmbuf, sizeof(tmbuf), "%a, %d %b %Y %H:%M:%S GMT", &tm_buf);
		return tmbuf;
	}

	// 根据 RFC 7231 的月名字符串(3 字母)返回月份(1-12), 无法识别返回 0.
	inline int http_month_from_name(const char* name) noexcept
	{
		static constexpr char months[] = "JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";

		for (int i = 0; i < 12; i++)
		{
			if (std::toupper(static_cast<unsigned char>(name[0])) == months[i * 3] &&
				std::toupper(static_cast<unsigned char>(name[1])) == months[i * 3 + 1] &&
				std::toupper(static_cast<unsigned char>(name[2])) == months[i * 3 + 2])
				return i + 1;
		}

		return 0;
	}

	// 以 UTC 时间构造 Unix 时间戳, 字段非法时返回 std::nullopt.
	inline std::optional<std::time_t> make_utc_time(int year, int month, int day,
		int hour, int minute, int second) noexcept
	{
		if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31 ||
			hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
			second < 0 || second > 60)
			return std::nullopt;

		struct tm tm_buf{};
		tm_buf.tm_year = year - 1900;
		tm_buf.tm_mon = month - 1;
		tm_buf.tm_mday = day;
		tm_buf.tm_hour = hour;
		tm_buf.tm_min = minute;
		tm_buf.tm_sec = second;

#ifdef WIN32
		std::time_t time_c = ::_mkgmtime(&tm_buf);
#else
		std::time_t time_c = ::timegm(&tm_buf);
#endif
		if (time_c == static_cast<std::time_t>(-1))
			return std::nullopt;

		return time_c;
	}

	// 解析 HTTP-date 为 Unix 时间戳. 兼容 RFC 1123 (IMF-fixdate),
	// RFC 850 与 asctime 三种历史格式, 解析失败返回 std::nullopt.
	inline std::optional<std::time_t> parse_http_date(std::string_view value)
	{
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
			value.remove_prefix(1);
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
			value.remove_suffix(1);

		if (value.empty())
			return std::nullopt;

		std::string text(value);
		char month_name[4] = { 0 };
		int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;

		// IMF-fixdate: "Sun, 06 Nov 1994 08:49:37 GMT".
		if (std::sscanf(text.c_str(), "%*3s, %d %3s %d %d:%d:%d",
			&day, month_name, &year, &hour, &minute, &second) == 6)
		{
			if ((month = http_month_from_name(month_name)) != 0)
				if (auto time_c = make_utc_time(year, month, day, hour, minute, second))
					return time_c;
		}

		// RFC 850: "Sunday, 06-Nov-94 08:49:37 GMT".
		std::memset(month_name, 0, sizeof(month_name));
		if (std::sscanf(text.c_str(), "%*[^,], %d-%3s-%d %d:%d:%d",
			&day, month_name, &year, &hour, &minute, &second) == 6)
		{
			if (year < 100)
				year += (year < 70) ? 2000 : 1900;
			if ((month = http_month_from_name(month_name)) != 0)
				if (auto time_c = make_utc_time(year, month, day, hour, minute, second))
					return time_c;
		}

		// asctime: "Sun Nov  6 08:49:37 1994".
		{
			char weekday[4] = { 0 };
			std::memset(month_name, 0, sizeof(month_name));
			if (std::sscanf(text.c_str(), "%3s %3s %d %d:%d:%d %d",
				weekday, month_name, &day, &hour, &minute, &second, &year) == 7)
			{
				if ((month = http_month_from_name(month_name)) != 0)
					if (auto time_c = make_utc_time(year, month, day, hour, minute, second))
						return time_c;
			}
		}

		return std::nullopt;
	}

	// 根据文件最后修改时间和大小生成强 ETag, 形如 "<mtime十六进制>-<大小十六进制>".
	inline std::string make_file_etag(std::time_t mtime, std::uintmax_t size)
	{
		char buf[64] = { 0 };
		std::snprintf(buf, sizeof(buf), "\"%llx-%llx\"",
			static_cast<unsigned long long>(mtime),
			static_cast<unsigned long long>(size));
		return buf;
	}

	// 判断 If-None-Match 请求头是否命中给定 ETag. 支持 "*" 通配以及逗号
	// 分隔的列表, 按 RFC 7232 使用弱比较(忽略 W/ 前缀).
	inline bool http_etag_match(std::string_view header, std::string_view etag) noexcept
	{
		size_t pos = 0;
		while (pos < header.size())
		{
			size_t comma = header.find(',', pos);
			std::string_view token = header.substr(pos,
				comma == std::string_view::npos ? std::string_view::npos : comma - pos);

			while (!token.empty() && (token.front() == ' ' || token.front() == '\t'))
				token.remove_prefix(1);
			while (!token.empty() && (token.back() == ' ' || token.back() == '\t'))
				token.remove_suffix(1);

			if (token == "*")
				return true;

			if (token.size() > 2 && token[0] == 'W' && token[1] == '/')
				token.remove_prefix(2);

			if (!token.empty() && token == etag)
				return true;

			if (comma == std::string_view::npos)
				break;
			pos = comma + 1;
		}

		return false;
	}

}


#endif // INCLUDE__2026_10_13__HTTP_DATE_HPP
