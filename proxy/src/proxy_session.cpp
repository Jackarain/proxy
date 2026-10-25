//
// proxy_session.cpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_session.hpp"
#include "proxy/http_error_body.hpp"
#include "proxy/async_connect.hpp"
#include "proxy/proxy_util.hpp"
#include "proxy/fileop.hpp"

#include <limits>
#include <vector>

namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// proxy_session 辅助函数

	std::string proxy_session::pauth_error_message(int code) noexcept
	{
		switch (code)
		{
		case PROXY_AUTH_SUCCESS:
			return "auth success";
		case PROXY_AUTH_FAILED:
			return "auth failed";
		case PROXY_AUTH_NONE:
			return "auth none";
		case PROXY_AUTH_ILLEGAL:
			return "auth illegal";
		default:
			return "auth unknown";
		}
	}

	void proxy_session::update_bind_interface(const std::string& addr) noexcept
	{
		if (addr.empty())
			return;

		boost::system::error_code ec;
		auto bind_if = net::ip::make_address(addr, ec);
		if (ec)
		{
			// bind 地址有问题, 忽略 bind 参数, 并输出日志.
			log_conn_warning()
				<< ", bind address: " << addr
				<< ", invalid: " << ec.message();
		}
		else
		{
			m_bind_interface = bind_if;
		}
	}

	size_t proxy_session::connection_id() const noexcept
	{
		return m_connection_id;
	}

	//////////////////////////////////////////////////////////////////////////
	// 会话状态与流量查询（供 launcher 状态上报）

	bool proxy_session::alive() const noexcept
	{
		return !m_abort.load();
	}

	uint64_t proxy_session::total_rx() const noexcept
	{
		return m_total_rx;
	}

	uint64_t proxy_session::total_tx() const noexcept
	{
		return m_total_tx;
	}

	std::string proxy_session::auth_user() const noexcept
	{
		return m_auth_user;
	}

	int64_t proxy_session::elapsed() const noexcept
	{
		return m_started_at == 0 ? 0
			: static_cast<int64_t>(std::time(nullptr)) - m_started_at;
	}

	//////////////////////////////////////////////////////////////////////////
	// 会话信息查询（供 launcher 连接列表上报）

	std::string proxy_session::target() const noexcept
	{
		std::lock_guard<std::mutex> lock(m_conn_mutex);
		return m_target;
	}

	std::string proxy_session::client() const noexcept
	{
		return m_client_addr;
	}

	std::string proxy_session::geoip() const noexcept
	{
		return m_region;
	}

	std::string proxy_session::proto() const noexcept
	{
		std::lock_guard<std::mutex> lock(m_conn_mutex);
		return m_proto;
	}

	void proxy_session::set_client_info(
		const std::string& client, const std::string& region) noexcept
	{
		m_client_addr = client;
		m_region = region;
	}

	void proxy_session::set_proto(std::string proto) noexcept
	{
		std::lock_guard<std::mutex> lock(m_conn_mutex);
		m_proto = std::move(proto);
	}

	void proxy_session::set_target(std::string target) noexcept
	{
		std::lock_guard<std::mutex> lock(m_conn_mutex);
		m_target = std::move(target);
	}

	bool proxy_session::is_crypto_stream() const noexcept
	{
		return boost::variant2::holds_alternative<ssl_tcp_stream>(m_remote_socket);
	}

	//////////////////////////////////////////////////////////////////////////
	// http 相关实现

	template <typename DirIter>
	net::awaitable<void>
	proxy_session::on_http_json_impl(const http_context& hctx) noexcept
	{
		boost::system::error_code ec;

		auto& request = hctx.request_;
		auto target = hctx.target_path_;

		DirIter end;
		DirIter it(target, fs::directory_options::skip_permission_denied, ec);
		if (ec)
		{
			string_response res{ http::status::found, request.version() };
			res.set(http::field::server, version_string);
			res.set(http::field::date, server_date_string());
			res.set(http::field::location, "/");
			res.keep_alive(request.keep_alive());
			res.prepare_payload();

			http::serializer<false, string_body, http::fields> sr(res);
			co_await http::async_write(
				m_local_socket,
				sr,
				net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", http_dir write location err: "
					<< ec.message();
			}

			co_return;
		}

		bool hash = false;

		urls::params_view qp(hctx.regex_results_[1]);
		if (qp.find("hash") != qp.end())
			hash = true;

		boost::json::array path_list;

		for (; it != end && !m_abort; it++)
		{
			const auto& item = it->path();
			boost::json::object obj;

			auto [ftime, unc_path] = file_last_write_time(item);
			obj["last_write_time"] = ftime;

			if (fs::is_directory(unc_path.empty() ? item : unc_path, ec))
			{
				auto leaf = boost::nowide::narrow(fs::relative(item, target).wstring());
				obj["filename"] = leaf;
				obj["is_dir"] = true;
			}
			else
			{
				auto leaf = boost::nowide::narrow(fs::relative(item, target).wstring());
				obj["filename"] = leaf;
				obj["is_dir"] = false;
				if (unc_path.empty())
					unc_path = item;
				auto sz = fs::file_size(unc_path, ec);
				if (ec)
					sz = 0;
				obj["filesize"] = sz;
				if (hash)
				{
					auto ret = co_await
						fileop::async_hash_file(unc_path, net_awaitable[ec]);
					if (ec)
						ret = "";
					obj["hash"] = ret;
				}
			}

			path_list.push_back(obj);
		}

		auto body = boost::json::serialize(path_list);

		string_response res{ http::status::ok, request.version() };
		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string());
		res.set(http::field::content_type, "application/json");
		res.keep_alive(request.keep_alive());
		res.body() = body;
		res.prepare_payload();

		http::serializer<false, string_body, http::fields> sr(res);
		// HEAD 请求只返回响应头, 不发送 body.
		if (request.method() == http::verb::head)
			co_await http::async_write_header(
				m_local_socket,
				sr,
				net_awaitable[ec]);
		else
			co_await http::async_write(
				m_local_socket,
				sr,
				net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", http dir write body err: "
				<< ec.message();
		}

		co_return;
	}

	// explicit instantiations for on_http_json_impl
	template
	net::awaitable<void>
	proxy_session::on_http_json_impl<fs::recursive_directory_iterator>(const http_context&) noexcept;

	template
	net::awaitable<void>
	proxy_session::on_http_json_impl<fs::directory_iterator>(const http_context&) noexcept;

	net::awaitable<void> proxy_session::on_http_all_json(const http_context& hctx) noexcept
	{
		co_await on_http_json_impl<fs::recursive_directory_iterator>(hctx);
	}

	net::awaitable<void> proxy_session::on_http_json(const http_context& hctx) noexcept
	{
		co_await on_http_json_impl<fs::directory_iterator>(hctx);
	}

	std::string proxy_session::server_date_string() noexcept
	{
		// 缓存 date string 以避免频繁调用 time/gmtime/strftime.
		// 每秒刷新一次即可满足 HTTP Date 头精度要求.
		// 使用 mutex 保护静态变量, 防止多协程并发访问导致数据竞争.
		static std::string cached;
		static std::chrono::steady_clock::time_point last_update;
		static std::mutex mtx;

		auto now = std::chrono::steady_clock::now();
		{
			std::lock_guard<std::mutex> lock(mtx);
			if (now - last_update >= std::chrono::seconds(1))
			{
				auto time = std::time(nullptr);
				auto gmt = gmtime((const time_t*)&time);

				cached.resize(64, '\0');
				auto ret = strftime(cached.data(), 64, "%a, %d %b %Y %H:%M:%S GMT", gmt);
				cached.resize(ret);
				last_update = now;
			}
			return cached;
		}
	}

	bool proxy_session::should_send_not_modified(
		const string_request& request,
		const std::string& etag,
		std::time_t mtime,
		bool mtime_valid) noexcept
	{
		// If-None-Match 的优先级高于 If-Modified-Since.
		if (request.count(http::field::if_none_match))
		{
			auto value = request[http::field::if_none_match];
			std::string_view tags(value.data(), value.size());

			// 存在 If-None-Match 时, 无论是否命中都不再使用 If-Modified-Since.
			return http_etag_match(tags, etag);
		}

		if (request.count(http::field::if_modified_since) && mtime_valid)
		{
			auto value = request[http::field::if_modified_since];
			std::string_view since(value.data(), value.size());

			if (auto time_c = parse_http_date(since))
				return mtime <= *time_c;
		}

		return false;
	}

	bool proxy_session::if_range_matches(
		std::string_view value,
		const std::string& etag,
		std::time_t mtime,
		bool mtime_valid) noexcept
	{
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
			value.remove_prefix(1);
		while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
			value.remove_suffix(1);

		if (value.empty())
			return false;

		// ETag 形式使用强比较, 弱校验前缀 W/ 不参与匹配.
		if (value.front() == '"' || value.starts_with("W/"))
			return !etag.empty() && value == etag;

		// 时间形式, 文件最后修改时间不得晚于 If-Range 给出的时间.
		if (!mtime_valid)
			return false;

		if (auto time_c = parse_http_date(value))
			return mtime <= *time_c;

		return false;
	}

	void proxy_session::user_rate_limit_config(const std::string& user) noexcept
	{
		// 在这里使用用户指定的速率设置替换全局速率配置.
		auto found = m_option.users_rate_limit_.find(user);
		if (found != m_option.users_rate_limit_.end())
		{
			auto& rate = *found;
			m_option.tcp_rate_limit_ = rate.second;
		}
	}

	bool proxy_session::user_conn_acquire(const std::string& user) noexcept
	{
		// 同一连接内重复认证同一用户（如 HTTP keep-alive 多次请求）不重复计数.
		if (m_conn_user == user)
			return true;

		auto server = m_proxy_server.lock();
		if (!server)
			return true;

		if (!server->acquire_user_connection(user))
			return false;

		// 同一连接切换为其他用户时释放旧计数.
		if (!m_conn_user.empty())
			server->release_user_connection(m_conn_user);
		m_conn_user = user;
		return true;
	}

	bool proxy_session::quota_exceeded() const noexcept
	{
		// 匿名/未认证会话不参与配额限制.
		if (m_auth_user.empty())
			return false;

		auto server = m_proxy_server.lock();
		if (!server)
			return false;

		// 未配置配额（<=0）表示不限制.
		auto quota = server->user_quota(m_auth_user);
		if (quota <= 0)
			return false;

		// 该用户总流量（上行+下行，含历史已用流量、已关闭会话与本会话）超过配额.
		return server->user_total_flow(this) >= quota;
	}

	void proxy_session::stream_expires_never(variant_stream_type& stream) noexcept
	{
		boost::variant2::visit([](auto& s) mutable
		{
			using ValueType = std::decay_t<decltype(s)>;
			using NextLayerType = util::proxy_tcp_socket::next_layer_type;

			if constexpr (std::same_as<NextLayerType, util::tcp_socket>)
			{
				if constexpr (std::same_as<util::proxy_tcp_socket, ValueType>)
				{
					auto& next_layer = s.next_layer();
					next_layer.expires_never();
				}
				else if constexpr (std::same_as<util::ssl_tcp_stream, ValueType>)
				{
					auto& next_layer = s.next_layer().next_layer();
					next_layer.expires_never();
				}
				else if constexpr (std::same_as<util::websocket_plain_stream, ValueType>)
				{
					s.next_layer().next_layer().expires_never();
				}
				else if constexpr (std::same_as<util::websocket_tls_stream, ValueType>)
				{
					s.next_layer().next_layer().next_layer().expires_never();
				}
				else if constexpr (std::same_as<util::ssl_websocket_plain_stream, ValueType>)
				{
					s.next_layer().next_layer().next_layer().expires_never();
				}
				else if constexpr (std::same_as<util::ssl_websocket_tls_stream, ValueType>)
				{
					s.next_layer().next_layer().next_layer().next_layer().expires_never();
				}
			}
		}, stream);
	}

	void proxy_session::stream_expires_after(
		variant_stream_type& stream, net::steady_timer::duration expiry_time) noexcept
	{
		// tcp_timeout <= 0 表示禁用超时, 与 idle_timeout 的语义保持一致.
		// 若对 0 调用 expires_after(0), 下一次 I/O 会立即超时, 导致
		// http 文件发送在写出响应头后无法发送 body.
		if (expiry_time.count() <= 0)
			return;

		boost::variant2::visit([expiry_time](auto& s) mutable
		{
			using ValueType = std::decay_t<decltype(s)>;
			using NextLayerType = util::proxy_tcp_socket::next_layer_type;

			if constexpr (std::same_as<NextLayerType, util::tcp_socket>)
			{
				if constexpr (std::same_as<util::proxy_tcp_socket, ValueType>)
				{
					auto& next_layer = s.next_layer();
					next_layer.expires_after(expiry_time);
				}
				else if constexpr (std::same_as<util::ssl_tcp_stream, ValueType>)
				{
					auto& next_layer = s.next_layer().next_layer();
					next_layer.expires_after(expiry_time);
				}
				else if constexpr (std::same_as<util::websocket_plain_stream, ValueType>)
				{
					s.next_layer().next_layer().expires_after(expiry_time);
				}
				else if constexpr (std::same_as<util::websocket_tls_stream, ValueType>)
				{
					s.next_layer().next_layer().next_layer().expires_after(expiry_time);
				}
				else if constexpr (std::same_as<util::ssl_websocket_plain_stream, ValueType>)
				{
					s.next_layer().next_layer().next_layer().expires_after(expiry_time);
				}
				else if constexpr (std::same_as<util::ssl_websocket_tls_stream, ValueType>)
				{
					s.next_layer().next_layer().next_layer().next_layer().expires_after(expiry_time);
				}
			}
		}, stream);
	}

	void proxy_session::stream_cancel(variant_stream_type& stream) noexcept
	{
		util::close_stream(stream);
	}

	void proxy_session::stream_rate_limit(variant_stream_type& stream, int rate) noexcept
	{
		boost::variant2::visit([rate](auto& s) mutable
			{
				using ValueType = std::decay_t<decltype(s)>;
				using NextLayerType = proxy_tcp_socket::next_layer_type;

				if constexpr (std::same_as<NextLayerType, tcp_socket>)
				{
					if constexpr (std::same_as<proxy_tcp_socket, ValueType>)
					{
						auto& next_layer = s.next_layer();
						next_layer.rate_limit(rate);
					}
					else if constexpr (std::same_as<ssl_tcp_stream, ValueType>)
					{
						auto& next_layer = s.next_layer().next_layer();
						next_layer.rate_limit(rate);
					}
					else if constexpr (std::same_as<websocket_plain_stream, ValueType>)
					{
						s.next_layer().next_layer().rate_limit(rate);
					}
					else if constexpr (std::same_as<websocket_tls_stream, ValueType>)
					{
						s.next_layer().next_layer().next_layer().rate_limit(rate);
					}
					else if constexpr (std::same_as<ssl_websocket_plain_stream, ValueType>)
					{
						s.next_layer().next_layer().next_layer().rate_limit(rate);
					}
					else if constexpr (std::same_as<ssl_websocket_tls_stream, ValueType>)
					{
						s.next_layer().next_layer().next_layer().next_layer().rate_limit(rate);
					}
				}
			}, stream);
	}

	//////////////////////////////////////////////////////////////////////////
	// 公共接口

	void proxy_session::start() noexcept
	{
		auto server = m_proxy_server.lock();
		if (!server)
			return;

		// 保存 server 的参数选项.
		m_option = server->option();

		// 将 local_ip 转换为 ip::address 对象, 用于后面向外发起连接时
		// 绑定到指定的本地地址.
		boost::system::error_code ec;
		m_bind_interface = net::ip::make_address(m_option.local_ip_, ec);
		if (ec)
		{
			// bind 地址有问题, 忽略bind参数.
			m_bind_interface.reset();
		}

		// 如果指定了 proxy_pass_ 参数, 则记录这个参数, 在后面若用户又特指了更具体的
		// proxy_pass, 则在 check_userpasswd 中更新 m_proxy_pass 为用户特定的
		// proxy_pass.
		m_proxy_pass = m_option.proxy_pass_;

		// 保持 self 对象指针, 以防止在协程完成后 this 被销毁.
		auto self = this->shared_from_this();

		// 如果是透明代理, 则启动透明代理协程.
		if (m_tproxy_remote)
		{
			set_proto("tproxy");

			net::co_spawn(m_executor,
				[this, self]() -> net::awaitable<void>
				{
					co_await transparent_proxy();
					co_return;
				}, net::detached);

			return;
		}

		// 如果是 stdio proxy, 则启动 stdio proxy 协程.
		if (!m_option.stdio_target_.empty())
		{
			set_proto("stdio");

			net::co_spawn(m_executor,
				[this, self]() -> net::awaitable<void>
				{
					co_await stdio_proxy();
					co_return;
				}, net::detached);

			return;
		}

		// 启动协议侦测协程.
		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				if (boost::variant2::holds_alternative<proxy_tcp_socket>(m_local_socket))
				{
					co_await proto_detect<proxy_tcp_socket>();
				}
				else if (boost::variant2::holds_alternative<proxy_uds_socket>(m_local_socket))
				{
					co_await proto_detect<proxy_uds_socket>();
				}

				co_return;
			}, net::detached);
	}

	void proxy_session::close() noexcept
	{
		if (m_abort)
			return;

		m_abort = true;

		boost::system::error_code ignore_ec;

		// 关闭所有 socket.
		m_local_socket.close(ignore_ec);
		m_remote_socket.close(ignore_ec);
	}

	void proxy_session::setup_tproxy(const net::ip::tcp::endpoint& tproxy_remote) noexcept
	{
		log_conn_debug()
			<< ", tproxy setup: " << tproxy_remote;

		m_tproxy_remote = tproxy_remote;
	}

	//////////////////////////////////////////////////////////////////////////
	// 专用代理模式

	net::awaitable<void> proxy_session::stdio_proxy() noexcept
	{
		auto executor = co_await net::this_coro::executor;
		boost::system::error_code ec;

		if (m_option.stdio_target_.empty())
		{
			log_conn_error() << ", stdio proxy requires a stdio_target";
			co_return;
		}

		try
		{
			boost::system::result<url_info> expect_url;
			std::string url = m_option.stdio_target_;

			if (url.find("://") == std::string::npos)
				url = "http://" + url;

			expect_url = parse_urlinfo(url);
			if (expect_url.has_error())
			{
				log_conn_error() << ", stdio proxy param stdio target is bad: " << url;
				co_return;
			}

			auto [scheme, user, passwd, host, port, resource] = *expect_url;

			// 查询 stdio_target_ 目标服务器的域名信息.
			auto targets = co_await resolve_proxy_pass_targets();

			// 创建 tcp socket 用于连接到目标服务器.
			tcp::socket& remote_socket = net_tcp_socket(m_remote_socket);

			// 发起连接到 stdio_target_ 目标服务器.
			ec = co_await connect_proxy_pass(remote_socket, targets);
			if (ec)
				co_return;

			// 与网关中继服务器握手.
			ec = co_await proxy_pass_handshake(remote_socket,
				std::string(host),
				port,
				*m_proxy_pass);
			if (ec)
				co_return;

			size_t l2r_transferred = 0;
			size_t r2l_transferred = 0;

#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
			net::posix::stream_descriptor stream_stdout(m_executor, ::dup(STDOUT_FILENO));
			stdio_stream stream(std::move(stream_stdout));

			auto out = init_proxy_stream(std::move(stream));
			auto& in = m_local_socket;

			// 并发读写, 在 local 和 remote 之间互传数据.
			// 同时运行 idle_timeout 检测整体连接空闲超时.
			// 只要任何一个方向有数据传输, idle_timeout 中的计时器就会被重置,
			// 从而避免单向大文件传输时因空闲方向超时而断开连接.
			// 当两个方向的传输都已完成(即两端都已断开)时, 立即结束传输,
			// 避免连接已断开后 session 仍一直存活到空闲超时.
			co_await(
				(transfer(in, m_remote_socket, l2r_transferred, true)
				 &&
				 transfer(m_remote_socket, out, r2l_transferred, false))
				||
				idle_timeout(in, m_remote_socket)
			);

#else
			auto in_executor = boost::variant2::get<stdio_stream>(m_local_socket).get_in_executor();
			auto out_executor = boost::variant2::get<stdio_stream>(m_local_socket).get_out_executor();

			stdio_stream stream(in_executor, out_executor);

			auto out = init_proxy_stream(std::move(stream));
			auto& in = m_local_socket;

			auto self = shared_from_this();

			net::co_spawn(in_executor,
				[this, self, &in, &l2r_transferred]() mutable -> net::awaitable<void>
				{
					boost::system::error_code ec;
					constexpr size_t buf_size = 4096;
					char buf[buf_size]{ 0 };

					while (!m_abort)
					{
						DWORD read_bytes = 0;
						read_bytes = (DWORD)co_await in.async_read_some(
							net::buffer(buf, buf_size), net_awaitable[ec]);
						if (ec)
							break;

						co_await net::async_write(
							m_remote_socket,
							net::buffer(buf, read_bytes),
							net_awaitable[ec]);
						if (ec)
							break;

						l2r_transferred += read_bytes;
					}

					co_return;
				}, net::detached);

			co_await net::co_spawn(out_executor,
				[this, self, &out, &r2l_transferred]() mutable -> net::awaitable<void>
				{
					boost::system::error_code ec;
					constexpr size_t buf_size = 4096;
					char buf[buf_size]{ 0 };

					while (!m_abort)
					{
						DWORD read_bytes = (DWORD)co_await m_remote_socket.async_read_some(
							net::buffer(buf, buf_size), net_awaitable[ec]);
						if (ec)
							break;

						co_await net::async_write(
							out,
							net::buffer(buf, read_bytes),
							net_awaitable[ec]);
						if (ec)
							break;

						r2l_transferred += read_bytes;
					}

					co_return;
				}, net::deferred);
#endif

			log_conn_debug()
				<< ", transfer completed"
				<< ", local to remote: "
				<< l2r_transferred
				<< ", remote to local: "
				<< r2l_transferred;
		}
		catch (const std::exception& e)
		{
			log_conn_error() << ", stdio_proxy exception: " << e.what();
		}
		co_return;
	}

	net::awaitable<void> proxy_session::transparent_proxy() noexcept
	{
		auto executor = co_await net::this_coro::executor;
		boost::system::error_code ec;

		// 记录透明代理的原始目标（供 launcher 连接列表上报）.
		if (m_tproxy_remote)
			set_target(m_tproxy_remote->address().to_string() + ":" +
				std::to_string(m_tproxy_remote->port()));

		if (!m_proxy_pass)
		{
			XLOG_ERR << "transparent proxy requires a proxy_pass";
			co_return;
		}

		try
		{
			tcp::socket& remote_socket = net_tcp_socket(m_remote_socket);

			// 查询网关代理中继服务器域名信息.
			auto targets = co_await resolve_proxy_pass_targets();

			// 发起连接到网关代理中继服务器.
			ec = co_await connect_proxy_pass(remote_socket, targets);
			if (ec)
				co_return;

			// 与网关中继服务器握手.
			ec = co_await proxy_pass_handshake(remote_socket,
				m_tproxy_remote->address().to_string(),
				m_tproxy_remote->port(),
				*m_proxy_pass);
			if (ec)
				co_return;

			co_await concurrent_transfer();
		}
		catch (const std::exception& e)
		{
			log_conn_error()
				<< ", transparent_proxy exception: " << e.what();
		}

		co_return;
	}

	//////////////////////////////////////////////////////////////////////////
	// 混淆与协议侦测

	template <typename T>
	net::awaitable<bool>
	proxy_session::noise_handshake(T& socket,
		std::array<uint8_t, 16>& inkey,
		std::array<uint8_t, 16>& outkey) noexcept
	{
		boost::system::error_code error;

		// 工作流程:
		// 1. 生成一段随机长度的随机数据(最大长度由配置文件中的参数 noise_length 指定), 用于发送给对方.
		// 2. 根据这些随机数据计算发送数据的 key, 这个 key 将会用于后续的代理时数据的加密.
		// 3. 发送随机数据.
		// 4. 对方在接收到随机数据后, 同样会发送噪声随机数据(包含随机数长度本身, 在前16字节中的最后一位,
		//    组成一个 16 位的整数表示长度).
		// 5. 计算接收随机数据的 key 用于后续的接收到的数据的解密.

		// 生成要发送的噪声数据.
		int noise_length = static_cast<int>(m_option.noise_length_);

		if (noise_length < 16 ||
			(noise_length > std::numeric_limits<uint16_t>::max() / 2))
			noise_length = noise_injection_max_len;

		std::vector<uint8_t> noise =
			generate_noise(global_random_device(), static_cast<uint16_t>(noise_length), global_known_proto);

		// 计算数据发送 key.
		outkey = compute_key(noise);

		log_conn_debug()
			<< ", send noise, length: "
			<< noise.size();

		// 发送 noise 消息.
		co_await net::async_write(
			socket,
			net::buffer(noise),
			net_awaitable[error]);
		if (error)
		{
			log_conn_warning()
				<< ", noise write error: "
				<< error.message();

			co_return false;
		}

		noise.resize(16);

		// 接收对方发过来的 noise 回应消息.
		co_await net::async_read(
			socket,
			net::buffer(noise, 16),
			net_awaitable[error]);

		if (error)
		{
			log_conn_warning()
				<< ", noise read header error: "
				<< error.message();

			co_return false;
		}

		noise_length = extract_noise_length(noise);

		// 计算要接收的剩余数据大小.
		int remainder = static_cast<int>(noise_length) - 16;
		if (remainder < 0 || remainder >= std::numeric_limits<uint16_t>::max())
		{
			log_conn_debug()
				<< ", noise length: "
				<< noise_length
				<< ", is invalid, noise size: "
				<< noise.size();

			co_return false;
		}

		noise.resize(noise_length);
		co_await net::async_read(
				socket,
				net::buffer(noise.data() + 16, remainder),
				net_awaitable[error]);

		if (error)
		{
			log_conn_warning()
				<< ", noise read body error: "
				<< error.message();

			co_return false;
		}

		log_conn_debug()
			<< ", recv noise, length: "
			<< noise.size();

		// 计算接收数据key.
		inkey = compute_key(noise);

		co_return true;
	}

	// explicit instantiations for noise_handshake
	template
	net::awaitable<bool>
	proxy_session::noise_handshake<proxy_tcp_socket>(
		proxy_tcp_socket& socket,
		std::array<uint8_t, 16>& inkey,
		std::array<uint8_t, 16>& outkey) noexcept;

	template
	net::awaitable<bool>
	proxy_session::noise_handshake<net::local::stream_protocol::socket>(
		net::local::stream_protocol::socket& socket,
		std::array<uint8_t, 16>& inkey,
		std::array<uint8_t, 16>& outkey) noexcept;

	template
	net::awaitable<bool>
	proxy_session::noise_handshake<tcp::socket>(
		tcp::socket& socket,
		std::array<uint8_t, 16>& inkey,
		std::array<uint8_t, 16>& outkey) noexcept;

	//////////////////////////////////////////////////////////////////////////
	// 协议侦测

	template <typename T>
	net::awaitable<void>
	proxy_session::proto_detect(bool handshake_before) noexcept
	{
		// 如果 server 对象已经撤销, 说明服务已经关闭则直接退出这个 session 连接不再
		// 进行任何处理.
		auto server = m_proxy_server.lock();
		if (!server)
			co_return;

		auto self = shared_from_this();

		// 从 m_local_socket 中获取 tcp::socket 对象的引用.
		auto& socket = boost::variant2::get<T>(m_local_socket);

		boost::system::error_code error;

		// 等待 read 事件以确保下面 recv 偷看数据时能有数据.
		co_await async_wait(m_local_socket, net_awaitable[error]);
		if (error)
		{
			log_conn_warning() << ", socket.async_wait error: " << error.message();
			co_return;
		}

		auto scramble_setup = [this](auto& sock) mutable
		{
			if (!m_option.scramble_)
				return;

			using Stream = std::decay_t<decltype(sock)>;

			if constexpr (std::same_as<Stream, tcp::socket> ||
				std::same_as<Stream, net::local::stream_protocol::socket>)
				return;

			if constexpr (std::same_as<Stream, proxy_tcp_socket> ||
				std::same_as<Stream, proxy_uds_socket>)
			{
				sock.set_scramble_key(m_server_tx_key);
				sock.set_unscramble_key(m_server_rx_key);
			}
		};

		// handshake_before 在调用 proto_detect 时第1次为 true, 第2次调用 proto_detect
		// 时 handshake_before 为 false, 此时就表示已经完成了 scramble 握手并协
		// 商好了 scramble 加解密用的 key, 则此时应该为 socket 配置好加解密用的 key.

		if (!handshake_before)
		{
			// 为 socket 设置 scramble key.
			scramble_setup(socket);
		}

		// 检查协议.
		auto fd = socket.native_handle();
		uint8_t detect[5] = { 0 };

#if defined(WIN32) || defined(__APPLE__)
		auto ret = recv(fd, (char*)detect, sizeof(detect), MSG_PEEK);
#else
		auto ret = recv(fd, (void*)detect, sizeof(detect), MSG_PEEK | MSG_NOSIGNAL | MSG_DONTWAIT);
#endif
		if (ret <= 0)
		{
			log_conn_warning() << ", peek message return: " << ret;
			co_return;
		}

		// detect 中的数据只有下面几种情况, 它是 http/socks4/5/ssl 协议固定的头
		// 几个字节, 如若不是, 在启用 scramble 的情况下, 则是 scramble 协议头
		// 此时应该进入 scramble 协商密钥, 协商密钥之后则重新进入 proto_detect
		// 以检测在 scramble 加密后的真实协议头.
		// 如果没启用 scramble, 接受到 http/socks4/5/ssl 协议固定的头之外的数据
		// 则视为未知协议退出.

		// scramble_peek 用于解密 peek 数据.
		auto scramble_peek = [this](auto& sock, std::span<uint8_t> detect) mutable
		{
			if (!m_option.scramble_)
				return;

			using Stream = std::decay_t<decltype(sock)>;

			if constexpr (std::same_as<Stream, tcp::socket> ||
				std::same_as<Stream, net::local::stream_protocol::socket>)
				return;

			if constexpr (std::same_as<Stream, proxy_tcp_socket> ||
				std::same_as<Stream, proxy_uds_socket>)
			{
				auto& unscramble = sock.unscramble();
				unscramble.peek_data(detect);
			}
		};

		if (!handshake_before)
		{
			// peek 方式解密混淆的数据, 用于检测加密混淆的数据的代理协议. 在双方启用
			// scramble 的情况下, 上面 recv 接收到的数据则会为 scramble 加密后的
			// 数据, 要像未启用 scramble 时那样探测协议, 就必须将上面 recv 中
			// peek 得到的数据：detect 临时解密(因为 proxy_stream 的加密为流式加
			// 密, 非临时解密则会对整个数据流产生错误解密), 从而得到具体的协议字节
			// 用于后面探测逻辑.
			scramble_peek(socket, detect);
		}

		// 保存第一个字节用于协议类型甄别.
		const uint8_t proto_byte = detect[0];

		// 非安全连接检查.
		if (m_option.disable_insecure_)
		{
			bool noise_proto = false;

			// 如果启用了 scramble, 则也认为是安全连接.
			if ((proto_byte != 0x05 &&
				proto_byte != 0x04 &&
				proto_byte != 0x47 &&
				proto_byte != 0x50 &&
				proto_byte != 0x48 &&
				proto_byte != 0x44 &&
				proto_byte != 0x4F &&
				proto_byte != 0x54 &&
				proto_byte != 0x43) ||
				!handshake_before)
			{
				noise_proto = true;
			}

			if (detect[0] != 0x16 && !noise_proto)
			{
				log_conn_debug() << ", insecure protocol disabled";
				co_return;
			}
		}

		// plain socks4/5 protocol.
		if (detect[0] == 0x05 || detect[0] == 0x04)
		{
			if (m_option.disable_socks_)
			{
				log_conn_debug() << ", socks protocol disabled";
				co_return;
			}

			log_conn_debug() << ", plain socks4/5 protocol";

			// 开始启动代理协议.
			co_await start_proxy();
		}
		else if (detect[0] == 0x16) // http/socks proxy with ssl crypto protocol.
		{
			log_conn_debug() << ", ssl protocol";

			auto& srv_ssl_context = server->ssl_context();

			// instantiate socks stream with ssl context.
			auto ssl_socks_stream = init_proxy_stream(std::move(socket), srv_ssl_context);

			// get origin ssl stream type.
			ssl_tcp_stream& ssl_socket = boost::variant2::get<ssl_tcp_stream>(ssl_socks_stream);

			// do async ssl handshake.
			co_await ssl_socket.async_handshake(net::ssl::stream_base::server, net_awaitable[error]);
			if (error)
			{
				log_conn_debug() << ", ssl server protocol handshake error: " << error.message();
				co_return;
			}

			// 使用 ssl_socks_stream 替换 m_local_socket.
			m_local_socket = std::move(ssl_socks_stream);

			// 开始启动代理协议.
			co_await start_proxy();
		}								// plain http protocol.
		else if (detect[0] == 0x47 ||	// 'G'
			detect[0] == 0x50 ||		// 'P'
			detect[0] == 0x48 ||		// 'H'
			detect[0] == 0x44 ||		// 'D'
			detect[0] == 0x4F ||		// 'O'
			detect[0] == 0x54 ||		// 'T'
			detect[0] == 0x43)			// 'C'
		{
			if (m_option.disable_http_)
			{
				log_conn_debug() << ", http protocol disabled";
				co_return;
			}

			log_conn_debug() << ", plain http protocol";

			// 开始启动代理协议.
			co_await start_proxy();
		}
		else if (handshake_before && m_option.scramble_)
		{
			// 进入噪声握手协议, 即: 返回一段噪声给客户端, 并等待客户端返回噪声.
			log_conn_debug() << ", noise protocol";

			if constexpr (std::same_as<T, proxy_tcp_socket>)
			{
				if (!co_await noise_handshake<tcp::socket>(
					net_tcp_socket(socket), m_server_rx_key, m_server_tx_key))
					co_return;
			}
			else if constexpr (std::same_as<T, proxy_uds_socket>)
			{
				if (!co_await noise_handshake<net::local::stream_protocol::socket>(
					net_uds_socket(socket), m_server_rx_key, m_server_tx_key))
					co_return;
			}

			// 在完成 noise 握手后, 重新检测被混淆之前的代理协议.
			co_await proto_detect<T>(false);
		}
		else
		{
			log_conn_debug() << ", unknown protocol";
		}

		co_return;
	}

	// explicit instantiations for proto_detect
	template
	net::awaitable<void>
	proxy_session::proto_detect<proxy_tcp_socket>(bool) noexcept;

	template
	net::awaitable<void>
	proxy_session::proto_detect<proxy_uds_socket>(bool) noexcept;

	//////////////////////////////////////////////////////////////////////////
	// 代理协议处理

	net::awaitable<void> proxy_session::start_proxy() noexcept
	{
		// read
		//  +----+----------+----------+
		//  |VER | NMETHODS | METHODS  |
		//  +----+----------+----------+
		//  | 1  |    1     | 1 to 255 |
		//  +----+----------+----------+
		//  [               ]
		// or
		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//  | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//    1    1      2        4                  variable       1
		//  [         ]
		// 读取[]里的部分.

		boost::system::error_code ec;

		[[maybe_unused]] auto bytes =
			co_await net::async_read(
				m_local_socket,
				m_local_buffer,
				net::transfer_exactly(2),
				net_awaitable[ec]);
		if (ec)
		{
			log_conn_error()
				<< ", read socks version: "
				<< ec.message();
			co_return;
		}
		BOOST_ASSERT(bytes == 2);

		auto p = (const char*)m_local_buffer.data().data();
		int socks_version = read<uint8_t>(p);

		if (socks_version == SOCKS_VERSION_5)
		{
			if (m_option.disable_socks_)
			{
				log_conn_debug()
					<< ", socks5 protocol disabled";
				co_return;
			}

			set_proto("socks5");

			log_conn_debug()
				<< ", socks version: "
				<< socks_version;

			co_await socks_connect_v5();
			co_return;
		}
		if (socks_version == SOCKS_VERSION_4)
		{
			if (m_option.disable_socks_)
			{
				log_conn_debug()
					<< ", socks4 protocol disabled";
				co_return;
			}

			set_proto("socks4");

			log_conn_debug()
				<< ", socks version: "
				<< socks_version;

			co_await socks_connect_v4();
			co_return;
		}

		bool ret = false;
		if (socks_version == 'G' || socks_version == 'P' ||
			socks_version == 'H' || socks_version == 'D' ||
			socks_version == 'O' || socks_version == 'T')
		{
			if (m_option.disable_http_)
			{
				log_conn_debug()
					<< ", http protocol disabled";
				co_return;
			}

			set_proto("http");

			ret = co_await http_proxy_get();
		}
		else if (socks_version == 'C')
		{
			if (m_option.disable_http_)
			{
				log_conn_debug()
					<< ", http protocol disabled";
				co_return;
			}

			set_proto("https");

			ret = co_await http_proxy_connect();
		}

		if (!ret)
		{
			auto date_string = server_date_string();
			auto fake_page =
				fmt::vformat(fake_400_content_fmt,
					fmt::make_format_args(date_string));

			co_await net::async_write(
				m_local_socket,
				net::buffer(fake_page),
				net::transfer_all(),
				net_awaitable[ec]);
		}

		co_return;
	}

} // namespace proxy
