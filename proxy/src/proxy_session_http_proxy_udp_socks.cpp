//
// proxy_session_http_proxy_udp_socks.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

	net::awaitable<void> proxy_session::run_socks5_udp_bridge(
		udp::socket& udp_socket,
		variant_stream_type& socks5_control,
		const std::string& target_host,
		uint16_t target_port) noexcept
	{
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
		auto cancel_idle =
			[this, self, &udp_socket, &socks5_control]()
		{
			stream_cancel(m_local_socket);
			boost::system::error_code ignore_ec;
			udp_socket.close(ignore_ec);
			stream_cancel(socks5_control);
		};

		co_await(
			(capsule_to_socks5_udp(
				udp_socket, socks5_control, socks5_udp_header) &&
				socks5_udp_to_capsule(udp_socket, socks5_control) &&
				keep_socks5_control_alive(udp_socket, socks5_control)) ||
			idle_timeout(std::move(cancel_idle))
		);
	}

	net::awaitable<void> proxy_session::capsule_to_socks5_udp(
		udp::socket& udp_socket,
		variant_stream_type& socks5_control,
		const std::vector<uint8_t>& socks5_udp_header) noexcept
	{
		const size_t socks5_udp_header_size = socks5_udp_header.size();

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

			m_last_activity.store(0, std::memory_order_relaxed);
		}

		m_abort = true;
		udp_socket.close(ec);
		m_local_socket.close(ec);
		stream_cancel(socks5_control);
		co_return;
	}

	net::awaitable<void> proxy_session::socks5_udp_to_capsule(
		udp::socket& udp_socket,
		variant_stream_type& socks5_control) noexcept
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
			m_last_activity.store(0, std::memory_order_relaxed);
		}

		m_abort = true;
		udp_socket.close(ec);
		m_local_socket.close(ec);
		stream_cancel(socks5_control);
		co_return;
	}

	net::awaitable<void> proxy_session::keep_socks5_control_alive(
		udp::socket& udp_socket,
		variant_stream_type& socks5_control) noexcept
	{
		boost::system::error_code ec;
		while (!m_abort)
		{
			char bufs[64];
			co_await socks5_control.async_read_some(
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
		udp_socket.close(ec);
		stream_cancel(m_local_socket);
		co_return;
	}

} // namespace proxy
