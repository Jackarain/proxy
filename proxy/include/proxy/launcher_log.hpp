//
// launcher_log.hpp
// ~~~~~~~~~~~~~~~~
//
// 日志转发到 launcher 控制通道的钩子声明。
//
// logging.hpp 的 logger_tag 自定义点（logger_writer__ 在输出到 console/文件
// 之前调用）经 ADL 在关联命名空间查找 tag_invoke 重载。此钩子必须定义在
// 头文件中：logger_writer__ 为 inline 函数，会在每个调用日志的翻译单元内
// 实例化，只有该翻译单元可见的 tag_invoke 才会被 ADL 采纳；定义在头文件
// 才能保证 proxy_server.cpp / proxy_session.cpp 等所有源文件中的日志都被
// 采集。采集到的日志由 launcher_log.cpp 中实现的队列缓存，控制通道上报时
// 批量发送（RPC notify "log"）。钩子返回 false 不拦截原有输出（launcher
// 管理下 console 已关闭，独立运行不受影响）。
//

#ifndef INCLUDE__PROXY_LAUNCHER_LOG_HPP
#define INCLUDE__PROXY_LAUNCHER_LOG_HPP

#include "proxy/logging.hpp"

#include <deque>
#include <string>

namespace proxy {
namespace detail {
// 日志转发队列入口（实现于 src/launcher_log.cpp）.
void launcher_log_enqueue(int64_t time, const int& level,
	const std::string& message);

// 启用/停用日志采集（launcher 控制通道启用时由 proxy_server 调用）.
void launcher_log_set_enabled(bool enable);

// 取出积压日志（控制通道上报时调用）.
std::deque<std::string> launcher_log_drain();
}
}

// logger_tag 定义于 xlogger 命名空间, 钩子须定义在 xlogger 内经 ADL 发现.
namespace xlogger {
inline bool tag_invoke(logger_tag, int64_t time, const int& level,
	const std::string& message) noexcept
{
	proxy::detail::launcher_log_enqueue(time, level, message);
	return false;
}
}

#endif // INCLUDE__PROXY_LAUNCHER_LOG_HPP
