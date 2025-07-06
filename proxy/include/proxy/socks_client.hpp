//
// socks_client.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2023_10_18__SOCKS_CLIENT_HPP
#define INCLUDE__2023_10_18__SOCKS_CLIENT_HPP


#include "proxy/socks_error_code.hpp"
#include "proxy/socks_enums.hpp"
#include "proxy/socks_io.hpp"
#include "proxy/use_awaitable.hpp"

#include <boost/system/error_code.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <cstdlib>
#include <string>
#include <memory>


namespace proxy {

	namespace net = boost::asio;

	using net::ip::tcp;

	using io_util::write;
	using io_util::read;

	enum {
		socks5_version = 5,
		socks4_version = 4,
		socks4a_version = 41,
	};

	struct socks_client_option
	{
		// target server
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

		template <typename Stream>
		net::awaitable<void> do_socks5(
			Stream& socket,
			socks_client_option opt,
			boost::system::error_code& ec)
		{
			[[maybe_unused]] auto& username = opt.username;
			[[maybe_unused]] auto& passwd = opt.password;
			[[maybe_unused]] auto& hostname = opt.target_host;
			[[maybe_unused]] auto& port = opt.target_port;

			std::size_t bytes_to_write = username.empty() ? 3 : 4;
			net::streambuf request;
			auto req = static_cast<char*>(
				request.prepare(bytes_to_write).data());

			write<uint8_t>(SOCKS_VERSION_5, req);		// SOCKS VERSION 5.
			if (username.empty())
			{
				// 1 method
				write<uint8_t>(1, req);

				// support no authentication
				write<uint8_t>(SOCKS5_AUTH_NONE, req);
			}
			else
			{
				// 2 methods
				write<uint8_t>(2, req);

				// support no authentication
				write<uint8_t>(SOCKS5_AUTH_NONE, req);

				// support username/password
				write<uint8_t>(SOCKS5_AUTH, req);
			}

			request.commit(bytes_to_write);
			[[maybe_unused]] auto bytes =
				co_await net::async_write(
					socket,
					request,
					net_awaitable[ec]);
			if (ec) co_return;
			BOOST_ASSERT(bytes_to_write == bytes);

			net::streambuf response;
			bytes = co_await net::async_read(
				socket,
				response,
				net::transfer_exactly(2),
				net_awaitable[ec]);
			if (ec) co_return;
			BOOST_ASSERT(response.size() == 2);

			auto resp = static_cast<const char*>(
				response.data().data());
			auto version = read<uint8_t>(resp);
			auto method = read<uint8_t>(resp);

			if (version != SOCKS_VERSION_5)
			{
				ec = proxy::errc::socks_unsupported_version;
				co_return;
			}

			if (method == SOCKS5_AUTH) // need username&password auth...
			{
				if (username.empty())
				{
					ec = proxy::errc::socks_username_required;
					co_return;
				}

				request.consume(request.size());

				bytes_to_write = username.size() + passwd.size() + 3;
				auto auth = static_cast<char*>(
					request.prepare(bytes_to_write).data());

				// auth version.
				write<uint8_t>(0x01, auth);

				// username length.
				const auto ulen = static_cast<uint8_t>(username.size());
				write<uint8_t>(ulen, auth);

				// username.
				for (size_t i = 0; i < ulen; i++)
					write<uint8_t>(username[i], auth);

				// password length.
				const auto plen = static_cast<uint8_t>(passwd.size());
				write<uint8_t>(plen, auth);

				// password.
				for (size_t i = 0; i < plen; i++)
					write<uint8_t>(passwd[i], auth);

				request.commit(bytes_to_write);

				// write username & password.
				bytes = co_await net::async_write(
					socket,
					request,
					net_awaitable[ec]);
				if (ec) co_return;
				BOOST_ASSERT(bytes_to_write == bytes);

				response.consume(response.size());
				bytes = co_await net::async_read(
					socket,
					response,
					net::transfer_exactly(2),
					net_awaitable[ec]);
				if (ec) co_return;
				BOOST_ASSERT(response.size() == 2);

				resp = static_cast<const char*>(response.data().data());
				version = read<uint8_t>(resp);
				auto status = read<uint8_t>(resp);
				if (version != 0x01)	// auth version.
				{
					ec = proxy::errc::socks_unsupported_authentication_version;
					co_return;
				}

				if (status != 0x00)
				{
					ec = errc::socks_authentication_error;
					co_return;
				}
			}
			else if (method == SOCKS5_AUTH_NONE) // no need auth...
			{
				// co_await net::this_coro::executor;
			}
			else
			{
				ec = proxy::errc::socks_unsupported_authentication_version;
				co_return;
			}

			request.consume(request.size());
			bytes_to_write = 7 + hostname.size();
			req = static_cast<char*>(
				request.prepare(std::max<std::size_t>(
					bytes_to_write, 22)).data());

			write<uint8_t>(SOCKS_VERSION_5, req);	// SOCKS VERSION 5.
			write<uint8_t>(SOCKS_CMD_CONNECT, req); // CONNECT command.
			write<uint8_t>(0, req);					// reserved.

			if (opt.proxy_hostname)
			{
				// atyp, domain name.
				write<uint8_t>(SOCKS5_ATYP_DOMAINNAME, req);

				// domain size.
				BOOST_ASSERT(hostname.size() <= 255);
				write<uint8_t>(static_cast<int8_t>(
					hostname.size()), req);

				// domain.
				std::copy(hostname.begin(), hostname.end(),
					req);
				req += hostname.size();

				// port.
				write<uint16_t>(port, req);
			}
			else
			{
				auto endp = net::ip::make_address(hostname, ec);
				if (ec)
				{
					auto executor = co_await net::this_coro::executor;
					tcp::resolver resolver{ executor };
					auto error = ec;

					auto target_endpoints =
						co_await resolver.async_resolve(
							hostname,
							std::to_string(port),
							net_awaitable[ec]);
					if (ec) co_return;

					if (target_endpoints.empty())
					{
						ec = error;
						co_return;
					}

					auto it = *target_endpoints.begin();
					endp = it.endpoint().address().to_v4();
				}

				if (endp.is_v4())
				{
					write<uint8_t>(SOCKS5_ATYP_IPV4, req); // ipv4.
					write<uint32_t>(endp.to_v4().to_uint(), req);
					write<uint16_t>(port, req);
					bytes_to_write = 10;
				}
				else
				{
					write<uint8_t>(SOCKS5_ATYP_IPV6, req); // ipv6.
					auto v6_bytes = endp.to_v6().to_bytes();
					std::copy(v6_bytes.begin(), v6_bytes.end(), req);
					req += 16;
					write<uint16_t>(port, req);
					bytes_to_write = 22;
				}
			}

			request.commit(bytes_to_write);
			bytes = co_await net::async_write(
				socket,
				request,
				net_awaitable[ec]);
			if (ec) co_return;
			BOOST_ASSERT(bytes_to_write == bytes);

			response.consume(response.size());
			bytes = co_await net::async_read(
				socket,
				response,
				net::transfer_exactly(10),
				net_awaitable[ec]);
			if (ec) co_return;
			BOOST_ASSERT(response.size() == bytes);

			resp = static_cast<const char*>(response.data().data());
			version = read<uint8_t>(resp);
			/*auto rep = */read<uint8_t>(resp);
			read<uint8_t>(resp);    // skip RSV.
			int atyp = read<uint8_t>(resp);

			if (version != SOCKS_VERSION_5)
			{
				ec = errc::socks_unsupported_version;
				co_return;
			}
			else if (atyp != SOCKS5_ATYP_IPV4 &&
				atyp != SOCKS5_ATYP_DOMAINNAME &&
				atyp != SOCKS5_ATYP_IPV6)
			{
				ec = errc::socks_general_failure;
				co_return;
			}
			else if (atyp == SOCKS5_ATYP_DOMAINNAME)
			{
				auto domain_length = read<uint8_t>(resp);

				bytes = co_await net::async_read(socket,
					response,
					net::transfer_exactly(domain_length - 3),
					net_awaitable[ec]);
				if (ec) co_return;
			}
			else if (atyp == SOCKS5_ATYP_IPV6)
			{
				bytes = co_await net::async_read(socket,
					response,
					net::transfer_exactly(12),
					net_awaitable[ec]);
				if (ec) co_return;
			}

			resp = static_cast<const char*>(response.data().data());
			read<uint8_t>(resp);
			auto rep = read<uint8_t>(resp);
			read<uint8_t>(resp);    // skip RSV.
			atyp = read<uint8_t>(resp);

			if (atyp == SOCKS5_ATYP_DOMAINNAME)
			{
				auto domain_length = read<uint8_t>(resp);

				std::string domain;
				for (int i = 0; i < domain_length; i++)
					domain.push_back(read<uint8_t>(resp));
				port = read<uint16_t>(resp);
			}
			else if (atyp == SOCKS5_ATYP_IPV4)
			{
				net::ip::tcp::endpoint remote_endp(
					net::ip::address_v4(read<uint32_t>(resp)),
					read<uint16_t>(resp));
			}
			else if (atyp == SOCKS5_ATYP_IPV6)
			{
				net::ip::address_v6::bytes_type v6_bytes;
				for (auto i = 0; i < 16; i++)
					v6_bytes[i] = read<uint8_t>(resp);

				net::ip::tcp::endpoint remote_endp(
					net::ip::address_v6(v6_bytes),
					read<uint16_t>(resp));
			}

			if (rep != 0)
			{
				switch (rep)
				{
				case SOCKS5_GENERAL_SOCKS_SERVER_FAILURE:
					ec = errc::socks_general_failure; break;
				case SOCKS5_CONNECTION_NOT_ALLOWED_BY_RULESET:
					ec = errc::socks_connection_not_allowed_by_ruleset; break;
				case SOCKS5_NETWORK_UNREACHABLE:
					ec = errc::socks_network_unreachable; break;
				case SOCKS5_CONNECTION_REFUSED:
					ec = errc::socks_connection_refused; break;
				case SOCKS5_TTL_EXPIRED:
					ec = errc::socks_ttl_expired; break;
				case SOCKS5_COMMAND_NOT_SUPPORTED:
					ec = errc::socks_command_not_supported; break;
				case SOCKS5_ADDRESS_TYPE_NOT_SUPPORTED:
					ec = errc::socks_address_type_not_supported; break;
				default:
					ec = errc::socks_unassigned; break;
				}

				co_return;
			}

			co_return;
		}

		template <typename Stream>
		net::awaitable<void> do_socks4(
			Stream& socket,
			socks_client_option opt,
			boost::system::error_code& ec)
		{
			auto& username = opt.username;
			auto& hostname = opt.target_host;
			auto& port = opt.target_port;
			net::streambuf request;

			std::size_t bytes_to_write = 9 + username.size();
			if (opt.version == socks4a_version)
				bytes_to_write += opt.target_host.size() + 1;
			auto req = static_cast<char*>(
				request.prepare(bytes_to_write).data());

			write<uint8_t>(SOCKS_VERSION_4, req);	// SOCKS VERSION 4.
			write<uint8_t>(SOCKS_CMD_CONNECT, req); // CONNECT.

			write<uint16_t>(port, req);				// DST PORT.

			auto address = net::ip::make_address_v4(hostname, ec);
			if (ec && opt.version != socks4a_version)
			{
				auto executor = co_await net::this_coro::executor;
				tcp::resolver resolver{ executor };
				auto error = ec;

				auto target_endpoints = co_await resolver.async_resolve(
					hostname,
					std::to_string(port),
					net_awaitable[ec]);
				if (ec) co_return;

				if (target_endpoints.empty())
				{
					ec = error;
					co_return;
				}

				auto it = *target_endpoints.begin();
				address = it.endpoint().address().to_v4();
			}

			// Using socks4a...
			if (opt.version == socks4a_version)
				address = net::ip::make_address("0.0.0.1").to_v4();

			write<uint32_t>(address.to_uint(), req); // DST I

			if (!username.empty())
			{
				for (size_t i = 0; i < username.size(); i++)
				{
					if (username[i] == '\0') {
						ec = errc::socks_invalid_userid;
						co_return;
					}
					write<uint8_t>(username[i], req);    // USERID
				}

				req += username.size();
			}
			write<uint8_t>(0, req); // NULL.

			if (opt.version == socks4a_version)
			{
				std::copy(opt.target_host.begin(), opt.target_host.end(), req);
				req += opt.target_host.size();
				write<uint8_t>(0, req); // NULL.
			}

			request.commit(bytes_to_write);
			co_await net::async_write(
				socket,
				request,
				net_awaitable[ec]);
			if (ec) co_return;

			net::streambuf response;
			co_await net::async_read(
				socket,
				response,
				net::transfer_exactly(8),
				net_awaitable[ec]);
			if (ec) co_return;

			auto resp = static_cast<const unsigned char*>(
				response.data().data());

			// VN is the version of the reply code and should be 0.
			read<uint8_t>(resp);
			auto cd = read<uint8_t>(resp);

			if (cd != SOCKS4_REQUEST_GRANTED)
			{
				switch (cd)
				{
				case SOCKS4_REQUEST_REJECTED_OR_FAILED:
					ec = errc::socks_request_rejected_or_failed;
					break;
				case SOCKS4_CANNOT_CONNECT_TARGET_SERVER:
					ec = errc::socks_request_rejected_cannot_connect;
					break;
				case SOCKS4_REQUEST_REJECTED_USER_NO_ALLOW:
					ec = errc::socks_request_rejected_incorrect_userid;
					break;
				default:
					ec = errc::socks_unknown_error;
					break;
				}
			}

			co_return;
		}

		template <typename Stream>
		net::awaitable<boost::system::error_code>
			do_socks_handshake(Stream& socket, socks_client_option opt = {})
		{
			boost::system::error_code ec;

			if (opt.version == socks5_version)
			{
				co_await do_socks5(socket, opt, ec);
			}
			else if (opt.version == socks4_version ||
				opt.version == socks4a_version)
			{
				co_await do_socks4(socket, opt, ec);
			}
			else
			{
				ec = proxy::errc::socks_unsupported_version;
			}

			co_return ec;
		}

		struct initiate_do_socks_proxy
		{
			template <typename Stream, typename Handler>
			void operator()(Handler&& handler,
				Stream* socket, socks_client_option opt) const
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

	template <typename Stream, typename Handler>
	auto async_socks_handshake(Stream& socket,
		socks_client_option opt, Handler&& handler)
	{
		return net::async_initiate<Handler,
			void(boost::system::error_code)>(
				detail::initiate_do_socks_proxy(), handler, &socket, opt);
	}

}

#endif // INCLUDE__2023_10_18__SOCKS_CLIENT_HPP
