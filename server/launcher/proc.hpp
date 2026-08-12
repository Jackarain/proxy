//
// proc.hpp
// ~~~~~~~~
//
// 被管理的 proxy_server 子进程封装（跨平台）：
// 独立进程组启动、stdout/stderr 逐行采集、优雅终止（TERM 后 KILL）、
// 退出监控回调。
//
// 平台实现：
//   - proc_unix.cpp：POSIX（fork/exec/pipe/waitpid + 进程组信号）。
//   - proc_windows.cpp：Windows（CreateProcessW + 匿名管道）。
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef LAUNCHER_PROC_HPP
#define LAUNCHER_PROC_HPP

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "ringbuf.hpp"

// 可移植进程 ID 类型（Windows 为 DWORD，POSIX 为 pid_t）。
#ifdef _WIN32
# include <cstdint>
using process_id = std::uint32_t;
#else
# include <sys/types.h>
using process_id = pid_t;
#endif

namespace launcher {

// 一个被管理的子进程。
struct proc : public std::enable_shared_from_this<proc> {
	process_id pid = 0;
	std::atomic<bool> alive{ false };
	std::atomic<bool> terminated{ false };
	int exit_code = 0;

	std::shared_ptr<ringbuf> logs;
	// 进程退出时调用（监控线程）。
	std::function<void()> on_exit;

	// 内部：管道读取与监控线程。
	std::thread reader_out;
	std::thread reader_err;
	std::thread monitor;

	~proc();
};

// 启动进程，并把 stdout/stderr 逐行写入环形缓冲（stderr 行带 "[stderr] " 前缀）。
// workdir 为空时继承当前目录。on_exit 在进程退出后于监控线程调用。
std::shared_ptr<proc> spawn_proc(const std::string& exe,
	const std::vector<std::string>& args, const std::string& workdir,
	std::shared_ptr<ringbuf> logs, std::function<void()> on_exit);

// 先优雅终止，超时后强制终止，等待进程退出。
void stop_proc(const std::shared_ptr<proc>& p, int timeout_seconds);

// 按 PID 终止进程（先优雅后强制），用于停止孤儿实例。
void stop_pid(process_id pid);

// 判断 PID 对应进程是否存活。
bool process_alive(process_id pid);

// 判断 pid 进程是否以 pidFilePath 作为 --pid_file 参数启动，
// 防止 pid 复用后误杀无关进程。Linux 通过 /proc/<pid>/cmdline 校验，
// 其余平台无法可靠读取命令行，直接信任 pid 文件内容。
bool process_matches_pid_file(process_id pid, const std::string& pid_file_path);

} // namespace launcher

#endif // LAUNCHER_PROC_HPP
