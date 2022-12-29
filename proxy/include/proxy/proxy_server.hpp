//
// socks_server.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "proxy/use_awaitable.hpp"
#include "proxy/scoped_exit.hpp"
#include "proxy/async_connect.hpp"
#include "proxy/logging.hpp"
#include "proxy/base_stream.hpp"
#include "proxy/default_cert.hpp"
#include "proxy/fileop.hpp"

#include "proxy/socks_enums.hpp"
#include "proxy/socks_client.hpp"
#include "proxy/socks_io.hpp"

#include <memory>
#include <string>
#include <array>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/core/detail/base64.hpp>

#include <boost/url.hpp>


namespace proxy {

	namespace net = boost::asio;

	using namespace net::experimental::awaitable_operators;
	using namespace util;

	using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
	using udp = net::ip::udp;               // from <boost/asio/ip/udp.hpp>

	namespace beast = boost::beast;			// from <boost/beast.hpp>
	namespace http = beast::http;           // from <boost/beast/http.hpp>

	namespace urls = boost::urls;			// form <boost/url.hpp>

	using ssl_stream = net::ssl::stream<tcp::socket>;

	using io_util::read;
	using io_util::write;

	// proxy server 参数选项.
	struct proxy_server_option
	{
		// 指定当前proxy server认证用户id名称.
		std::string usrdid_;
		// 指定当前proxy server认证密码.
		std::string passwd_;

		// 指定当前proxy server向外发起连接时, 绑定到哪个本地地址.
		std::string bind_addr_;

		// 多层代理, 当前服务器级连下一个服务器, 对于 client 而言
		// 是无感的, 这是当前服务器通过 next_proxy_ 指定的下一个
		// 代理服务器, 为 client 实现多层代理.
		// 例如: socks5://user:passwd@proxy.server.com:1080
		// 或:   https://user:passwd@proxy.server.com:1080
		// socks5 默认使用 hostname 模式, 即 dns 解析在远程执行.
		std::string next_proxy_;

		// 多层代理模式中, 与下一个代理服务器是否使用tls加密(ssl).
		bool next_proxy_use_ssl_{ false };

		// 使用ssl的时候, 指定证书目录.
		std::string ssl_cert_path_;
	};

	// proxy server 虚基类, 任何 proxy server 的实现, 必须基于这个基类.
	// 这样 proxy_session 才能通过虚基类指针访问proxy server的具体实
	// 现以及虚函数方法.
	class proxy_server_base {
	public:
		virtual ~proxy_server_base() {}
		virtual void remove_client(size_t id) = 0;
		virtual const proxy_server_option& option() = 0;
	};

	// proxy session 虚基类.
	class proxy_session_base {
	public:
		virtual ~proxy_session_base() {}
		virtual void start() = 0;
		virtual void close() = 0;
	};

	// proxy_session 抽象类, 它被设计为一个模板抽象类, 模板参数Stream
	// 指定与本地通信的stream对象, 默认使用tcp::socket, 可根据此
	// async_read/async_write等接口实现专用的stream类, 比如实现加密.
	class proxy_session
		: public proxy_session_base
		, public std::enable_shared_from_this<proxy_session>
	{
		proxy_session(const proxy_session&) = delete;
		proxy_session& operator=(const proxy_session&) = delete;

	public:
		proxy_session(proxy_stream_type&& socket,
			size_t id, std::weak_ptr<proxy_server_base> server)
			: m_local_socket(std::move(socket))
			, m_remote_socket(instantiate_proxy_stream(
				m_local_socket.get_executor()))
			, m_connection_id(id)
			, m_proxy_server(server)
		{
		}

		~proxy_session()
		{
			auto server = m_proxy_server.lock();
			if (!server)
				return;

			server->remove_client(m_connection_id);
		}

	public:
		virtual void start() override
		{
			auto server = m_proxy_server.lock();
			if (!server)
				return;

			m_option = server->option();

			if (!m_option.next_proxy_.empty())
			{
				try
				{
					m_next_proxy =
						std::make_unique<urls::url_view>(m_option.next_proxy_);
				}
				catch (const std::exception& e)
				{
					LOG_ERR << "socks id: " << m_connection_id
						<< ", params next_proxy error: " << m_option.next_proxy_
						<< ", exception: " << e.what();
					return;
				}
			}

			auto self = shared_from_this();

			net::co_spawn(m_local_socket.get_executor(),
				[self, this]() -> net::awaitable<void>
				{
					co_await start_socks_proxy();
				}, net::detached);
		}

		virtual void close() override
		{
			m_abort = true;

			boost::system::error_code ignore_ec;
			m_local_socket.close(ignore_ec);
			m_remote_socket.close(ignore_ec);
		}

	private:
		net::awaitable<void> start_socks_proxy()
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

			[[maybe_unused]] auto bytes =
				co_await net::async_read(
					m_local_socket,
					m_local_buffer,
					net::transfer_exactly(2),
					net_awaitable[ec]);
			if (ec)
			{
				LOG_ERR << "socks id: " << m_connection_id
					<< ", read socks version: " << ec.message();
				co_return;
			}
			BOOST_ASSERT(bytes == 2);

			auto p = net::buffer_cast<const char*>(m_local_buffer.data());
			int socks_version = read<uint8_t>(p);

			if (socks_version == SOCKS_VERSION_5)
			{
				LOG_DBG << "connection id: " << m_connection_id
					<< ", socks version: " << socks_version;

				co_await socks_connect_v5();
				co_return;
			}
			if (socks_version == SOCKS_VERSION_4)
			{
				LOG_DBG << "connection id: " << m_connection_id
					<< ", socks version: " << socks_version;

				co_await socks_connect_v4();
				co_return;
			}
			if (socks_version == 'G')
			{
				co_await net::async_write(
					m_local_socket,
					net::buffer(fake_web_page()),
					net::transfer_all(),
					net_awaitable[ec]);
			}
			else if (socks_version == 'C')
			{
				auto ret = co_await http_proxy_connect();
				if (!ret)
				{
					co_await net::async_write(
						m_local_socket,
						net::buffer(bad_request_page()),
						net::transfer_all(),
						net_awaitable[ec]);
				}
			}

			co_return;
		}

		net::awaitable<void> socks_connect_v5()
		{
			auto p = net::buffer_cast<const char*>(m_local_buffer.data());

			auto socks_version = read<int8_t>(p);
			BOOST_ASSERT(socks_version == SOCKS_VERSION_5);
			int nmethods = read<int8_t>(p);
			if (nmethods <= 0 || nmethods > 255)
			{
				LOG_ERR << "socks id: " << m_connection_id
					<< ", unsupported method : " << nmethods;
				co_return;
			}

			//  +----+----------+----------+
			//  |VER | NMETHODS | METHODS  |
			//  +----+----------+----------+
			//  | 1  |    1     | 1 to 255 |
			//  +----+----------+----------+
			//                  [          ]
			m_local_buffer.consume(m_local_buffer.size());
			boost::system::error_code ec;
			auto bytes = co_await net::async_read(m_local_socket,
				m_local_buffer,
				net::transfer_exactly(nmethods),
				net_awaitable[ec]);
			if (ec)
			{
				LOG_ERR << "socks id: " << m_connection_id
					<< ", read socks methods: " << ec.message();
				co_return;
			}

			auto server = m_proxy_server.lock();
			if (!server)
				co_return;

			// 服务端是否需要认证.
			const auto& srv_opt = server->option();
			auto auth_required = !srv_opt.usrdid_.empty();

			// 循环读取客户端支持的代理方式.
			p = net::buffer_cast<const char*>(m_local_buffer.data());

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

			net::streambuf wbuf;

			// 客户端不支持认证, 而如果服务端需要认证, 回复客户端不接受.
			if (method == SOCKS5_AUTH_UNACCEPTABLE)
			{
				// 回复客户端, 不接受客户端的的代理请求.
				auto wp = net::buffer_cast<char*>(wbuf.prepare(1024));
				write<uint8_t>(socks_version, wp);
				write<uint8_t>(SOCKS5_AUTH_UNACCEPTABLE, wp);
			}
			else
			{
				// 回复客户端, server所选择的代理方式.
				auto wp = net::buffer_cast<char*>(wbuf.prepare(1024));
				write<uint8_t>(socks_version, wp);
				write<uint8_t>((uint8_t)method, wp);
			}
			wbuf.commit(2);

			//  +----+--------+
			//  |VER | METHOD |
			//  +----+--------+
			//  | 1  |   1    |
			//  +----+--------+
			//  [             ]
			bytes = co_await net::async_write(m_local_socket,
				wbuf,
				net::transfer_exactly(2),
				net_awaitable[ec]);
			if (ec)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", write server method error : " << ec.message();
				co_return;
			}

			if (method == SOCKS5_AUTH_UNACCEPTABLE)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", no acceptable methods for server";
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
			m_local_buffer.consume(m_local_buffer.size());
			bytes = co_await net::async_read(m_local_socket,
				m_local_buffer,
				net::transfer_exactly(5),
				net_awaitable[ec]);
			if (ec)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", read client request error: " << ec.message();
				co_return;
			}

			p = net::buffer_cast<const char*>(m_local_buffer.data());
			auto ver = read<int8_t>(p);
			if (ver != SOCKS_VERSION_5)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", socks requests, invalid protocol: " << ver;
				co_return;
			}

			int command = read<int8_t>(p);		// CONNECT/BIND/UDP
			read<int8_t>(p);					// reserved.
			int atyp = read<int8_t>(p);			// atyp.

			//  +----+-----+-------+------+----------+----------+
			//  |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
			//  +----+-----+-------+------+----------+----------+
			//  | 1  |  1  | X'00' |  1   | Variable |    2     |
			//  +----+-----+-------+------+----------+----------+
			//                              [                   ]
			int length = 0;

			// 消费掉前4个字节, 保存第1个字节.
			m_local_buffer.consume(4);

			if (atyp == SOCKS5_ATYP_IPV4)
				length = 5; // 6 - 1
			else if (atyp == SOCKS5_ATYP_DOMAINNAME)
			{
				length = read<uint8_t>(p) + 2;
				m_local_buffer.consume(1);
			}
			else if (atyp == SOCKS5_ATYP_IPV6)
				length = 17; // 18 - 1

			bytes = co_await net::async_read(m_local_socket,
				m_local_buffer,
				net::transfer_exactly(length),
				net_awaitable[ec]);
			if (ec)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", read client request dst.addr error: "
					<< ec.message();
				co_return;
			}

			tcp::endpoint dst_endpoint;
			std::string domain;
			uint16_t port = 0;

			auto executor = co_await net::this_coro::executor;

			p = net::buffer_cast<const char*>(m_local_buffer.data());
			if (atyp == SOCKS5_ATYP_IPV4)
			{
				dst_endpoint.address(net::ip::address_v4(read<uint32_t>(p)));
				dst_endpoint.port(read<uint16_t>(p));

				LOG_DBG << "socks id: " << m_connection_id
					<< ", " << m_local_socket.remote_endpoint()
					<< " use ipv4: " << dst_endpoint;

				if (command == SOCKS_CMD_CONNECT)
				{
					co_await connect_host(
						dst_endpoint.address().to_string(),
						dst_endpoint.port(), ec);
				}
			}
			else if (atyp == SOCKS5_ATYP_DOMAINNAME)
			{
				for (size_t i = 0; i < bytes - 2; i++)
					domain.push_back(read<int8_t>(p));
				port = read<uint16_t>(p);
				LOG_DBG << "socks id: " << m_connection_id
					<< ", " << m_local_socket.remote_endpoint()
					<< " use domain: " << domain << ":" << port;

				if (command == SOCKS_CMD_CONNECT)
				{
					co_await connect_host(domain, port, ec, true);
				}
			}
			else if (atyp == SOCKS5_ATYP_IPV6)
			{
				net::ip::address_v6::bytes_type addr;
				for (auto i = addr.begin();
					i != addr.end(); ++i)
				{
					*i = read<int8_t>(p);
				}

				dst_endpoint.address(net::ip::address_v6(addr));
				dst_endpoint.port(read<uint16_t>(p));
				LOG_DBG << "socks id: " << m_connection_id
					<< ", " << m_local_socket.remote_endpoint()
					<< " use ipv6: " << dst_endpoint;

				if (command == SOCKS_CMD_CONNECT)
				{
					co_await connect_host(
						dst_endpoint.address().to_string(),
						dst_endpoint.port(),
						ec);
				}
			}

			// 连接成功或失败.
			{
				int8_t error_code = SOCKS5_SUCCEEDED;

				if (ec == net::error::connection_refused)
					error_code = SOCKS5_CONNECTION_REFUSED;
				else if (ec == net::error::network_unreachable)
					error_code = SOCKS5_NETWORK_UNREACHABLE;
				else if (ec)
					error_code = SOCKS5_GENERAL_SOCKS_SERVER_FAILURE;

				//  +----+-----+-------+------+----------+----------+
				//  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
				//  +----+-----+-------+------+----------+----------+
				//  | 1  |  1  | X'00' |  1   | Variable |    2     |
				//  +----+-----+-------+------+----------+----------+
				//  [                                               ]

				wbuf.consume(wbuf.size());
				auto wp = net::buffer_cast<char*>(
					wbuf.prepare(64 + domain.size()));

				write<uint8_t>(SOCKS_VERSION_5, wp); // VER
				write<uint8_t>(error_code, wp);		// REP
				write<uint8_t>(0x00, wp);			// RSV

				if (dst_endpoint.address().is_v4())
				{
					auto uaddr = dst_endpoint.address().to_v4().to_uint();

					write<uint8_t>(SOCKS5_ATYP_IPV4, wp);
					write<uint32_t>(uaddr, wp);
					write<uint16_t>(dst_endpoint.port(), wp);
				}
				else if (dst_endpoint.address().is_v6())
				{
					write<uint8_t>(SOCKS5_ATYP_IPV6, wp);
					auto data = dst_endpoint.address().to_v6().to_bytes();
					for (auto c : data)
						write<uint8_t>(c, wp);
					write<uint16_t>(dst_endpoint.port(), wp);
				}
				else if (!domain.empty())
				{
					write<uint8_t>(SOCKS5_ATYP_DOMAINNAME, wp);
					write<uint8_t>(static_cast<int8_t>(domain.size()), wp);
					std::copy(domain.begin(), domain.end(), wp);
					wp += domain.size();
					write<uint16_t>(port, wp);
				}
				else
				{
					write<uint8_t>(0x1, wp);
					write<uint32_t>(0, wp);
					write<uint16_t>(0, wp);
				}

				auto len = wp - net::buffer_cast<const char*>(wbuf.data());
				wbuf.commit(len);
				bytes = co_await net::async_write(m_local_socket,
					wbuf,
					net::transfer_exactly(len),
					net_awaitable[ec]);
				if (ec)
				{
					LOG_WARN << "socks id: " << m_connection_id
						<< ", write server response error: " << ec.message();
					co_return;
				}

				if (error_code != SOCKS5_SUCCEEDED)
					co_return;
			}

			LOG_DBG << "socks id: " << m_connection_id
				<< ", connected start transfer";

			// 发起数据传输协程.
			if (command == SOCKS_CMD_CONNECT)
			{
				co_await(
					transfer(m_local_socket, m_remote_socket)
					&&
					transfer(m_remote_socket, m_local_socket)
					);

				LOG_DBG << "socks id: " << m_connection_id
					<< ", transfer completed";
			}
			else
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", SOCKS_CMD_BIND and SOCKS5_CMD_UDP is unsupported";
			}

			co_return;
		}

		net::awaitable<void> socks_connect_v4()
		{
			auto self = shared_from_this();
			auto p = net::buffer_cast<const char*>(m_local_buffer.data());

			[[maybe_unused]] auto socks_version = read<int8_t>(p);
			BOOST_ASSERT(socks_version == SOCKS_VERSION_4);
			auto command = read<int8_t>(p);

			//  +----+----+----+----+----+----+----+----+----+----+....+----+
			//  | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
			//  +----+----+----+----+----+----+----+----+----+----+....+----+
			//  | 1  | 1  |    2    |         4         | variable     | 1  |
			//  +----+----+----+----+----+----+----+----+----+----+....+----+
			//            [                             ]
			m_local_buffer.consume(m_local_buffer.size());
			boost::system::error_code ec;
			auto bytes = co_await net::async_read(m_local_socket,
				m_local_buffer,
				net::transfer_exactly(6),
				net_awaitable[ec]);
			if (ec)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", read socks4 dst: " << ec.message();
				co_return;
			}

			tcp::endpoint dst_endpoint;
			p = net::buffer_cast<const char*>(m_local_buffer.data());

			auto port = read<uint16_t>(p);
			dst_endpoint.port(port);
			dst_endpoint.address(net::ip::address_v4(read<uint32_t>(p)));

			bool socks4a = false;
			auto tmp = dst_endpoint.address().to_v4().to_uint() ^ 0x000000ff;
			if (0xff > tmp)
				socks4a = true;

			//  +----+----+----+----+----+----+----+----+----+----+....+----+
			//  | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
			//  +----+----+----+----+----+----+----+----+----+----+....+----+
			//  | 1  | 1  |    2    |         4         | variable     | 1  |
			//  +----+----+----+----+----+----+----+----+----+----+....+----+
			//                                          [                   ]
			m_local_buffer.consume(m_local_buffer.size());
			bytes = co_await net::async_read_until(m_local_socket,
				m_local_buffer, '\0', net_awaitable[ec]);
			if (ec)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", read socks4 userid: " << ec.message();
				co_return;
			}

			std::string userid;
			if (bytes > 1)
			{
				userid.resize(bytes - 1);
				m_local_buffer.sgetn(&userid[0], bytes - 1);
			}
			m_local_buffer.consume(1); // consume `null`

			std::string hostname;
			if (socks4a)
			{
				bytes = co_await net::async_read_until(m_local_socket,
					m_local_buffer, '\0', net_awaitable[ec]);
				if (ec)
				{
					LOG_WARN << "socks id: " << m_connection_id
						<< ", read socks4a hostname: " << ec.message();
					co_return;
				}

				if (bytes > 1)
				{
					hostname.resize(bytes - 1);
					m_local_buffer.sgetn(&hostname[0], bytes - 1);
				}
			}

			LOG_DBG << "socks id: " << m_connection_id << ", use "
				<< (socks4a ? "domain: " : "ip: ")
				<< (socks4a ? hostname : dst_endpoint.address().to_string());

			// 用户认证逻辑.
			bool verify_passed = false;
			auto server = m_proxy_server.lock();

			if (server)
			{
				const auto& srv_opt = server->option();

				verify_passed = srv_opt.usrdid_ == userid;
				if (verify_passed)
					LOG_DBG << "socks id: " << m_connection_id
					<< ", auth passed";
				else
					LOG_WARN << "socks id: " << m_connection_id
					<< ", auth no pass";
				server = {};
			}

			if (!verify_passed)
			{
				//  +----+----+----+----+----+----+----+----+
				//  | VN | CD | DSTPORT |      DSTIP        |
				//  +----+----+----+----+----+----+----+----+
				//  | 1  | 1  |    2    |         4         |
				//  +----+----+----+----+----+----+----+----+
				//  [                                       ]

				net::streambuf wbuf;
				auto wp = net::buffer_cast<char*>(wbuf.prepare(16));

				write<uint8_t>(0, wp);
				write<uint8_t>(SOCKS4_REQUEST_REJECTED_USER_NO_ALLOW, wp);

				write<uint16_t>(dst_endpoint.port(), wp);
				write<uint32_t>(dst_endpoint.address().to_v4().to_ulong(), wp);

				wbuf.commit(8);
				bytes = co_await net::async_write(m_local_socket,
					wbuf,
					net::transfer_exactly(8),
					net_awaitable[ec]);
				if (ec)
				{
					LOG_WARN << "socks id: " << m_connection_id
						<< ", write socks4 no allow: " << ec.message();
					co_return;
				}

				LOG_WARN << "socks id: " << m_connection_id
					<< ", socks4 " << userid << " auth fail";
				co_return;
			}

			int error_code = SOCKS4_REQUEST_GRANTED;
			if (command == SOCKS_CMD_CONNECT)
			{
				if (socks4a)
					co_await connect_host(hostname, port, ec, true);
				else
					co_await connect_host(
						dst_endpoint.address().to_string(),
						port,
						ec);
				if (ec)
				{
					LOG_WFMT("socks id: {},"
						" connect to target {}:{} error: {}",
						m_connection_id,
						dst_endpoint.address().to_string(),
						port,
						ec.message());
					error_code = SOCKS4_CANNOT_CONNECT_TARGET_SERVER;
				}
			}
			else
			{
				error_code = SOCKS4_REQUEST_REJECTED_OR_FAILED;
				LOG_WFMT("socks id: {},"
					" unsupported command for socks4", m_connection_id);
			}

			//  +----+----+----+----+----+----+----+----+
			//  | VN | CD | DSTPORT |      DSTIP        |
			//  +----+----+----+----+----+----+----+----+
			//  | 1  | 1  |    2    |         4         |
			//  +----+----+----+----+----+----+----+----+
			//  [                                       ]

			net::streambuf wbuf;
			auto wp = net::buffer_cast<char*>(wbuf.prepare(16));

			write<uint8_t>(0, wp);
			write<uint8_t>((uint8_t)error_code, wp);

			// 返回IP:PORT.
			write<uint16_t>(dst_endpoint.port(), wp);
			write<uint32_t>(dst_endpoint.address().to_v4().to_ulong(), wp);

			wbuf.commit(8);
			bytes = co_await net::async_write(m_local_socket,
				wbuf,
				net::transfer_exactly(8),
				net_awaitable[ec]);
			if (ec)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", write socks4 response: " << ec.message();
				co_return;
			}

			if (error_code != SOCKS4_REQUEST_GRANTED)
				co_return;

			co_await(
				transfer(m_local_socket, m_remote_socket)
				&&
				transfer(m_remote_socket, m_local_socket)
				);

			LOG_DBG << "socks id: " << m_connection_id
				<< ", transfer completed";
			co_return;
		}

		net::awaitable<bool> http_proxy_connect()
		{
			http::request<http::string_body> req;
			boost::system::error_code ec;

			co_await http::async_read(
				m_local_socket, m_local_buffer, req, net_awaitable[ec]);

			auto mth = std::string(req.method_string());
			auto target_view = std::string(req.target());
			auto pa = std::string(req[http::field::proxy_authorization]);

			LOG_DBG << "http proxy id: "
				<< m_connection_id
				<< ", method: " << mth
				<< ", target: " << target_view
				<< (pa.empty() ? std::string()
					: ", proxy_authorization: " + pa);

			if (!m_option.usrdid_.empty())
			{
				if (pa.empty())
				{
					LOG_ERR << "http proxy id: "
						<< m_connection_id
						<< ", missing proxy auth";
					co_return false;
				}

				auto pos = pa.find(' ');
				if (pos == std::string::npos)
				{
					LOG_ERR << "http proxy id: "
						<< m_connection_id
						<< ", illegal proxy type: "
						<< pa;
					co_return false;
				}

				auto type = pa.substr(0, pos);
				auto auth = pa.substr(pos + 1);

				if (type != "Basic")
				{
					LOG_ERR << "http proxy id: "
						<< m_connection_id
						<< ", illegal proxy type: "
						<< pa;
					co_return false;
				}

				std::string userinfo(
					beast::detail::base64::decoded_size(auth.size()), 0);
				auto [len, _] = beast::detail::base64::decode(
					(char*)userinfo.data(),
					auth.c_str(),
					auth.size());
				userinfo.resize(len);

				pos = userinfo.find(':');
				std::string uname = userinfo.substr(0, pos);
				std::string passwd = userinfo.substr(pos + 1);

				bool verify_passed =
					m_option.usrdid_ == uname &&
					m_option.passwd_ == passwd;

				auto endp = m_local_socket.remote_endpoint();
				auto client = endp.address().to_string();
				client += ":" + std::to_string(endp.port());

				LOG_DBG << "socks id: " << m_connection_id
					<< ", auth: " << m_option.usrdid_
					<< " := " << uname
					<< ", passwd: " << m_option.passwd_
					<< " := " << passwd
					<< ", client: " << client;

				if (!verify_passed)
					co_return false;
			}

			auto pos = target_view.find(':');
			if (pos == std::string::npos)
			{
				LOG_ERR  << "http proxy id: "
					<< m_connection_id
					<< ", illegal target: "
					<< target_view;
				co_return false;
			}

			std::string host(target_view.substr(0, pos));
			std::string port(target_view.substr(pos + 1));

			co_await connect_host(host,
				static_cast<uint16_t>(std::atol(port.c_str())), ec, true);
			if (ec)
			{
				LOG_WFMT("http proxy id: {},"
					" connect to target {}:{} error: {}",
					m_connection_id,
					host,
					port,
					ec.message());
				co_return false;
			}

			http::response<http::empty_body> res{
				http::status::ok, req.version() };
			res.reason("Connection established");

			co_await http::async_write(
				m_local_socket,
				res,
				net_awaitable[ec]);
			if (ec)
			{
				LOG_WFMT("http proxy id: {},"
					" async write response {}:{} error: {}",
					m_connection_id,
					host,
					port,
					ec.message());
				co_return false;
			}

			co_await(
				transfer(m_local_socket, m_remote_socket)
				&&
				transfer(m_remote_socket, m_local_socket)
				);

			LOG_DBG << "http proxy id: " << m_connection_id
				<< ", transfer completed";

			co_return true;
		}

		net::awaitable<bool> socks_auth()
		{
			//  +----+------+----------+------+----------+
			//  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
			//  +----+------+----------+------+----------+
			//  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
			//  +----+------+----------+------+----------+
			//  [           ]

			boost::system::error_code ec;
			m_local_buffer.consume(m_local_buffer.size());
			auto bytes = co_await net::async_read(m_local_socket,
				m_local_buffer,
				net::transfer_exactly(2),
				net_awaitable[ec]);
			if (ec)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", read client username/passwd error: " << ec.message();
				co_return false;
			}

			auto p = net::buffer_cast<const char*>(m_local_buffer.data());
			int auth_version = read<int8_t>(p);
			if (auth_version != 1)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", socks negotiation, unsupported socks5 protocol";
				co_return false;
			}
			int name_length = read<uint8_t>(p);
			if (name_length <= 0 || name_length > 255)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", socks negotiation, invalid name length";
				co_return false;
			}
			name_length += 1;

			//  +----+------+----------+------+----------+
			//  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
			//  +----+------+----------+------+----------+
			//  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
			//  +----+------+----------+------+----------+
			//              [                 ]
			m_local_buffer.consume(m_local_buffer.size());
			bytes = co_await net::async_read(m_local_socket,
				m_local_buffer,
				net::transfer_exactly(name_length),
				net_awaitable[ec]);
			if (ec)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", read client username error: " << ec.message();
				co_return false;
			}

			std::string uname;

			p = net::buffer_cast<const char*>(m_local_buffer.data());
			for (size_t i = 0; i < bytes - 1; i++)
				uname.push_back(read<int8_t>(p));

			int passwd_len = read<uint8_t>(p);
			if (passwd_len <= 0 || passwd_len > 255)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", socks negotiation, invalid passwd length";
				co_return false;
			}

			//  +----+------+----------+------+----------+
			//  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
			//  +----+------+----------+------+----------+
			//  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
			//  +----+------+----------+------+----------+
			//                                [          ]
			m_local_buffer.consume(m_local_buffer.size());
			bytes = co_await net::async_read(m_local_socket,
				m_local_buffer,
				net::transfer_exactly(passwd_len),
				net_awaitable[ec]);
			if (ec)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", read client passwd error: " << ec.message();
				co_return false;
			}

			std::string passwd;

			p = net::buffer_cast<const char*>(m_local_buffer.data());
			for (size_t i = 0; i < bytes; i++)
				passwd.push_back(read<int8_t>(p));

			// SOCKS5验证用户和密码.
			auto endp = m_local_socket.remote_endpoint();
			auto client = endp.address().to_string();
			client += ":" + std::to_string(endp.port());

			// 用户认证逻辑.
			bool verify_passed = false;
			auto server = m_proxy_server.lock();

			if (server)
			{
				const auto& srv_opt = server->option();

				verify_passed =
					srv_opt.usrdid_ == uname && srv_opt.passwd_ == passwd;
				server.reset();

				LOG_DBG << "socks id: " << m_connection_id
					<< ", auth: " << srv_opt.usrdid_
					<< " := " << uname
					<< ", passwd: " << srv_opt.passwd_
					<< " := " << passwd
					<< ", client: " << client;
			}

			net::streambuf wbuf;
			auto wp = net::buffer_cast<char*>(wbuf.prepare(16));
			write<uint8_t>(0x01, wp);			// version 只能是1.
			if (verify_passed)
			{
				write<uint8_t>(0x00, wp);		// 认证通过返回0x00, 其它值为失败.
			}
			else
			{
				write<uint8_t>(0x01, wp);		// 认证返回0x01为失败.
			}

			// 返回认证状态.
			//  +----+--------+
			//  |VER | STATUS |
			//  +----+--------+
			//  | 1  |   1    |
			//  +----+--------+
			wbuf.commit(2);
			co_await net::async_write(m_local_socket,
				wbuf,
				net::transfer_exactly(2),
				net_awaitable[ec]);
			if (ec)
			{
				LOG_WARN << "socks id: " << m_connection_id
					<< ", server write status error: " << ec.message();
				co_return false;
			}

			co_return verify_passed;
		}

		template<typename S1, typename S2>
		net::awaitable<void> transfer(S1& from, S2& to)
		{
			std::vector<char> data(512 * 1024, 0);
			boost::system::error_code ec;

			for (; !m_abort;)
			{
				auto bytes = co_await from.async_read_some(
					net::buffer(data), net_awaitable[ec]);
				if (ec || m_abort)
				{
					to.shutdown(net::ip::tcp::socket::shutdown_send, ec);
					co_return;
				}

				co_await net::async_write(to,
					net::buffer(data, bytes), net_awaitable[ec]);
				if (ec || m_abort)
				{
					from.shutdown(net::ip::tcp::socket::shutdown_receive, ec);
					co_return;
				}
			}
		}

		net::awaitable<void> connect_host(
			std::string target_host, uint16_t target_port,
			boost::system::error_code& ec, bool resolve = false)
		{
			auto executor = co_await net::this_coro::executor;

			// 获取构造函数中临时创建的tcp::socket.
			tcp::socket& remote_socket =
				boost::variant2::get<tcp::socket>(m_remote_socket);

			auto bind_interface = net::ip::address::from_string(
				m_option.bind_addr_, ec);
			if (ec)
			{
				// bind 地址有问题, 忽略bind参数.
				m_option.bind_addr_.clear();
			}

			auto check_condition = [this, bind_interface](
				const boost::system::error_code&,
				tcp::socket& stream, auto&) mutable
			{
				if (m_option.bind_addr_.empty())
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

			if (m_next_proxy)
			{
				tcp::resolver resolver{ executor };

				auto proxy_host = std::string(m_next_proxy->host());
				auto proxy_port = std::string(m_next_proxy->port());

				auto targets = co_await resolver.async_resolve(
					proxy_host, proxy_port, net_awaitable[ec]);
				if (ec)
				{
					LOG_WFMT("socks id: {},"
						" resolver to next proxy {}:{} error: {}",
						m_connection_id,
						std::string(m_next_proxy->host()),
						std::string(m_next_proxy->port()),
						ec.message());

					co_return;
				}

				co_await asio_util::async_connect(remote_socket,
					targets, check_condition, net_awaitable[ec]);
				if (ec)
				{
					LOG_WFMT("socks id: {},"
						" connect to next proxy {}:{} error: {}",
						m_connection_id,
						std::string(m_next_proxy->host()),
						std::string(m_next_proxy->port()),
						ec.message());

					co_return;
				}

				// 使用ssl加密与下一级代理通信.
				if (m_option.next_proxy_use_ssl_)
				{
					// 设置 ssl cert 证书目录.
					if (std::filesystem::exists(m_option.ssl_cert_path_))
					{
						m_ssl_context.add_verify_path(
							m_option.ssl_cert_path_, ec);
						if (ec)
						{
							LOG_WFMT("socks id: {}, "
								"load cert path: {}, "
								"error: {}",
								m_connection_id,
								m_option.ssl_cert_path_,
								ec.message());
						}
					}
				}

				auto instantiate_stream =
					[this, &proxy_host, &remote_socket, &ec]
				() mutable -> net::awaitable<proxy_stream_type>
				{
					ec = {};

					if (m_option.next_proxy_use_ssl_)
					{
						m_ssl_context.set_verify_mode(net::ssl::verify_peer);
						auto cert = default_root_certificates();
						m_ssl_context.add_certificate_authority(
							net::buffer(cert.data(), cert.size()),
							ec);
						if (ec)
						{
							LOG_WFMT("socks id: {},"
								" add_certificate_authority error: {}",
								m_connection_id, ec.message());
						}

						m_ssl_context.set_verify_callback(
							net::ssl::rfc2818_verification(proxy_host), ec);
						if (ec)
						{
							LOG_WFMT("socks id: {},"
								" set_verify_callback error: {}",
								m_connection_id, ec.message());
						}

						auto socks_stream = instantiate_proxy_stream(
							std::move(remote_socket), m_ssl_context);

						// get origin ssl stream type.
						ssl_stream& ssl_socket =
							boost::variant2::get<ssl_stream>(socks_stream);

						if (!SSL_set_tlsext_host_name(
							ssl_socket.native_handle(), proxy_host.c_str()))
						{
							LOG_WFMT("socks id: {},"
							" SSL_set_tlsext_host_name error: {}",
								m_connection_id, ::ERR_get_error());
						}

						// do async handshake.
						co_await ssl_socket.async_handshake(
							net::ssl::stream_base::client,
							net_awaitable[ec]);
						if (ec)
						{
							LOG_WFMT("socks id: {},"
								" ssl protocol handshake error: {}",
								m_connection_id, ec.message());
						}

						co_return socks_stream;
					}

					co_return instantiate_proxy_stream(
						std::move(remote_socket));
				};

				m_remote_socket = std::move(co_await instantiate_stream());

				socks_client_option opt;

				opt.target_host = target_host;
				opt.target_port = target_port;
				opt.proxy_hostname = true;
				opt.username = std::string(m_next_proxy->user());
				opt.password = std::string(m_next_proxy->password());

				if (m_next_proxy->scheme() == "socks4")
					opt.version = socks4_version;
				else if (m_next_proxy->scheme() == "socks4a")
					opt.version = socks4a_version;

				co_await async_socks_handshake(
					m_remote_socket, opt, net_awaitable[ec]);
				if (ec)
				{
					LOG_WFMT("socks id: {},"
						" connect to next host {}:{} error: {}",
						m_connection_id,
						target_host,
						target_port,
						ec.message());
				}
			}
			else
			{
				net::ip::basic_resolver_results<tcp> targets;
				if (resolve)
				{
					tcp::resolver resolver{ executor };

					targets = co_await resolver.async_resolve(
						target_host,
						std::to_string(target_port),
						net_awaitable[ec]);
					if (ec)
					{
						LOG_WARN << "socks id: " << m_connection_id
							<< ", resolve: " << target_host
							<< ", error: " << ec.message();

						co_return;
					}
				}
				else
				{
					tcp::endpoint dst_endpoint;

					dst_endpoint.address(
						net::ip::address::from_string(target_host));
					dst_endpoint.port(target_port);

					targets = net::ip::basic_resolver_results<tcp>::create(
						dst_endpoint, "", "");
				}

				co_await asio_util::async_connect(remote_socket,
					targets, check_condition, net_awaitable[ec]);
				if (ec)
				{
					LOG_WFMT("socks id: {}, connect to target {}:{} error: {}",
						m_connection_id,
						target_host,
						target_port,
						ec.message());
				}

				m_remote_socket = instantiate_proxy_stream(
					std::move(remote_socket));
			}

			co_return;
		}

		const std::string& fake_web_page() const
		{
			static std::string fake_content =
R"xxxxxx(HTTP/1.1 404 Not Found
Server: nginx/1.20.2
Content-Type: text/html
Connection: close

<html><head><title>404 Not Found</title></head>
<body>
<center><h1>404 Not Found</h1></center>
<hr>
<center>nginx/1.20.2</center>
</body>
</html>)xxxxxx";
			return fake_content;
		}

		const std::string& bad_request_page() const
		{
			static std::string fake_content =
R"xxxxxx(HTTP/1.1 400 Bad Request
Server: nginx
Date: Tue, 27 Dec 2022 01:36:17 GMT
Content-Type: text/html
Content-Length: 150
Connection: close

)xxxxxx";
			return fake_content;
		}

	private:
		proxy_stream_type m_local_socket;
		proxy_stream_type m_remote_socket;
		size_t m_connection_id;
		net::streambuf m_local_buffer{};
		std::weak_ptr<proxy_server_base> m_proxy_server;
		proxy_server_option m_option;
		std::unique_ptr<urls::url_view> m_next_proxy;
		net::ssl::context m_ssl_context{ net::ssl::context::sslv23_client };
		bool m_abort{ false };
	};

	//////////////////////////////////////////////////////////////////////////

	class proxy_server
		: public proxy_server_base
		, public std::enable_shared_from_this<proxy_server>
	{
		proxy_server(const proxy_server&) = delete;
		proxy_server& operator=(const proxy_server&) = delete;

	public:
		proxy_server(net::any_io_executor executor,
			const tcp::endpoint& endp, proxy_server_option opt = {})
			: m_executor(executor)
			, m_acceptor(executor, endp)
			, m_option(std::move(opt))
		{
			init_ssl_context();

			boost::system::error_code ec;
			m_acceptor.listen(net::socket_base::max_listen_connections, ec);
		}

		virtual ~proxy_server() = default;

		void init_ssl_context()
		{
			m_ssl_context.set_options(
				net::ssl::context::default_workarounds
				| net::ssl::context::no_sslv2
				| net::ssl::context::single_dh_use);

			auto dir = std::filesystem::path(m_option.ssl_cert_path_);
			auto pwd = dir / "ssl_crt.pwd";

			if (std::filesystem::exists(pwd))
				m_ssl_context.set_password_callback(
					[this, &pwd]([[maybe_unused]] auto... args) {
						std::string password;
						fileop::read(pwd, password);
						return password;
					}
			);

			auto cert = dir / "ssl_crt.pem";
			auto key = dir / "ssl_key.pem";
			auto dh = dir / "ssl_dh.pem";

			if (std::filesystem::exists(cert))
				m_ssl_context.use_certificate_chain_file(cert.string());

			if (std::filesystem::exists(key))
				m_ssl_context.use_private_key_file(
					key.string(), boost::asio::ssl::context::pem);

			if (std::filesystem::exists(dh))
				m_ssl_context.use_tmp_dh_file(dh.string());
		}

	public:
		inline void start()
		{
			// 同时启动32个连接协程, 开始为proxy client提供服务.
			for (int i = 0; i < 32; i++)
			{
				net::co_spawn(m_executor,
					start_proxy_listen(m_acceptor), net::detached);
			}
		}

		inline void close()
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

	private:
		virtual void remove_client(size_t id) override
		{
			m_clients.erase(id);
		}

		virtual const proxy_server_option& option() override
		{
			return m_option;
		}

	private:
		inline net::awaitable<void> start_proxy_listen(tcp::acceptor& a)
		{
			auto self = shared_from_this();
			boost::system::error_code error;

			while (!m_abort)
			{
				tcp::socket socket(m_executor);
				co_await a.async_accept(
					socket, net_awaitable[error]);
				if (error)
				{
					LOG_ERR << "start_proxy_listen"
						", async_accept: " << error.message();

					if (error == net::error::operation_aborted ||
						error == net::error::bad_descriptor)
					{
						co_return;
					}

					if (!a.is_open())
						co_return;

					continue;
				}

				{
					net::socket_base::keep_alive option(true);
					socket.set_option(option, error);
				}

				{
					net::ip::tcp::no_delay option(true);
					socket.set_option(option);
				}

				static std::atomic_size_t id{ 1 };
				size_t connection_id = id++;

				auto endp = socket.remote_endpoint();
				auto client = endp.address().to_string();
				client += ":" + std::to_string(endp.port());

				LOG_DBG << "start client incoming id: "
					<< connection_id
					<< ", client: "
					<< client;

				// 等待读取事件.
				co_await socket.async_wait(
					tcp::socket::wait_read, net_awaitable[error]);
				if (error)
				{
					LOG_WARN << "socket.async_wait error: " << error.message();
					continue;
				}

				// 检查协议.
				auto fd = socket.native_handle();
				uint8_t detect[5] = { 0 };

#if defined(WIN32) || defined(__APPLE__)
				auto ret = recv(fd, (char*)detect, sizeof(detect),
					MSG_PEEK);
#else
				auto ret = recv(fd, (void*)detect, sizeof(detect),
					MSG_PEEK | MSG_NOSIGNAL | MSG_DONTWAIT);
#endif
				if (ret <= 0)
				{
					LOG_WARN << "start_tcp_listen,"
						" peek message return: " << ret;
					continue;
				}

				// socks4/5 protocol.
				if (detect[0] == 0x05 || detect[0] == 0x04)
				{
					LOG_DBG << "socks protocol:"
						" connection id: " << connection_id;

					auto new_session =
						std::make_shared<proxy_session>(
							instantiate_proxy_stream(std::move(socket)),
								connection_id, self);

					m_clients[connection_id] = new_session;

					new_session->start();
				}
				else if (detect[0] == 0x16) // socks5 with ssl protocol.
				{
					LOG_DBG << "ssl protocol"
						", connection id: " << connection_id;

					// instantiate socks stream with ssl context.
					auto ssl_socks_stream = instantiate_proxy_stream(
						std::move(socket), m_ssl_context);

					// get origin ssl stream type.
					ssl_stream& ssl_socket =
						boost::variant2::get<ssl_stream>(ssl_socks_stream);

					// do async handshake.
					co_await ssl_socket.async_handshake(
						net::ssl::stream_base::server,
						net_awaitable[error]);
					if (error)
					{
						LOG_WARN << "ssl protocol handshake error: "
							<< error.message();
						continue;
					}

					// make socks session shared ptr.
					auto new_session =
						std::make_shared<proxy_session>(
							std::move(ssl_socks_stream), connection_id, self);
					m_clients[connection_id] = new_session;

					new_session->start();
				}
				else if (detect[0] == 0x47
					|| detect[0] == 0x50
					|| detect[0] == 0x43)
				{
					LOG_DBG << "http protocol"
						", connection id: " << connection_id;

					auto new_session =
						std::make_shared<proxy_session>(
							instantiate_proxy_stream(std::move(socket)),
								connection_id, self);
					m_clients[connection_id] = new_session;

					new_session->start();
				}
			}

			LOG_WARN << "start_proxy_listen exit ...";
			co_return;
		}

	private:
		net::any_io_executor m_executor;
		tcp::acceptor m_acceptor;
		proxy_server_option m_option;
		using proxy_session_weak_ptr =
			std::weak_ptr<proxy_session>;
		std::unordered_map<size_t, proxy_session_weak_ptr> m_clients;
		net::ssl::context m_ssl_context{ net::ssl::context::sslv23 };
		bool m_abort{ false };
	};

}
