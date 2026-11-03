//
// proxy_session_http_proxy_udp_socks_connect.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

	net::awaitable<void> proxy_session::http_udp_proxy_pass_socks5(
		const http::request<http::string_body>& req,
		const std::string& target_host,
		uint16_t target_port,
		const std::string& scheme)
	{
		boost::system::error_code ec;

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
		// UDP ASSOCIATE 的 DST.ADDR/DST.PORT 表示客户端发送数据报
		// 所用的端点；此时尚未创建 UDP socket，使用全零占位.
		opt.target_host = "0.0.0.0";
		opt.target_port = 0;
		opt.proxy_hostname = false;
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

		if (m_remote_udp_endpoint.address().is_unspecified() ||
			m_remote_udp_endpoint.port() == 0)
		{
			log_conn_warning()
				<< ", connect-udp: invalid socks5 udp relay endpoint: "
				<< m_remote_udp_endpoint;

			http::response<http::string_body> res{
				http::status::bad_gateway, req.version() };
			res.set(http::field::content_type, "text/html");
			res.body() = fake_502_socks_associate_content;
			res.prepare_payload();
			co_await http::async_write(
				m_local_socket, res, net_awaitable[ec]);
			co_return;
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

		stream_expires_never(m_local_socket);
		m_last_activity.store(0, std::memory_order_relaxed);
		m_transfer_timeout.store(
			m_option.udp_timeout_, std::memory_order_relaxed);

		co_await run_socks5_udp_bridge(
			udp_socket, *socks5_control, target_host, target_port);

		log_conn_debug()
			<< ", connect-udp: socks forwarding completed for "
			<< target_host << ":" << target_port;

		co_return;
	}

} // namespace proxy
