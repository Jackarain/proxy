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

	//////////////////////////////////////////////////////////////////////////
	// HTTP 代理请求处理

	net::awaitable<int> proxy_session::http_authorization(std::string_view pa) noexcept
	{
		if (!auth_required())
			co_return PROXY_AUTH_SUCCESS;

		if (pa.empty())
			co_return PROXY_AUTH_NONE;

		auto pos = pa.find(' ');
		if (pos == std::string::npos)
			co_return PROXY_AUTH_ILLEGAL;

		auto type = pa.substr(0, pos);
		auto auth = pa.substr(pos + 1);

		if (!boost::iequals(type, "Basic"))
			co_return PROXY_AUTH_ILLEGAL;

		std::string userinfo;
		if (!strutil::base64_decode_strict(auth, userinfo))
			co_return PROXY_AUTH_ILLEGAL;

		pos = userinfo.find(':');
		if (pos == std::string::npos)
			co_return PROXY_AUTH_ILLEGAL;

		std::string uname = userinfo.substr(0, pos);
		std::string passwd = userinfo.substr(pos + 1);

		bool verify_passed = co_await check_userpasswd(uname, passwd);
		if (!verify_passed)
			co_return PROXY_AUTH_FAILED;

		co_return PROXY_AUTH_SUCCESS;
	}

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

	net::awaitable<bool> proxy_session::http_proxy_connect() noexcept
	{
		boost::system::error_code ec;
		http::request_parser<http::string_body> parser;

		parser.header_limit(128 * 1024);

		// 慢速请求防护: CONNECT 请求头读取同样受 tcp_timeout 约束.
		stream_expires_after(m_local_socket, std::chrono::seconds(m_option.tcp_timeout_));

		// 读取 http 请求头.
		co_await http::async_read_header(m_local_socket,
			m_local_buffer, parser, net_awaitable[ec]);
		if (ec)
		{
			log_conn_error()
				<< ", http_proxy_connect async_read: "
				<< ec.message();

			co_return false;
		}

		http::request<http::string_body> req = parser.release();

		auto mth = std::string(req.method_string());
		auto target_view = std::string(req.target());
		auto pa = std::string(req[http::field::proxy_authorization]);

		std::string padding_value;
		std::size_t padding_length = 0;

		// 如果有 padding 头, 则回复一个随机长度的 padding 头, 以迷惑扫描者.
		if (req.find("Proxy-Padding") != req.end())
		{
			auto noise_length = req["Proxy-Padding"].size();
			padding_length = noise_length;

			std::mt19937 gen(global_random_device()());
			std::uniform_int_distribution<std::size_t> dis(16, std::max<std::size_t>(16, noise_length));
			noise_length = dis(gen);

			auto noise = generate_noise(global_random_device(), static_cast<uint16_t>(noise_length));
			auto len = boost::beast::detail::base64::encoded_size(noise_length);
			padding_value.resize(len);
			len = beast::detail::base64::encode(padding_value.data(), noise.data(), noise.size());
			padding_value.resize(len);
		}

		log_conn_debug()
			<< ", method: " << mth
			<< ", target: " << target_view
			<< (pa.empty() ? std::string()
				: ", proxy_authorization: " + pa)
			<< (padding_length > 0 ?
				", in padding: " + std::to_string(padding_length)
				: std::string())
			<< (padding_value.empty() ? std::string() :
				", out padding: " + std::to_string(padding_value.size()));

		// http 代理认证.
		auto auth = co_await http_authorization(pa);
		if (auth != PROXY_AUTH_SUCCESS)
		{
			log_conn_warning()
				<< ", proxy auth error: "
				<< pauth_error_message(auth);

			auto date_string = server_date_string();
			auto fake_page = fmt::vformat(fake_407_content_fmt,
				fmt::make_format_args(date_string));

			co_await net::async_write(
				m_local_socket,
				net::buffer(fake_page),
				net::transfer_all(),
				net_awaitable[ec]);

			co_return true;
		}

		std::string host;
		uint16_t port = 0;
		if (!ws_tunnel::parse_authority(target_view, host, port))
		{
			log_conn_error()
				<< ", illegal target: "
				<< target_view;
			co_return false;
		}

		ec = co_await start_connect_host(host, port, false);
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

		http::response<http::empty_body> res{
			http::status::ok, req.version() };
		res.reason("Connection established");

		if (!padding_value.empty())
			res.insert("Proxy-Padding", padding_value);

		co_await http::async_write(
			m_local_socket,
			res,
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", write http connect response "
				<< host
				<< ":"
				<< port
				<< " error: "
				<< ec.message();

			co_return false;
		}

		// 清除请求阶段设置的 socket 超时, 隧道期的超时交给 idle_timeout
		// 管理, 否则 Beast 的固定截止时间会因上传方向读挂起而误断活跃连接.
		stream_expires_never(m_local_socket);

		co_await concurrent_transfer();

		co_return true;
	}

	net::awaitable<bool> proxy_session::http_proxy_websocket_connect(
		const string_request& req) noexcept
	{
		boost::system::error_code ec;

		auto send_error = [this, &req](http::status status,
			std::string_view authenticate = {}) -> net::awaitable<void>
		{
			http::response<http::string_body> res{ status, req.version() };
			res.set(http::field::server, version_string);
			res.set(http::field::date, server_date_string());
			res.keep_alive(false);
			if (!authenticate.empty())
				res.set(http::field::proxy_authenticate, authenticate);
			res.prepare_payload();

			boost::system::error_code ignore_ec;
			co_await http::async_write(
				m_local_socket, res, net_awaitable[ignore_ec]);
		};

		auto pa = std::string(req[http::field::proxy_authorization]);
		auto auth = co_await http_authorization(pa);
		if (auth != PROXY_AUTH_SUCCESS)
		{
			log_conn_warning()
				<< ", websocket proxy auth error: "
				<< pauth_error_message(auth);
			co_await send_error(
				http::status::proxy_authentication_required,
				"Basic realm=\"proxy\"");
			co_return true;
		}

		if (req.method() != http::verb::get || req.version() != 11 ||
			!beast::websocket::is_upgrade(req) ||
			req.find(http::field::host) == req.end())
		{
			co_await send_error(http::status::bad_request);
			co_return true;
		}

		auto tcp_count = req.count(ws_tunnel::proxy_connect_header) +
			req.count(ws_tunnel::proxy_connect_header_legacy);
		auto udp_count = req.count(ws_tunnel::proxy_connect_udp_header) +
			req.count(ws_tunnel::proxy_connect_udp_header_legacy);
		if (tcp_count + udp_count != 1)
		{
			co_await send_error(http::status::bad_request);
			co_return true;
		}

		const bool datagram = udp_count == 1;
		auto protocol = req[http::field::sec_websocket_protocol];
		auto expected_protocol = datagram ?
			ws_tunnel::proxy_connect_udp_subprotocol :
			ws_tunnel::proxy_connect_subprotocol;
		if (!ws_tunnel::header_has_token(protocol, expected_protocol))
		{
			co_await send_error(http::status::bad_request);
			co_return true;
		}

		auto target_it = datagram ?
			req.find(ws_tunnel::proxy_connect_udp_header) :
			req.find(ws_tunnel::proxy_connect_header);
		if (target_it == req.end())
		{
			target_it = req.find(datagram ?
				ws_tunnel::proxy_connect_udp_header_legacy :
				ws_tunnel::proxy_connect_header_legacy);
		}

		std::string target_host;
		uint16_t target_port = 0;
		if (!ws_tunnel::parse_authority(
			target_it->value(), target_host, target_port))
		{
			co_await send_error(http::status::bad_request);
			co_return true;
		}

		if (!boost::variant2::holds_alternative<proxy_tcp_socket>(m_local_socket) &&
			!boost::variant2::holds_alternative<ssl_tcp_stream>(m_local_socket))
		{
			co_await send_error(http::status::not_implemented);
			co_return true;
		}

		log_conn_debug()
			<< ", websocket proxy target: "
			<< target_host << ":" << target_port;

		const bool downstream_tls =
			boost::variant2::holds_alternative<ssl_tcp_stream>(m_local_socket);
		set_target(target_host + ":" + std::to_string(target_port));
		if (datagram)
		{
			co_await http_proxy_websocket_connect_udp(
				req, target_host, target_port);
			co_return true;
		}

		ec = co_await start_connect_host(target_host, target_port, false);
		if (ec)
		{
			log_conn_warning()
				<< ", websocket proxy connect target "
				<< target_host << ":" << target_port
				<< " error: " << ec.message();
			co_await send_error(http::status::bad_gateway);
			co_return true;
		}

		if (boost::variant2::holds_alternative<proxy_tcp_socket>(m_local_socket))
		{
			websocket_plain_stream websocket(
				std::move(boost::variant2::get<proxy_tcp_socket>(m_local_socket)));
			m_local_socket = init_proxy_stream(std::move(websocket));
			auto buffered = m_local_buffer.size();
			ec = co_await boost::variant2::get<websocket_plain_stream>(
				m_local_socket).async_accept(req, m_local_buffer.data());
			if (!ec)
				m_local_buffer.consume(buffered);
		}
		else
		{
			websocket_tls_stream websocket(
				std::move(boost::variant2::get<ssl_tcp_stream>(m_local_socket)));
			m_local_socket = init_proxy_stream(std::move(websocket));
			auto buffered = m_local_buffer.size();
			ec = co_await boost::variant2::get<websocket_tls_stream>(
				m_local_socket).async_accept(req, m_local_buffer.data());
			if (!ec)
				m_local_buffer.consume(buffered);
		}

		if (ec)
		{
			log_conn_warning()
				<< ", websocket proxy handshake error: " << ec.message();
			co_return true;
		}

		set_proto(std::string(ws_tunnel::websocket_proto_name(downstream_tls)));
		stream_expires_never(m_local_socket);
		co_await concurrent_transfer();
		co_return true;
	}

	net::awaitable<std::optional<udp::socket>> proxy_session::open_udp_target(
		const std::string& target_host, uint16_t target_port) noexcept
	{
		boost::system::error_code ec;
		net::ip::address target_addr;
		udp protocol = udp::v4();

		target_addr = net::ip::make_address(target_host, ec);
		if (ec)
		{
			co_await switch_to_backend_executor();

			udp::resolver resolver(m_backend_context);
			auto results = co_await resolver.async_resolve(
				target_host, std::to_string(target_port), net_awaitable[ec]);

			co_await switch_from_backend_executor();

			if (ec || results.empty())
			{
				log_conn_warning()
					<< ", websocket udp: DNS resolve failed for "
					<< target_host << ": " << ec.message();
				co_return std::nullopt;
			}

			target_addr = results.begin()->endpoint().address();
		}

		protocol = target_addr.is_v4() ? udp::v4() : udp::v6();
		udp::socket socket(m_executor);
		socket.open(protocol, ec);
		if (ec)
		{
			log_conn_warning()
				<< ", websocket udp: open socket: " << ec.message();
			co_return std::nullopt;
		}

		if (m_bind_interface)
		{
			socket.bind(udp::endpoint(*m_bind_interface, 0), ec);
			if (ec)
			{
				log_conn_warning()
					<< ", websocket udp: bind socket: " << ec.message();
				co_return std::nullopt;
			}
		}

		if (m_option.so_mark_)
		{
			auto ret = apply_so_mark(socket.native_handle(), m_option.so_mark_);
			if (ret.has_error())
			{
				log_conn_warning()
					<< ", websocket udp: set socket mark: "
					<< ret.error().message();
			}
		}

		socket.connect(udp::endpoint(target_addr, target_port), ec);
		if (ec)
		{
			log_conn_warning()
				<< ", websocket udp: connect "
				<< target_host << ":" << target_port
				<< ": " << ec.message();
			co_return std::nullopt;
		}

		co_return std::optional<udp::socket>(std::move(socket));
	}

	net::awaitable<bool> proxy_session::connect_websocket_udp_proxy_pass(
		const std::string& target_host, uint16_t target_port) noexcept
	{
		boost::system::error_code ec;
		tcp::socket remote_socket(m_executor);
		auto targets = co_await resolve_proxy_pass_targets();
		ec = co_await connect_proxy_pass(remote_socket, targets);
		if (ec)
		{
			log_conn_warning()
				<< ", websocket udp: connect proxy_pass: " << ec.message();
			co_return false;
		}

		bool use_ssl = m_option.proxy_pass_use_ssl_ ||
			proxy_scheme_use_ssl(m_proxy_pass->scheme());
		m_remote_socket = co_await instantiate_proxy_pass(
			std::move(remote_socket), use_ssl);

		auto options = make_websocket_handshake_options(
			*m_proxy_pass, target_host, target_port, true);
		ec = co_await async_websocket_proxy_handshake(
			m_remote_socket, std::move(options));
		if (ec)
		{
			log_conn_warning()
				<< ", websocket udp: upstream handshake: " << ec.message();
			stream_cancel(m_remote_socket);
			co_return false;
		}

		stream_expires_never(m_remote_socket);
		co_return true;
	}

	net::awaitable<boost::system::error_code>
	proxy_session::accept_websocket_udp(const string_request& req) noexcept
	{
		if (boost::variant2::holds_alternative<proxy_tcp_socket>(
			m_local_socket))
		{
			websocket_plain_stream websocket(std::move(
				boost::variant2::get<proxy_tcp_socket>(m_local_socket)));
			m_local_socket = init_proxy_stream(std::move(websocket));
			auto buffered = m_local_buffer.size();
			auto ec = co_await boost::variant2::get<websocket_plain_stream>(
				m_local_socket).async_accept(req, m_local_buffer.data(),
					ws_tunnel::proxy_connect_udp_subprotocol);
			if (!ec)
				m_local_buffer.consume(buffered);
			co_return ec;
		}

		if (boost::variant2::holds_alternative<ssl_tcp_stream>(m_local_socket))
		{
			websocket_tls_stream websocket(std::move(
				boost::variant2::get<ssl_tcp_stream>(m_local_socket)));
			m_local_socket = init_proxy_stream(std::move(websocket));
			auto buffered = m_local_buffer.size();
			auto ec = co_await boost::variant2::get<websocket_tls_stream>(
				m_local_socket).async_accept(req, m_local_buffer.data(),
					ws_tunnel::proxy_connect_udp_subprotocol);
			if (!ec)
				m_local_buffer.consume(buffered);
			co_return ec;
		}

		co_return make_error_code(boost::system::errc::operation_not_supported);
	}

	net::awaitable<void> proxy_session::run_websocket_udp_bridge() noexcept
	{
		auto self = shared_from_this();
		m_last_activity.store(0, std::memory_order_relaxed);
		m_transfer_timeout.store(m_option.udp_timeout_, std::memory_order_relaxed);

		auto local_to_remote = [this, self]() -> net::awaitable<void>
		{
			beast::flat_buffer message;
			while (!m_abort)
			{
				auto ec = co_await async_websocket_read_binary(
					m_local_socket, message);
				if (ec)
					break;

				auto data = message.data();
				std::string_view payload;
				if (!decode_datagram_message(std::string_view(
					static_cast<const char*>(data.data()), data.size()),
					payload))
					continue;

				ec = co_await async_websocket_write_binary(
					m_remote_socket, data);
				if (ec)
					break;

				m_total_rx += static_cast<uint64_t>(payload.size());
				m_last_activity.store(0, std::memory_order_relaxed);
			}

			m_abort = true;
			stream_cancel(m_local_socket);
			stream_cancel(m_remote_socket);
		};

		auto remote_to_local = [this, self]() -> net::awaitable<void>
		{
			beast::flat_buffer message;
			while (!m_abort)
			{
				auto ec = co_await async_websocket_read_binary(
					m_remote_socket, message);
				if (ec)
					break;

				auto data = message.data();
				std::string_view payload;
				if (!decode_datagram_message(std::string_view(
					static_cast<const char*>(data.data()), data.size()),
					payload))
					continue;

				ec = co_await async_websocket_write_binary(
					m_local_socket, data);
				if (ec)
					break;

				m_total_tx += static_cast<uint64_t>(payload.size());
				m_last_activity.store(0, std::memory_order_relaxed);
			}

			m_abort = true;
			stream_cancel(m_local_socket);
			stream_cancel(m_remote_socket);
		};

		co_await(
			(local_to_remote() && remote_to_local()) ||
			idle_timeout(m_local_socket, m_remote_socket)
		);
	}

	net::awaitable<void> proxy_session::run_websocket_udp_target(
		udp::socket& socket) noexcept
	{
		auto self = shared_from_this();
		m_last_activity.store(0, std::memory_order_relaxed);
		m_transfer_timeout.store(m_option.udp_timeout_, std::memory_order_relaxed);

		auto websocket_to_udp = [this, self, &socket]()
			-> net::awaitable<void>
		{
			beast::flat_buffer message;
			boost::system::error_code ec;
			while (!m_abort)
			{
				ec = co_await async_websocket_read_binary(
					m_local_socket, message);
				if (ec)
					break;

				auto data = message.data();
				std::string_view payload;
				if (!decode_datagram_message(std::string_view(
					static_cast<const char*>(data.data()), data.size()),
					payload))
					continue;

				co_await socket.async_send(
					net::buffer(payload.data(), payload.size()),
					net_awaitable[ec]);
				if (ec)
					break;

				m_total_rx += static_cast<uint64_t>(payload.size());
				m_last_activity.store(0, std::memory_order_relaxed);
			}

			m_abort = true;
			stream_cancel(m_local_socket);
			socket.close(ec);
		};

		auto udp_to_websocket = [this, self, &socket]()
			-> net::awaitable<void>
		{
			std::array<char, 65535> payload{};
			std::array<uint8_t, 65536> capsule{};
			boost::system::error_code ec;
			while (!m_abort)
			{
				auto bytes = co_await socket.async_receive(
					net::buffer(payload), net_awaitable[ec]);
				if (ec)
					break;

				auto size = encode_datagram_capsule(
					payload.data(), bytes, capsule.data(), capsule.size());
				if (size == 0)
					continue;

				ec = co_await async_websocket_write_binary(
					m_local_socket, net::buffer(capsule.data(), size));
				if (ec)
					break;

				m_total_tx += static_cast<uint64_t>(bytes);
				m_last_activity.store(0, std::memory_order_relaxed);
			}

			m_abort = true;
			stream_cancel(m_local_socket);
			socket.close(ec);
		};

		co_await(
			(websocket_to_udp() && udp_to_websocket()) ||
			idle_timeout(m_local_socket, m_remote_socket)
		);
	}

	net::awaitable<void> proxy_session::http_proxy_websocket_connect_udp(
		const string_request& req,
		const std::string& target_host,
		uint16_t target_port) noexcept
	{
		auto send_error = [this, &req](http::status status,
			std::string_view message) -> net::awaitable<void>
		{
			http::response<http::string_body> res{ status, req.version() };
			res.set(http::field::server, version_string);
			res.set(http::field::date, server_date_string());
			res.set(http::field::content_type, "text/plain");
			res.keep_alive(false);
			res.body() = message;
			res.prepare_payload();

			boost::system::error_code ignore_ec;
			co_await http::async_write(
				m_local_socket, res, net_awaitable[ignore_ec]);
		};

		auto scheme = m_proxy_pass ?
			boost::to_lower_copy(std::string(m_proxy_pass->scheme())) :
			std::string();
		const bool ws_upstream = m_proxy_pass &&
			proxy_scheme_is_websocket(scheme);
		const bool downstream_tls =
			boost::variant2::holds_alternative<ssl_tcp_stream>(m_local_socket);

		if (m_option.disable_udp_)
		{
			co_await send_error(http::status::forbidden,
				"UDP proxy is disabled.\n");
			co_return;
		}

		if (m_proxy_pass && !ws_upstream)
		{
			co_await send_error(http::status::not_implemented,
				"WebSocket UDP requires a ws/wss proxy_pass.\n");
			co_return;
		}

		if (ws_upstream)
		{
			if (!co_await connect_websocket_udp_proxy_pass(
				target_host, target_port))
			{
				co_await send_error(http::status::bad_gateway,
					"Failed to establish upstream WebSocket UDP tunnel.\n");
				co_return;
			}

			auto ec = co_await accept_websocket_udp(req);
			if (ec)
			{
				log_conn_warning()
					<< ", websocket udp: downstream handshake: "
					<< ec.message();
				stream_cancel(m_remote_socket);
				co_return;
			}

			stream_expires_never(m_local_socket);
			set_proto(std::string(ws_tunnel::websocket_proto_name(
				downstream_tls, true)));
			co_await run_websocket_udp_bridge();
			co_return;
		}

		auto udp_socket = co_await open_udp_target(target_host, target_port);
		if (!udp_socket)
		{
			co_await send_error(http::status::bad_gateway,
				"Failed to open UDP target.\n");
			co_return;
		}

		auto ec = co_await accept_websocket_udp(req);
		if (ec)
		{
			log_conn_warning()
				<< ", websocket udp: downstream handshake: "
				<< ec.message();
			udp_socket->close(ec);
			co_return;
		}

		stream_expires_never(m_local_socket);
		set_proto(std::string(ws_tunnel::websocket_proto_name(
			downstream_tls, true)));
		co_await run_websocket_udp_target(*udp_socket);
	}

	net::awaitable<void> proxy_session::http_capsules_to_websocket_proxy() noexcept
	{
		auto self = shared_from_this();
		m_last_activity.store(0, std::memory_order_relaxed);
		m_transfer_timeout.store(m_option.udp_timeout_, std::memory_order_relaxed);

		auto http_to_websocket = [this, self]() -> net::awaitable<void>
		{
			std::array<uint8_t, 65536> capsule{};
			boost::system::error_code ec;
			while (!m_abort)
			{
				auto [type, value] = co_await read_capsule_from_stream(
					m_local_socket, m_local_buffer, ec);
				if (ec)
					break;

				if (type != udp_proxy_capsule_type)
					continue;

				std::string_view payload;
				if (!decode_datagram_capsule(value, payload))
					continue;

				auto size = encode_datagram_capsule(payload.data(),
					payload.size(), capsule.data(), capsule.size());
				if (size == 0)
					continue;

				ec = co_await async_websocket_write_binary(
					m_remote_socket, net::buffer(capsule.data(), size));
				if (ec)
					break;

				m_total_rx += static_cast<uint64_t>(payload.size());
				m_last_activity.store(0, std::memory_order_relaxed);
			}

			m_abort = true;
			stream_cancel(m_local_socket);
			stream_cancel(m_remote_socket);
		};

		auto websocket_to_http = [this, self]() -> net::awaitable<void>
		{
			beast::flat_buffer message;
			boost::system::error_code ec;
			while (!m_abort)
			{
				ec = co_await async_websocket_read_binary(
					m_remote_socket, message);
				if (ec)
					break;

				auto data = message.data();
				std::string_view payload;
				if (!decode_datagram_message(std::string_view(
					static_cast<const char*>(data.data()), data.size()),
					payload))
					continue;

				co_await net::async_write(
					m_local_socket, data, net_awaitable[ec]);
				if (ec)
					break;

				m_total_tx += static_cast<uint64_t>(payload.size());
				m_last_activity.store(0, std::memory_order_relaxed);
			}

			m_abort = true;
			stream_cancel(m_local_socket);
			stream_cancel(m_remote_socket);
		};

		co_await(
			(http_to_websocket() && websocket_to_http()) ||
			idle_timeout(m_local_socket, m_remote_socket)
		);
		co_return;
	}

	net::awaitable<void> proxy_session::http_proxy_connect_udp(
		const http::request<http::string_body>& req,
		const std::string& target_host,
		uint16_t target_port) noexcept
	{
		boost::system::error_code ec;

		// 如果禁用了 UDP 代理服务, 则拒绝 connect-udp 请求.
		if (m_option.disable_udp_)
		{
			log_conn_warning()
				<< ", connect-udp: udp protocol disabled";

			http::response<http::string_body> res{
				http::status::forbidden, req.version()};
			res.set(http::field::content_type, "text/html");
			res.body() = fake_403_forbidden_content;
			res.prepare_payload();

			co_await http::async_write(
				m_local_socket, res, net_awaitable[ec]);
			co_return;
		}

		// 如果指定了 proxy_pass（有下级代理服务器）.
		if (m_proxy_pass)
		{
			co_await http_proxy_udp_proxy_pass(req, target_host, target_port);
			co_return;
		}

		// 以下为终端代理场景: 无 proxy_pass, 直接与目标 UDP 服务器通信.
		log_conn_debug()
			<< ", connect-udp: final proxy, creating UDP socket to "
			<< target_host << ":" << target_port;

		// 解析目标主机 (如果是域名需要 DNS 解析).
		auto protocol = udp::v4();
		net::ip::address target_addr;

		// 尝试将目标主机解析为 IP 地址, 若发生错误, 则说明是域名需要通过 DNS 解析.
		auto addr = net::ip::make_address(target_host, ec);
		if (ec)
		{
			co_await switch_to_backend_executor();

			udp::resolver resolver(m_backend_context);
			auto results = co_await resolver.async_resolve(
				target_host, std::to_string(target_port), net_awaitable[ec]);

			co_await switch_from_backend_executor();

			if (ec || results.empty())
			{
				log_conn_warning()
					<< ", connect-udp: DNS resolve failed for "
					<< target_host << ": " << ec.message();

				http::response<http::string_body> res{
					http::status::bad_gateway,
					req.version() };

				res.set(http::field::content_type, "text/html");
				res.set("Proxy-Status", "dns_error");
				res.body() = fake_502_dns_content;
				res.prepare_payload();

				co_await http::async_write(
					m_local_socket, res, net_awaitable[ec]);

				co_return;
			}

			// 解析成功, 从结果中提取地址, 只取第一个地址即可.
			target_addr = results.begin()->endpoint().address();
			protocol = target_addr.is_v4() ? udp::v4() : udp::v6();
		}
		else
		{
			target_addr = addr;
			protocol = target_addr.is_v4() ? udp::v4() : udp::v6();
		}

		// 创建并连接 UDP socket, 专为目标地址的 udp socket 服务.
		udp::socket udp_socket(m_executor);
		udp_socket.open(protocol, ec);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: open UDP socket: " << ec.message();
			co_return;
		}

		// 如果有绑定接口, 则绑定到指定接口.
		if (m_bind_interface)
		{
			udp::endpoint bind_endpoint(*m_bind_interface, 0);
			udp_socket.bind(bind_endpoint, ec);
			if (ec)
			{
				log_conn_warning()
					<< ", connect-udp: bind UDP socket: " << ec.message();
				co_return;
			}
		}

		// 设置 SO_MARK.
		if (m_option.so_mark_)
		{
			auto ret = apply_so_mark(
				udp_socket.native_handle(), m_option.so_mark_);
			if (ret.has_error())
			{
				log_conn_warning()
					<< ", connect-udp: set socket mark: "
					<< ret.error().message();
			}
		}

		// "连接" UDP socket 到目标地址.
		udp::endpoint target_endpoint(target_addr, target_port);
		udp_socket.connect(target_endpoint, ec);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: connect UDP socket to "
				<< target_host << ":" << target_port
				<< " error: " << ec.message();
			co_return;
		}

		log_conn_debug()
			<< ", connect-udp: UDP socket connected to "
			<< target_endpoint;

		// 发送 101 Switching Protocols 响应.
		{
			http::response<http::empty_body> res;
			res.version(11);
			res.result(http::status::switching_protocols);
			res.set(http::field::connection, "Upgrade");
			res.set(http::field::upgrade, "connect-udp");
			res.set("Capsule-Protocol", "?1");

			co_await http::async_write(
				m_local_socket, res, net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", connect-udp: send 101 response: "
					<< ec.message();
				co_return;
			}
		}

		log_conn_debug()
			<< ", connect-udp: 101 sent, starting capsule/UDP forwarding";

		// 开始 capsule ↔ UDP 双向转发.
		auto self = shared_from_this();

		// HTTP → UDP 转发协程: 从 HTTP 流读取 capsule, 解析出 UDP 数据并发送到 UDP socket.
		auto http_to_udp = [this, self, &udp_socket]
			() mutable -> net::awaitable<void>
		{
			boost::system::error_code ec;

			while (!m_abort)
			{
				// 读取一个完整 capsule (type + value).
				auto [capsule_type, capsule_value] = co_await
					read_capsule_from_stream(
						m_local_socket, m_local_buffer, ec);
				if (ec)
				{
					log_conn_warning()
						<< ", connect-udp: read capsule error: "
						<< ec.message();
					break;
				}

				// 仅处理 DATAGRAM capsule 类型 (RFC 9297).
				if (capsule_type != udp_proxy_capsule_type)
				{
					log_conn_debug()
						<< ", connect-udp: unknown capsule type: "
						<< capsule_type
						<< ", skipping "
						<< capsule_value.size() << " bytes";
					continue;
				}

				// 解析 DATAGRAM capsule, 取出 UDP payload.
				std::string_view payload;
				if (!decode_datagram_capsule(capsule_value, payload))
				{
					log_conn_debug()
						<< ", connect-udp: invalid datagram capsule, skipping";
					continue;
				}

				co_await udp_socket.async_send(
					net::buffer(payload.data(), payload.size()),
					net_awaitable[ec]);
				if (ec)
				{
					log_conn_warning()
						<< ", connect-udp: UDP send error: "
						<< ec.message();
					break;
				}
			}

			// 关闭连接.
			m_abort = true;

			udp_socket.close(ec);
			m_local_socket.close(ec);

			co_return;
		};

		// UDP → HTTP 转发协程: 从 UDP socket 读取数据, 封装成 capsule 发送到 HTTP 流.
		auto udp_to_http = [this, self, &udp_socket]
			() mutable -> net::awaitable<void>
		{
			boost::system::error_code ec;
			// 最大 UDP payload + capsule header (type + length + ctx_id).
			uint8_t buf[32 + 65535];

			while (!m_abort)
			{
				// 从 UDP socket 接收数据.
				auto bytes = co_await udp_socket.async_receive(
					net::buffer(buf + 32, sizeof(buf) - 32),
					net_awaitable[ec]);
				if (ec) break;

				// capsule 头部紧邻数据写入 buf, 便于 header 与 payload 一次写出.
				auto capsule_length = datagram_capsule_header_size(bytes);
				auto capsule_start = buf + 32 - capsule_length;
				encode_datagram_capsule_header(bytes, capsule_start);

				co_await net::async_write(
					m_local_socket,
					net::buffer(capsule_start, capsule_length + bytes),
					net_awaitable[ec]);
				if (ec)
				{
					log_conn_warning()
						<< ", connect-udp: write capsule error: "
						<< ec.message();
					break;
				}
			}

			m_abort = true;

			udp_socket.close(ec);
			m_local_socket.close(ec);

			co_return;
		};

		// 并发运行 HTTP↔UDP 双向转发.
		co_await(
			http_to_udp()
			&&
			udp_to_http()
		);

		log_conn_debug()
			<< ", connect-udp: forwarding completed for "
			<< target_host << ":" << target_port;

		co_return;
	}

	net::awaitable<void> proxy_session::http_proxy_udp_proxy_pass(
		const http::request<http::string_body>& req,
		const std::string& target_host,
		uint16_t target_port
	)
	{
		boost::system::error_code ec;

		auto scheme = boost::to_lower_copy(std::string(m_proxy_pass->scheme()));

		if (proxy_scheme_is_websocket(scheme))
		{
			if (!co_await connect_websocket_udp_proxy_pass(
				target_host, target_port))
			{
				http::response<http::string_body> res{
					http::status::bad_gateway, req.version() };
				res.set(http::field::content_type, "text/plain");
				res.body() = "Failed to establish upstream WebSocket UDP tunnel.\n";
				res.prepare_payload();
				co_await http::async_write(
					m_local_socket, res, net_awaitable[ec]);
				co_return;
			}

			http::response<http::empty_body> res;
			res.version(11);
			res.result(http::status::switching_protocols);
			res.set(http::field::connection, "Upgrade");
			res.set(http::field::upgrade, "connect-udp");
			res.set("Capsule-Protocol", "?1");

			co_await http::async_write(
				m_local_socket, res, net_awaitable[ec]);
			if (ec)
			{
				stream_cancel(m_remote_socket);
				co_return;
			}

			stream_expires_never(m_local_socket);
			co_await http_capsules_to_websocket_proxy();
			co_return;
		}

		if (scheme.starts_with("socks5"))
		{
			// SOCKS proxy_pass: 通过 SOCKS5 UDP ASSOCIATE 转发 UDP 数据.
			log_conn_debug()
				<< ", connect-udp over socks proxy_pass: "
				<< m_proxy_pass->encoded_origin()
				<< ", target: " << target_host << ":" << target_port;

			// 1. 连接到 SOCKS5 代理服务器.
			tcp::socket remote_socket(m_executor);
			auto targets = co_await resolve_proxy_pass_targets();

			ec = co_await connect_proxy_pass(remote_socket, targets);
			if (ec)
			{
				log_conn_warning()
					<< ", connect-udp: connect to socks proxy_pass failed: "
					<< ec.message();

				http::response<http::string_body> res{
					http::status::bad_gateway, req.version() };
				res.set(http::field::content_type, "text/html");
				res.body() = fake_502_socks_connect_content;
				res.prepare_payload();
				co_await http::async_write(
					m_local_socket, res, net_awaitable[ec]);
				co_return;
			}

			// 2. 判断是否使用 SSL 加密.
			bool use_ssl = m_option.proxy_pass_use_ssl_;
			if (proxy_scheme_use_ssl(scheme))
				use_ssl = true;

			// 3. 执行 SOCKS5 UDP ASSOCIATE 握手.
			socks_client_option opt;
			opt.target_host = target_host;
			opt.target_port = target_port;
			opt.proxy_hostname = is_hostname(target_host);
			opt.command = SOCKS5_CMD_UDP;
			opt.username = std::string(m_proxy_pass->user());
			opt.password = std::string(m_proxy_pass->password());

			std::optional<variant_stream_type> socks5_control;
			endpoint_opt backend_endpoint;

			if (use_ssl)
			{
				// 获取 proxy_pass 的主机名作为 SNI.
				auto proxy_host = std::string(m_proxy_pass->host_address());
				auto sni = m_option.proxy_ssl_name_.empty()
					? proxy_host : m_option.proxy_ssl_name_;

				// 复用进程内共享的 SSL context (按 是否校验证书/SNI/自定义 CA 路径
				// 缓存), 避免每次建立 UDP 关联都重新创建 context 并加载 CA 根证书.
				auto cli_ctx = get_shared_ssl_client_ctx(
					m_option.disable_check_cert_,
					sni,
					m_option.ssl_cacert_path_);
				if (cli_ctx.has_error())
				{
					log_conn_warning()
						<< ", connect-udp: configure ssl context: "
						<< cli_ctx.error().message();

					http::response<http::string_body> res{
						http::status::bad_gateway, req.version() };
					res.set(http::field::content_type, "text/html");
					res.body() = fake_502_ssl_config_content;
					res.prepare_payload();
					co_await http::async_write(
						m_local_socket, res, net_awaitable[ec]);
					co_return;
				}

				socks5_control.emplace(
					init_proxy_stream(std::move(remote_socket), *cli_ctx.value()));
				auto& ssl_socket =
					boost::variant2::get<ssl_tcp_stream>(*socks5_control);

				SSL_set_tlsext_host_name(
					ssl_socket.native_handle(), sni.c_str());

				co_await ssl_socket.async_handshake(
					net::ssl::stream_base::client, net_awaitable[ec]);
				if (ec)
				{
					log_conn_warning()
						<< ", connect-udp: ssl handshake with socks proxy: "
						<< ec.message();

					http::response<http::string_body> res{
						http::status::bad_gateway, req.version() };
					res.set(http::field::content_type, "text/html");
					res.body() = fake_502_ssl_handshake_content;
					res.prepare_payload();
					co_await http::async_write(
						m_local_socket, res, net_awaitable[ec]);
					co_return;
				}

				backend_endpoint = co_await async_socks_handshake(
					*socks5_control, opt, net_awaitable[ec]);
			}
			else
			{
				socks5_control.emplace(
					init_proxy_stream(std::move(remote_socket)));

				backend_endpoint = co_await async_socks_handshake(
					*socks5_control, opt, net_awaitable[ec]);
			}

			if (ec || !backend_endpoint)
			{
				log_conn_warning()
					<< ", connect-udp: socks5 udp associate failed: "
					<< ec.message();

				http::response<http::string_body> res{
					http::status::bad_gateway, req.version() };
				res.set(http::field::content_type, "text/html");
				res.body() = fake_502_socks_associate_content;
				res.prepare_payload();
				co_await http::async_write(
					m_local_socket, res, net_awaitable[ec]);
				co_return;
			}

			// 4. 保存 UDP 中继端点.
			m_remote_udp_endpoint = *backend_endpoint;

			// 若返回的地址是未指定的(如 0.0.0.0), 则使用 TCP 控制连接的远程地址.
			if (m_remote_udp_endpoint.address().is_unspecified())
			{
				if (use_ssl)
				{
					auto& ssl_sock =
						boost::variant2::get<ssl_tcp_stream>(*socks5_control);
					auto remote_addr = ssl_sock.lowest_layer()
						.remote_endpoint(ec).address();
					if (!ec)
						m_remote_udp_endpoint.address(remote_addr);
				}
				else
				{
					auto& tcp_sock =
						boost::variant2::get<proxy_tcp_socket>(*socks5_control);
					auto remote_addr = tcp_sock.lowest_layer()
						.remote_endpoint(ec).address();
					if (!ec)
						m_remote_udp_endpoint.address(remote_addr);
				}
			}

			log_conn_debug()
				<< ", connect-udp: socks5 udp relay endpoint: "
				<< m_remote_udp_endpoint;

			// 5. 创建本地 UDP socket 用于与 SOCKS5 UDP 中继通信.
			auto protocol = m_remote_udp_endpoint.address().is_v4()
				? udp::v4() : udp::v6();
			udp::socket udp_socket(m_executor);
			udp_socket.open(protocol, ec);
			if (ec)
			{
				log_conn_warning()
					<< ", connect-udp: open udp socket: " << ec.message();
				co_return;
			}

			// 绑定到随机端口.
			udp_socket.bind(udp::endpoint(protocol, 0), ec);
			if (ec)
			{
				log_conn_warning()
					<< ", connect-udp: bind udp socket: " << ec.message();
				co_return;
			}

			if (m_option.so_mark_)
			{
				auto ret = apply_so_mark(
					udp_socket.native_handle(), m_option.so_mark_);
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", connect-udp: set socket mark: "
						<< ret.error().message();
				}
			}

			// 6. 发送 101 Switching Protocols 响应给客户端.
			{
				http::response<http::empty_body> res;
				res.version(11);
				res.result(http::status::switching_protocols);
				res.set(http::field::connection, "Upgrade");
				res.set(http::field::upgrade, "connect-udp");
				res.set("Capsule-Protocol", "?1");

				co_await http::async_write(
					m_local_socket, res, net_awaitable[ec]);
				if (ec)
				{
					log_conn_warning()
						<< ", connect-udp: send 101 response: "
						<< ec.message();
					co_return;
				}
			}

			log_conn_debug()
				<< ", connect-udp: 101 sent, "
				<< "starting capsule/SOCKS5 UDP forwarding";

			// 7. 启动双向转发.
			auto self = shared_from_this();

			// 预计算 SOCKS5 UDP 头部.
			bool is_ipv4_target = false;
			bool is_ipv6_target = false;
			net::ip::address target_addr;
			auto addr_ec = boost::system::error_code{};
			target_addr = net::ip::make_address(target_host, addr_ec);
			if (!addr_ec)
			{
				is_ipv4_target = target_addr.is_v4();
				is_ipv6_target = target_addr.is_v6();
			}

			// 预构建 SOCKS5 UDP 头部 (RSV + FRAG + ATYP + DST.ADDR + DST.PORT).
			size_t socks5_udp_header_size = 0;
			if (is_ipv4_target)
				socks5_udp_header_size = 10; // 2+1+1+4+2
			else if (is_ipv6_target)
				socks5_udp_header_size = 22; // 2+1+1+16+2
			else
				socks5_udp_header_size = 7 + target_host.size(); // 2+1+1+1+N+2

			std::vector<uint8_t> socks5_udp_header(socks5_udp_header_size);
			{
				uint8_t* wp = socks5_udp_header.data();
				write<uint16_t>(0x0, wp);  // RSV
				write<uint8_t>(0x0, wp);   // FRAG

				if (is_ipv4_target)
				{
					write<uint8_t>(SOCKS5_ATYP_IPV4, wp);
					write<uint32_t>(target_addr.to_v4().to_uint(), wp);
					write<uint16_t>(target_port, wp);
				}
				else if (is_ipv6_target)
				{
					write<uint8_t>(SOCKS5_ATYP_IPV6, wp);
					auto v6_bytes = target_addr.to_v6().to_bytes();
					for (auto b : v6_bytes)
						write<uint8_t>(b, wp);
					write<uint16_t>(target_port, wp);
				}
				else
				{
					write<uint8_t>(SOCKS5_ATYP_DOMAINNAME, wp);
					write<uint8_t>(
						static_cast<uint8_t>(target_host.size()), wp);
					std::copy(target_host.begin(), target_host.end(), wp);
					wp += target_host.size();
					write<uint16_t>(target_port, wp);
				}
			}

			// HTTP → SOCKS5 UDP: 读取 capsule, 提取 UDP payload,
			// 附加 SOCKS5 UDP 头部, 发送到中继.
			auto http_to_udp =
				[this, self, &udp_socket, &socks5_udp_header, socks5_udp_header_size]
				() -> net::awaitable<void>
			{
				boost::system::error_code ec;

				while (!m_abort)
				{
					// 读取一个完整 capsule (type + value).
					auto [capsule_type, capsule_value] = co_await
						read_capsule_from_stream(
							m_local_socket, m_local_buffer, ec);
					if (ec) break;

					// 仅处理 DATAGRAM capsule.
					if (capsule_type != udp_proxy_capsule_type)
						continue;

					// 解析 DATAGRAM capsule, 取出 UDP payload.
					std::string_view payload;
					if (!decode_datagram_capsule(capsule_value, payload))
						continue;

					static constexpr size_t max_socks5_udp_datagram_size = 65535;
					if (socks5_udp_header_size > max_socks5_udp_datagram_size ||
						payload.size() > max_socks5_udp_datagram_size - socks5_udp_header_size)
					{
						log_conn_warning()
							<< ", connect-udp: SOCKS5 UDP datagram too large: "
							<< socks5_udp_header_size + payload.size();
						continue;
					}

					// 构建完整 SOCKS5 UDP 数据报并发送到中继.
					std::vector<uint8_t> send_buf(
						socks5_udp_header_size + payload.size());
					std::memcpy(send_buf.data(),
						socks5_udp_header.data(),
						socks5_udp_header_size);
					std::memcpy(
						send_buf.data() + socks5_udp_header_size,
						payload.data(),
						payload.size());

					co_await udp_socket.async_send_to(
						net::buffer(send_buf),
						m_remote_udp_endpoint,
						net_awaitable[ec]);
					if (ec)
					{
						log_conn_warning()
							<< ", connect-udp: send to relay error: "
							<< ec.message();
						break;
					}
				}

				m_abort = true;
				udp_socket.close(ec);
				m_local_socket.close(ec);
				co_return;
			};

			// SOCKS5 UDP → HTTP: 从 UDP socket 接收数据,
			// 解析 SOCKS5 头部, 封装成 capsule 发送到 HTTP 流.
			auto udp_to_http =
				[this, self, &udp_socket]()
				-> net::awaitable<void>
			{
				boost::system::error_code ec;
				uint8_t buf[65536];

				while (!m_abort)
				{
					udp::endpoint relay_endp;
					auto bytes = co_await udp_socket.async_receive_from(
						net::buffer(buf), relay_endp, net_awaitable[ec]);
					if (ec) break;

					// SOCKS5 UDP 中继的应答必须来自 UDP ASSOCIATE 返回的端点.
					if (relay_endp != m_remote_udp_endpoint)
					{
						log_conn_debug()
							<< ", connect-udp: ignore unexpected UDP source: "
							<< relay_endp;
						continue;
					}

					// 解析 SOCKS5 UDP 头部.
					const uint8_t* rp = buf;
					if (bytes < 4) continue;

					read<uint16_t>(rp);  // RSV
					auto frag = read<uint8_t>(rp);  // FRAG
					if (frag != 0) continue;  // 不支持分片.

					auto atyp = read<uint8_t>(rp);
					const uint8_t* data_start = nullptr;
					size_t data_size = 0;

					if (atyp == SOCKS5_ATYP_IPV4)
					{
						if (bytes < 10) continue;
						rp += 4;  // Skip IPv4 addr
						rp += 2;  // Skip port
						data_start = rp;
						data_size = bytes - (rp - buf);
					}
					else if (atyp == SOCKS5_ATYP_DOMAINNAME)
					{
						if (bytes < 5) continue;
						auto dlen = read<uint8_t>(rp);
						if (rp + dlen + 2 > buf + bytes) continue;
						rp += dlen;  // Skip domain
						rp += 2;     // Skip port
						data_start = rp;
						data_size = bytes - (rp - buf);
					}
					else if (atyp == SOCKS5_ATYP_IPV6)
					{
						if (bytes < 22) continue;
						rp += 16;  // Skip IPv6 addr
						rp += 2;   // Skip port
						data_start = rp;
						data_size = bytes - (rp - buf);
					}
					else
					{
						continue;
					}

					if (data_size == 0) continue;

					// 构建 DATAGRAM capsule 头部, payload 随后单独写出.
					uint8_t header_buf[32];
					auto pos = encode_datagram_capsule_header(
						data_size, header_buf);

					co_await net::async_write(
						m_local_socket,
						net::buffer(header_buf, pos),
						net_awaitable[ec]);
					if (ec) break;

					co_await net::async_write(
						m_local_socket,
						net::buffer(data_start, data_size),
						net_awaitable[ec]);
					if (ec) break;
				}

				m_abort = true;
				udp_socket.close(ec);
				m_local_socket.close(ec);
				co_return;
			};

			// 保持 TCP 控制连接存活 (SOCKS5 UDP ASSOCIATE
			// 需要在控制连接存活期间保持有效).
			auto keep_control_alive =
				[this, self, &socks5_control]()
				-> net::awaitable<void>
			{
				boost::system::error_code ec;
				while (!m_abort)
				{
					char bufs[64];
					co_await socks5_control->async_read_some(
						net::buffer(bufs, sizeof(bufs)),
						net_awaitable[ec]);
					if (ec)
					{
						log_conn_debug()
							<< ", connect-udp: control connection closed: "
							<< ec.message();
						break;
					}
				}

				m_abort = true;
				co_return;
			};

			// 并发运行三个协程.
			co_await(
				http_to_udp()
				&&
				udp_to_http()
				&&
				keep_control_alive()
			);

			log_conn_debug()
				<< ", connect-udp: socks forwarding completed for "
				<< target_host << ":" << target_port;

			co_return;
		}

		// HTTP proxy_pass: 连接上游并转发原始请求.
		tcp::socket remote_socket(m_executor);
		auto targets = co_await resolve_proxy_pass_targets();

		ec = co_await connect_proxy_pass(remote_socket, targets);
		if (ec)
		{
			http::response<http::string_body> res{
				http::status::bad_gateway,
				req.version() };
			res.set(http::field::content_type, "text/html");
			res.body() = fake_502_proxy_connect_content;
			res.prepare_payload();

			co_await http::async_write(
				m_local_socket, res, net_awaitable[ec]);
			co_return;
		}

		// 将 m_remote_socket 初始化为 TCP 流类型.
		const bool use_ssl = m_option.proxy_pass_use_ssl_ ||
			proxy_scheme_use_ssl(scheme);
		m_remote_socket = std::move(
			co_await instantiate_proxy_pass(
				std::move(remote_socket),
				use_ssl));

		// 转发到上游前替换下游代理认证信息.
		auto upstream_req = req;
		upstream_req.erase(http::field::proxy_authorization);
		upstream_req.erase(http::field::proxy_connection);
		if (!m_proxy_pass->user().empty())
		{
			const auto userinfo =
				std::string(m_proxy_pass->user()) + ":" +
				std::string(m_proxy_pass->password());
			upstream_req.set(http::field::proxy_authorization,
				"Basic " + strutil::base64_encode(userinfo));
		}

		// 发送请求头到上游.
		co_await http::async_write(
			m_remote_socket, upstream_req, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: forward request to upstream: "
				<< ec.message();
			co_return;
		}

		// 读取上游的响应.
		beast::flat_buffer buf;

		http::response_parser<http::string_body> parser;
		parser.body_limit(1024 * 1024 * 10);

		co_await http::async_read(
			m_remote_socket, buf, parser, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: read upstream response: "
				<< ec.message();
			co_return;
		}

		auto upstream_res = parser.release();

		// 把上游响应转发给客户端.
		co_await http::async_write(
			m_local_socket, upstream_res, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: forward response to client: "
				<< ec.message();
			co_return;
		}

		// 如果上游返回 101, 则开始原始 TCP 双向转发 (上游负责 capsule ↔ UDP).
		if (upstream_res.result() == http::status::switching_protocols)
		{
			// 响应头之后可能已预读到隧道首包, 交给并发转发前补发.
			if (buf.size() > 0)
			{
				m_remote_buffer.commit(net::buffer_copy(
					m_remote_buffer.prepare(buf.size()), buf.data()));
				buf.consume(buf.size());
			}

			log_conn_debug()
				<< ", connect-udp: tunnel established via upstream, "
				<< "starting raw tcp forwarding";

			// 同上, 隧道建立后清除请求阶段的 socket 超时, 交由 idle_timeout 管理.
			stream_expires_never(m_local_socket);

			co_await concurrent_transfer();
		}

		co_return;
	}

	bool proxy_session::parse_connect_udp_target(std::string_view path,
		std::string& target_host, uint16_t& target_port)
	{
		if (path.empty() || path[0] != '/') return false;

		std::string_view r_host, r_port;

		// /.well-known/masque/udp/{target_host}/{target_port}/
		if (size_t pos = path.find("/.well-known/masque/udp/");
			pos != std::string_view::npos)
		{
			std::string_view sub = path.substr(pos + 24);
			size_t slash = sub.find('/');

			if (slash == std::string_view::npos || slash == 0)
				return false;

			r_host = sub.substr(0, slash);
			r_port = sub.substr(slash + 1);

			if (!r_port.empty() && r_port.back() == '/')
				r_port.remove_suffix(1);
		}
		// ?h=...&p=... 或者 ?target_host=...&target_port=...
		else if (size_t q_pos = path.find('?'); q_pos != std::string_view::npos)
		{
			std::string_view query = path.substr(q_pos + 1);
			size_t pos = 0;

			while (pos < query.size())
			{
				size_t next = query.find_first_of("&,", pos);
				std::string_view pair = query.substr(
					pos, next == std::string_view::npos ? next : next - pos);
				pos = (next == std::string_view::npos) ? next : next + 1;

				if (size_t eq = pair.find('='); eq != std::string_view::npos)
				{
					std::string_view k = pair.substr(0, eq);
					std::string_view v = pair.substr(eq + 1);

					if (k == "h" || k == "target_host")
						r_host = v;
					else if (k == "p" || k == "target_port")
						r_port = v;
				}
			}
		}

		if (r_host.empty() || r_port.empty())
			return false;

		// 解析端口
		auto [ptr, ec] = std::from_chars(
			r_port.data(), r_port.data() + r_port.size(), target_port);

		if (ec != std::errc{} || ptr != r_port.data() + r_port.size() ||
			target_port == 0)
			return false;

		// 还原 Host, 处理 IPv6 中的 %3A 等情况.
		target_host.clear();
		target_host.reserve(r_host.size());

		auto hex_value = [](char c) noexcept -> int
		{
			if (c >= '0' && c <= '9')
				return c - '0';
			if (c >= 'a' && c <= 'f')
				return c - 'a' + 10;
			if (c >= 'A' && c <= 'F')
				return c - 'A' + 10;
			return -1;
		};

		for (size_t i = 0; i < r_host.size(); ++i)
		{
			char ch = r_host[i];
			if (ch == '%')
			{
				if (i + 2 >= r_host.size())
					return false;

				auto high = hex_value(r_host[i + 1]);
				auto low = hex_value(r_host[i + 2]);
				if (high < 0 || low < 0)
					return false;

				ch = static_cast<char>((high << 4) | low);
				i += 2;
			}

			auto uch = static_cast<unsigned char>(ch);
			if (uch <= 0x20 || uch >= 0x7f || ch == '/' || ch == '@' ||
				ch == '?' || ch == '#' || ch == '[' || ch == ']')
				return false;

			target_host.push_back(ch);
		}

		// SOCKS5 UDP 域名长度字段为 1 字节.
		if (target_host.empty() || target_host.size() > 255)
			return false;

		return true;
	}

} // namespace proxy
