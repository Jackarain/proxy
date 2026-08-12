//
// proc_windows.cpp
// ~~~~~~~~~~~~~~~~
//
// Windows 平台的子进程管理实现：CreateProcessW + 匿名管道 +
// TerminateProcess（Windows 无 SIGTERM/进程组语义，直接强制终止）。
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proc.hpp"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

namespace launcher {

namespace {

// UTF-8 → UTF-16（CreateProcessW 需要宽字符路径/命令行）。
std::wstring utf8_to_wide(const std::string& s)
{
	if (s.empty())
		return {};
	int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
	if (len <= 0)
		return {};
	std::wstring out(static_cast<std::size_t>(len), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), len);
	return out;
}

// 单个命令行参数加引号（遵循 CommandLineToArgvW 的解析规则）：
// 含空格/Tab/引号或为空时用双引号包裹，引号前反斜杠翻倍。
std::wstring quote_arg(const std::string& s)
{
	bool need_quote = s.empty();
	for (char c : s) {
		if (c == ' ' || c == '\t' || c == '"') {
			need_quote = true;
			break;
		}
	}
	if (!need_quote)
		return utf8_to_wide(s);
	std::wstring out = L"\"";
	int backslashes = 0;
	for (char c : s) {
		if (c == '\\') {
			backslashes++;
			continue;
		}
		if (c == '"') {
			out.append(static_cast<std::size_t>(backslashes) * 2, L'\\');
			backslashes = 0;
			out += L"\\\"";
			continue;
		}
		out.append(static_cast<std::size_t>(backslashes), L'\\');
		backslashes = 0;
		out += static_cast<wchar_t>(static_cast<unsigned char>(c));
	}
	out.append(static_cast<std::size_t>(backslashes) * 2, L'\\');
	out += L"\"";
	return out;
}

// 从管道句柄逐行读取到环形缓冲。prefix 为行前缀。
void pump_handle(HANDLE h, const std::shared_ptr<ringbuf>& logs, const std::string& prefix)
{
	char buf[8192];
	std::string pending;
	for (;;) {
		DWORD n = 0;
		if (!ReadFile(h, buf, sizeof(buf), &n, nullptr) || n == 0)
			break;
		pending.append(buf, static_cast<std::size_t>(n));
		std::size_t pos;
		while ((pos = pending.find('\n')) != std::string::npos) {
			std::string line = pending.substr(0, pos);
			pending.erase(0, pos + 1);
			// 去掉行尾 \r（Windows 换行）。
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			logs->add(prefix + line);
		}
	}
	if (!pending.empty())
		logs->add(prefix + pending);
	CloseHandle(h);
}

} // namespace

proc::~proc()
{
	// 析构可能发生在自身线程内（监控/读取线程持有 proc 的最后引用）：
	// 不能 join 当前线程（死锁），此时 detach 让线程自然结束。
	auto this_id = std::this_thread::get_id();
	if (reader_out_.joinable() && reader_out_.get_id() != this_id)
		reader_out_.join();
	else if (reader_out_.joinable())
		reader_out_.detach();
	if (reader_err_.joinable() && reader_err_.get_id() != this_id)
		reader_err_.join();
	else if (reader_err_.joinable())
		reader_err_.detach();
	if (monitor_.joinable() && monitor_.get_id() != this_id)
		monitor_.join();
	else if (monitor_.joinable())
		monitor_.detach();
}

std::shared_ptr<proc> spawn_proc(const std::string& exe,
	const std::vector<std::string>& args, const std::string& workdir,
	std::shared_ptr<ringbuf> logs, std::function<void()> on_exit)
{
	// 可继承句柄的匿名管道（子进程 stdout/stderr）。
	SECURITY_ATTRIBUTES sa{ sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
	HANDLE out_read = nullptr, out_write = nullptr;
	HANDLE err_read = nullptr, err_write = nullptr;
	if (!CreatePipe(&out_read, &out_write, &sa, 0))
		return nullptr;
	if (!CreatePipe(&err_read, &err_write, &sa, 0)) {
		CloseHandle(out_read);
		CloseHandle(out_write);
		return nullptr;
	}
	// 读端仅在父进程使用，去掉继承标志（避免句柄泄漏到子进程）。
	SetHandleInformation(out_read, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(err_read, HANDLE_FLAG_INHERIT, 0);

	// 命令行：exe + 各参数（带引号）。
	std::wstring cmdline = quote_arg(exe);
	for (const auto& a : args) {
		cmdline += L' ';
		cmdline += quote_arg(a);
	}

	STARTUPINFOW si{};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput = out_write;
	si.hStdError = err_write;

	PROCESS_INFORMATION pi{};
	std::wstring exe_w = utf8_to_wide(exe);
	std::wstring workdir_w;
	if (!workdir.empty())
		workdir_w = utf8_to_wide(workdir);
	// CREATE_NO_WINDOW：不弹出控制台窗口（子进程 std 已重定向）。
	BOOL ok = CreateProcessW(
		exe_w.c_str(),            // lpApplicationName
		cmdline.data(),           // lpCommandLine（可被修改）
		nullptr, nullptr,         // 不限制句柄继承
		TRUE,                     // bInheritHandles：子进程继承 std 句柄
		CREATE_NO_WINDOW,         // dwCreationFlags
		nullptr,                  // 继承父进程环境
		workdir.empty() ? nullptr : workdir_w.c_str(),
		&si, &pi);
	if (!ok) {
		CloseHandle(out_read);
		CloseHandle(out_write);
		CloseHandle(err_read);
		CloseHandle(err_write);
		return nullptr;
	}
	// 父进程关闭写端（子进程退出后读端读到 EOF）。
	CloseHandle(out_write);
	CloseHandle(err_write);

	auto p = std::make_shared<proc>();
	p->pid_ = pi.dwProcessId;
	p->alive_ = true;
	p->logs_ = logs;
	p->on_exit_ = std::move(on_exit);

	// 监控线程：等待进程退出并上报退出码。
	HANDLE hprocess = pi.hProcess;
	p->monitor_ = std::thread([p, hprocess]() {
		WaitForSingleObject(hprocess, INFINITE);
		DWORD code = 0;
		GetExitCodeProcess(hprocess, &code);
		p->exit_code_ = static_cast<int>(code);
		CloseHandle(hprocess);
		p->alive_ = false;
		p->terminated_ = true;
		if (p->on_exit_)
			p->on_exit_();
	});

	// 输出采集线程。
	p->reader_out_ = std::thread([p, h = out_read]() { pump_handle(h, p->logs_, ""); });
	p->reader_err_ = std::thread([p, h = err_read]() { pump_handle(h, p->logs_, "[stderr] "); });

	return p;
}

void stop_proc(const std::shared_ptr<proc>& p, int timeout_seconds)
{
	if (!p || !p->alive_)
		return;
	// Windows 无 SIGTERM，直接 TerminateProcess。
	// 优雅退出由 manager 先经 RPC shutdown 完成，这里仅兜底强制终止。
	HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, p->pid_);
	if (h) {
		TerminateProcess(h, 1);
		CloseHandle(h);
	}
	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
	while (p->alive_ && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void stop_pid(process_id pid) {
	if (pid == 0)
		return;
	HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
	if (h) {
		TerminateProcess(h, 1);
		CloseHandle(h);
	}
}

bool process_alive(process_id pid) {
	if (pid == 0)
		return false;
	HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!h) {
		// ACCESS_DENIED 通常表示进程存在但当前权限不足，视为存活。
		return GetLastError() == ERROR_ACCESS_DENIED;
	}
	DWORD code = 0;
	bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
	CloseHandle(h);
	return alive;
}

bool process_matches_pid_file(process_id /*pid*/, const std::string& /*pid_file_path*/) {
	// Windows 无法可靠读取命令行，直接信任 pid 文件内容。
	return true;
}

} // namespace launcher
