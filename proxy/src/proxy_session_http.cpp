//
// proxy_session_http.cpp
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
	net::awaitable<void>
	proxy_session::static_web_server(http::request<http::string_body>& req, std::optional<request_parser>& parser) noexcept
	{
		boost::system::error_code ec;

		bool keep_alive = false;
		bool has_read_header = true;

		for (; !m_abort;)
		{
			if (!has_read_header)
			{
				// normal_web_server 调用是从 http_proxy_get
				// 跳转过来的, 该函数已经读取了请求头, 所以第1次不需
				// 要再次读取请求头, 即 has_read_header 为 true.
				// 当 keepalive 时，需要读取请求头, 此时 has_read_header
				// 为 false, 则在此读取和解析后续的 http 请求头.
				parser.emplace();
				parser->body_limit(1024 * 1024 * 10);
				m_local_buffer.consume(m_local_buffer.size());

				// 慢速请求防护: keepalive 后续请求读取同样受 tcp_timeout 约束.
				stream_expires_after(m_local_socket, std::chrono::seconds(m_option.tcp_timeout_));

				// keepalive 下须连同 body 一起读取: /dns-query 走 POST,
				// 只读请求头会导致后续请求 body 为空而被拒.
				co_await http::async_read(
					m_local_socket,
					m_local_buffer,
					*parser,
					net_awaitable[ec]);
				if (ec)
				{
					log_conn_debug()
						<< (keep_alive ? ", keepalive" : "")
						<< ", web async_read: "
						<< ec.message();
					co_return;
				}

				req = parser->release();
			}

			if (req[http::field::expect] == "100-continue")
			{
				http::response<http::empty_body> res;
				res.version(11);
				res.result(http::status::method_not_allowed);

				co_await http::async_write(
					m_local_socket,
					res,
					net_awaitable[ec]);
				if (ec)
				{
					log_conn_debug()
						<< ", web expect async_write: "
						<< ec.message();
				}
				co_return;
			}

			has_read_header = false;
			keep_alive = req.keep_alive();

			if (beast::websocket::is_upgrade(req))
			{
				auto date_string = server_date_string();
				auto fake_page = fmt::vformat(fake_404_content_fmt,
					fmt::make_format_args(date_string));

				co_await net::async_write(
					m_local_socket,
					net::buffer(fake_page),
					net::transfer_all(),
					net_awaitable[ec]);

				co_return;
			}

			// 静态文件服务只支持 GET/HEAD 方法, 其他方法返回 405.
			// /dns-query 为 DNS over HTTPS 接口, 单独支持 POST (由
			// on_http_dns_query 处理), 因此需要排除该路由.
			bool dns_proxy = m_option.dns_upstream_.has_value();
			if (req.method() != http::verb::get &&
				req.method() != http::verb::head &&
				!(dns_proxy &&
					boost::regex_match(
						std::string(req.target()),
						boost::regex{R"(^\/dns-query(.*)$)"})))
			{
				co_await method_not_allowed_http_route(req);
				co_return;
			}

			std::string target = req.target();
			boost::smatch what;
			auto target_path = make_real_target_path(
				m_option.doc_directory_, req.target());
			if (target_path.empty())
			{
				log_conn_warning()
					<< ", reject unsafe http target: "
					<< std::string(req.target());
				co_await default_http_route(
					req, fake_400_content, http::status::bad_request);
				co_return;
			}

			http_context http_ctx{
				{}, req, req.target(), target_path };

#define BEGIN_HTTP_ROUTE() if (false) {}
#define ON_HTTP_ROUTE(skip, exp, func) \
			else if (skip && boost::regex_match( \
				target, what, boost::regex{ exp })) { \
				for (auto i = 1; i < static_cast<int>(what.size()); i++) \
					http_ctx.regex_results_.emplace_back(what[i]); \
				co_await func(http_ctx); \
			}
#define END_HTTP_ROUTE() else { \
				co_await default_http_route( \
					req, \
					fake_400_content, \
					http::status::bad_request ); }

			BEGIN_HTTP_ROUTE()
				ON_HTTP_ROUTE(true, R"(^(.*)?\/$)", on_http_dir)
				ON_HTTP_ROUTE(true, R"(^(.*)?(\/\?r=json.*)$)", on_http_all_json)
				ON_HTTP_ROUTE(true, R"(^(.*)?(\/\?q=json.*)$)", on_http_json)
				ON_HTTP_ROUTE(dns_proxy, R"(^\/dns-query(.*)$)", on_http_dns_query)
				ON_HTTP_ROUTE(true, R"(^(?!.*\/$).*$)", on_http_get)
			END_HTTP_ROUTE()

			if (!keep_alive || !m_local_socket.is_open())
			{
				if (m_local_socket.is_open())
					co_await async_shutdown(m_local_socket, net_awaitable[ec]);
				break;
			}
		}

		co_await async_wait(m_local_socket, net_awaitable[ec]);

		co_return;
	}

} // namespace proxy
