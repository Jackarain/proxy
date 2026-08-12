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

	bool ok() const { return error_code_ == 0; }
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
	using base_type = boost::variant2::variant<
		std::shared_ptr<jsonrpc::jsonrpc_session<ws_plain>>,
		std::shared_ptr<jsonrpc::jsonrpc_session<ws_tls>>>;

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
	bool valid() const
	{
		return boost::variant2::visit(
			[](const auto& sp) { return sp != nullptr; }, *this);
	}

	void start()
	{
		boost::variant2::visit([](auto& sp) { if (sp) sp->start(); }, *this);
	}

	void stop()
	{
		boost::variant2::visit([](auto& sp) { if (sp) sp->stop(); }, *this);
	}

	bool running() const
	{
		return boost::variant2::visit(
			[](const auto& sp) { return sp != nullptr && sp->running(); }, *this);
	}

	// 发送 JSON-RPC 通知（无 id）。
	void notify(const std::string& method, const json::value& params)
	{
		boost::variant2::visit([&](auto& sp) {
			if (sp) sp->notify(method, params);
		}, *this);
	}

	net::any_io_executor get_executor()
	{
		return boost::variant2::visit([](auto& sp) { return sp->get_executor(); }, *this);
	}

	// 异步 JSON-RPC 调用（协程）。与超时定时器竞争，先完成者胜出；
	// 响应 / 错误 / 超时统一转为 rpc_result，不抛异常。
	// 注意：底层 tinyrpc 的调用不支持取消，不能用 awaitable_operators 的 ||
	// （它要等所有分支完成才返回，RPC 分支挂起会导致整体永久挂起）。
	// 这里改为：RPC 完成时取消定时器立即返回，定时器先触发则返回超时。
	net::awaitable<rpc_result> async_call(const std::string& method,
		const json::value& params, std::chrono::milliseconds timeout)
	{
		auto ex = co_await net::this_coro::executor;
		// 持有会话拷贝，保证后台 RPC 协程执行期间会话对象存活。
		auto sess = *this;

		struct race_state
		{
			bool done_ = false;
			boost::system::error_code ec_;
			json::object resp_;
		};
		auto st = std::make_shared<race_state>();
		net::steady_timer timer(ex, timeout);
		auto cancel_sig = std::make_shared<net::cancellation_signal>();

		// 后台 RPC 分支：完成后记录结果并取消定时器，唤醒等待的调用者。
		net::co_spawn(ex,
			[sess = std::move(sess), st, method, params, cancel_sig]() mutable -> net::awaitable<void>
			{
				boost::system::error_code ec;
				json::object resp;
				try {
					resp = co_await boost::variant2::visit(
						[&](auto& sp) -> net::awaitable<json::object> {
							return sp->async_call(method, params,
								net::redirect_error(net::use_awaitable, ec));
						}, sess);
				} catch (...) {
					ec = boost::asio::error::operation_aborted;
				}
				// 在 io_context 上串行记录结果（单线程，与超时判断互斥）。
				net::dispatch(co_await net::this_coro::executor,
					[st, resp = std::move(resp), ec, cancel_sig]() mutable {
						if (!st->done_) {
							st->done_ = true;
							st->ec_ = ec;
							st->resp_ = std::move(resp);
							cancel_sig->emit(net::cancellation_type::all);
						}
					});
			}, net::detached);

		// 等待定时器；RPC 完成时会取消定时器。
		auto slot = cancel_sig->slot();
		slot.assign([&timer](net::cancellation_type_t) { timer.cancel(); });
		boost::system::error_code tec;
		co_await timer.async_wait(net::redirect_error(net::use_awaitable, tec));
		slot.clear();

		if (!st->done_) {
			// 定时器先触发：超时。
			rpc_result res;
			res.error_code_ = kCodeServer;
			res.error_message_ = "rpc call timeout";
			co_return res;
		}

		rpc_result res;
		if (st->ec_) {
			res.error_code_ = kCodeServer;
			res.error_message_ = st->ec_.message();
			co_return res;
		}
		const auto& resp = st->resp_;
		if (auto e = resp.if_contains("error"); e && e->is_object()) {
			const auto& eo = e->as_object();
			res.error_code_ = json_num(eo, "code");
			res.error_message_ = json_str(eo, "message");
		} else if (auto r = resp.if_contains("result"); r) {
			res.result_ = *r;
		}
		co_return res;
	}
};

} // namespace launcher

#endif // LAUNCHER_JSONRPC_HPP
