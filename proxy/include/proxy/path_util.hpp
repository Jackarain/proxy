//
// path_util.hpp
// ~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_10_13__PATH_UTIL_HPP
#define INCLUDE__2026_10_13__PATH_UTIL_HPP


#include <boost/algorithm/string/replace.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/nowide/convert.hpp>
#include <boost/url.hpp>

#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>
#include <tuple>

#ifdef USE_BOOST_FILESYSTEM
# include <boost/filesystem.hpp>
#else
# include <filesystem>
#endif

#if defined(BOOST_WINDOWS)
# include <windows.h>
#else
# include <sys/stat.h>
#endif


namespace proxy {

	namespace net = boost::asio;

	namespace urls = boost::urls;

#ifdef USE_BOOST_FILESYSTEM
	namespace fs = boost::filesystem;
#else
	namespace fs = std::filesystem;
#endif

	// 将路径转换为 UNC 路径 (Windows 下处理长路径).
	template<typename Path>
	static fs::path make_unc_path(const Path& path) noexcept
	{
#ifndef WIN32
		return path;
#else
		auto ret = path.string();
		if (ret.size() > MAX_PATH)
		{
			boost::replace_all(ret, "/", "\\");
			return "\\\\?\\" + ret;
		}
		return ret;
#endif
	}

	// 获取文件的最后修改时间(Unix 时间戳, 秒)及 UNC 路径.
	// success 表示是否成功获取; returned_path 仅在 Windows 长路径处理时使用.
	inline std::tuple<std::time_t, bool, fs::path>
		file_last_write_time_raw(const fs::path& file) noexcept
	{
		fs::path returned_path; // 仅在 Windows 长路径处理时使用
		std::time_t time_c = 0;
		bool success = false;

#ifdef WIN32
		WIN32_FILE_ATTRIBUTE_DATA file_attr;
		std::wstring wpath = file.wstring();

		// 如果路径超长且没有 UNC 前缀，尝试转换
		if (wpath.size() > MAX_PATH && wpath.compare(0, 4, L"\\\\?\\") != 0)
		{
			returned_path = make_unc_path(file);
			wpath = returned_path.wstring();
		}

		if (GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &file_attr))
		{
			// 将 Windows 的 FILETIME (100纳秒为单位) 转换为 Unix 的 time_t (秒为单位)
			ULARGE_INTEGER ull;
			ull.LowPart = file_attr.ftLastWriteTime.dwLowDateTime;
			ull.HighPart = file_attr.ftLastWriteTime.dwHighDateTime;

			// Windows Epoch 是 1601年1月1日，Unix Epoch 是 1970年1月1日，相差 11644473600 秒
			time_c = static_cast<std::time_t>((ull.QuadPart / 10000000ULL) - 11644473600ULL);
			success = true;
		}
#else
		struct stat file_stat;
		if (::stat(file.c_str(), &file_stat) == 0)
		{
#if defined(__APPLE__)
			time_c = file_stat.st_mtimespec.tv_sec;
#else
			time_c = file_stat.st_mtim.tv_sec; // 现代 Linux 的纳秒级结构体中的秒部分
#endif
			success = true;
		}
#endif

		return { time_c, success, returned_path };
	}

	// 获取文件的最后修改时间和 UNC 路径(本地时间字符串).
	inline std::tuple<std::string, fs::path> file_last_write_time(const fs::path& file) noexcept
	{
		auto [time_c, success, returned_path] = file_last_write_time_raw(file);

		std::string time_string;
		if (success)
		{
			struct tm tm_buf{};
#ifdef WIN32
			localtime_s(&tm_buf, &time_c);
#else
			localtime_r(&time_c, &tm_buf);
#endif

			char tmbuf[64] = { 0 };
			std::strftime(tmbuf, sizeof(tmbuf), "%m-%d-%Y %H:%M", &tm_buf);
			time_string = tmbuf;
		}

		return { time_string, returned_path };
	}

	//////////////////////////////////////////////////////////////////////////
	// HTTP 路径工具

	// 拼接文档目录和目标路径.
	inline fs::path path_cat(
		const std::wstring& doc, const std::wstring& target) noexcept
	{
		size_t start_pos = 0;
		for (auto& c : target)
		{
			if (!(c == L'/' || c == '\\'))
				break;

			start_pos++;
		}

		std::wstring_view sv;
		std::wstring slash = L"/";

		if (start_pos < target.size())
			sv = std::wstring_view(target.c_str() + start_pos);
#ifdef WIN32
		slash = L"\\";
		if (doc.back() == L'/' ||
			doc.back() == L'\\')
			slash = L"";
		return fs::path(doc + slash + std::wstring(sv));
#else
		if (doc.back() == L'/')
			slash = L"";
		return fs::path(
			boost::nowide::narrow(doc + slash + std::wstring(sv)));
#endif // WIN32
	}

	// 从 HTTP 请求目标路径生成文件系统路径字符串.
	inline std::wstring make_target_path(const std::string& target) noexcept
	{
		try
		{
			auto result = urls::url(
				target.starts_with("/") ? target : "/" + target);
			auto path = result.path();

			// HTTP 路径允许携带百分号编码, path() 返回解码后的结果. 含 NUL
			// 的路径会在文件系统接口中被截断, 不能用于文档目录映射.
			if (path.find('\0') != std::string::npos)
				return {};

			auto wide_path = boost::nowide::widen(path);

			// 同时按 '/' 与 '\\' 切分, 防止 Windows 下编码后的反斜杠绕过点段检查.
			std::size_t begin = 0;
			while (begin <= wide_path.size())
			{
				auto end = wide_path.find_first_of(L"/\\", begin);
				auto segment = std::wstring_view(wide_path).substr(begin,
					end == std::wstring::npos ? end : end - begin);
				if (segment == L"..")
					return {};

				if (end == std::wstring::npos)
					break;
				begin = end + 1;
			}

			return wide_path;
		}
		catch (const std::exception&)
		{}

		return {};
	}

	// 从文档根目录和请求目标生成实际文件系统路径.
	inline fs::path make_real_target_path(const std::string& doc_directory,
		const std::string& target) noexcept
	{
		auto target_path = make_target_path(target);
		if (target_path.empty())
			return {};

		auto doc_path = boost::nowide::widen(doc_directory);

#ifdef WIN32
		auto ret = make_unc_path(path_cat(doc_path, target_path));
#else
		auto ret = path_cat(doc_path, target_path).string();
#endif

		return ret;
	}


}


#endif // INCLUDE__2026_10_13__PATH_UTIL_HPP
