//
// wintun_install.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/wintun_install.hpp"

#if defined(_WIN32)

#include "proxy/logging.hpp"

#include <windows.h>

#include <filesystem>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace proxy {

	namespace {

		// 检查 wintun 驱动是否已安装 (枚举网络适配器 ComponentId).
		bool wintun_driver_installed() noexcept
		{
			const char* adapter_key = "SYSTEM\\CurrentControlSet\\Control\\"
				"Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}";

			HKEY key;
			if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, adapter_key, 0,
					KEY_READ, &key) != ERROR_SUCCESS)
				return false;

			bool installed = false;
			for (int i = 0;; i++)
			{
				char enum_name[256] = { 0 };
				DWORD len = static_cast<DWORD>(sizeof(enum_name));
				if (RegEnumKeyExA(key, i, enum_name, &len, nullptr,
						nullptr, nullptr, nullptr) != ERROR_SUCCESS)
					break;

				std::string unit = std::string(adapter_key) + "\\" + enum_name;
				HKEY unit_key;
				if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, unit.data(), 0,
						KEY_READ, &unit_key) != ERROR_SUCCESS)
					continue;

				char component_id[256] = { 0 };
				DWORD type = 0;
				len = static_cast<DWORD>(sizeof(component_id));
				LONG status = RegQueryValueExA(unit_key, "ComponentId",
					nullptr, &type, reinterpret_cast<LPBYTE>(component_id),
					&len);
				RegCloseKey(unit_key);
				if (status != ERROR_SUCCESS)
					continue;

				std::string comp_id(component_id);
				std::transform(comp_id.begin(), comp_id.end(),
					comp_id.begin(), [](unsigned char c)
					{
						return static_cast<char>(std::tolower(c));
					});
				if (comp_id.find("wintun") != std::string::npos)
				{
					installed = true;
					break;
				}
			}
			RegCloseKey(key);
			return installed;
		}

		// 将 exe 内嵌资源解压到文件.
		bool resource_copy_to_file(const char* name,
			const std::filesystem::path& dest) noexcept
		{
			HRSRC res = FindResourceA(nullptr, name,
				reinterpret_cast<LPCSTR>(RT_RCDATA));
			if (!res)
				return false;
			HGLOBAL data = LoadResource(nullptr, res);
			if (!data)
				return false;
			void* ptr = LockResource(data);
			DWORD size = SizeofResource(nullptr, res);
			if (!ptr || size == 0)
				return false;

			HANDLE file = CreateFileW(dest.c_str(), GENERIC_WRITE, 0, nullptr,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE)
				return false;

			DWORD written = 0;
			BOOL ok = WriteFile(file, ptr, size, &written, nullptr);
			CloseHandle(file);
			return ok && written == size;
		}

		// 执行命令并等待结束, 返回退出码是否为 0.
		bool run_command(const std::wstring& cmdline) noexcept
		{
			STARTUPINFOW si;
			std::memset(&si, 0, sizeof(si));
			si.cb = sizeof(si);
			PROCESS_INFORMATION pi;
			std::memset(&pi, 0, sizeof(pi));

			std::wstring cmd = cmdline;
			if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
					CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
				return false;

			WaitForSingleObject(pi.hProcess, INFINITE);
			DWORD code = 0;
			GetExitCodeProcess(pi.hProcess, &code);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			return code == 0;
		}
	}

	bool ensure_wintun_driver() noexcept
	{
		// 已安装 wintun 驱动则直接返回.
		if (wintun_driver_installed())
			return true;

		// 解压驱动文件到临时目录.
		std::error_code ec;
		auto tmp = std::filesystem::temp_directory_path(ec);
		if (ec)
		{
			XLOG_ERR << "ensure wintun driver, temp path: " << ec.message();
			return false;
		}
		tmp /= std::to_string(GetCurrentProcessId());
		std::filesystem::create_directories(tmp, ec);
		if (ec)
		{
			XLOG_ERR << "ensure wintun driver, create dir: " << ec.message();
			return false;
		}

		auto sys_path = tmp / "wintun.sys";
		auto inf_path = tmp / "wintun.inf";
		auto cat_path = tmp / "wintun.cat";
		if (!resource_copy_to_file("wintun.sys", sys_path) ||
			!resource_copy_to_file("wintun.inf", inf_path) ||
			!resource_copy_to_file("wintun.cat", cat_path))
		{
			XLOG_ERR << "ensure wintun driver, extract resource failed";
			std::filesystem::remove_all(tmp, ec);
			return false;
		}

		// 使用 pnputil 安装驱动.
		wchar_t win_dir[MAX_PATH] = { 0 };
		GetWindowsDirectoryW(win_dir, MAX_PATH);
		std::wstring cmd = L"\"" + std::wstring(win_dir) +
			L"\\System32\\pnputil.exe\" /add-driver \"" +
			inf_path.wstring() + L"\"";
		bool ok = run_command(cmd);

		std::filesystem::remove_all(tmp, ec);
		if (!ok)
			XLOG_ERR << "ensure wintun driver, pnputil add-driver failed";
		else
			XLOG_INFO << "wintun driver installed";
		return ok;
	}

} // namespace proxy

#endif // _WIN32
