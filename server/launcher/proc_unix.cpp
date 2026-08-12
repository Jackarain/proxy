//
// proc_unix.cpp
// ~~~~~~~~~~~~~
//
// POSIX 平台的子进程管理实现：fork/exec/pipe/waitpid + 进程组信号。
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// musl 下 id_t 等类型仅在 _GNU_SOURCE/_BSD_SOURCE 时可见；
// 严格 ISO 模式（-std=c++23）不会自动定义，需在此显式声明。
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "proc.hpp"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace launcher {

namespace {

// 从 fd 逐行读取到环形缓冲。prefix 为行前缀。
void pump_fd(int fd, const std::shared_ptr<ringbuf>& logs, const std::string& prefix)
{
	char buf[8192];
	std::string pending;
	for (;;) {
		ssize_t n = ::read(fd, buf, sizeof(buf));
		if (n <= 0)
			break;
		pending.append(buf, static_cast<std::size_t>(n));
		std::size_t pos;
		while ((pos = pending.find('\n')) != std::string::npos) {
			std::string line = pending.substr(0, pos);
			pending.erase(0, pos + 1);
			// 去掉行尾 \r。
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			logs->add(prefix + line);
		}
	}
	if (!pending.empty())
		logs->add(prefix + pending);
	::close(fd);
}

} // namespace

proc::~proc()
{
	// 析构可能发生在自身线程内（监控/读取线程持有 proc 的最后引用）：
	// 不能 join 当前线程（EDEADLK），此时 detach 让线程自然结束。
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
	int out_pipe[2];
	int err_pipe[2];
	if (::pipe(out_pipe) != 0 || ::pipe(err_pipe) != 0)
		return nullptr;

	pid_t pid = ::fork();
	if (pid < 0) {
		::close(out_pipe[0]);
		::close(out_pipe[1]);
		::close(err_pipe[0]);
		::close(err_pipe[1]);
		return nullptr;
	}

	if (pid == 0) {
		// 子进程：独立进程组，便于整组终止（防止残留孙进程）。
		::setpgid(0, 0);
		if (!workdir.empty())
			::chdir(workdir.c_str());
		::dup2(out_pipe[1], STDOUT_FILENO);
		::dup2(err_pipe[1], STDERR_FILENO);
		::close(out_pipe[0]);
		::close(out_pipe[1]);
		::close(err_pipe[0]);
		::close(err_pipe[1]);

		// 对继承自父进程（launcher）的所有 fd 设置 FD_CLOEXEC，exec 时自动关闭。
		// 否则 launcher 的监听 socket / 连接 socket 会被 proxy_server 继承，
		// launcher 意外退出（如 kill -9）后残留进程仍占用 launcher 的监听端口，
		// 导致 launcher 无法再次启动。
		const int maxfd = static_cast<int>(::sysconf(_SC_OPEN_MAX));
		for (int fd = STDERR_FILENO + 1; fd < maxfd; fd++) {
			const int flags = ::fcntl(fd, F_GETFD);
			if (flags != -1 && !(flags & FD_CLOEXEC))
				::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
		}

		// 构建 exec 参数。
		std::vector<char*> argv;
		argv.reserve(args.size() + 2);
		argv.push_back(const_cast<char*>(exe.c_str()));
		for (const auto& a : args)
			argv.push_back(const_cast<char*>(a.c_str()));
		argv.push_back(nullptr);
		::execv(exe.c_str(), argv.data());
		// exec 失败。
		::_exit(127);
	}

	// 父进程。
	::close(out_pipe[1]);
	::close(err_pipe[1]);

	auto p = std::make_shared<proc>();
	p->pid_ = pid;
	p->alive_ = true;
	p->logs_ = logs;
	p->on_exit_ = std::move(on_exit);

	int out_fd = out_pipe[0];
	int err_fd = err_pipe[0];

	// 监控线程：等待进程退出。
	p->monitor_ = std::thread([p, pid]() {
		int status = 0;
		::waitpid(pid, &status, 0);
		p->exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status)
			: (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 0);
		p->alive_ = false;
		p->terminated_ = true;
		if (p->on_exit_)
			p->on_exit_();
	});

	// 输出采集线程。
	p->reader_out_ = std::thread([p, out_fd]() { pump_fd(out_fd, p->logs_, ""); });
	p->reader_err_ = std::thread([p, err_fd]() { pump_fd(err_fd, p->logs_, "[stderr] "); });

	return p;
}

void stop_proc(const std::shared_ptr<proc>& p, int timeout_seconds)
{
	if (!p || !p->alive_)
		return;
	// 向进程组发送 SIGTERM。
	::kill(-p->pid_, SIGTERM);
	// 等待退出。
	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
	while (p->alive_ && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	if (p->alive_) {
		// 超时后 SIGKILL。
		::kill(-p->pid_, SIGKILL);
		deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
		while (p->alive_ && std::chrono::steady_clock::now() < deadline)
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}

void stop_pid(process_id pid)
{
	if (pid <= 0)
		return;
	::kill(pid, SIGTERM);
	for (int i = 0; i < 50; i++) {
		if (!process_alive(pid))
			return;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	::kill(pid, SIGKILL);
}

bool process_alive(process_id pid)
{
	if (pid <= 0)
		return false;
	int ret = ::kill(pid, 0);
	return ret == 0 || errno == EPERM;
}

bool process_matches_pid_file(process_id pid, const std::string& pid_file_path)
{
	std::string cmdline_path = "/proc/" + std::to_string(pid) + "/cmdline";
	int fd = ::open(cmdline_path.c_str(), O_RDONLY);
	if (fd < 0)
		return false;
	std::string data;
	char buf[4096];
	ssize_t n;
	while ((n = ::read(fd, buf, sizeof(buf))) > 0)
		data.append(buf, static_cast<std::size_t>(n));
	::close(fd);
	return data.find(pid_file_path) != std::string::npos;
}

} // namespace launcher
