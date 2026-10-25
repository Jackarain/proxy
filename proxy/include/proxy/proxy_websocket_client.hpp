//
// proxy_websocket_client.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_10_25__PROXY_WEBSOCKET_CLIENT_HPP
#define INCLUDE__2026_10_25__PROXY_WEBSOCKET_CLIENT_HPP

#include "proxy/proxy_stream.hpp"

#include <boost/url.hpp>

#include <string>
#include <utility>

namespace proxy {

	inline ws_tunnel::client_handshake_options
		make_websocket_handshake_options(const urls::url& proxy_url,
			std::string authority)
	{
		ws_tunnel::client_handshake_options options;
		options.host = std::string(proxy_url.encoded_host_and_port());
		options.target = std::string(proxy_url.encoded_target());
		if (options.target.empty())
			options.target = std::string(ws_tunnel::proxy_connect_path);
		options.authority = std::move(authority);
		options.username = std::string(proxy_url.user());
		options.password = std::string(proxy_url.password());
		return options;
	}

	inline ws_tunnel::client_handshake_options
		make_websocket_handshake_options(const urls::url& proxy_url,
			std::string target_host, uint16_t target_port)
	{
		return make_websocket_handshake_options(proxy_url,
			ws_tunnel::format_authority(target_host, target_port));
	}

	// 将已连接的 TCP/TLS 上游流升级为 WebSocket CONNECT 隧道.
	inline net::awaitable<boost::system::error_code>
		async_websocket_proxy_handshake(variant_stream_type& stream,
			ws_tunnel::client_handshake_options options)
	{
		if (boost::variant2::holds_alternative<websocket_plain_stream>(stream) ||
			boost::variant2::holds_alternative<websocket_tls_stream>(stream))
		{
			co_return make_error_code(
				boost::system::errc::operation_not_supported);
		}

		if (boost::variant2::holds_alternative<proxy_tcp_socket>(stream))
		{
			websocket_plain_stream websocket(
				std::move(boost::variant2::get<proxy_tcp_socket>(stream)));
			stream = init_proxy_stream(std::move(websocket));
			co_return co_await boost::variant2::get<websocket_plain_stream>(
				stream).async_handshake(std::move(options));
		}

		if (boost::variant2::holds_alternative<ssl_tcp_stream>(stream))
		{
			websocket_tls_stream websocket(
				std::move(boost::variant2::get<ssl_tcp_stream>(stream)));
			stream = init_proxy_stream(std::move(websocket));
			co_return co_await boost::variant2::get<websocket_tls_stream>(
				stream).async_handshake(std::move(options));
		}

		co_return make_error_code(boost::system::errc::operation_not_supported);
	}

	// 在已建立的 WebSocket 隧道内叠加目标 TLS 层.
	inline bool websocket_stream_to_tls(variant_stream_type& stream,
		net::ssl::context& sslctx)
	{
		if (boost::variant2::holds_alternative<websocket_plain_stream>(stream))
		{
			auto next = std::move(
				boost::variant2::get<websocket_plain_stream>(stream));
			stream = init_proxy_stream(std::move(next), sslctx);
			return true;
		}
		if (boost::variant2::holds_alternative<websocket_tls_stream>(stream))
		{
			auto next = std::move(
				boost::variant2::get<websocket_tls_stream>(stream));
			stream = init_proxy_stream(std::move(next), sslctx);
			return true;
		}
		return false;
	}

} // namespace proxy

#endif // INCLUDE__2026_10_25__PROXY_WEBSOCKET_CLIENT_HPP
