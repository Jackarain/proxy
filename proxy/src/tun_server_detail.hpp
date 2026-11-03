//
// tun_server_detail.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__PROXY_TUN_SERVER_DETAIL_HPP
#define INCLUDE__PROXY_TUN_SERVER_DETAIL_HPP

#include "proxy/tun_server.hpp"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace proxy::tun_detail {

	int udp_idle_timeout(const proxy_server_option& opt) noexcept;

#if defined(__linux__)
	void query_tun_device_ip(const std::string& dev, std::string& ip);

	bool is_tun_loopback_source(const net::ip::address& addr,
		const std::string& dev) noexcept;
#endif

	std::string build_socks5_udp_header(
		const net::ip::udp::endpoint& target);

	size_t socks5_udp_header_size(const char* data, size_t len) noexcept;

	net::awaitable<tcp::socket> connect_direct(
		net::any_io_executor executor,
		const proxy_server_option& opt,
		const net::ip::address& dst, uint16_t port,
		const std::function<net::awaitable<bool>(int)>& protect);

	http::request<http::empty_body> build_connect_udp_request(
		const urls::url& proxy_url,
		const net::ip::udp::endpoint& target,
		bool use_ssl);

	std::optional<net::ip::udp::endpoint> pick_dns_server(
		const std::vector<std::string>& dns_list,
		const net::ip::udp::endpoint& target) noexcept;

	bool parse_dns_response(const char* data, size_t len,
		std::string& qname,
		std::vector<std::pair<net::ip::address, uint32_t>>& answers);

} // namespace proxy::tun_detail

#endif // INCLUDE__PROXY_TUN_SERVER_DETAIL_HPP
