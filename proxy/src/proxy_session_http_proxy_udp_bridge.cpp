//
// proxy_session_http_proxy_udp_bridge.cpp
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
	net::awaitable<void> proxy_session::run_connected_udp_bridge(
		udp::socket& udp_socket) noexcept
	{
		auto self = shared_from_this();

		auto cancel_idle = [this, self, &udp_socket]()
		{
			stream_cancel(m_local_socket);
			boost::system::error_code ignore_ec;
			udp_socket.close(ignore_ec);
		};

		co_await(
			(capsule_to_connected_udp(udp_socket) &&
				connected_udp_to_capsule(udp_socket)) ||
			idle_timeout(std::move(cancel_idle))
		);
	}

	net::awaitable<void> proxy_session::capsule_to_connected_udp(
		udp::socket& udp_socket) noexcept
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

			m_last_activity.store(0, std::memory_order_relaxed);
		}

		// 关闭连接.
		m_abort = true;

		udp_socket.close(ec);
		m_local_socket.close(ec);

		co_return;
	}

	net::awaitable<void> proxy_session::connected_udp_to_capsule(
		udp::socket& udp_socket) noexcept
	{
		boost::system::error_code ec;
		// 最大 UDP payload + capsule header (type + length + ctx_id).
		uint8_t buf[32 + max_udp_payload_size];

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

			m_last_activity.store(0, std::memory_order_relaxed);
		}

		m_abort = true;

		udp_socket.close(ec);
		m_local_socket.close(ec);

		co_return;
	}

} // namespace proxy
