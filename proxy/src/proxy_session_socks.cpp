//
// proxy_session_socks.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_session.hpp"
#include "proxy/http_error_body.hpp"

namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// SOCKS 协议处理与 UDP 转发

	net::awaitable<void> proxy_session::socks_connect_v5() noexcept
	{
		auto p = (const char*)m_local_buffer.data().data();

		auto socks_version = read<int8_t>(p);
		BOOST_ASSERT(socks_version == SOCKS_VERSION_5);
		int nmethods = read<int8_t>(p);
		if (nmethods <= 0 || nmethods > 255)
		{
			log_conn_error()
				<< ", unsupported method : "
				<< nmethods;
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
			log_conn_error()
				<< ", read socks methods: "
				<< ec.message();
			co_return;
		}

		// 服务端是否需要认证.
		auto is_auth_required = auth_required();

		// 循环读取客户端支持的代理方式.
		p = (const char*)m_local_buffer.data().data();

		int method = SOCKS5_AUTH_UNACCEPTABLE;
		while (bytes != 0)
		{
			int m = read<int8_t>(p);

			if (!is_auth_required)
			{
				if (m == SOCKS5_AUTH_NONE)
				{
					method = m;
					break;
				}

				method = m;
			}
			else if (m == SOCKS5_AUTH)
			{
				method = m;
				break;
			}

			bytes--;
		}

		net::streambuf wbuf{};

		// 回复客户端, server所选择的代理方式.
		auto wp = (char*)wbuf.prepare(1024).data();
		write<uint8_t>(socks_version, wp);
		write<uint8_t>((uint8_t)method, wp);

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
			log_conn_warning()
				<< ", write server method error : "
				<< ec.message();
			co_return;
		}

		if (method == SOCKS5_AUTH_UNACCEPTABLE)
		{
			log_conn_warning()
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
			log_conn_warning()
				<< ", read client request error: "
				<< ec.message();
			co_return;
		}

		p = (const char*)m_local_buffer.data().data();
		auto ver = read<int8_t>(p);
		if (ver != SOCKS_VERSION_5)
		{
			log_conn_warning()
				<< ", socks requests, invalid protocol: "
				<< ver;
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
		else
		{
			log_conn_warning()
				<< ", socks5 request, unsupported address type: "
				<< atyp;
			co_return;
		}

		bytes = co_await net::async_read(m_local_socket,
			m_local_buffer,
			net::transfer_exactly(length),
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", read client request dst.addr error: "
				<< ec.message();
			co_return;
		}

		tcp::endpoint dst_endpoint;
		std::string domain;
		uint16_t port = 0;
		bool resolved = true;

		auto executor = co_await net::this_coro::executor;

		p = (const char*)m_local_buffer.data().data();
		if (atyp == SOCKS5_ATYP_IPV4)
		{
			dst_endpoint.address(net::ip::address_v4(read<uint32_t>(p)));
			dst_endpoint.port(read<uint16_t>(p));

			domain = dst_endpoint.address().to_string();
			port = dst_endpoint.port();

			log_conn_debug()
				<< ", "
				<< remote_endpoint_string(m_local_socket)
				<< " to ipv4: "
				<< dst_endpoint
				<< ", command: "
				<< command;
		}
		else if (atyp == SOCKS5_ATYP_DOMAINNAME)
		{
			for (size_t i = 0; i < bytes - 2; i++)
				domain.push_back(read<int8_t>(p));
			port = read<uint16_t>(p);

			dst_endpoint.port(port);
			dst_endpoint.address(
				net::ip::make_address(domain, ec));
			if (ec ||
				dst_endpoint.address() == net::ip::make_address_v4("0.0.0.0") ||
				dst_endpoint.address() == net::ip::make_address_v6("::0"))
			{
				if (ec)
					resolved = false;

				auto address = tcp_remote_endpoint(m_local_socket).address();
				dst_endpoint.address(address);
			}

			log_conn_debug()
				<< ", "
				<< remote_endpoint_string(m_local_socket)
				<< " to domain: "
				<< domain
				<< ":"
				<< port
				<< ", command: "
				<< command;
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

			domain = dst_endpoint.address().to_string();
			port = dst_endpoint.port();

			log_conn_debug()
				<< ", "
				<< remote_endpoint_string(m_local_socket)
				<< " to ipv6: "
				<< dst_endpoint
				<< ", command: "
				<< command;
		}

		if (command == SOCKS_CMD_CONNECT)
		{
			// 连接目标主机.
			ec = co_await start_connect_host(domain, port, resolved);
			if (ec)
			{
				log_conn_warning()
					<< ", connect to target "
					<< domain
					<< ":"
					<< port
					<< " error: "
					<< ec.message();
			}
		}
		else if (command == SOCKS5_CMD_UDP)
		do {
			if (m_option.disable_udp_)
			{
				log_conn_debug()
					<< ", udp protocol disabled";
				ec = net::error::connection_refused;
				break;
			}

			auto remote_endp = tcp_remote_endpoint(m_local_socket);
			auto protocol = remote_endp.address().is_v4() ? udp::v4() : udp::v6();

			// 创建UDP端口.
			udp::socket local_udp_socket(m_executor);
			local_udp_socket.open(protocol, ec);
			if (ec)
			{
				log_conn_warning()
					<< ", open udp socket error: "
					<< ec.message();
				break;
			}

			// 如果有指定绑定本地地址, 则创建绑定到指定地址的 udp socket 用于数据发送.
			std::optional<udp::socket> remote_bind_socket;

			// 绑定输出网络.
			auto bind_if = udp::endpoint(protocol, 0);
			if (m_bind_interface)
			{
				bind_if.address(*m_bind_interface);

				remote_bind_socket.emplace(m_executor);
				remote_bind_socket->open(protocol, ec);
				if (ec)
				{
					log_conn_warning()
						<< ", open remote bind udp socket error: "
						<< ec.message();
					break;
				}

				// 绑定到指定的地址.
				remote_bind_socket->bind(bind_if, ec);
				if (ec)
				{
					log_conn_warning()
						<< ", bind remote udp socket to "
						<< bind_if
						<< " error: "
						<< ec.message();
					break;
				}

				if (m_option.so_mark_)
				{
					auto ret = apply_so_mark(remote_bind_socket->native_handle(), m_option.so_mark_);
					if (ret.has_error())
					{
						log_conn_warning()
							<< ", set socket mark error: "
							<< ret.error().message();
					}
				}
			}

			// 绑定到和 tcp socket 相同的地址.
			udp::endpoint local_udp_endp(tcp_local_endpoint(m_local_socket).address(), 0);
			local_udp_socket.bind(local_udp_endp, ec);
			if (ec)
			{
				log_conn_warning()
					<< ", bind local udp socket to "
					<< local_udp_endp
					<< " error: "
					<< ec.message();
				break;
			}

			// 对 local_udp_socket 也设置 SO_MARK，确保非 bridge 模式下
			// 由 local_udp_socket 发出的 UDP 数据包也带有正确的标记。
			if (m_option.so_mark_)
			{
				auto ret = apply_so_mark(local_udp_socket.native_handle(), m_option.so_mark_);
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", set socket mark error: "
						<< ret.error().message();
				}
			}

			if (m_proxy_pass)
			{
				// 如果是代理桥接模式, 则连接桥接服务器, 然后所有接收到 client 端的数据
				// 统统转发给桥接服务器, 桥接服务器上接收到的数据再转发给客户端.
				// 中间不需要解包和封包 udp 数据报.

				uint16_t port = 0;
				if (remote_bind_socket)
					port = remote_bind_socket->local_endpoint(ec).port();
				else
					port = local_udp_socket.local_endpoint(ec).port();

				ec = co_await start_connect_host("0.0.0.0", port, true, SOCKS5_CMD_UDP);
				if (ec)
				{
					log_conn_warning()
						<< ", start connect SOCKS5_CMD_UDP error: "
						<< ec.message();
					break;
				}

				log_conn_debug()
					<< ", start connect SOCKS5_CMD_UDP success";
			}

			// 所有发向 udp socket 的数据, 都将转发到 m_local_udp_endpoint
			// 除非地址是 m_local_udp_endpoint 本身除外.
			m_local_udp_endpoint.address(remote_endp.address());
			m_local_udp_endpoint.port(dst_endpoint.port());

			// 开始回复客户端 udp 关联成功消息.
			wbuf.consume(wbuf.size());
			auto wp = (char*)wbuf.prepare(64 + domain.size()).data();

			write<uint8_t>(SOCKS_VERSION_5, wp);	// VER
			write<uint8_t>(0, wp);					// REP
			write<uint8_t>(0x00, wp);				// RSV

			auto local_endp = local_udp_socket.local_endpoint(ec);
			if (ec)
			{
				log_conn_warning()
					<< ", get local udp socket endpoint error: "
					<< ec.message();
				break;
			}

			log_conn_debug()
				<< ", udp client address: "
				<< m_local_udp_endpoint
				<< ", udp local socket: "
				<< local_endp
				<< ", tcp local socket: "
				<< local_endpoint_string(m_local_socket)
				<< ", udp bind interface: "
				<< bind_if.address().to_string();

			if (local_endp.address().is_v4())
			{
				auto uaddr = local_endp.address().to_v4().to_uint();
				if (uaddr == 0)
					uaddr = tcp_local_endpoint(m_local_socket).address().to_v4().to_uint();

				write<uint8_t>(SOCKS5_ATYP_IPV4, wp);
				write<uint32_t>(uaddr, wp);
				write<uint16_t>(local_endp.port(), wp);
			}
			else if (local_endp.address().is_v6())
			{
				write<uint8_t>(SOCKS5_ATYP_IPV6, wp);
				auto data = local_endp.address().to_v6().to_bytes();
				for (auto c : data)
					write<uint8_t>(c, wp);
				write<uint16_t>(local_endp.port(), wp);
			}

			auto len = wp - (const char*)wbuf.data().data();
			wbuf.commit(len);
			bytes = co_await net::async_write(m_local_socket,
				wbuf,
				net::transfer_exactly(len),
				net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", write server response error: "
					<< ec.message();
				co_return;
			}

			// 在此等待 tcp 连接断开.
			if (!remote_bind_socket)
			{
				co_await(m_local_socket.async_read_some(m_local_buffer.prepare(1), net_awaitable[ec])
					||
					forward_udp(local_udp_socket, local_udp_socket));
			}
			else
			{
				co_await(m_local_socket.async_read_some(m_local_buffer.prepare(1), net_awaitable[ec])
					||
					forward_udp(local_udp_socket, *remote_bind_socket)
					||
					forward_udp(*remote_bind_socket, local_udp_socket)
					);
			}

			// 关闭 udp socket.
			local_udp_socket.close(ec);
			if (remote_bind_socket)
				remote_bind_socket->close(ec);

			co_return;
		} while (0);
		else
		{
			log_conn_warning()
				<< ", unsupported socks5 command: "
				<< command;
			ec = make_error_code(boost::system::errc::not_supported);
		}

		// 连接成功或失败.
		{
			int8_t error_code = SOCKS5_SUCCEEDED;

			if (ec == net::error::connection_refused)
				error_code = SOCKS5_CONNECTION_REFUSED;
			else if (ec == net::error::network_unreachable)
				error_code = SOCKS5_NETWORK_UNREACHABLE;
			else if (ec == net::error::host_unreachable)
				error_code = SOCKS5_HOST_UNREACHABLE;
			else if (ec == boost::system::errc::not_supported)
				error_code = SOCKS5_COMMAND_NOT_SUPPORTED;
			else if (ec)
				error_code = SOCKS5_GENERAL_SOCKS_SERVER_FAILURE;

			if (ec)
			{
				log_conn_warning()
					<< ", connect to target error: "
					<< ec.message();
			}

			//  +----+-----+-------+------+----------+----------+
			//  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
			//  +----+-----+-------+------+----------+----------+
			//  | 1  |  1  | X'00' |  1   | Variable |    2     |
			//  +----+-----+-------+------+----------+----------+
			//  [                                               ]

			wbuf.consume(wbuf.size());
			auto wp = (char*)wbuf.prepare(64 + domain.size()).data();

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

			auto len = wp - (const char*)wbuf.data().data();
			wbuf.commit(len);
			bytes = co_await net::async_write(m_local_socket,
				wbuf,
				net::transfer_exactly(len),
				net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", write server response error: "
					<< ec.message();
				co_return;
			}

			if (error_code != SOCKS5_SUCCEEDED)
				co_return;
		}

		log_conn_debug()
			<< ", connected start transfer";

		// 发起数据传输协程.
		if (command == SOCKS_CMD_CONNECT)
		{
			co_await concurrent_transfer();
		}
		else
		{
			log_conn_warning()
				<< ", SOCKS_CMD_BIND and SOCKS5_CMD_UDP is unsupported";
		}

		co_return;
	}

	//////////////////////////////////////////////////////////////////////////
	// UDP 转发

	net::awaitable<void> proxy_session::forward_udp(udp::socket& client, udp::socket& server) noexcept
	{
		[[maybe_unused]] auto self = shared_from_this();

		boost::system::error_code ec;

		udp::endpoint remote_endp;
		udp::endpoint final_remote_endp;
		std::string latest_domain;

		// SOCKS5 UDP 最大头部长度: RSV(2) + FRAG(1) + ATYP(1) + 地址(最大255) + PORT(2)
		static constexpr size_t udp_max_header_size = 261;

		char read_buffer[4096];
		size_t send_total = 0;
		size_t recv_total = 0;

		const char* rbuf = &read_buffer[udp_max_header_size];

		while (!m_abort)
		{
			auto bytes = co_await client.async_receive_from(
				net::buffer((char*)rbuf, sizeof(read_buffer) - udp_max_header_size),
				remote_endp,
				net_awaitable[ec]);
			if (ec)
				break;

			// 桥接代理模式下, 直接转发数据包到桥接服务器.
			if (m_proxy_pass)
			{
				if (remote_endp.address() == m_local_udp_endpoint.address())
				{
					m_local_udp_endpoint = remote_endp;

					send_total++;
					co_await server.async_send_to(
						net::buffer(rbuf, bytes),
						m_remote_udp_endpoint,
						net_awaitable[ec]);
				}
				else if (remote_endp.address() == m_remote_udp_endpoint.address())
				{
					recv_total++;
					co_await server.async_send_to(
						net::buffer(rbuf, bytes),
						m_local_udp_endpoint,
						net_awaitable[ec]);
				}

				continue;
			}

			auto rp = rbuf;
			const auto packet_end = rbuf + bytes;

			// 如果数据包来自 socks 客户端, 则解析数据包并将数据转发给目标主机.
			if (remote_endp.address() == m_local_udp_endpoint.address())
			{
				m_local_udp_endpoint = remote_endp;

				//  +----+------+------+----------+-----------+----------+
				//  |RSV | FRAG | ATYP | DST.ADDR | DST.PORT  |   DATA   |
				//  +----+------+------+----------+-----------+----------+
				//  | 2  |  1   |  1   | Variable |    2      | Variable |
				//  +----+------+------+----------+-----------+----------+

				// 去掉包头转发至远程主机.
				if (static_cast<size_t>(packet_end - rp) < 4)
				{
					log_conn_warning()
						<< ", truncated SOCKS5 UDP header, packet_size: "
						<< bytes;
					continue;
				}

				read<uint16_t>(rp); // rsv
				auto frag = read<uint8_t>(rp);  // frag

				// 不支持udp分片.
				if (frag != 0)
					continue;

				auto atyp = read<uint8_t>(rp);

				if (atyp == SOCKS5_ATYP_IPV4)
				{
					if (static_cast<size_t>(packet_end - rp) < 6)
					{
						log_conn_warning()
							<< ", truncated SOCKS5 UDP IPv4 header, packet_size: "
							<< bytes;
						continue;
					}

					final_remote_endp.address(
						net::ip::address_v4(read<uint32_t>(rp)));
					final_remote_endp.port(read<uint16_t>(rp));
				}
				else if (atyp == SOCKS5_ATYP_DOMAINNAME)
				{
					if (static_cast<size_t>(packet_end - rp) < 1)
					{
						log_conn_warning()
							<< ", truncated SOCKS5 UDP domain header, packet_size: "
							<< bytes;
						continue;
					}

					auto length = read<uint8_t>(rp);
					std::string domain;

					if (static_cast<size_t>(packet_end - rp) < length + 2u)
					{
						log_conn_warning()
							<< ", truncated SOCKS5 UDP domain address, packet_size: "
							<< bytes;
						continue;
					}

					for (size_t i = 0; i < length; i++)
						domain.push_back(read<int8_t>(rp));
					auto port = read<uint16_t>(rp);

					if (latest_domain != domain)
					{
						auto executor = co_await switch_to_backend_executor();

						udp::resolver resolver{ executor };

						auto targets =
							co_await resolver.async_resolve(
								domain,
								std::to_string(port),
								net_awaitable[ec]);

						co_await switch_from_backend_executor();

						if (ec)
							break;

						if (targets.empty())
							continue;

						final_remote_endp = targets.begin()->endpoint();
						latest_domain = domain;
					}
				}
				else if (atyp == SOCKS5_ATYP_IPV6)
				{
					if (static_cast<size_t>(packet_end - rp) < 18)
					{
						log_conn_warning()
							<< ", truncated SOCKS5 UDP IPv6 header, packet_size: "
							<< bytes;
						continue;
					}

					net::ip::address_v6::bytes_type addr;
					for (auto i = addr.begin();
						i != addr.end(); ++i)
					{
						*i = read<int8_t>(rp);
					}

					final_remote_endp.address(net::ip::address_v6(addr));
					final_remote_endp.port(read<uint16_t>(rp));
				}
				else
				{
					continue;
				}

				auto head_size = rp - rbuf;

				// 检查解析出的 SOCKS5 UDP 头部是否越界, 防止恶意或畸形
				// 数据包导致无符号整数下溢 (head_size > bytes).
				if (head_size > bytes)
				{
					log_conn_warning()
						<< ", malformed SOCKS5 UDP header, head_size: "
						<< head_size << ", packet_size: " << bytes;
					continue;
				}

				auto udp_size = bytes - head_size;

				send_total++;

				co_await server.async_send_to(
					net::buffer(rp, udp_size),
					final_remote_endp,
					net_awaitable[ec]);
			}
			else // 如果数据包来自远程主机, 则解析数据包并将数据转发给 socks 客户端.
			{
				// 6 + 4 表示 socks5 udp 头部长度, 6 是 (RSV + FRAG + ATYP + DST.PORT)
				// 这部分的固定长度, 4 是 DST.ADDR 的长度.
				auto head_size = 6 + (remote_endp.address().is_v6() ? 16 : 4);
				auto udp_size = bytes + head_size;

				// 在数据包前面添加 socks5 udp 头部, 然后转发给 socks 客户端.
				char* wp = (char*)(rbuf - head_size);

				write<uint16_t>(0x0, wp); // rsv
				write<uint8_t>(0x0, wp); // frag

				if (remote_endp.address().is_v4())
				{
					auto uaddr = remote_endp.address().to_v4().to_uint();
					write<uint8_t>(SOCKS5_ATYP_IPV4, wp); // atyp

					write<uint32_t>(uaddr, wp);
					write<uint16_t>(remote_endp.port(), wp);
				}
				if (remote_endp.address().is_v6())
				{
					write<uint8_t>(SOCKS5_ATYP_IPV6, wp); // atyp

					auto data = remote_endp.address().to_v6().to_bytes();
					for (auto c : data)
						write<uint8_t>(c, wp);
					write<uint16_t>(remote_endp.port(), wp);
				}

				recv_total++;

				// 指向 udp header 位置开始发送数据.
				auto wbuf = rbuf - head_size;

				co_await server.async_send_to(
					net::buffer(wbuf, udp_size),
					m_local_udp_endpoint,
					net_awaitable[ec]);
			}
		}

		log_conn_debug()
			<< ", recv total: "
			<< recv_total
			<< ", send total: "
			<< send_total
			<< ", forward_udp quit";

		co_return;
	}

	net::awaitable<void> proxy_session::socks_connect_v4() noexcept
	{
		auto self = shared_from_this();
		auto p = (const char*)m_local_buffer.data().data();

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
			log_conn_warning()
				<< ", read socks4 dst: "
				<< ec.message();
			co_return;
		}

		tcp::endpoint dst_endpoint;
		p = (const char*)m_local_buffer.data().data();

		auto port = read<uint16_t>(p);
		dst_endpoint.port(port);
		dst_endpoint.address(net::ip::address_v4(read<uint32_t>(p)));

		bool socks4a = false;
		auto ip_bytes = dst_endpoint.address().to_v4().to_bytes();
		if (ip_bytes[0] == 0 && ip_bytes[1] == 0 && ip_bytes[2] == 0 && ip_bytes[3] != 0)
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
			log_conn_warning()
				<< ", read socks4 userid: "
				<< ec.message();
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
				log_conn_warning()
					<< ", read socks4a hostname: "
					<< ec.message();
				co_return;
			}

			if (bytes > 1)
			{
				hostname.resize(bytes - 1);
				m_local_buffer.sgetn(&hostname[0], bytes - 1);
			}
		}

		log_conn_debug()
			<< ", use "
			<< (socks4a ? "domain: " : "ip: ")
			<< (socks4a ? hostname : dst_endpoint.address().to_string());

		// 用户认证逻辑, 如果用户认证列表为空, 则表示不需要用户认证.
		bool verify_passed = co_await check_userpasswd(userid, "", true);

		if (auth_required())
		{
			if (verify_passed)
				log_conn_debug() << ", socks4 userid: " << userid << ", user authenticated successfully";
			else
				log_conn_warning() << ", socks4 userid: " << userid << ", user authentication failed";
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
			auto wp = (char*)wbuf.prepare(16).data();

			write<uint8_t>(0, wp);
			write<uint8_t>(SOCKS4_REQUEST_REJECTED_USER_NO_ALLOW, wp);

			write<uint16_t>(dst_endpoint.port(), wp);
			write<uint32_t>(dst_endpoint.address().to_v4().to_uint(), wp);

			wbuf.commit(8);
			bytes = co_await net::async_write(m_local_socket,
				wbuf,
				net::transfer_exactly(8),
				net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", write socks4 no allow: "
					<< ec.message();
				co_return;
			}

			log_conn_warning()
				<< ", socks4 "
				<< userid
				<< " auth fail";
			co_return;
		}

		int error_code = SOCKS4_REQUEST_GRANTED;
		if (command == SOCKS_CMD_CONNECT)
		{
			if (socks4a)
				ec = co_await start_connect_host(
					hostname,
					port,
					false);
			else
				ec = co_await start_connect_host(
					dst_endpoint.address().to_string(),
					port,
					true);
			if (ec)
			{
				log_conn_warning()
					<< ", connect to target "
					<< dst_endpoint.address().to_string()
					<< ":"
					<< port
					<< " error: "
					<< ec.message();
				error_code = SOCKS4_CANNOT_CONNECT_TARGET_SERVER;
			}
		}
		else
		{
			error_code = SOCKS4_REQUEST_REJECTED_OR_FAILED;
			XLOG_FWARN("connection id: {},"
				" unsupported command for socks4",
				m_connection_id);
		}

		//  +----+----+----+----+----+----+----+----+
		//  | VN | CD | DSTPORT |      DSTIP        |
		//  +----+----+----+----+----+----+----+----+
		//  | 1  | 1  |    2    |         4         |
		//  +----+----+----+----+----+----+----+----+
		//  [                                       ]

		net::streambuf wbuf;
		auto wp = (char*)wbuf.prepare(16).data();

		write<uint8_t>(0, wp);
		write<uint8_t>((uint8_t)error_code, wp);

		// 返回IP:PORT.
		write<uint16_t>(dst_endpoint.port(), wp);
		write<uint32_t>(dst_endpoint.address().to_v4().to_uint(), wp);

		wbuf.commit(8);
		bytes = co_await net::async_write(m_local_socket,
			wbuf,
			net::transfer_exactly(8),
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", write socks4 response: "
				<< ec.message();
			co_return;
		}

		if (error_code != SOCKS4_REQUEST_GRANTED)
			co_return;

		co_await concurrent_transfer();

		co_return;
	}

	net::awaitable<bool> proxy_session::socks_auth() noexcept
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
			log_conn_warning()
				<< ", read client username/passwd error: "
				<< ec.message();
			co_return false;
		}

		auto p = (const char*)m_local_buffer.data().data();
		int auth_version = read<int8_t>(p);
		if (auth_version != 1)
		{
			log_conn_warning()
				<< ", socks negotiation, unsupported socks5 protocol";
			co_return false;
		}
		int name_length = read<uint8_t>(p);
		if (name_length < 0 || name_length > 255)
		{
			log_conn_warning()
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
			log_conn_warning()
				<< ", read client username error: "
				<< ec.message();
			co_return false;
		}

		std::string uname;

		p = (const char*)m_local_buffer.data().data();
		for (size_t i = 0; i < bytes - 1; i++)
			uname.push_back(read<int8_t>(p));

		int passwd_len = read<uint8_t>(p);
		if (passwd_len < 0 || passwd_len > 255)
		{
			log_conn_warning()
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
			log_conn_warning()
				<< ", read client passwd error: "
				<< ec.message();
			co_return false;
		}

		std::string passwd;

		p = (const char*)m_local_buffer.data().data();
		for (size_t i = 0; i < bytes; i++)
			passwd.push_back(read<int8_t>(p));

		// SOCKS5验证用户和密码, 用户认证逻辑.
		bool verify_passed = co_await check_userpasswd(uname, passwd);

		if (auth_required())
		{
			if (verify_passed)
				log_conn_debug() << ", socks5 userid: " << uname << ", user authenticated successfully";
			else
				log_conn_warning() << ", socks5 userid: " << uname << ", user authentication failed";
		}

		net::streambuf wbuf{};

		auto wp = (char*)wbuf.prepare(16).data();
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
			log_conn_warning()
				<< ", server write status error: "
				<< ec.message();
			co_return false;
		}

		co_return verify_passed;
	}

} // namespace proxy
