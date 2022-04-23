//
// socks_server.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "socks/socks_server.hpp"
#include "socks/socks_enums.hpp"

#include "socks/use_awaitable.hpp"
#include "socks/scoped_exit.hpp"
#include "socks/async_connect.hpp"

#include <boost/asio/streambuf.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace socks {
	namespace detail {

		template<typename type, typename source>
		type read(source& p)
		{
			type ret = 0;
			for (std::size_t i = 0; i < sizeof(type); i++)
				ret = (ret << 8) | (static_cast<unsigned char>(*p++));
			return ret;
		}

		template<typename type, typename target>
		void write(type v, target& p)
		{
			for (auto i = (int)sizeof(type) - 1; i >= 0; i--, p++)
				*p = static_cast<unsigned char>((v >> (i * 8)) & 0xff);
		}

	} // detail

	using namespace boost::asio::experimental::awaitable_operators;
	using namespace util;
	using namespace boost::asio;

	using detail::write;
	using detail::read;

	socks_session::socks_session(tcp::socket&& socket, size_t id, std::weak_ptr<socks_server> server)
		: m_local_socket(std::move(socket))
		, m_remote_socket(m_local_socket.get_executor())
		, m_connection_id(id)
		, m_socks_server(server)
	{
	}

	socks_session::~socks_session()
	{
		auto server = m_socks_server.lock();
		if (!server)
			return;

		server->remove_client(m_connection_id);
	}

	void socks_session::start(std::string bind_addr)
	{
		m_bind_addr = bind_addr;

		boost::asio::co_spawn(m_local_socket.get_executor(),
			start_socks_proxy(), boost::asio::detached);
	}

	void socks_session::close()
	{
		m_abort = true;

		boost::system::error_code ignore_ec;
		m_local_socket.close(ignore_ec);
		m_remote_socket.close(ignore_ec);
	}

	boost::asio::awaitable<void> socks_session::start_socks_proxy()
	{
		// 保持整个生命周期在协程栈上.
		auto self = shared_from_this();

		// read
		//  +----+----------+----------+
		//  |VER | NMETHODS | METHODS  |
		//  +----+----------+----------+
		//  | 1  |    1     | 1 to 255 |
		//  +----+----------+----------+
		//  [               ]
		// or
		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//  | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//    1    1      2        4                  variable       1
		//  [         ]
		// 读取[]里的部分.

		boost::system::error_code ec;

		[[maybe_unused]] auto bytes = co_await boost::asio::async_read(m_local_socket,
			boost::asio::buffer(m_local_buffer),
				boost::asio::transfer_exactly(2),
					asio_util::use_awaitable[ec]);
		if (ec)
		{
			LOG_ERR << "id: " << m_connection_id << ", read socks version: " << ec.message();
			co_return;
		}
		BOOST_ASSERT(bytes == 2);

		char* p = m_local_buffer.data();
		int socks_version = read<uint8_t>(p);
		if (socks_version == SOCKS_VERSION_5)
		{
			co_await socks_connect_v5();
			co_return;
		}
		if (socks_version == SOCKS_VERSION_4)
		{
			co_await socks_connect_v4();
			co_return;
		}

		co_return;
	}

	boost::asio::awaitable<void> socks_session::socks_connect_v5()
	{
		char* p = m_local_buffer.data();

		auto socks_version = read<int8_t>(p);
		BOOST_ASSERT(socks_version == SOCKS_VERSION_5);
		int nmethods = read<int8_t>(p);
		if (nmethods <= 0 || nmethods > 255)
		{
			LOG_ERR << "id: " << m_connection_id << ", unsupported method : " << nmethods;
			co_return;
		}

		//  +----+----------+----------+
		//  |VER | NMETHODS | METHODS  |
		//  +----+----------+----------+
		//  | 1  |    1     | 1 to 255 |
		//  +----+----------+----------+
		//                  [          ]

		boost::system::error_code ec;
		auto bytes = co_await boost::asio::async_read(m_local_socket,
			boost::asio::buffer(m_local_buffer, nmethods),
				boost::asio::transfer_exactly(nmethods),
					asio_util::use_awaitable[ec]);
		if (ec)
		{
			LOG_ERR << "id: " << m_connection_id << ", read socks methods: " << ec.message();
			co_return;
		}

		auto server = m_socks_server.lock();
		if (!server)
			co_return;

		// 服务端是否需要认证.
		auto auth_required = server->auth_require();

		// 循环读取客户端支持的代理方式.
		p = m_local_buffer.data();

		int method = SOCKS5_AUTH_UNACCEPTABLE;
		while (bytes != 0)
		{
			int m = read<int8_t>(p);

			if (auth_required)
			{
				if (m == SOCKS5_AUTH)
				{
					method = m;
					break;
				}
			}
			else
			{
				if (m == SOCKS5_AUTH_NONE || m == SOCKS5_AUTH)
				{
					method = m;
					break;
				}
			}

			bytes--;
		}

		// 客户端不支持认证, 而如果服务端需要认证, 回复客户端不接受.
		if (method == SOCKS5_AUTH_UNACCEPTABLE)
		{
			// 回复客户端, 不接受客户端的的代理请求.
			p = m_local_buffer.data();
			write<uint8_t>(socks_version, p);
			write<uint8_t>(SOCKS5_AUTH_UNACCEPTABLE, p);
		}
		else
		{
			// 回复客户端, server所选择的代理方式.
			p = m_local_buffer.data();
			write<uint8_t>(socks_version, p);
			write<uint8_t>((uint8_t)method, p);
		}

		//  +----+--------+
		//  |VER | METHOD |
		//  +----+--------+
		//  | 1  |   1    |
		//  +----+--------+
		//  [             ]
		bytes = co_await boost::asio::async_write(m_local_socket,
			boost::asio::buffer(m_local_buffer, 2),
				boost::asio::transfer_exactly(2),
					asio_util::use_awaitable[ec]);
		if (ec)
		{
			LOG_WARN << "id: " << m_connection_id << ", write server method error : " << ec.message();
			co_return;
		}

		if (method == SOCKS5_AUTH_UNACCEPTABLE)
		{
			LOG_WARN << "id: " << m_connection_id << ", no acceptable methods for server";
			co_return;
		}

		// 认证模式, 则进入认证子协程.
		if (method == SOCKS5_AUTH)
		{
			auto ret = co_await socks_auth();
			if (!ret)
				co_return;
		}

		//  +----+-----+-------+------+----------+----------+
		//  |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
		//  +----+-----+-------+------+----------+----------+
		//  | 1  |  1  | X'00' |  1   | Variable |    2     |
		//  +----+-----+-------+------+----------+----------+
		//  [                          ]
		bytes = co_await boost::asio::async_read(m_local_socket,
			boost::asio::buffer(m_local_buffer, 5),
				boost::asio::transfer_exactly(5),
					asio_util::use_awaitable[ec]);
		if (ec)
		{
			LOG_WARN << "id: " << m_connection_id << ", read client request error: " << ec.message();
			co_return;
		}

		p = m_local_buffer.data();
		auto ver = read<int8_t>(p);
		if (ver != SOCKS_VERSION_5)
		{
			LOG_WARN << "id: " << m_connection_id << ", socks requests, invalid protocol: " << ver;
			co_return;
		}

		int command = read<int8_t>(p);		// CONNECT/BIND/UDP
		read<int8_t>(p);					// reserved.
		int atyp = read<int8_t>(p);		// atyp.

		//  +----+-----+-------+------+----------+----------+
		//  |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
		//  +----+-----+-------+------+----------+----------+
		//  | 1  |  1  | X'00' |  1   | Variable |    2     |
		//  +----+-----+-------+------+----------+----------+
		//                              [                   ]
		int length = 0;
		int prefix = 1;

		// 保存第一个字节.
		m_local_buffer[0] = m_local_buffer[4];

		if (atyp == SOCKS5_ATYP_IPV4)
			length = 5; // 6 - 1
		else if (atyp == SOCKS5_ATYP_DOMAINNAME)
		{
			length = read<uint8_t>(p) + 2;
			prefix = 0;
		}
		else if (atyp == SOCKS5_ATYP_IPV6)
			length = 17; // 18 - 1

		bytes = co_await boost::asio::async_read(m_local_socket,
			boost::asio::buffer(m_local_buffer.data() + prefix, length),
				boost::asio::transfer_exactly(length),
					asio_util::use_awaitable[ec]);
		if (ec)
		{
			LOG_WARN << "id: " << m_connection_id << ", read client request dst.addr error: " << ec.message();
			co_return;
		}

		auto bind_interface = ip::address::from_string(m_bind_addr, ec);
		if (ec)
			m_bind_addr.clear();

		auto check_condition = [this, bind_interface](const boost::system::error_code&,
			tcp::socket& stream, auto&) mutable
		{
			if (m_bind_addr.empty())
				return true;

			tcp::endpoint bind_endpoint(bind_interface, 0);
			boost::system::error_code err;

			stream.open(bind_endpoint.protocol(), err);
			if (err)
				return false;

			stream.bind(bind_endpoint, err);
			if (err)
				return false;

			return true;
		};

		tcp::endpoint dst_endpoint;
		std::string domain;
		uint16_t port = 0;

		auto executor = co_await boost::asio::this_coro::executor;
		tcp::socket& remote_socket = m_remote_socket;

		p = m_local_buffer.data();
		if (atyp == SOCKS5_ATYP_IPV4)
		{
			dst_endpoint.address(boost::asio::ip::address_v4(read<uint32_t>(p)));
			dst_endpoint.port(read<uint16_t>(p));

			LOG_DBG << "id: " << m_connection_id << ", " << m_local_socket.remote_endpoint() << " use ipv4: "
				<< dst_endpoint;

			if (command == SOCKS_CMD_CONNECT)
			{
				auto target = ip::basic_resolver_results<tcp>::create(dst_endpoint, "", "");
				co_await asio_util::async_connect(remote_socket, target, check_condition, asio_util::use_awaitable[ec]);
				if (ec)
				{
					LOG_WFMT("id: {}, connect to target {}:{} error: {}",
						m_connection_id, dst_endpoint.address().to_string(), port, ec.message());
				}
			}
		}
		else if (atyp == SOCKS5_ATYP_DOMAINNAME)
		{
			for (size_t i = 0; i < bytes - 2; i++)
				domain.push_back(read<int8_t>(p));
			port = read<uint16_t>(p);
			LOG_DBG << "id: " << m_connection_id << ", " << m_local_socket.remote_endpoint()
				<< " use domain: " << domain << ":" << port;

			if (command == SOCKS_CMD_CONNECT)
			{
				tcp::resolver resolver{ executor };

				auto target_endpoints = co_await resolver.async_resolve(
					domain, std::to_string(port), asio_util::use_awaitable[ec]);
				bool connected = false;

				for (auto endpoint : target_endpoints)
				{
					dst_endpoint = endpoint;

					auto target = ip::basic_resolver_results<tcp>::create(dst_endpoint, "", "");
					co_await asio_util::async_connect(remote_socket, target, check_condition, asio_util::use_awaitable[ec]);
					if (!ec)
					{
						connected = true;
						break;
					}
				}

				if (!connected)
				{
					LOG_WFMT("id: {}, connect to target {}:{} error: {}",
						m_connection_id, domain, port, ec.message());
				}
			}
		}
		else if (atyp == SOCKS5_ATYP_IPV6)
		{
			boost::asio::ip::address_v6::bytes_type addr;
			for (boost::asio::ip::address_v6::bytes_type::iterator i = addr.begin();
				i != addr.end(); ++i)
			{
				*i = read<int8_t>(p);
			}

			dst_endpoint.address(boost::asio::ip::address_v6(addr));
			dst_endpoint.port(read<uint16_t>(p));
			LOG_DBG << "id: " << m_connection_id << ", "
				<< m_local_socket.remote_endpoint() << " use ipv6: " << dst_endpoint;

			if (command == SOCKS_CMD_CONNECT)
			{
				auto target = ip::basic_resolver_results<tcp>::create(dst_endpoint, "", "");
				co_await asio_util::async_connect(remote_socket, target, check_condition, asio_util::use_awaitable[ec]);
				if (ec)
				{
					LOG_WFMT("id: {}, connect to target {}:{} error: {}",
						m_connection_id, dst_endpoint.address().to_string(), port, ec.message());
				}
			}
		}

		// 连接成功或失败.
		{
			int8_t error_code = SOCKS5_SUCCEEDED;

			if (ec == boost::asio::error::connection_refused)
				error_code = SOCKS5_CONNECTION_REFUSED;
			else if (ec == boost::asio::error::network_unreachable)
				error_code = SOCKS5_NETWORK_UNREACHABLE;
			else if (ec)
				error_code = SOCKS5_GENERAL_SOCKS_SERVER_FAILURE;

			//  +----+-----+-------+------+----------+----------+
			//  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
			//  +----+-----+-------+------+----------+----------+
			//  | 1  |  1  | X'00' |  1   | Variable |    2     |
			//  +----+-----+-------+------+----------+----------+
			//  [                                               ]

			p = m_local_buffer.data();

			write<uint8_t>(SOCKS_VERSION_5, p); // VER
			write<uint8_t>(error_code, p);		// REP
			write<uint8_t>(0x00, p);			// RSV

			if (dst_endpoint.address().is_v4())
			{
				write<uint8_t>(SOCKS5_ATYP_IPV4, p);
				write<uint32_t>(dst_endpoint.address().to_v4().to_ulong(), p);
				write<uint16_t>(dst_endpoint.port(), p);
			}
			else if (dst_endpoint.address().is_v6())
			{
				write<uint8_t>(SOCKS5_ATYP_IPV6, p);
				auto data = dst_endpoint.address().to_v6().to_bytes();
				for (auto c : data)
					write<uint8_t>(c, p);
				write<uint16_t>(dst_endpoint.port(), p);
			}
			else if (!domain.empty())
			{
				write<uint8_t>(SOCKS5_ATYP_DOMAINNAME, p);
				write<uint8_t>(static_cast<int8_t>(domain.size()), p);
				std::copy(domain.begin(), domain.end(), p);
				write<uint16_t>(port, p);
			}
			else
			{
				write<uint8_t>(0x1, p);
				write<uint32_t>(0, p);
				write<uint16_t>(0, p);
			}

			bytes = co_await boost::asio::async_write(m_local_socket,
				boost::asio::buffer(m_local_buffer, 10),
					boost::asio::transfer_exactly(10),
						asio_util::use_awaitable[ec]);
			if (ec)
			{
				LOG_WARN << "id: " << m_connection_id << ", write server response error: " << ec.message();
				co_return;
			}

			if (error_code != SOCKS5_SUCCEEDED)
				co_return;
		}

		LOG_DBG << "id: " << m_connection_id << ", connected start transfer";

		// 发起数据传输协程.
		if (command == SOCKS_CMD_CONNECT)
		{
			co_await(
					transfer(m_local_socket, remote_socket)
					&&
					transfer(remote_socket, m_local_socket)
				);

			LOG_DBG << "id: " << m_connection_id << ", transfer completed";
		}
		else
		{
			LOG_WARN << "id: " << m_connection_id << ", SOCKS_CMD_BIND and SOCKS5_CMD_UDP is unsupported";
		}

		co_return;
	}

	boost::asio::awaitable<void> socks_session::socks_connect_v4()
	{
		char* p = m_local_buffer.data();

		[[maybe_unused]] auto socks_version = read<int8_t>(p);
		BOOST_ASSERT(socks_version == SOCKS_VERSION_4);
		auto command = read<int8_t>(p);

		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//  | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//  | 1  | 1  |    2    |         4         | variable     | 1  |
		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//            [                             ]

		boost::system::error_code ec;
		auto bytes = co_await boost::asio::async_read(m_local_socket,
			boost::asio::buffer(m_local_buffer, 6),
				boost::asio::transfer_exactly(6),
					asio_util::use_awaitable[ec]);
		if (ec)
		{
			LOG_WARN << "id: " << m_connection_id << ", read socks4 dst: " << ec.message();
			co_return;
		}

		tcp::endpoint dst_endpoint;
		p = m_local_buffer.data();

		auto port = read<uint16_t>(p);
		dst_endpoint.port(port);
		dst_endpoint.address(boost::asio::ip::address_v4(read<uint32_t>(p)));

		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//  | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//  | 1  | 1  |    2    |         4         | variable     | 1  |
		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//                                          [                   ]
		boost::asio::streambuf sbuf;
		bytes = co_await boost::asio::async_read_until(m_local_socket,
			sbuf, '\0', asio_util::use_awaitable[ec]);
		if (ec)
		{
			LOG_WARN << "id: " << m_connection_id << ", read socks4 userid: " << ec.message();
			co_return;
		}

		std::string userid;
		if (bytes > 1)
		{
			userid.resize(bytes - 1);
			sbuf.sgetn(&userid[0], bytes - 1);
		}
		sbuf.commit(1);

		// 用户认证逻辑.
		bool verify_passed = false;
		auto server = m_socks_server.lock();

		if (server)
		{
			verify_passed = server->do_auth(userid, "", SOCKS_VERSION_4);
			server.reset();
		}

		if (!verify_passed)
		{
			//  +----+----+----+----+----+----+----+----+
			//  | VN | CD | DSTPORT |      DSTIP        |
			//  +----+----+----+----+----+----+----+----+
			//  | 1  | 1  |    2    |         4         |
			//  +----+----+----+----+----+----+----+----+
			//  [                                       ]

			p = m_local_buffer.data();
			write<uint8_t>(0, p);
			write<uint8_t>(SOCKS4_REQUEST_REJECTED_USER_NO_ALLOW, p);

			write<uint16_t>(dst_endpoint.port(), p);
			write<uint32_t>(dst_endpoint.address().to_v4().to_ulong(), p);

			bytes = co_await boost::asio::async_write(m_local_socket,
				boost::asio::buffer(m_local_buffer, 8),
					boost::asio::transfer_exactly(8),
						asio_util::use_awaitable[ec]);
			if (ec)
			{
				LOG_WARN << "id: " << m_connection_id << ", write socks4 no allow: " << ec.message();
				co_return;
			}

			LOG_WARN << "id: " << m_connection_id << ", socks4 " << userid << " auth fail";
			co_return;
		}

		int error_code = SOCKS4_REQUEST_GRANTED;
		tcp::socket& remote_socket = m_remote_socket;
		if (command == SOCKS_CMD_CONNECT)
		{
			auto target = ip::basic_resolver_results<tcp>::create(dst_endpoint, "", "");
			co_await asio_util::async_connect(remote_socket, target, asio_util::use_awaitable[ec]);
			if (ec)
			{
				LOG_WFMT("id: {}, connect to target {}:{} error: {}",
					m_connection_id, dst_endpoint.address().to_string(), port, ec.message());
				error_code = SOCKS4_CANNOT_CONNECT_TARGET_SERVER;
			}
		}
		else
		{
			error_code = SOCKS4_REQUEST_REJECTED_OR_FAILED;
			LOG_WFMT("id: {}, unsupported command for socks4", m_connection_id);
		}

		//  +----+----+----+----+----+----+----+----+
		//  | VN | CD | DSTPORT |      DSTIP        |
		//  +----+----+----+----+----+----+----+----+
		//  | 1  | 1  |    2    |         4         |
		//  +----+----+----+----+----+----+----+----+
		//  [                                       ]
		p = m_local_buffer.data();
		write<uint8_t>(0, p);
		write<uint8_t>((uint8_t)error_code, p);

		// 返回IP:PORT.
		write<uint16_t>(dst_endpoint.port(), p);
		write<uint32_t>(dst_endpoint.address().to_v4().to_ulong(), p);

		bytes = co_await boost::asio::async_write(m_local_socket,
			boost::asio::buffer(m_local_buffer, 8),
				boost::asio::transfer_exactly(8),
					asio_util::use_awaitable[ec]);
		if (ec)
		{
			LOG_WARN << "id: " << m_connection_id << ", write socks4 response: " << ec.message();
			co_return;
		}

		if (error_code != SOCKS4_REQUEST_GRANTED)
			co_return;

		co_await(
			transfer(m_local_socket, remote_socket)
			&&
			transfer(remote_socket, m_local_socket)
			);

		LOG_DBG << "id: " << m_connection_id << ", transfer completed";
		co_return;
	}

	boost::asio::awaitable<bool> socks_session::socks_auth()
	{
		//  +----+------+----------+------+----------+
		//  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
		//  +----+------+----------+------+----------+
		//  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
		//  +----+------+----------+------+----------+
		//  [           ]

		boost::system::error_code ec;

		auto bytes = co_await boost::asio::async_read(m_local_socket,
			boost::asio::buffer(m_local_buffer, 2),
			boost::asio::transfer_exactly(2),
			asio_util::use_awaitable[ec]);
		if (ec)
		{
			LOG_WARN << "id: " << m_connection_id
				<< ", read client username/passwd error: " << ec.message();
			co_return false;
		}

		auto p = m_local_buffer.data();
		int auth_version = read<int8_t>(p);
		if (auth_version != 1)
		{
			LOG_WARN << "id: " << m_connection_id << ", socks negotiation, unsupported socks5 protocol";
			co_return false;
		}
		int name_length = read<uint8_t>(p);
		if (name_length <= 0 || name_length > 255)
		{
			LOG_WARN << "id: " << m_connection_id << ", socks negotiation, invalid name length";
			co_return false;
		}
		name_length += 1;

		//  +----+------+----------+------+----------+
		//  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
		//  +----+------+----------+------+----------+
		//  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
		//  +----+------+----------+------+----------+
		//              [                 ]

		bytes = co_await boost::asio::async_read(m_local_socket,
			boost::asio::buffer(m_local_buffer, name_length),
			boost::asio::transfer_exactly(name_length),
			asio_util::use_awaitable[ec]);
		if (ec)
		{
			LOG_WARN << "id: " << m_connection_id << ", read client username error: " << ec.message();
			co_return false;
		}

		std::string uname;

		p = m_local_buffer.data();
		for (size_t i = 0; i < bytes - 1; i++)
			uname.push_back(read<int8_t>(p));

		int passwd_len = read<uint8_t>(p);
		if (passwd_len <= 0 || passwd_len > 255)
		{
			LOG_WARN << "id: " << m_connection_id << ", socks negotiation, invalid passwd length";
			co_return false;
		}

		//  +----+------+----------+------+----------+
		//  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
		//  +----+------+----------+------+----------+
		//  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
		//  +----+------+----------+------+----------+
		//                                [          ]

		bytes = co_await boost::asio::async_read(m_local_socket,
			boost::asio::buffer(m_local_buffer, passwd_len),
			boost::asio::transfer_exactly(passwd_len),
			asio_util::use_awaitable[ec]);
		if (ec)
		{
			LOG_WARN << "id: " << m_connection_id << ", read client passwd error: " << ec.message();
			co_return false;
		}

		std::string passwd;

		p = m_local_buffer.data();
		for (size_t i = 0; i < bytes; i++)
			passwd.push_back(read<int8_t>(p));

		// SOCKS5验证用户和密码.
		auto endp = m_local_socket.remote_endpoint();
		auto client = endp.address().to_string();
		client += ":" + std::to_string(endp.port());

		// 用户认证逻辑.
		bool verify_passed = false;
		auto server = m_socks_server.lock();

		if (server)
		{
			verify_passed = server->do_auth(uname, passwd);
			server.reset();
		}

		p = m_local_buffer.data();
		write<uint8_t>(0x01, p);			// version 只能是1.
		if (verify_passed)
		{
			write<uint8_t>(0x00, p);		// 认证通过返回0x00, 其它值为失败.
		}
		else
		{
			write<uint8_t>(0x01, p);		// 认证返回0x01为失败.
		}

		// 返回认证状态.
		//  +----+--------+
		//  |VER | STATUS |
		//  +----+--------+
		//  | 1  |   1    |
		//  +----+--------+
		co_await boost::asio::async_write(m_local_socket,
			boost::asio::buffer(m_local_buffer, 2),
			boost::asio::transfer_exactly(2),
			asio_util::use_awaitable[ec]);
		if (ec)
		{
			LOG_WARN << "id: " << m_connection_id << ", server write status error: " << ec.message();
			co_return false;
		}

		co_return true;
	}

	boost::asio::awaitable<void> socks_session::transfer(tcp::socket& from, tcp::socket& to)
	{
		std::vector<char> data(65536, 0);
		boost::system::error_code ec;

		for (; !m_abort;)
		{
			auto bytes = co_await from.async_read_some(
				boost::asio::buffer(data), asio_util::use_awaitable[ec]);
			if (ec || m_abort)
			{
				to.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
				co_return;
			}

			co_await boost::asio::async_write(to,
				boost::asio::buffer(data, bytes), asio_util::use_awaitable[ec]);
			if (ec || m_abort)
			{
				from.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
				co_return;
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////

	socks_server::socks_server(boost::asio::any_io_executor& executor,
		const tcp::endpoint& endp, socks_server_option opt)
		: m_executor(executor)
		, m_acceptor(executor, endp)
		, m_option(std::move(opt))
	{
		boost::system::error_code ec;
		m_acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
	}

	void socks_server::open()
	{
		// 同时启动32个连接协程, 开始为socks client提供服务.
		for (int i = 0; i < 32; i++)
		{
			boost::asio::co_spawn(m_executor,
				start_socks_listen(m_acceptor), boost::asio::detached);
		}
	}

	void socks_server::close()
	{
		boost::system::error_code ignore_ec;
		m_abort = true;

		m_acceptor.close(ignore_ec);

		for (auto& [id, c] : m_clients)
		{
			auto client = c.lock();
			if (!client)
				continue;
			client->close();
		}
	}

	void socks_server::remove_client(size_t id)
	{
		m_clients.erase(id);
	}

	bool socks_server::do_auth(const std::string& userid,
		const std::string& passwd, int version)
	{
		if (m_option.usrdid_.empty())
			return true;

		if (userid == m_option.usrdid_
			&& (passwd == m_option.passwd_ || version == SOCKS_VERSION_4))
			return true;

		return false;
	}

	bool socks_server::auth_require()
	{
		if (!m_option.usrdid_.empty())
			return true;

		return false;
	}

	boost::asio::awaitable<void> socks_server::start_socks_listen(tcp::acceptor& a)
	{
		auto self = shared_from_this();
		boost::system::error_code error;

		while (!m_abort)
		{
			tcp::socket socket(m_executor);
			co_await a.async_accept(socket, asio_util::use_awaitable[error]);
			if (error)
			{
				LOG_ERR << "start_socks_listen, async_accept: " << error.message();

				if (error == boost::asio::error::operation_aborted ||
					error == boost::asio::error::bad_descriptor)
				{
					co_return;
				}

				if (!a.is_open())
					co_return;

				continue;
			}

			{
				boost::asio::socket_base::keep_alive option(true);
				socket.set_option(option, error);
			}

			{
				boost::asio::ip::tcp::no_delay option(true);
				socket.set_option(option);
			}

			static std::atomic_size_t id{ 1 };
			size_t connection_id = id++;

			LOG_DBG << "start client incoming id: " << connection_id;

			socks_session_ptr new_session =
				std::make_shared<socks_session>(std::move(socket), connection_id, self);
			m_clients[connection_id] = new_session;

			new_session->start(m_option.bind_addr_);
		}

		LOG_WARN << "start_socks_listen exit ...";
		co_return;
	}

}
