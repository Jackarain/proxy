//
// version.cpp
// ~~~~~~~~~~~
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// launcher 构建版本信息：返回 git commit hash 前 6 位
// （供 /api/version 与 index.html 模板 {{.Version}} 使用）。
//

#ifndef VERSION_GIT
# define VERSION_GIT ""
#endif

#include <string>

namespace launcher {

std::string build_version()
{
	// VERSION_GIT 形如 "abc1234 (2026-08-08 10:00:00)"，取 hash 前 6 位。
	std::string v = VERSION_GIT;
	auto pos = v.find(' ');
	if (pos != std::string::npos)
		v = v.substr(0, pos);
	if (v.size() > 6)
		v = v.substr(0, 6);
	return v;
}

} // namespace launcher
