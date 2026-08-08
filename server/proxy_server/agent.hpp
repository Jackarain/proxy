//
// agent.hpp
// ~~~~~~~~~
//
// proxy_server 侧的 launcher 控制代理（C++ 版本，与 golang internal/agent
// 协议兼容）：作为 WebSocket 客户端主动连接 launcher，上报实时状态
// （register/status），并处理 launcher 下发的运行期配置（set_config）与
// 用户管理请求。
//
// JSON-RPC 基于 launcher/json_rpc.hpp（tinyrpc jsonrpc_session 封装）。
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
#include <thread>

#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/url.hpp>

#include "json_rpc.hpp" // launcher::rpc（协议与 golang internal/rpc 一致）

namespace proxy_agent {

using proxy_server_ptr = std::shared_ptr<class proxy::proxy_server>;

namespace json = boost::json;
namespace net = boost::asio;
namespace beast = boost::beast;

// 状态上报间隔（与 golang agent 一致）.
inline constexpr int kStatusIntervalMs = 2000;
// 重连最大退避.
inline constexpr int kMaxBackoffMs = 30000;

class agent {
public:
	agent(proxy_server_ptr server, std::string url, std::function<void()> on_shutdown)
		: server_(std::move(server))
		, url_(std::move(url))
		, on_shutdown_(std::move(on_shutdown))
	{
		// 从 --launcher URL 解析 instance ID.
		if (auto u = boost::urls::parse_uri(url_); u.has_value())
		{
			auto p = u->params().find("instance");
			if (p != u->params().end())
				instance_id_ = std::string((*p).value);
		}
	}

	void stop()
	{
		stopped_ = true;
	}

	// 循环连接 launcher，断线自动退避重连（阻塞）.
	void run()
	{
		int backoff_ms = 1000;
		while (!stopped_)
		{
			bool connected = run_once();
			if (stopped_)
				break;
			if (!connected)
				std::fprintf(stderr, "[warn] launcher connection lost, reconnect in %dms\n", backoff_ms);
			if (stopped_)
				break;
			for (int i = 0; i < backoff_ms / 100 && !stopped_; i++)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			if (backoff_ms < kMaxBackoffMs)
				backoff_ms *= 2;
		}
	}

private:
	// 一次连接的服务流程，返回 false 表示连接失败/断开.
	bool run_once()
	{
		boost::asio::io_context ioc;

		// 建立连接（ws/wss）.
		boost::system::error_code ec;
		boost::urls::url_view u = boost::urls::parse_uri(url_).value();
		std::string scheme = std::string(u.scheme());
		std::string host = std::string(u.host());
		std::string port = u.has_port() ? std::string(u.port()) : (scheme == "wss" ? "443" : "80");
		std::string target = std::string(u.encoded_target().empty() ? "/" : u.encoded_target());

		boost::asio::ip::tcp::resolver resolver(ioc);
		auto results = resolver.resolve(host, port, ec);
		if (ec)
			return false;

		if (scheme == "wss")
		{
			boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tls_client);
			ssl_ctx.set_verify_mode(boost::asio::ssl::verify_none); // 信任 launcher 自签证书.
			boost::beast::ssl_stream<boost::asio::ip::tcp::socket> ssl_stream(ioc, ssl_ctx);
			boost::asio::connect(beast::get_lowest_layer(ssl_stream), results, ec);
			if (ec)
				return false;
			if (!SSL_set_tlsext_host_name(ssl_stream.native_handle(), host.c_str()))
				return false;
			ssl_stream.handshake(boost::asio::ssl::stream_base::client, ec);
			if (ec)
				return false;
			boost::beast::websocket::stream<boost::beast::ssl_stream<boost::asio::ip::tcp::socket>> ws(std::move(ssl_stream));
			ws.handshake(host, target, ec);
			if (ec)
				return false;
			return serve(std::move(ws), ioc);
		}

		boost::asio::ip::tcp::socket sock(ioc);
		boost::asio::connect(sock, results, ec);
		if (ec)
			return false;
		boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws(std::move(sock));
		ws.handshake(host, target, ec);
		if (ec)
			return false;
		return serve(std::move(ws), ioc);
	}

	// 通用服务逻辑（注册 + 状态循环 + 请求处理）.
	template <class WsStream>
	bool serve(WsStream ws, boost::asio::io_context& ioc)
	{
		auto ep = std::make_shared<launcher::rpc::endpoint<WsStream>>(std::move(ws));
		register_handlers(ep);

		// 注册实例信息.
		{
			json::object reg;
			reg["instance_id"] = instance_id_;
			reg["pid"] = static_cast<int64_t>(::getpid());
			reg["version"] = server_->server_version();
			reg["started_at"] = static_cast<int64_t>(server_->started_at());
			ep->notify("register", reg);
		}

		// 立即上报一次状态.
		last_report_ = json::value(json::object_kind);
		update_report(ep);

		// 状态循环线程（连接断开后自动退出）.
		std::thread status_thread([this, ep]() {
			while (!stopped_ && !ep->closed())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(kStatusIntervalMs));
				if (stopped_ || ep->closed())
					break;
				update_report(ep);
			}
		});

		// 运行会话直到连接关闭（阻塞）.
		ep->run(ioc);
		if (status_thread.joinable())
			status_thread.join();
		return false;
	}

	// 注册 launcher → proxy_server 的请求处理器.
	void register_handlers(const std::shared_ptr<launcher::rpc::endpoint_base>& ep)
	{
		ep->set_handler("get_status", [this](const json::value&) -> json::value {
			if (last_report_.is_object())
				return last_report_;
			return server_->snapshot_report();
		});

		ep->set_handler("set_config", [this](const json::value& params) -> json::value {
			if (!params.is_object())
				throw launcher::rpc::error(launcher::rpc::kCodeParams, "invalid set_config params");
			auto opt_it = params.as_object().find("options");
			if (opt_it == params.as_object().end() || !opt_it->value().is_object())
				throw launcher::rpc::error(launcher::rpc::kCodeParams, "missing options");
			return server_->apply_options(opt_it->value().as_object());
		});

		ep->set_handler("add_user", [this](const json::value& params) -> json::value {
			std::string user, password, addr, proxy_url;
			if (params.is_object())
			{
				const auto& obj = params.as_object();
				auto get = [&obj](const char* k) {
					auto it = obj.find(k);
					return (it != obj.end() && it->value().is_string()) ? std::string(it->value().as_string()) : std::string{};
				};
				user = get("user");
				password = get("password");
				addr = get("addr");
				proxy_url = get("proxy_url");
			}
			if (user.empty())
				throw launcher::rpc::error(launcher::rpc::kCodeParams, "user is required");
			std::string err;
			if (!server_->add_auth_user(user, password, addr, proxy_url, err))
				throw launcher::rpc::error(launcher::rpc::kCodeServer, err);
			return server_->users_state();
		});

		ep->set_handler("del_user", [this](const json::value& params) -> json::value {
			std::string user;
			if (params.is_object())
			{
				auto it = params.as_object().find("user");
				if (it != params.as_object().end() && it->value().is_string())
					user = std::string(it->value().as_string());
			}
			if (user.empty())
				throw launcher::rpc::error(launcher::rpc::kCodeParams, "user is required");
			if (!server_->del_auth_user(user))
				throw launcher::rpc::error(launcher::rpc::kCodeServer, "user not found: " + user);
			return server_->users_state();
		});

		ep->set_handler("set_user_password", [this](const json::value& params) -> json::value {
			std::string user, password;
			if (params.is_object())
			{
				const auto& obj = params.as_object();
				auto get = [&obj](const char* k) {
					auto it = obj.find(k);
					return (it != obj.end() && it->value().is_string()) ? std::string(it->value().as_string()) : std::string{};
				};
				user = get("user");
				password = get("password");
			}
			if (user.empty())
				throw launcher::rpc::error(launcher::rpc::kCodeParams, "user is required");
			if (!server_->set_auth_user_password(user, password))
				throw launcher::rpc::error(launcher::rpc::kCodeServer, "user not found: " + user);
			return server_->users_state();
		});

		ep->set_handler("set_user_rate_limit", [this](const json::value& params) -> json::value {
			std::string user;
			int rate = 0;
			if (params.is_object())
			{
				const auto& obj = params.as_object();
				auto uit = obj.find("user");
				if (uit != obj.end() && uit->value().is_string())
					user = std::string(uit->value().as_string());
				auto rit = obj.find("rate");
				if (rit != obj.end() && rit->value().is_int64())
					rate = static_cast<int>(rit->value().as_int64());
			}
			if (user.empty())
				throw launcher::rpc::error(launcher::rpc::kCodeParams, "user is required");
			server_->set_auth_user_rate_limit(user, rate);
			return server_->users_state();
		});

		ep->set_handler("set_user_quota", [this](const json::value& params) -> json::value {
			std::string user;
			std::int64_t quota = 0;
			if (params.is_object())
			{
				const auto& obj = params.as_object();
				auto uit = obj.find("user");
				if (uit != obj.end() && uit->value().is_string())
					user = std::string(uit->value().as_string());
				auto qit = obj.find("quota");
				if (qit != obj.end() && qit->value().is_int64())
					quota = qit->value().as_int64();
			}
			if (user.empty())
				throw launcher::rpc::error(launcher::rpc::kCodeParams, "user is required");
			server_->set_auth_user_quota(user, quota);
			return server_->users_state();
		});

		// 续接 launcher 持久化的用户已用量（重启后配额延续）.
		ep->set_handler("set_user_usage", [this](const json::value& params) -> json::value {
			if (params.is_object())
			{
				auto u = params.as_object().find("usage");
				if (u != params.as_object().end() && u->value().is_object())
					server_->set_user_usage(u->value().as_object());
			}
			json::object r;
			return r;
		});

		ep->set_handler("shutdown", [this](const json::value&) -> json::value {
			// 延迟退出：先让本请求的响应帧写出，launcher 才能收到关闭确认.
			auto cb = on_shutdown_;
			std::thread([cb]() {
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
				if (cb)
					cb();
			}).detach();
			json::object r;
			return r;
		});
	}

	// 采集快照、计算速率并上报.
	void update_report(const std::shared_ptr<launcher::rpc::endpoint_base>& ep)
	{
		json::object rep = server_->snapshot_report();
		// 差分速率.
		json::object rates;
		double rx_rate = 0, tx_rate = 0;
		json::object user_rates;
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
					cur_rx = json_num(g->as_object(), "rx_bytes");
					cur_tx = json_num(g->as_object(), "tx_bytes");
				}
				if (auto g = last_report_.as_object().if_contains("global"); g && g->is_object())
				{
					prev_rx = json_num(g->as_object(), "rx_bytes");
					prev_tx = json_num(g->as_object(), "tx_bytes");
				}
				rx_rate = cur_rx > prev_rx ? (cur_rx - prev_rx) / sec : 0;
				tx_rate = cur_tx > prev_tx ? (cur_tx - prev_tx) / sec : 0;
			}
		}
		rates["rx_rate_bps"] = rx_rate;
		rates["tx_rate_bps"] = tx_rate;
		rep["rates"] = std::move(rates);
		rep["user_rates"] = std::move(user_rates);

		last_report_ = rep;
		ep->notify("status", rep);
	}

	static int64_t json_num(const json::object& obj, const char* key)
	{
		auto it = obj.find(key);
		if (it == obj.end())
			return 0;
		if (it->value().is_int64())
			return it->value().as_int64();
		if (it->value().is_uint64())
			return static_cast<int64_t>(it->value().as_uint64());
		return 0;
	}

	proxy_server_ptr server_;
	std::string url_;
	std::string instance_id_;
	std::function<void()> on_shutdown_;
	std::atomic<bool> stopped_{ false };
	json::value last_report_;
};

} // namespace proxy_agent

#endif // PROXY_SERVER_AGENT_HPP
