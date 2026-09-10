//
// jsonrpc.hpp
// ~~~~~~~~~~~~
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// /rpc 控制通道会话：对 plain / TLS 两种 WebSocket 流的
// jsonrpc::jsonrpc_session 做类型擦除。
//

#ifndef LAUNCHER_JSONRPC_HPP
#define LAUNCHER_JSONRPC_HPP

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>
#include <boost/variant2.hpp>

#include <tinyrpc/jsonrpc.hpp>

namespace launcher {

namespace net = boost::asio;
namespace json = boost::json;

// JSON-RPC 标准错误码。
inline constexpr int kCodeMethod = -32601; // 方法不存在
inline constexpr int kCodeServer = -32000; // 应用错误 / 超时

// RPC 调用结果（与 JSON-RPC 响应对应）。
struct rpc_result
{
	json::value result_;
	int error_code_ = 0;
	std::string error_message_;

	bool ok() const;
};

// 控制通道 WebSocket 流：明文 / TLS。
using ws_plain = boost::beast::websocket::stream<net::ip::tcp::socket>;
using ws_tls = boost::beast::websocket::stream<boost::beast::ssl_stream<net::ip::tcp::socket>>;

// 对象字段取值辅助。
inline std::int64_t json_num(const json::object& obj, const char* key)
{
	auto it = obj.find(key);
	if (it == obj.end())
		return 0;
	const auto& v = it->value();
	if (v.is_int64())
		return v.as_int64();
	if (v.is_uint64())
		return static_cast<std::int64_t>(v.as_uint64());
	if (v.is_double())
		return static_cast<std::int64_t>(v.as_double());
	return 0;
}

inline std::string json_str(const json::object& obj, const char* key)
{
	auto it = obj.find(key);
	if (it == obj.end() || !it->value().is_string())
		return {};
	return std::string(it->value().as_string());
}

// 控制通道会话。对 jsonrpc::jsonrpc_session<ws_plain> / <ws_tls> 的类型擦除。
// 拷贝语义共享底层会话句柄（shared_ptr），可按实例 id 在 manager 中查找后复制调用。
class jsonrpc_session
	: public boost::variant2::variant<
		std::shared_ptr<jsonrpc::jsonrpc_session<ws_plain>>,
		std::shared_ptr<jsonrpc::jsonrpc_session<ws_tls>>>
{
public:
	using executor_type = net::any_io_executor;

	// 默认构造：空会话（valid() == false）。
	jsonrpc_session() = default;

	// 用具体类型的会话句柄构造。
	template <class S>
	explicit jsonrpc_session(std::shared_ptr<S> sess)
		: base_type(std::move(sess)) {}
	~jsonrpc_session() = default;

	jsonrpc_session(jsonrpc_session&&) = default;
	jsonrpc_session& operator=(jsonrpc_session&&) = default;
	jsonrpc_session(const jsonrpc_session&) = default;
	jsonrpc_session& operator=(const jsonrpc_session&) = default;

	// 是否持有有效会话。
	bool valid() const;

	void start();

	void stop();

	bool running() const;

	// 发送 JSON-RPC 通知（无 id）。
	void notify(const std::string& method, const json::value& params);

	net::any_io_executor get_executor();

	// 异步 JSON-RPC 调用（协程）。与超时定时器竞争，先完成者胜出；
	// 响应 / 错误 / 超时统一转为 rpc_result，不抛异常。
	// 注意：底层 tinyrpc 的调用不支持取消，不能用 awaitable_operators 的 ||
	// （它要等所有分支完成才返回，RPC 分支挂起会导致整体永久挂起）。
	// 这里改为：RPC 完成时取消定时器立即返回，定时器先触发则返回超时。
	net::awaitable<rpc_result> async_call(const std::string& method,
		const json::value& params, std::chrono::milliseconds timeout);

private:
	using base_type = boost::variant2::variant<
		std::shared_ptr<jsonrpc::jsonrpc_session<ws_plain>>,
		std::shared_ptr<jsonrpc::jsonrpc_session<ws_tls>>>;
};

} // namespace launcher

#endif // LAUNCHER_JSONRPC_HPP
