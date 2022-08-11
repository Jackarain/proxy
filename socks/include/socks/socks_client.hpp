//
// socks_client.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "socks/socks_error_code.hpp"

#include <string>
#include <memory>

#include <boost/system/error_code.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

namespace net = boost::asio;


namespace socks {

	using net::ip::tcp;
	using net::ip::udp;

	enum {
		socks5_version = 5,
		socks4_version = 4,
		socks4a_version = 41,
	};

	struct socks_client_option
	{
		// socks server
		std::string target_host;
		uint16_t target_port;

		// user auth info
		std::string username;
		std::string password;

		// socks version: 4 or 5
		int version{ socks5_version };

		// pass hostname to proxy
		bool proxy_hostname{ true };
	};

	namespace detail {

		net::awaitable<boost::system::error_code>
			do_socks_handshake(tcp::socket& socket, socks_client_option opt = {});

		struct initiate_do_something
		{
			template <typename Handler>
			void operator()(Handler&& handler,
				tcp::socket* socket, socks_client_option opt) const
			{
				auto executor = net::get_associated_executor(handler);
				net::co_spawn(executor,
				[socket, opt = opt, handler = std::move(handler)]
				() mutable -> net::awaitable<void>
				{
					auto ec = co_await do_socks_handshake(*socket, opt);
					handler(ec);

					co_return;
				}, net::detached);
			}
		};
	}

	template<typename Handler>
	auto async_socks_handshake(tcp::socket& socket,
		socks_client_option opt, Handler&& handler)
	{
		return net::async_initiate<Handler,
			void(boost::system::error_code)>(
				detail::initiate_do_something(),
					handler, &socket, opt);
	}

}

