//
// socks_server.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "socks/logging.hpp"

#include <memory>
#include <string>
#include <array>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>


namespace socks {

	using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
	using udp = boost::asio::ip::udp;               // from <boost/asio/ip/udp.hpp>

	class socks_server;
	class socks_session
		: public std::enable_shared_from_this<socks_session>
	{
		socks_session(const socks_session&) = delete;
		socks_session& operator=(const socks_session&) = delete;

	public:
		socks_session(tcp::socket&& socket, size_t id, std::weak_ptr<socks_server> server);
		~socks_session();

	public:
		void start(std::string bind_addr);
		void close();

	private:
		boost::asio::awaitable<void> start_socks_proxy();
		boost::asio::awaitable<void> socks_connect_v5();
		boost::asio::awaitable<void> socks_connect_v4();
		boost::asio::awaitable<bool> socks_auth();
		boost::asio::awaitable<void> transfer(tcp::socket& from, tcp::socket& to);

	private:
		tcp::socket m_local_socket;
		tcp::socket m_remote_socket;
		size_t m_connection_id;
		std::array<char, 2048> m_local_buffer{};
		std::weak_ptr<socks_server> m_socks_server;
		std::string m_bind_addr;
		bool m_abort{ false };
	};

	using socks_session_ptr = std::shared_ptr<socks_session>;
	using socks_session_weak_ptr = std::weak_ptr<socks_session>;


	//////////////////////////////////////////////////////////////////////////

	struct socks_server_option
	{
		std::string usrdid_;
		std::string passwd_;

		std::string bind_addr_;
	};

	class socks_server
		: public std::enable_shared_from_this<socks_server>
	{
		socks_server(const socks_server&) = delete;
		socks_server& operator=(const socks_server&) = delete;

	public:
		socks_server(boost::asio::io_context& ioc,
			const tcp::endpoint& endp, socks_server_option opt = {});
		~socks_server() = default;

	public:
		void close();

		void remove_client(size_t id);
		bool do_auth(const std::string& userid, const std::string& passwd);
		bool auth_require();

	private:
		boost::asio::awaitable<void> start_socks_listen(tcp::acceptor& a);

	private:
		boost::asio::io_context& m_io_context;
		tcp::acceptor m_acceptor;
		socks_server_option m_option;
		std::unordered_map<size_t, socks_session_weak_ptr> m_clients;
		bool m_abort{ false };
	};

}
