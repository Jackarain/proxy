//
// ssl_connect.hpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_10_13__SSL_CONNECT_HPP
#define INCLUDE__2026_10_13__SSL_CONNECT_HPP


#include "proxy/proxy_stream.hpp"
#include "proxy/ssl_client.hpp"
#include "proxy/use_awaitable.hpp"

#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>


namespace proxy {

	namespace net = boost::asio;

	// 与上游建立 TLS 连接时的握手超时, 避免对端无响应导致协程长期挂起.
	inline constexpr auto ssl_connect_timeout = std::chrono::seconds(15);

	// 将已建立的 TCP socket 升级为 TLS 客户端流.
	//
	// SSL context 由 get_shared_ssl_client_ctx 按 (是否校验证书, SNI, CA 路径) 共享,
	// 避免每个服务实例各自加载一份 CA 根证书. 成功时 ssl_sock 持有 TLS 流,
	// 失败返回错误码.
	inline net::awaitable<boost::system::error_code> ssl_upgrade_client_stream(
		tcp::socket& remote_socket,
		std::optional<variant_stream_type>& ssl_sock,
		std::string_view sni,
		bool disable_check_cert,
		const std::string& cacert_path)
	{
		boost::system::error_code ec;

		auto cli_ctx = get_shared_ssl_client_ctx(
			disable_check_cert, std::string(sni), cacert_path);
		if (cli_ctx.has_error())
			co_return cli_ctx.error();

		ssl_sock.emplace(init_proxy_stream(
			std::move(remote_socket), *cli_ctx.value()));
		auto& ssl_socket = boost::variant2::get<ssl_tcp_stream>(*ssl_sock);

		// 设置 SNI 主机名以兼容需要 SNI 的服务器.
		SSL_set_tlsext_host_name(ssl_socket.native_handle(), sni.data());

		// 握手超时: 服务器无响应时关闭底层连接, 使握手失败返回.
		net::steady_timer tls_timer(ssl_socket.get_executor());
		tls_timer.expires_after(ssl_connect_timeout);
		tls_timer.async_wait(
			[&ssl_sock](const boost::system::error_code& tec)
			{
				if (tec || !ssl_sock)
					return;

				boost::system::error_code sec;
				net_tcp_socket(*ssl_sock).close(sec);
			});

		co_await ssl_socket.async_handshake(
			net::ssl::stream_base::client, net_awaitable[ec]);
		tls_timer.cancel();

		co_return ec;
	}

}


#endif // INCLUDE__2026_10_13__SSL_CONNECT_HPP
