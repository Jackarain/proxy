//
// proxy_session_http_proxy_websocket_udp_bridge.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
			std::array<uint8_t, datagram_capsule_max_size> capsule{};
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

} // namespace proxy
