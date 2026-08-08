//
// version.hpp
// ~~~~~~~~~~~
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef LAUNCHER_VERSION_HPP
#define LAUNCHER_VERSION_HPP

#include <string>

namespace launcher {

// 返回构建 git commit hash 前 6 位（与 golang buildVersion 行为一致）。
std::string build_version();

} // namespace launcher

#endif // LAUNCHER_VERSION_HPP
