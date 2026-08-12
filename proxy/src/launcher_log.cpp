//
// launcher_log.cpp
// ~~~~~~~~~~~~~~~~
//
// 日志转发到 launcher 控制通道的队列实现。
//
// 日志经 launcher_log.hpp 中定义的 logger_tag 钩子采集（可能来自多个
// 线程，logger_writer__ 内部已有全局锁，此处再加互斥保证队列安全），
// 由 proxy_server 控制通道上报时调用 launcher_log_drain 批量取出并通过
// RPC notify "log" 发送给 launcher。
//

#include "proxy/launcher_log.hpp"

#include <atomic>
#include <deque>
#include <mutex>

namespace proxy {
namespace detail {

namespace {

// 仅在启用 launcher 控制通道时采集, 避免独立运行时无谓加锁.
std::atomic_bool g_launcher_log_enabled{ false };
std::mutex g_launcher_log_mutex;
std::deque<std::string> g_launcher_log_lines;
inline constexpr std::size_t k_launcher_log_max = 2000;

} // namespace

void launcher_log_set_enabled(bool enable)
{
	g_launcher_log_enabled.store(enable, std::memory_order_relaxed);
}

void launcher_log_enqueue(int64_t time, const int& level,
	const std::string& message)
{
	if (!g_launcher_log_enabled.load(std::memory_order_relaxed))
		return;
	char ts[64] = { 0 };
	xlogger::logger_aux__::time_to_string(ts, time);
	std::string line(ts);
	line += xlogger::logger_level_string__(
		static_cast<xlogger::logger_level__>(level));
	line += message;
	std::lock_guard<std::mutex> lock(g_launcher_log_mutex);
	if (g_launcher_log_lines.size() >= k_launcher_log_max)
		g_launcher_log_lines.pop_front();
	g_launcher_log_lines.push_back(std::move(line));
}

std::deque<std::string> launcher_log_drain()
{
	std::lock_guard<std::mutex> lock(g_launcher_log_mutex);
	std::deque<std::string> out;
	out.swap(g_launcher_log_lines);
	return out;
}

} // namespace detail
} // namespace proxy
