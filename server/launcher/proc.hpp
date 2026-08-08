//
// proc.hpp
// ~~~~~~~~
//
// 被管理的 proxy_server 子进程封装：
// 独立进程组启动、stdout/stderr 逐行采集、SIGTERM/SIGKILL 优雅终止、
// 退出监控回调。与 golang 版本 internal/launcher/proc*.go 行为一致。
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

namespace launcher {

// 一个被管理的子进程。
struct proc : public std::enable_shared_from_this<proc> {
	pid_t pid = -1;
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

// 先 SIGTERM，超时后 SIGKILL，等待进程退出。
void stop_proc(const std::shared_ptr<proc>& p, int timeout_seconds);

// 按 PID 终止进程（SIGTERM 后超时 SIGKILL），用于停止孤儿实例。
void stop_pid(pid_t pid);

// 判断 PID 对应进程是否存活。
bool process_alive(pid_t pid);

// 判断 pid 进程是否以 pidFilePath 作为 --pid_file 参数启动，
// 防止 pid 复用后误杀无关进程。Linux 通过 /proc/<pid>/cmdline 校验。
bool process_matches_pid_file(pid_t pid, const std::string& pid_file_path);

} // namespace launcher

#endif // LAUNCHER_PROC_HPP
