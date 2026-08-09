//
// datetime.hpp
// ~~~~~~~~~~~~
//
// RFC3339 时间格式化/解析，与 golang time.Time 的 JSON 序列化格式兼容
// （YYYY-MM-DDTHH:MM:SS±HH:MM，支持解析小数秒与 Z 后缀）。
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef LAUNCHER_DATETIME_HPP
#define LAUNCHER_DATETIME_HPP

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

namespace launcher {

using time_point = std::chrono::system_clock::time_point;

// time.Time 零值（0001-01-01T00:00:00Z）。
inline time_point zero_time() {
	return time_point{};
}

// 当前时间。
inline time_point now_time() {
	return std::chrono::system_clock::now();
}

// 按本地时区格式化为 RFC3339。
inline std::string rfc3339_format(time_point tp) {
	if (tp == zero_time())
		return "0001-01-01T00:00:00Z";
	auto tt = std::chrono::system_clock::to_time_t(tp);
	std::tm tm{};
#ifdef _WIN32
	localtime_s(&tm, &tt);
	long off = 0;
#else
	localtime_r(&tt, &tm);
	long off = tm.tm_gmtoff;
#endif
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec,
		off < 0 ? '-' : '+', static_cast<int>(std::abs(off) / 3600),
		static_cast<int>((std::abs(off) % 3600) / 60));
	return buf;
}

// 解析 RFC3339（含小数秒与 Z/±HH:MM 时区），转换为绝对时间点。
inline bool rfc3339_parse(const std::string& s, time_point& out) {
	int y = 0, mo = 0, d = 0, h = 0, mi = 0, sec = 0;
	double frac = 0;
	char tz = 'Z';
	int tzh = 0, tzm = 0;

	const char* p = s.c_str();
	if (std::sscanf(p, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) != 6)
		return false;
	// 定位秒之后的位置。
	const char* sp = std::strchr(p, 'T');
	if (!sp)
		return false;
	const char* q = sp + 1;
	// 跳过 HH:MM:SS。
	for (int i = 0; i < 2 && (q = std::strchr(q, ':')); i++)
		q++;
	if (!q)
		return false;
	q++; // 越过最后一个 ':'

	// 秒。
	if (*q == '.') {
		// 小数秒。
		char* end = nullptr;
		frac = std::strtod(q, &end);
		if (end == q)
			return false;
		q = end;
	}
	if (*q == 'Z' || *q == 'z') {
		tz = 'Z';
		q++;
	} else if (*q == '+' || *q == '-') {
		tz = *q;
		q++;
		if (std::sscanf(q, "%d:%d", &tzh, &tzm) != 2)
			return false;
	} else {
		return false;
	}
	// 解析完剩余部分必须为空。
	while (*q)
		if (*q != ' ' && *q != '\r' && *q != '\n')
			return false;
		else
			q++;

	// 构造时间点（UTC）。
	auto days = std::chrono::days(0);
	std::tm utc{};
	utc.tm_year = y - 1900;
	utc.tm_mon = mo - 1;
	utc.tm_mday = d;
	utc.tm_hour = h;
	utc.tm_min = mi;
	utc.tm_sec = sec;
	utc.tm_isdst = 0;
	// 使用 days_from_civil 式换算：直接用 timegm（Linux）。
#ifdef _WIN32
	std::time_t tt = _mkgmtime(&utc);
#else
	std::time_t tt = ::timegm(&utc);
#endif
	if (tt == static_cast<std::time_t>(-1))
		return false;
	auto tp = std::chrono::system_clock::from_time_t(tt);
	// 加上小数秒。
	tp += std::chrono::duration_cast<time_point::duration>(
		std::chrono::duration<double>(frac));
	// 减去时区偏移（本地时间 → UTC）。
	int offset_sec = (tz == 'Z') ? 0 : (tzh * 3600 + tzm * 60) * (tz == '-' ? -1 : 1);
	tp -= std::chrono::seconds(offset_sec);
	out = tp;
	return true;
}

} // namespace launcher

#endif // LAUNCHER_DATETIME_HPP
