//
// proxy_session_http_response.cpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_session.hpp"
#include "proxy/http_error_body.hpp"


namespace proxy {
	//////////////////////////////////////////////////////////////////////////
	// 构造函数与析构函数

	proxy_session::proxy_session(
		net::any_io_executor executor,
		net::io_context& backend_context,
		bool scheduler_locking,
		dns_cache& cache,
		variant_stream_type&& socket,
		size_t id,
		std::weak_ptr<proxy_server_base> server
	)
		: m_executor(executor)
		, m_backend_context(backend_context)
		, m_scheduler_locking(scheduler_locking)
		, m_dns_cache(cache)
		, m_local_socket(std::move(socket))
		, m_remote_socket(init_proxy_stream(executor))
		, m_connection_id(id)
		, m_local_buffer(10485760u) // 10MB max buffer size.
		, m_proxy_server(server)
	{
		// 记录连接建立时间（供 launcher 连接列表计算 elapsed）.
		m_started_at = static_cast<int64_t>(std::time(nullptr));
	}

	proxy_session::~proxy_session()
	{
		auto server = m_proxy_server.lock();
		if (!server)
			return;

		// 释放用户连接计数槽位（用户连接数限制）.
		if (!m_conn_user.empty())
			server->release_user_connection(m_conn_user);

		// 上报会话结束，并汇报累计流量.
		server->session_closed(m_connection_id,
			m_total_rx.load(), m_total_tx.load(), m_auth_user);

		// 打印当前 session 数量相关日志.
		auto num = server->num_session();

		log_conn_debug() << ", terminated, " << num << " active connections remaining.";
	}

	//////////////////////////////////////////////////////////////////////////
	// HTTP 响应辅助

	net::awaitable<void> proxy_session::send_http_string_response(
		const string_request& request,
		http::status status,
		std::string content_type,
		std::string body,
		bool keep_alive) noexcept
	{
		boost::system::error_code ec;

		string_response res{ status, request.version() };
		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string());
		res.set(http::field::content_type, std::move(content_type));
		res.keep_alive(keep_alive);
		res.body() = std::move(body);
		res.prepare_payload();

		http::serializer<false, string_body, http::fields> sr(res);
		co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", send http response error: "
				<< ec.message();
		}

		co_return;
	}

	net::awaitable<void> proxy_session::send_not_modified_http_route(
		const string_request& request,
		const std::string& etag,
		const std::string& last_modified) noexcept
	{
		boost::system::error_code ec;

		string_response res{ http::status::not_modified, request.version() };
		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string());
		if (!etag.empty())
			res.set(http::field::etag, etag);
		if (!last_modified.empty())
			res.set(http::field::last_modified, last_modified);
		res.keep_alive(request.keep_alive());
		res.body() = std::string{};
		res.prepare_payload();

		http::serializer<false, string_body, http::fields> sr(res);
		co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", send 304 response error: "
				<< ec.message();
		}

		co_return;
	}

	net::awaitable<void> proxy_session::default_http_route(
		const string_request& request, std::string response, http::status status) noexcept
	{
		co_await send_http_string_response(
			request, status, "text/html", std::move(response), true);
		co_return;
	}

	net::awaitable<void> proxy_session::location_http_route(
		const string_request& request, const std::string& path) noexcept
	{
		boost::system::error_code ec;

		string_response res{ http::status::moved_permanently, request.version() };
		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string());
		res.set(http::field::content_type, "text/html");
		res.set(http::field::location, path);

		res.keep_alive(true);
		res.body() = fake_302_content;
		res.prepare_payload();

		http::serializer<false, string_body, http::fields> sr(res);
		co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", location http route err: "
				<< ec.message();
		}

		co_return;
	}

	net::awaitable<void> proxy_session::forbidden_http_route(const string_request& request) noexcept
	{
		co_await send_http_string_response(
			request, http::status::forbidden, "text/html",
			fake_403_content, true);
		co_return;
	}

	net::awaitable<void> proxy_session::method_not_allowed_http_route(const string_request& request) noexcept
	{
		boost::system::error_code ec;

		// 405 响应必须携带 Allow 头, 声明服务端支持的方法.
		string_response res{ http::status::method_not_allowed, request.version() };
		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string());
		res.set(http::field::content_type, "text/html");
		res.set(http::field::allow, "GET, HEAD");
		res.keep_alive(true);
		res.body() = fake_405_content;
		res.prepare_payload();

		http::serializer<false, string_body, http::fields> sr(res);
		co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", method not allowed http route err: "
				<< ec.message();
		}

		co_return;
	}

	net::awaitable<void> proxy_session::unauthorized_http_route(const string_request& request) noexcept
	{
		boost::system::error_code ec;

		// WWW-Authenticate 头无法通过 send_http_string_response 设置, 需单独构建.
		string_response res{ http::status::unauthorized, request.version() };
		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string());
		res.set(http::field::content_type, "text/html; charset=UTF-8");
		res.set(http::field::www_authenticate, "Basic realm=\"proxy\"");
		res.keep_alive(true);
		res.body() = fake_401_content;
		res.prepare_payload();

		http::serializer<false, string_body, http::fields> sr(res);
		co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", unauthorized http route err: "
				<< ec.message();
		}

		co_return;
	}

} // namespace proxy
