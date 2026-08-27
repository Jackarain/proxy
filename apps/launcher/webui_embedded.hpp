//
// webui_embedded.hpp
// ~~~~~~~~~~~~~~~~~~
//
// WebUI 静态资源内嵌查找接口。资源由构建期脚本 embed_webui.cmake 生成的
// webui_embedded.cpp 提供（静态资源直接编入可执行文件），
// 使 launcher 单文件即可提供完整 WebUI，无需在可执行文件旁放置 webui 目录。
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef LAUNCHER_WEBUI_EMBEDDED_HPP
#define LAUNCHER_WEBUI_EMBEDDED_HPP

#include <cstddef>
#include <string>

namespace launcher {

// 一个内嵌文件。
struct embedded_file {
	const unsigned char* data;
	std::size_t size;
};

// 按相对路径查找内嵌 WebUI 资源（如 "index.html"、"assets/app.js"）。
// 未找到返回 nullptr。
const embedded_file* find_embedded_file(const std::string& path);

} // namespace launcher

#endif // LAUNCHER_WEBUI_EMBEDDED_HPP
