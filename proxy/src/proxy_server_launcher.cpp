//
// proxy_server_launcher.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy_server_launcher.hpp"
#include "proxy/authority.hpp"
#include "proxy/proxy_websocket_client.hpp"
#include "proxy/proxy_util.hpp"
#include "proxy/ssl_connect.hpp"
#include "proxy/strutil.hpp"
#include "proxy/tun_server.hpp"

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/functional/hash.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <future>
#include <optional>
#include <sstream>
#include <unordered_set>
#include <cstring>

#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
# include <pthread.h>
# include <ifaddrs.h>
# include <netinet/in.h>
#endif

#include <tinyrpc/jsonrpc.hpp>

namespace proxy {

//////////////////////////////////////////////////////////////////////////
// launcher 控制通道支持.

// 启动 launcher 控制通道（由 start() 调用, URL 来自 m_option.launcher_url_,
// 为空则不启动）.
void proxy_server::launcher_start() noexcept
{
	if (m_option.launcher_url_.empty())
		return;

	XLOG_DBG << "launcher control channel start: " << m_option.launcher_url_;

	auto self = shared_from_this();
	net::co_spawn(m_executor, [self]() -> net::awaitable<void> {
		co_await self->launcher_worker();
	}, net::detached);
}

// 停止 launcher 控制通道（由 close() 调用, 设置停止标志使 serve 协程退出）.
void proxy_server::launcher_stop() noexcept
{
	m_launcher_state->stopped_ = true;
	// serve 协程在状态上报循环中检查该标志后退出, 并在退出时自行关闭会话.
}

std::string proxy_server::launcher_parse_instance_id(const std::string& url)
{
	if (auto u = boost::urls::parse_uri(url); u.has_value())
	{
		auto p = u->params().find("instance");
		if (p != u->params().end())
			return std::string((*p).value);
	}
	return {};
}

// 建立 ws/wss 连接并返回 JSON-RPC 会话；失败返回 nullopt.
// 连接/握手全程受 k_launcher_dial_timeout 超时保护（超时后关闭 socket
// 使异步操作失败）. 会话对象由调用方持有, 后续均通过引用访问.
template <typename WsStream>
net::awaitable<std::optional<jsonrpc::jsonrpc_session<WsStream>>>
proxy_server::launcher_connect(const std::string& host,
	const std::string& handshake_host, const std::string& port,
	const std::string& target)
{
	auto ex = co_await net::this_coro::executor;
	boost::system::error_code ec;

	// DNS 解析.
	net::ip::tcp::resolver resolver(ex);
	auto results = co_await resolver.async_resolve(host, port, net_awaitable[ec]);
	if (ec)
	{
		XLOG_WARN << "launcher resolve " << host << ": " << ec.message();
		co_return std::nullopt;
	}

	if constexpr (detail::is_ssl_stream<typename WsStream::next_layer_type>::value)
	{
		// wss.
		WsStream ws(ex, m_launcher_state->ssl_ctx_);
		if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str()))
			co_return std::nullopt;

		// 超时保护: 超时后关闭 socket, 使进行中的异步操作立即失败.
		net::steady_timer dial_timer(ex);
		dial_timer.expires_after(k_launcher_dial_timeout);
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
			co_return std::nullopt;

		// TLS 握手.
		co_await ws.next_layer().async_handshake(net::ssl::stream_base::client, net_awaitable[ec]);
		dial_timer.cancel();
		if (ec)
			co_return std::nullopt;

		// WebSocket 握手.
		co_await ws.async_handshake(handshake_host, target, net_awaitable[ec]);
		dial_timer.cancel();
		if (ec)
			co_return std::nullopt;

		co_return jsonrpc::jsonrpc_session<WsStream>(std::move(ws));
	}
	else
	{
		// ws.
		WsStream ws(ex);

		net::steady_timer dial_timer(ex);
		dial_timer.expires_after(k_launcher_dial_timeout);
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
			co_return std::nullopt;

		// WebSocket 握手.
		co_await ws.async_handshake(handshake_host, target, net_awaitable[ec]);
		dial_timer.cancel();
		if (ec)
			co_return std::nullopt;

		co_return jsonrpc::jsonrpc_session<WsStream>(std::move(ws));
	}
}

// 一次连接的服务流程: 注册实例信息、启动读循环、状态上报循环,
// 直到连接断开或 stop. 全程协程, 不创建线程. 会话对象由调用方
// （launcher_run_once）持有, 此处通过引用访问.
template <typename WsStream>
net::awaitable<void> proxy_server::launcher_serve(jsonrpc::jsonrpc_session<WsStream>& sess)
{
	auto ex = co_await net::this_coro::executor;

	// 注册请求/通知处理器.
	launcher_register_handlers(sess);

	m_launcher_state->session_closed_ = false;
	sess.closed_callback([self = shared_from_this()]() { self->m_launcher_state->session_closed_ = true; });

	// 先启动读循环（同 executor 上的独立协程）, 再发送通知;
	// 否则会话尚未进入运行态, 入队的写消息可能无法发出.
	sess.start();

	// 设置 protect 请求发送器: tun 出站连接经它请求 app 放行 socket.
	// 会话生命周期由 launcher_serve 管理, 发送器仅在 serve 期间有效.
	m_launcher_state->call_protect_ =
		[&sess](int fd) -> net::awaitable<bool>
		{
			json::object params;
			params["fd"] = fd;
			try
			{
				auto ex = co_await net::this_coro::executor;
				// app 不响应时取消挂起的调用, 避免等待协程永久悬挂.
				net::steady_timer timer(ex);
				timer.expires_after(k_launcher_protect_timeout);
				net::cancellation_signal cancel_sig;
				timer.async_wait(
					[&cancel_sig](const boost::system::error_code& tec)
					{
						if (tec)
							return;
						cancel_sig.emit(net::cancellation_type::terminal);
					});
				auto result = co_await sess.async_call("protect", params,
					net::bind_cancellation_slot(
						cancel_sig.slot(), net::use_awaitable));
				timer.cancel();
				// tinyrpc async_call 返回完整 JSON-RPC 响应
				// ({"jsonrpc","id","result":{...}}), ok 位于 result 内.
				if (auto r = result.if_contains("result");
					r && r->is_object())
				{
					auto& robj = r->as_object();
					if (auto ok = robj.if_contains("ok");
						ok && ok->is_bool())
					{
						XLOG_DBG << "protect fd=" << fd
							<< " ok=" << ok->as_bool();
						co_return ok->as_bool();
					}
				}
			}
			catch (...)
			{
				// Android VpnService 下未放行的 socket 会回环进 tun,
				// 异常时返回失败阻断连接, 避免形成环路.
				XLOG_WARN << "protect rpc failed";
			}
			co_return false;
		};

	// 注册实例信息.
	json::object reg;
	reg["instance_id"] = launcher_parse_instance_id(m_option.launcher_url_);
	reg["pid"] = static_cast<int64_t>(::getpid());
	reg["version"] = server_version();
	reg["started_at"] = static_cast<int64_t>(started_at());
	sess.notify("register", reg);

	// 立即上报一次状态.
	m_launcher_state->last_report_ = json::value(json::object_kind);
	launcher_update_report(sess);

	// 状态上报循环: 连接断开或 stop 时退出.
	net::steady_timer timer(ex);
	boost::system::error_code sec;
	while (!m_launcher_state->stopped_ && !m_launcher_state->session_closed_)
	{
		timer.expires_after(k_launcher_status_interval);
		co_await timer.async_wait(net_awaitable[sec]);
		if (m_launcher_state->stopped_ || m_launcher_state->session_closed_)
			break;
		launcher_update_report(sess);
	}

	// 清理: 关闭会话并清空 protect 请求发送器.
	sess.stop();
	m_launcher_state->call_protect_ = {};

	// 等待所有在途请求处理完成（它们通过引用访问本会话, 必须在本协程
	// 返回前结束, 调用方才能安全销毁会话对象）. 会话已停止, 不再有新请求.
	while (m_launcher_state->active_requests_.load() > 0)
	{
		timer.expires_after(std::chrono::milliseconds(10));
		co_await timer.async_wait(net_awaitable[sec]);
	}

	co_return;
}

// 注册 launcher → proxy_server 的请求/通知处理器.
template <typename WsStream>
void proxy_server::launcher_register_handlers(jsonrpc::jsonrpc_session<WsStream>& sess)
{
	// 请求（带 id）: 协程方式处理并回复（支持错误响应）.
	// 会话对象由调用方持有, 回调捕获其指针; serve 退出前等待在途处理归零.
	sess.default_method_callback([this, &sess](json::object req) {
		auto self = shared_from_this();
		++m_launcher_state->active_requests_;
		net::co_spawn(m_executor, [self, &sess, req = std::move(req)]() mutable -> net::awaitable<void> {
			// 无论协程如何结束都递减计数, 避免 serve 退出时永久等待.
			auto guard = boost::scope::scope_exit(
				[self]() { --self->m_launcher_state->active_requests_; });
			co_await self->launcher_handle_request(sess, std::move(req));
		}, net::detached);
	});

	// 通知（无 id）: 处理 set_user_usage 等（不引用会话, 无需计数）.
	sess.notify_callback([this](json::object req) {
		auto self = shared_from_this();
		net::co_spawn(m_executor, [self, req = std::move(req)]() mutable -> net::awaitable<void> {
			co_await self->launcher_handle_notify(std::move(req));
		}, net::detached);
	});
}

// 协程方式处理一个请求, 分发到对应方法并回复（支持错误响应）.
template <typename WsStream>
net::awaitable<void> proxy_server::launcher_handle_request(jsonrpc::jsonrpc_session<WsStream>& sess, json::object req)
{
	std::string method = detail::json_str(req, "method");
	json::value params;
	if (auto it = req.if_contains("params"); it)
		params = *it;
	json::value id;
	if (auto it = req.if_contains("id"); it)
		id = *it;

	auto reply_result = [&](json::value r) { sess.reply(std::move(r), id, false); };
	auto reply_error = [&](int code, const std::string& msg) {
		json::object err;
		err["code"] = code;
		err["message"] = msg;
		sess.reply(std::move(err), id, true);
	};

	try
	{
		reply_result(launcher_dispatch(method, params));
	}
	catch (const launcher_error& e)
	{
		reply_error(e.code, e.message);
	}
	catch (const std::exception& e)
	{
		reply_error(-32000, e.what());
	}

	co_return;
}

// 处理 launcher 下发的通知（无 id 消息, 如 set_user_usage）.
net::awaitable<void> proxy_server::launcher_handle_notify(json::object req)
{
	std::string method = detail::json_str(req, "method");
	json::value params;
	if (auto it = req.if_contains("params"); it)
		params = *it;

	// launcher 在连接建立后，下发该用户的历史已用流量（用于配额续接）.
	if (method == "set_user_usage")
	{
		if (params.is_object())
		{
			auto u = params.as_object().if_contains("usage");
			if (u && u->is_object())
				set_user_usage(u->as_object());
		}
	}

	co_return;
}

// 方法分发. 返回结果 json::value；失败抛出 launcher_error.
json::value proxy_server::launcher_dispatch(const std::string& method, const json::value& params)
{
	if (method == "get_status")
	{
		if (m_launcher_state->last_report_.is_object())
			return m_launcher_state->last_report_;
		return snapshot_report();
	}

	if (method == "set_config")
	{
		if (!params.is_object())
			throw launcher_error{ -32602, "invalid set_config params" };
		auto opt = params.as_object().if_contains("options");
		if (!opt || !opt->is_object())
			throw launcher_error{ -32602, "missing options" };
		return apply_options(opt->as_object());
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
			throw launcher_error{ -32602, "user is required" };
		std::string err;
		if (!add_auth_user(user, password, addr, proxy_url, err))
			throw launcher_error{ -32000, err };
		return users_state();
	}

	if (method == "del_user")
	{
		std::string user = params.is_object() ? detail::json_str(params.as_object(), "user") : "";
		if (user.empty())
			throw launcher_error{ -32602, "user is required" };
		if (!del_auth_user(user))
			throw launcher_error{ -32000, "user not found: " + user };
		return users_state();
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
			throw launcher_error{ -32602, "user is required" };
		if (!set_auth_user_password(user, password))
			throw launcher_error{ -32000, "user not found: " + user };
		return users_state();
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
			throw launcher_error{ -32602, "user is required" };
		set_auth_user_rate_limit(user, rate);
		return users_state();
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
			throw launcher_error{ -32602, "user is required" };
		set_auth_user_quota(user, quota);
		return users_state();
	}

	if (method == "set_user_conn_limit")
	{
		std::string user;
		int limit = 0;
		if (params.is_object())
		{
			user = detail::json_str(params.as_object(), "user");
			limit = static_cast<int>(detail::json_num(params.as_object(), "limit"));
		}
		if (user.empty())
			throw launcher_error{ -32602, "user is required" };
		set_auth_user_conn_limit(user, limit);
		return users_state();
	}


	if (method == "set_user_usage")
	{
		if (params.is_object())
		{
			auto u = params.as_object().if_contains("usage");
			if (u && u->is_object())
				set_user_usage(u->as_object());
		}
		return json::object{};
	}

	if (method == "set_tun_fd")
	{
		// Android VpnService 场景: app 建立 tun 后经控制通道注入 fd.
		int fd = -1;
		if (params.is_object())
		{
			auto it = params.as_object().if_contains("fd");
			if (it && it->is_int64())
				fd = static_cast<int>(it->as_int64());
		}
		if (fd < 0)
			throw launcher_error{ -32602, "invalid set_tun_fd params" };
		if (!m_tun_server)
			throw launcher_error{ -32000, "tun server not running" };
		m_tun_server->set_tun_fd(fd);
		json::object ok;
		ok["ok"] = true;
		return ok;
	}

	if (method == "shutdown")
	{
		// 延迟退出: 先让本请求的响应帧写出, launcher 才能收到关闭确认.
		// 用协程定时器实现, 不创建线程. 之后关闭 proxy_server 的所有
		// session/连接/accept/定时器, 使 io_context 无待处理工作而自然退出.
		auto self = shared_from_this();
		net::co_spawn(m_executor, [self]() -> net::awaitable<void> {
			auto ex = co_await net::this_coro::executor;
			net::steady_timer t(ex);
			t.expires_after(std::chrono::milliseconds(200));
			boost::system::error_code sec;
			co_await t.async_wait(net_awaitable[sec]);
			self->close();
		}, net::detached);
		return json::object{};
	}

	throw launcher_error{ -32601, "method not found: " + method };
}

namespace detail {

using traffic_totals = std::pair<int64_t, int64_t>;
using user_traffic_totals = std::map<std::string, traffic_totals>;
using connection_traffic_totals = std::map<int64_t, traffic_totals>;

inline user_traffic_totals collect_user_traffic(const json::value& report)
{
	user_traffic_totals totals;
	if (!report.is_object())
		return totals;

	auto users = report.as_object().if_contains("users");
	if (!users || !users->is_array())
		return totals;

	for (const auto& user : users->as_array())
	{
		if (!user.is_object())
			continue;

		const auto& obj = user.as_object();
		auto name = obj.find("user");
		if (name == obj.end() || !name->value().is_string())
			continue;

		std::string user_name(name->value().as_string());
		totals[std::move(user_name)] = {
			json_num(obj, "rx_bytes"),
			json_num(obj, "tx_bytes")
		};
	}
	return totals;
}

inline connection_traffic_totals collect_connection_traffic(
	const json::value& report)
{
	connection_traffic_totals totals;
	if (!report.is_object())
		return totals;

	auto users = report.as_object().if_contains("users");
	if (!users || !users->is_array())
		return totals;

	for (const auto& user : users->as_array())
	{
		if (!user.is_object())
			continue;

		auto connections = user.as_object().if_contains("connections");
		if (!connections || !connections->is_array())
			continue;

		for (const auto& connection : connections->as_array())
		{
			if (!connection.is_object())
				continue;

			const auto& obj = connection.as_object();
			auto id = obj.find("id");
			if (id == obj.end() || !id->value().is_int64())
				continue;

			totals[id->value().as_int64()] = {
				json_num(obj, "rx_bytes"),
				json_num(obj, "tx_bytes")
			};
		}
	}
	return totals;
}

inline void apply_user_rates(const json::object& report,
	const user_traffic_totals& previous, double seconds, json::object& rates)
{
	auto users = report.if_contains("users");
	if (!users || !users->is_array())
		return;

	for (const auto& user : users->as_array())
	{
		if (!user.is_object())
			continue;

		const auto& obj = user.as_object();
		auto name = obj.find("user");
		if (name == obj.end() || !name->value().is_string())
			continue;

		std::string user_name(name->value().as_string());
		auto prev = previous.find(user_name);
		if (prev == previous.end())
			continue;

		int64_t rx = json_num(obj, "rx_bytes");
		int64_t tx = json_num(obj, "tx_bytes");
		json::object user_rate;
		user_rate["rx_rate_bps"] = rx > prev->second.first
			? (rx - prev->second.first) / seconds : 0;
		user_rate["tx_rate_bps"] = tx > prev->second.second
			? (tx - prev->second.second) / seconds : 0;
		rates[std::move(user_name)] = std::move(user_rate);
	}
}

inline void apply_connection_rates(json::object& report,
	const connection_traffic_totals& previous, double seconds)
{
	auto users = report.if_contains("users");
	if (!users || !users->is_array())
		return;

	for (auto& user : users->as_array())
	{
		if (!user.is_object())
			continue;

		auto connections = user.as_object().if_contains("connections");
		if (!connections || !connections->is_array())
			continue;

		for (auto& connection : connections->as_array())
		{
			if (!connection.is_object())
				continue;

			auto& obj = connection.as_object();
			auto id = obj.find("id");
			if (id == obj.end() || !id->value().is_int64())
				continue;

			auto prev = previous.find(id->value().as_int64());
			if (prev == previous.end())
				continue;

			int64_t rx = json_num(obj, "rx_bytes");
			int64_t tx = json_num(obj, "tx_bytes");
			obj["rx_rate_bps"] = rx > prev->second.first
				? (rx - prev->second.first) / seconds : 0;
			obj["tx_rate_bps"] = tx > prev->second.second
				? (tx - prev->second.second) / seconds : 0;
		}
	}
}

} // namespace detail

// 采集快照、计算速率并上报.
template <typename WsStream>
void proxy_server::launcher_update_report(jsonrpc::jsonrpc_session<WsStream>& sess)
{
	json::object rep = snapshot_report();

	// 差分速率: 用相邻两次快照的差值除以时间间隔, 计数回退时钳制为 0.
	json::object rates;
	json::object user_rates;
	double rx_rate = 0, tx_rate = 0;
	if (m_launcher_state->last_report_.is_object())
	{
		auto cur_ts = rep.if_contains("ts") && rep.at("ts").is_int64() ? rep.at("ts").as_int64() : 0;
		auto prev_ts = m_launcher_state->last_report_.as_object().if_contains("ts") && m_launcher_state->last_report_.as_object().at("ts").is_int64()
			? m_launcher_state->last_report_.as_object().at("ts").as_int64() : 0;
		// 首次上报或重连后快照被清空时 prev_ts 为 0, 此时没有可做差分的
		// 上一份数据, 不计算速率, 避免用断线前的旧快照差分导致速率失真.
		if (prev_ts > 0)
		{
			double sec = static_cast<double>(cur_ts - prev_ts);
			if (sec > 0)
			{
				int64_t cur_rx = 0, cur_tx = 0, prev_rx = 0, prev_tx = 0;
				if (auto g = rep.if_contains("global"); g && g->is_object())
				{
					cur_rx = detail::json_num(g->as_object(), "rx_bytes");
					cur_tx = detail::json_num(g->as_object(), "tx_bytes");
				}
				if (auto g = m_launcher_state->last_report_.as_object().if_contains("global"); g && g->is_object())
				{
					prev_rx = detail::json_num(g->as_object(), "rx_bytes");
					prev_tx = detail::json_num(g->as_object(), "tx_bytes");
				}
				rx_rate = cur_rx > prev_rx ? (cur_rx - prev_rx) / sec : 0;
				tx_rate = cur_tx > prev_tx ? (cur_tx - prev_tx) / sec : 0;

				// 单用户速率: 对上一份报告存在且计数不回退的用户计算差分
				// （回退时钳制为 0, 如用户被删除重建）.
				auto prev_users = detail::collect_user_traffic(
					m_launcher_state->last_report_);
				detail::apply_user_rates(rep, prev_users, sec, user_rates);

				// 连接级速率: 按连接 id 对上一份快照差分, 与全局/用户速率
				// 同口径 (计数回退或新连接时钳制为 0).
				auto prev_conns = detail::collect_connection_traffic(
					m_launcher_state->last_report_);
				detail::apply_connection_rates(rep, prev_conns, sec);
			}
		}
	}
	rates["rx_rate_bps"] = rx_rate;
	rates["tx_rate_bps"] = tx_rate;
	rep["rates"] = std::move(rates);
	if (!user_rates.empty())
		rep["user_rates"] = std::move(user_rates);

	m_launcher_state->last_report_ = rep;
	sess.notify("status", rep);

	// 批量转发经 logger_tag 钩子采集的日志（逐条 notify 开销大）.
	auto lines = detail::launcher_log_drain();
	if (!lines.empty())
	{
		json::array arr;
		for (auto& l : lines)
			arr.emplace_back(std::move(l));
		json::object log;
		log["lines"] = std::move(arr);
		sess.notify("log", log);
	}
}

// 连接循环: 连接失败/断开后退避重连（全部协程, 不创建线程）.
// 注意: 此函数及其调用的 launcher_run_once 定义在所有模板函数之后,
// 以便模板定义在实例化点可见.
net::awaitable<void> proxy_server::launcher_worker()
{
	auto ex = co_await net::this_coro::executor;
	int backoff_ms = 1000;
	boost::system::error_code sec;

	while (!m_launcher_state->stopped_)
	{
		bool connected = co_await launcher_run_once();
		if (m_launcher_state->stopped_)
			break;
		// 建连成功（即使后来断开）: 重置退避, 避免稳定运行后一次抖动
		// 仍要等满上次退避.
		if (connected)
			backoff_ms = 1000;

		XLOG_WARN << "launcher connection lost, reconnect in "
			<< backoff_ms << "ms";

		// 分小段退避等待, 便于及时响应 launcher_stop.
		net::steady_timer timer(ex);
		int left = backoff_ms;
		while (left > 0 && !m_launcher_state->stopped_)
		{
			int chunk = (std::min)(left, 200);
			timer.expires_after(std::chrono::milliseconds(chunk));
			co_await timer.async_wait(net_awaitable[sec]);
			left -= chunk;
		}
		if (backoff_ms < k_launcher_max_backoff_ms)
			backoff_ms *= 2;
	}

	co_return;
}

// 单次连接流程. 返回 true 表示成功建立了连接（尽管之后断开）.
net::awaitable<bool> proxy_server::launcher_run_once()
{
	auto parsed = boost::urls::parse_uri(m_option.launcher_url_);
	if (!parsed)
	{
		XLOG_WARN << "launcher invalid url: "
			<< m_option.launcher_url_;
		co_return false;
	}

	boost::urls::url_view u = *parsed;
	std::string scheme = std::string(u.scheme());
	if (scheme != "ws" && scheme != "wss")
	{
		XLOG_WARN << "launcher unsupported scheme: " << scheme;
		co_return false;
	}
	std::string host = std::string(u.host_address());
	uint16_t port_number = u.port_number();
	if (port_number == 0)
		port_number = urls::default_port(u.scheme_id());
	std::string port = std::to_string(port_number);
	std::string handshake_host = authority::format_authority(host, port_number);
	std::string target = std::string(u.encoded_target().empty() ? "/" : u.encoded_target());

	if (scheme == "wss")
	{
		auto sess = co_await launcher_connect<launcher_wss>(
			host, handshake_host, port, target);
		if (!sess)
			co_return false;
		co_await launcher_serve(*sess);
	}
	else
	{
		auto sess = co_await launcher_connect<launcher_ws>(
			host, handshake_host, port, target);
		if (!sess)
			co_return false;
		co_await launcher_serve(*sess);
	}

	co_return true;
}

// 显式实例化模板 launcher_connect
template net::awaitable<std::optional<jsonrpc::jsonrpc_session<launcher_ws>>>
proxy_server::launcher_connect<launcher_ws>(const std::string& host,
	const std::string& handshake_host, const std::string& port,
	const std::string& target);
template net::awaitable<std::optional<jsonrpc::jsonrpc_session<launcher_wss>>>
proxy_server::launcher_connect<launcher_wss>(const std::string& host,
	const std::string& handshake_host, const std::string& port,
	const std::string& target);

// 显式实例化模板 launcher_serve
template net::awaitable<void> proxy_server::launcher_serve<launcher_ws>(jsonrpc::jsonrpc_session<launcher_ws>& sess);
template net::awaitable<void> proxy_server::launcher_serve<launcher_wss>(jsonrpc::jsonrpc_session<launcher_wss>& sess);

// 显式实例化模板 launcher_register_handlers
template void proxy_server::launcher_register_handlers<launcher_ws>(jsonrpc::jsonrpc_session<launcher_ws>& sess);
template void proxy_server::launcher_register_handlers<launcher_wss>(jsonrpc::jsonrpc_session<launcher_wss>& sess);

// 显式实例化模板 launcher_handle_request
template net::awaitable<void> proxy_server::launcher_handle_request<launcher_ws>(jsonrpc::jsonrpc_session<launcher_ws>& sess, json::object req);
template net::awaitable<void> proxy_server::launcher_handle_request<launcher_wss>(jsonrpc::jsonrpc_session<launcher_wss>& sess, json::object req);

// 显式实例化模板 launcher_update_report
template void proxy_server::launcher_update_report<launcher_ws>(jsonrpc::jsonrpc_session<launcher_ws>& sess);
template void proxy_server::launcher_update_report<launcher_wss>(jsonrpc::jsonrpc_session<launcher_wss>& sess);

} // namespace proxy
