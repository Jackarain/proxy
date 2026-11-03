//
// proxy_session_http_proxy.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_session.hpp"
#include "proxy/authority.hpp"
#include "proxy/http_error_body.hpp"
#include "proxy/proxy_websocket_client.hpp"

#include <array>
#include <charconv>

namespace proxy {

	net::awaitable<bool> proxy_session::http_proxy_get() noexcept
	{
		boost::system::error_code ec;
		bool keep_alive = false;
		std::optional<request_parser> parser;
		bool first = true;
		std::string remote_host;
		uint16_t remote_port = 0;

		while (!m_abort)
		{
			parser.emplace();
			parser->body_limit(1024 * 1024 * 10);
			if (!first)
				m_local_buffer.consume(m_local_buffer.size());

			// 慢速请求防护: 请求头/请求体读取期间同样受 tcp_timeout 约束,
			// 避免攻击者只发半个请求头或缓慢发送 body 长期占用连接.
			stream_expires_after(m_local_socket, std::chrono::seconds(m_option.tcp_timeout_));

			// 读取 http 请求头.
			auto req_bytes = co_await http::async_read(
				m_local_socket,
				m_local_buffer,
				*parser,
				net_awaitable[ec]);
			// 客户端上行请求数据计入 RX（供 launcher 状态统计）.
			m_total_rx += static_cast<uint64_t>(req_bytes);

			// 流量配额检查（上行+下行总和）: 超出配额立即断开连接.
			if (quota_exceeded())
			{
				log_conn_warning()
					<< ", http_proxy_get traffic quota exceeded, closing connection, user: "
					<< m_auth_user;
				close();
				co_return false;
			}

			if (ec)
			{
				log_conn_warning()
					<< (keep_alive ? ", keepalive" : "")
					<< ", http_proxy_get request async_read: "
					<< ec.message();

				co_return !first;
			}

			auto req = parser->release();
			auto mth = std::string(req.method_string());
			auto target_view = std::string(req.target());
			auto pa = std::string(req[http::field::proxy_authorization]);

			keep_alive = req.keep_alive();

			log_conn_debug()
				<< ", method: " << mth
				<< ", target: " << target_view
				<< (pa.empty() ? std::string()
					: ", proxy_authorization: " + pa);

			// WebSocket CONNECT 隧道优先于普通 HTTP 静态站点/URL 代理处理.
			if (m_option.websocket_proxy_ &&
				beast::websocket::is_upgrade(req) &&
				(req.find(ws_tunnel::proxy_connect_header) != req.end() ||
				 req.find(ws_tunnel::proxy_connect_header_legacy) != req.end() ||
				 req.find(ws_tunnel::proxy_connect_udp_header) != req.end() ||
				 req.find(ws_tunnel::proxy_connect_udp_header_legacy) != req.end()))
			{
				co_await http_proxy_websocket_connect(req);
				co_return true;
			}

			// 判定是否为 GET url 代理模式.
			bool get_url_proxy = false;
			boost::system::result<url_info> expect_url;

			if (boost::istarts_with(target_view, "https://") ||
				boost::istarts_with(target_view, "http://"))
			{
				get_url_proxy = true;
				expect_url = parse_urlinfo(target_view);
			}

			// http 代理认证, 如果请求的 rarget 不是 http url 或认证
			// 失败, 则按正常 web 请求处理.
			auto auth_result = co_await http_authorization(pa);
			if (auth_result != PROXY_AUTH_SUCCESS || !get_url_proxy)
			{
				// 如果 doc 目录为空, 则不允许访问目录
				// 这里直接返回错误页面.
				if (m_option.doc_directory_.empty())
					co_return !first;

				// htpasswd 表示需要用户认证.
				if (m_option.htpasswd_)
				{
					// 处理 http 认证, 如果客户没有传递认证信息, 则返回 401.
					// 如果用户认证信息没有设置, 则直接返回 401.
					if (pa.empty() || auth_required())
					{
						log_conn_warning()
							<< ", auth error: "
							<< (pa.empty() ? "no auth" : "no user");

						co_await unauthorized_http_route(req);
						co_return true;
					}

					if (auth_result != PROXY_AUTH_SUCCESS)
					{
						log_conn_warning()
							<< ", auth error: "
							<< pauth_error_message(auth_result);

						co_await unauthorized_http_route(req);
						co_return true;
					}
				}

				// 如果不允许目录索引, 检查请求的是否为文件, 如果是具体文件则按文
				// 件请求处理, 否则返回 403.
				if (!m_option.autoindex_)
				{
					auto path = make_real_target_path(m_option.doc_directory_, req.target());

					if (!fs::is_directory(path, ec))
					{
						co_await static_web_server(req, parser);
						co_return true;
					}

					// 如果不允许目录索引, 则直接返回 403 forbidden.
					co_await forbidden_http_route(req);

					co_return true;
				}

				// 按正常 http 目录请求来处理.
				co_await static_web_server(req, parser);
				co_return true;
			}

			// 检查 url 是否解析成功.
			if (expect_url.has_error())
			{
				log_conn_warning()
					<< ", parse url: " << target_view
					<< ", error: " << expect_url.error().message();

				co_return false;
			}

			// 解构 url 中的信息.
			auto [scheme, user, passwd, host, port, resource] = *expect_url;
			std::string request_host(host);

			// RFC 9298 CONNECT-UDP 检测: 检查是否为 connect-udp upgrade 请求.
			bool http_connect_udp = is_connect_udp_upgrade_request(req);

			if (http_connect_udp)
			{
				// 从 URI path 中解析 target_host 和 target_port.
				std::string target_host;
				uint16_t target_port = 0;

				// 格式: /.well-known/masque/udp/{target_host}/{target_port}/
				if (!parse_connect_udp_target(resource, target_host, target_port))
				{
					log_conn_warning()
						<< ", connect-udp: invalid target from path: "
						<< resource;

					co_return false;
				}

				log_conn_debug()
					<< ", connect-udp request for target: "
					<< target_host << ":" << target_port;

				// 处理 connect-udp 请求.
				co_await http_proxy_connect_udp(req, target_host, target_port);

				// 完成 connect-udp 请求后返回.
				co_return !first;
			}

			// 客户端 keep-alive 连接可能切换目标, 不能复用旧目标的上游连接.
			if (m_remote_socket.is_open() &&
				(remote_host != request_host || remote_port != port))
			{
				boost::system::error_code close_ec;
				m_remote_socket.close(close_ec);
				m_remote_socket = init_proxy_stream(m_executor);
			}

			if (!m_remote_socket.is_open())
			{
				// 连接到目标主机.
				ec = co_await start_connect_host(request_host, port, false);
				if (ec)
				{
					log_conn_warning()
						<< ", connect to target "
						<< host
						<< ":"
						<< port
						<< " error: "
						<< ec.message();

					co_return true;
				}

				remote_host = request_host;
				remote_port = port;
			}

			// 处理代理请求头.
			if (resource.empty())
				req.target("/");
			else
				req.target(std::string(resource));

			req.set(http::field::host,
				authority::format_authority(host, port));

			if (req.find(http::field::connection) == req.end() &&
				req.find(http::field::proxy_connection) != req.end())
				req.set(http::field::connection, req[http::field::proxy_connection]);

			req.erase(http::field::proxy_authorization);
			req.erase(http::field::proxy_connection);

			co_await http::async_write(
				m_remote_socket, req, net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", http_proxy_get request async_write: "
					<< ec.message();
				co_return !first;
			}

			m_local_buffer.consume(m_local_buffer.size());
			beast::flat_buffer buf;
			// 预分配读缓冲, 否则 flat_buffer 每次只读 512 字节,
			// 导致大文件传输退化为每块 512 字节的系统调用.
			buf.reserve(1024 * 1024);

			// 流式转发响应 body, 避免将整个响应缓冲在内存中, 同时解除
			// 对超大响应(Content-Length 超过 10MiB)的 body_limit 限制.
			http::response_parser<http::buffer_body> resp_parser;
			resp_parser.body_limit(boost::none);
			resp_parser.eager(true);

			// HEAD 请求的响应不包含 body, 让解析器跳过 body 解析.
			if (mth == "HEAD")
				resp_parser.skip(true);

			// 读取响应头.
			co_await http::async_read_header(
				m_remote_socket, buf, resp_parser, net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", http_proxy_get response async_read_header: "
					<< ec.message();
				co_return !first;
			}

			auto& res = resp_parser.get();

			const bool skip_body = resp_parser.skip();
			const bool is_chunked = res.chunked();
			const bool has_length = res.payload_size().has_value();

			// chunked 响应在解码后透传, 因此必须移除 Transfer-Encoding,
			// 否则客户端会按 chunked 编码解析解码后的数据.
			if (is_chunked)
				res.erase(http::field::transfer_encoding);

			// 若 body 长度未知(chunked 或依赖连接关闭终止)且未跳过 body,
			// 客户端无法依据长度判断 body 结束, 只能依赖连接关闭, 因此
			// 强制本地连接在响应后关闭.
			if (!skip_body && (is_chunked || !has_length))
				res.keep_alive(false);

			// 仅转发响应头.
			{
				http::serializer<false, http::buffer_body> sr(res);
				// 请求阶段的固定截止时间不随传输顺延, 转发响应前须按
				// 当前时刻刷新超时, 避免长时间回传时首个写操作被立即判超时.
				stream_expires_after(m_local_socket, std::chrono::seconds(m_option.tcp_timeout_));
				co_await http::async_write_header(
					m_local_socket, sr, net_awaitable[ec]);
			}
			if (ec)
			{
				log_conn_warning()
					<< ", http_proxy_get response async_write_header: "
					<< ec.message();
				co_return true;
			}

			// 流式转发响应 body. 使用 malloc 而非 std::vector<char>,
			// 避免构造时对缓冲做无谓的零初始化, 浪费 CPU 周期.
			constexpr std::size_t body_buf_size = 1024 * 1024;
			std::unique_ptr<char, decltype(&std::free)> body_buf(
				(char*)std::malloc(body_buf_size), &std::free);
			res.body().data = body_buf.get();
			res.body().size = body_buf_size;

			std::size_t resp_bytes = 0;

			while (!resp_parser.is_done())
			{
				boost::system::error_code sec;
				auto bytes = co_await http::async_read_some(
					m_remote_socket, buf, resp_parser, net_awaitable[sec]);
				resp_bytes += bytes;
				// 响应数据发送给客户端，计入 TX（供 launcher 状态统计）.
				m_total_tx += static_cast<uint64_t>(bytes);

				// 流量配额检查（上行+下行总和）: 超出配额立即断开连接.
				if (quota_exceeded())
				{
					log_conn_warning()
						<< ", http_proxy_get traffic quota exceeded, closing connection, user: "
						<< m_auth_user;
					close();
					co_return true;
				}

				if (sec == http::error::need_buffer)
				{
					// body 缓冲已满, 需要转发其中的数据.
					sec = {};
				}
				else if (sec)
				{
					log_conn_warning()
						<< ", http_proxy_get response async_read_some: "
						<< sec.message();
					break;
				}

				const auto n = body_buf_size - res.body().size;
				if (n > 0)
				{
					// 每轮写出前刷新超时, 使活跃下载不受固定截止时间限制.
					stream_expires_after(m_local_socket, std::chrono::seconds(m_option.tcp_timeout_));
					co_await net::async_write(
						m_local_socket,
						net::buffer(body_buf.get(), n),
						net_awaitable[ec]);
					if (ec)
					{
						log_conn_warning()
							<< ", http_proxy_get response body async_write: "
							<< ec.message();
						break;
					}

					res.body().data = body_buf.get();
					res.body().size = body_buf_size;
				}
			}

			// 响应 body 未完整转发(出错或提前截断)时, 不能继续复用连接.
			if (!resp_parser.is_done())
			{
				if (m_local_socket.is_open())
					co_await async_shutdown(m_local_socket, net_awaitable[ec]);
				if (m_remote_socket.is_open())
					co_await async_shutdown(m_remote_socket, net_awaitable[ec]);

				co_return true;
			}

			log_conn_debug()
				<< ", transfer completed"
				<< ", remote to local: "
				<< resp_bytes;

			first = false;

			if (!keep_alive || !res.keep_alive())
			{
				if (m_local_socket.is_open())
					co_await async_shutdown(m_local_socket, net_awaitable[ec]);
				if (m_remote_socket.is_open())
					co_await async_shutdown(m_remote_socket, net_awaitable[ec]);

				break;
			}
		}

		co_return true;
	}

} // namespace proxy
