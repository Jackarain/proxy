//
// agent.hpp
// ~~~~~~~~~
//
// proxy_server 侧的 launcher 控制代理（C++ 版本，与 golang internal/agent
// 协议兼容）：作为 WebSocket 客户端主动连接 launcher，上报实时状态
// （register/status），并处理 launcher 下发的运行期配置（set_config）与
// 用户管理请求。
//
// 实现完全基于 C++20 协程 + Boost.Asio 异步（co_await / co_spawn /
// use_awaitable，与 proxy_server.cpp 中 monitor_worker 等协程同风格），
// 不创建任何线程：连接循环、退避重连、状态上报、请求处理全部在代理服务器
// 的 io_context 上以协程方式运行。
//
// JSON-RPC 直接使用 third_party/tinyrpc/include/tinyrpc/jsonrpc.hpp，
// 不依赖 launcher 中的任何代码。
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef PROXY_SERVER_AGENT_HPP
#define PROXY_SERVER_AGENT_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <unistd.h>

#include <openssl/ssl.h>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/url.hpp>

#include "proxy/use_awaitable.hpp"
#include <tinyrpc/jsonrpc.hpp>

namespace proxy_agent {

using proxy_server_ptr = std::shared_ptr<class proxy::proxy_server>;

namespace net = boost::asio;
namespace beast = boost::beast;
namespace json = boost::json;

// 状态上报间隔（与 golang agent 一致）.
inline constexpr std::chrono::milliseconds kStatusInterval{ 2000 };
// 建立连接（含解析/连接/握手）超时.
inline constexpr std::chrono::milliseconds kDialTimeout{ 10000 };
// 重连最大退避.
inline constexpr int kMaxBackoffMs = 30000;

namespace detail {

// 判断 websocket 底层流是否为 ssl::stream（wss）.
template <class T>
struct is_ssl_stream : std::false_type {};
template <class T>
struct is_ssl_stream<beast::ssl_stream<T>> : std::true_type {};

// 取 json 对象中的整数字段.
inline std::int64_t json_num(const json::object& obj, const char* key) {
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

// 取 json 对象中的字符串字段.
inline std::string json_str(const json::object& obj, const char* key) {
	auto it = obj.find(key);
	if (it == obj.end() || !it->value().is_string())
		return {};
	return std::string(it->value().as_string());
}

} // namespace detail

// 请求处理错误：由 dispatch_method 抛出，handle_request 转换为 JSON-RPC error 响应.
struct agent_error {
	int code{ -32000 };
	std::string message;
};

class agent : public std::enable_shared_from_this<agent> {
public:
	agent(proxy_server_ptr server, std::string url, std::function<void()> on_shutdown)
		: server_(std::move(server))
		, url_(std::move(url))
		, on_shutdown_(std::move(on_shutdown))
	{
		// 信任 launcher 自签证书.
		ssl_ctx_.set_verify_mode(net::ssl::verify_none);

		// 从 --launcher URL 解析 instance ID.
		if (auto u = boost::urls::parse_uri(url_); u.has_value())
		{
			auto p = u->params().find("instance");
			if (p != u->params().end())
				instance_id_ = std::string((*p).value);
		}
	}

	// 在指定执行器上启动连接循环（协程方式，不创建线程）.
	void start(net::any_io_executor ex)
	{
		executor_ = std::move(ex);
		auto self = shared_from_this();
		net::co_spawn(executor_, [self]() -> net::awaitable<void> {
			co_await self->run_loop();
		}, net::detached);
	}

	// 停止代理：关闭当前连接，使连接循环协程退出.
	void stop()
	{
		stopped_ = true;
		// 在 io_context 上关闭当前会话，使 serve 协程尽快退出.
		if (auto close = close_current_)
			net::post(executor_, close);
	}

private:
	// 连接循环：连接失败/断开后退避重连（全部协程，不创建线程）.
	net::awaitable<void> run_loop()
	{
		auto ex = co_await net::this_coro::executor;
		int backoff_ms = 1000;
		boost::system::error_code sec;

		while (!stopped_)
		{
			bool connected = co_await run_once();
			if (stopped_)
				break;
			// 建连成功（即使后来断开）：重置退避，避免稳定运行后一次抖动
			// 仍要等满上次退避.
			if (connected)
				backoff_ms = 1000;

			std::fprintf(stderr, "[warn] launcher connection lost, reconnect in %dms\n", backoff_ms);

			// 分小段退避等待，便于及时响应 stop.
			net::steady_timer timer(ex);
			int left = backoff_ms;
			while (left > 0 && !stopped_)
			{
				int chunk = (std::min)(left, 200);
				timer.expires_after(std::chrono::milliseconds(chunk));
				co_await timer.async_wait(net_awaitable[sec]);
				left -= chunk;
			}
			if (backoff_ms < kMaxBackoffMs)
				backoff_ms *= 2;
		}

		co_return;
	}

	// 单次连接流程。返回 true 表示成功建立了连接（尽管之后断开）.
	net::awaitable<bool> run_once()
	{
		boost::urls::url_view u = boost::urls::parse_uri(url_).value();
		std::string scheme = std::string(u.scheme());
		std::string host = std::string(u.host());
		std::string port = u.has_port() ? std::string(u.port()) : (scheme == "wss" ? "443" : "80");
		std::string target = std::string(u.encoded_target().empty() ? "/" : u.encoded_target());

		if (scheme == "wss")
		{
			auto sess = co_await connect_session<wss_type>(host, port, target);
			if (!sess)
				co_return false;
			co_await serve(sess);
			co_return true;
		}

		auto sess = co_await connect_session<ws_type>(host, port, target);
		if (!sess)
			co_return false;
		co_await serve(sess);
		co_return true;
	}

	// 建立 ws/wss 连接并返回 JSON-RPC 会话；失败返回 nullptr.
	// 连接/握手全程受 kDialTimeout 超时保护（超时后关闭 socket 使异步操作失败）.
	template <class WsStream>
	net::awaitable<std::shared_ptr<jsonrpc::jsonrpc_session<WsStream>>>
	connect_session(const std::string& host, const std::string& port, const std::string& target)
	{
		auto ex = co_await net::this_coro::executor;
		boost::system::error_code ec;

		// DNS 解析.
		net::ip::tcp::resolver resolver(ex);
		auto results = co_await resolver.async_resolve(host, port, net_awaitable[ec]);
		if (ec)
		{
			std::fprintf(stderr, "[warn] launcher resolve %s: %s\n", host.c_str(), ec.message().c_str());
			co_return nullptr;
		}

		if constexpr (detail::is_ssl_stream<typename WsStream::next_layer_type>::value)
		{
			// wss.
			WsStream ws(ex, ssl_ctx_);
			if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str()))
				co_return nullptr;

			// 超时保护：超时后关闭 socket，使进行中的异步操作立即失败.
			net::steady_timer dial_timer(ex);
			dial_timer.expires_after(kDialTimeout);
			auto cancel_conn = [&ws](const boost::system::error_code& tec) {
				if (tec)
					return; // 定时器被取消（连接已完成）.
				boost::system::error_code sec;
				beast::get_lowest_layer(ws).close(sec);
			};
			dial_timer.async_wait(cancel_conn);

			// TCP 连接.
			co_await net::async_connect(beast::get_lowest_layer(ws), results, net_awaitable[ec]);
			dial_timer.cancel();
			if (ec)
				co_return nullptr;

			// TLS 握手.
			co_await ws.next_layer().async_handshake(net::ssl::stream_base::client, net_awaitable[ec]);
			dial_timer.cancel();
			if (ec)
				co_return nullptr;

			// WebSocket 握手.
			co_await ws.async_handshake(host, target, net_awaitable[ec]);
			dial_timer.cancel();
			if (ec)
				co_return nullptr;

			co_return std::make_shared<jsonrpc::jsonrpc_session<WsStream>>(std::move(ws));
		}
		else
		{
			// ws.
			WsStream ws(ex);

			net::steady_timer dial_timer(ex);
			dial_timer.expires_after(kDialTimeout);
			auto cancel_conn = [&ws](const boost::system::error_code& tec) {
				if (tec)
					return;
				boost::system::error_code sec;
				beast::get_lowest_layer(ws).close(sec);
			};
			dial_timer.async_wait(cancel_conn);

			// TCP 连接.
			co_await net::async_connect(beast::get_lowest_layer(ws), results, net_awaitable[ec]);
			dial_timer.cancel();
			if (ec)
				co_return nullptr;

			// WebSocket 握手.
			co_await ws.async_handshake(host, target, net_awaitable[ec]);
			dial_timer.cancel();
			if (ec)
				co_return nullptr;

			co_return std::make_shared<jsonrpc::jsonrpc_session<WsStream>>(std::move(ws));
		}
	}

	// 一次连接的服务流程：注册实例信息、启动读循环、状态上报循环，直到
	// 连接断开或 stop。全程协程，不创建线程.
	template <class WsStream>
	net::awaitable<void> serve(std::shared_ptr<jsonrpc::jsonrpc_session<WsStream>> sess)
	{
		auto ex = co_await net::this_coro::executor;

		// 注册请求处理器.
		register_handlers(sess);

		// 记录当前会话，供 stop() 主动关闭连接以退出 serve.
		close_current_ = [sess]() { sess->stop(); };
		session_closed_ = false;
		sess->closed_callback([self = shared_from_this()]() { self->session_closed_ = true; });

		// 先启动读循环（同 executor 上的独立协程），再发送通知；
		// 否则会话尚未进入运行态，入队的写消息可能无法发出.
		sess->start();

		// 注册实例信息.
		json::object reg;
		reg["instance_id"] = instance_id_;
		reg["pid"] = static_cast<int64_t>(::getpid());
		reg["version"] = server_->server_version();
		reg["started_at"] = static_cast<int64_t>(server_->started_at());
		sess->notify("register", reg);

		// 立即上报一次状态.
		last_report_ = json::value(json::object_kind);
		update_report(sess);

		// 状态上报循环：连接断开或 stop 时退出.
		net::steady_timer timer(ex);
		boost::system::error_code sec;
		while (!stopped_ && !session_closed_)
		{
			timer.expires_after(kStatusInterval);
			co_await timer.async_wait(net_awaitable[sec]);
			if (stopped_ || session_closed_)
				break;
			update_report(sess);
		}

		// 清理：关闭会话.
		close_current_ = nullptr;
		sess->stop();

		co_return;
	}

	// 注册 launcher → proxy_server 的请求处理器.
	template <class WsStream>
	void register_handlers(const std::shared_ptr<jsonrpc::jsonrpc_session<WsStream>>& sess)
	{
		sess->default_method_callback([this, sess](json::object req) {
			handle_request(sess, std::move(req));
		});
	}

	// 处理一个请求：分发到对应方法并回复（支持错误响应）.
	template <class WsStream>
	void handle_request(const std::shared_ptr<jsonrpc::jsonrpc_session<WsStream>>& sess, json::object req)
	{
		std::string method = detail::json_str(req, "method");
		json::value params;
		if (auto it = req.if_contains("params"); it)
			params = *it;
		json::value id;
		if (auto it = req.if_contains("id"); it)
			id = *it;

		auto reply_result = [&](json::value r) { sess->reply(std::move(r), id, false); };
		auto reply_error = [&](int code, const std::string& msg) {
			json::object err;
			err["code"] = code;
			err["message"] = msg;
			sess->reply(std::move(err), id, true);
		};

		try
		{
			reply_result(dispatch_method(method, params));
		}
		catch (const agent_error& e)
		{
			reply_error(e.code, e.message);
		}
		catch (const std::exception& e)
		{
			reply_error(-32000, e.what());
		}
	}

	// 方法分发。返回结果 json::value；失败抛出 agent_error.
	json::value dispatch_method(const std::string& method, const json::value& params)
	{
		if (method == "get_status")
		{
			if (last_report_.is_object())
				return last_report_;
			return server_->snapshot_report();
		}

		if (method == "set_config")
		{
			if (!params.is_object())
				throw agent_error{ -32602, "invalid set_config params" };
			auto opt = params.as_object().if_contains("options");
			if (!opt || !opt->is_object())
				throw agent_error{ -32602, "missing options" };
			return server_->apply_options(opt->as_object());
		}

		if (method == "add_user")
		{
			std::string user, password, addr, proxy_url;
			if (params.is_object())
			{
				user = detail::json_str(params.as_object(), "user");
				password = detail::json_str(params.as_object(), "password");
				addr = detail::json_str(params.as_object(), "addr");
				proxy_url = detail::json_str(params.as_object(), "proxy_url");
			}
			if (user.empty())
				throw agent_error{ -32602, "user is required" };
			std::string err;
			if (!server_->add_auth_user(user, password, addr, proxy_url, err))
				throw agent_error{ -32000, err };
			return server_->users_state();
		}

		if (method == "del_user")
		{
			std::string user = params.is_object() ? detail::json_str(params.as_object(), "user") : "";
			if (user.empty())
				throw agent_error{ -32602, "user is required" };
			if (!server_->del_auth_user(user))
				throw agent_error{ -32000, "user not found: " + user };
			return server_->users_state();
		}

		if (method == "set_user_password")
		{
			std::string user, password;
			if (params.is_object())
			{
				user = detail::json_str(params.as_object(), "user");
				password = detail::json_str(params.as_object(), "password");
			}
			if (user.empty())
				throw agent_error{ -32602, "user is required" };
			if (!server_->set_auth_user_password(user, password))
				throw agent_error{ -32000, "user not found: " + user };
			return server_->users_state();
		}

		if (method == "set_user_rate_limit")
		{
			std::string user;
			int rate = 0;
			if (params.is_object())
			{
				user = detail::json_str(params.as_object(), "user");
				rate = static_cast<int>(detail::json_num(params.as_object(), "rate"));
			}
			if (user.empty())
				throw agent_error{ -32602, "user is required" };
			server_->set_auth_user_rate_limit(user, rate);
			return server_->users_state();
		}

		if (method == "set_user_quota")
		{
			std::string user;
			std::int64_t quota = 0;
			if (params.is_object())
			{
				user = detail::json_str(params.as_object(), "user");
				quota = detail::json_num(params.as_object(), "quota");
			}
			if (user.empty())
				throw agent_error{ -32602, "user is required" };
			server_->set_auth_user_quota(user, quota);
			return server_->users_state();
		}

		if (method == "set_user_usage")
		{
			if (params.is_object())
			{
				auto u = params.as_object().if_contains("usage");
				if (u && u->is_object())
					server_->set_user_usage(u->as_object());
			}
			return json::object{};
		}

		if (method == "shutdown")
		{
			// 延迟退出：先让本请求的响应帧写出，launcher 才能收到关闭确认.
			// 用协程定时器实现，不创建线程.
			auto self = shared_from_this();
			net::co_spawn(executor_, [self]() -> net::awaitable<void> {
				auto ex = co_await net::this_coro::executor;
				net::steady_timer t(ex);
				t.expires_after(std::chrono::milliseconds(200));
				boost::system::error_code sec;
				co_await t.async_wait(net_awaitable[sec]);
				if (self->on_shutdown_)
					self->on_shutdown_();
			}, net::detached);
			return json::object{};
		}

		throw agent_error{ -32601, "method not found: " + method };
	}

	// 采集快照、计算速率并上报.
	template <class WsStream>
	void update_report(const std::shared_ptr<jsonrpc::jsonrpc_session<WsStream>>& sess)
	{
		json::object rep = server_->snapshot_report();

		// 差分速率.
		json::object rates;
		double rx_rate = 0, tx_rate = 0;
		if (last_report_.is_object())
		{
			auto cur_ts = rep.if_contains("ts") && rep.at("ts").is_int64() ? rep.at("ts").as_int64() : 0;
			auto prev_ts = last_report_.as_object().if_contains("ts") && last_report_.as_object().at("ts").is_int64()
				? last_report_.as_object().at("ts").as_int64() : 0;
			double sec = static_cast<double>(cur_ts - prev_ts);
			if (sec > 0)
			{
				int64_t cur_rx = 0, cur_tx = 0, prev_rx = 0, prev_tx = 0;
				if (auto g = rep.if_contains("global"); g && g->is_object())
				{
					cur_rx = detail::json_num(g->as_object(), "rx_bytes");
					cur_tx = detail::json_num(g->as_object(), "tx_bytes");
				}
				if (auto g = last_report_.as_object().if_contains("global"); g && g->is_object())
				{
					prev_rx = detail::json_num(g->as_object(), "rx_bytes");
					prev_tx = detail::json_num(g->as_object(), "tx_bytes");
				}
				rx_rate = cur_rx > prev_rx ? (cur_rx - prev_rx) / sec : 0;
				tx_rate = cur_tx > prev_tx ? (cur_tx - prev_tx) / sec : 0;
			}
		}
		rates["rx_rate_bps"] = rx_rate;
		rates["tx_rate_bps"] = tx_rate;
		rep["rates"] = std::move(rates);
		rep["user_rates"] = json::object();

		last_report_ = rep;
		sess->notify("status", rep);
	}

	using ws_type = beast::websocket::stream<net::ip::tcp::socket>;
	using wss_type = beast::websocket::stream<beast::ssl_stream<net::ip::tcp::socket>>;

	proxy_server_ptr server_;
	std::string url_;
	std::string instance_id_;
	std::function<void()> on_shutdown_;

	// 执行器（由 start 传入，agent 协程运行于此）.
	net::any_io_executor executor_;

	// TLS 客户端上下文（信任自签证书）.
	net::ssl::context ssl_ctx_{ net::ssl::context::tls_client };

	// 停止标志与当前连接关闭标志.
	std::atomic<bool> stopped_{ false };
	std::atomic<bool> session_closed_{ false };
	// 当前会话的关闭函数（供 stop() 在 io_context 上关闭连接）.
	std::function<void()> close_current_;

	// 最近一次状态报告（get_status 返回用；仅 io_context 线程访问）.
	json::value last_report_;
};

} // namespace proxy_agent

#endif // PROXY_SERVER_AGENT_HPP
