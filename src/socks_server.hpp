#ifndef SOCKS_SERVER_HPP
#define SOCKS_SERVER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#if __linux__
#include <unistd.h>
#else
#include <io.h>
#endif

#include <deque>
#include <cstring> // for std::memcpy

#include <boost/logic/tribool.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/array.hpp>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "io.hpp"
extern "C"
{
#include "v7.h"
}

namespace socks {

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

class socks_session
	: public boost::enable_shared_from_this<socks_session>
{
	enum {
		SOCKS_VERSION_4 = 4,
		SOCKS_VERSION_5 = 5
	};
	enum {
		SOCKS5_AUTH_NONE = 0x00,
		SOCKS5_AUTH = 0x02,
		SOCKS5_AUTH_UNACCEPTABLE = 0xFF
	};
	enum {
		SOCKS_CMD_CONNECT = 0x01,
		SOCKS_CMD_BIND = 0x02,
		SOCKS5_CMD_UDP = 0x03
	};
	enum {
		SOCKS5_ATYP_IPV4 = 0x01,
		SOCKS5_ATYP_DOMAINNAME = 0x03,
		SOCKS5_ATYP_IPV6 = 0x04
	};
	enum {
		SOCKS5_SUCCEEDED = 0x00,
		SOCKS5_GENERAL_SOCKS_SERVER_FAILURE,
		SOCKS5_CONNECTION_NOT_ALLOWED_BY_RULESET,
		SOCKS5_NETWORK_UNREACHABLE,
		SOCKS5_CONNECTION_REFUSED,
		SOCKS5_TTL_EXPIRED,
		SOCKS5_COMMAND_NOT_SUPPORTED,
		SOCKS5_ADDRESS_TYPE_NOT_SUPPORTED,
		SOCKS5_UNASSIGNED
	};
	enum {
		SOCKS4_REQUEST_GRANTED = 90,
		SOCKS4_REQUEST_REJECTED_OR_FAILED,
		SOCKS4_CANNOT_CONNECT_TARGET_SERVER,
		SOCKS4_REQUEST_REJECTED_USER_NO_ALLOW,
	};

	enum {
		MAX_RECV_BUFFER_SIZE = 768,	// 最大udp接收缓冲大小.
		MAX_SEND_BUFFER_SIZE = 768	// 最大udp发送缓冲大小.
	};

public:
	explicit socks_session(boost::asio::io_service &io)
		: m_io_service(io)
		, m_local_socket(io)
		, m_remote_socket(io)
		, m_udp_socket(io)
		, m_resolver(io)
		, m_version(-1)
		, m_method(-1)
		, m_command(-1)
		, m_atyp(-1)
		, m_port(0)
		, m_verify_passed(boost::indeterminate)
		, m_udp_timer(io)
	{}
	~socks_session() = default;

public:
	void start()
	{
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
		boost::asio::async_read(m_local_socket, boost::asio::buffer(m_local_buffer, 2),
			boost::asio::transfer_exactly(2),
			boost::bind(&socks_session::socks_handle_connect_1, shared_from_this(),
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred
			)
		);
	}

	tcp::socket& socket() { return m_local_socket; }

protected:
	void socks_handle_connect_1(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		if (!error)
		{
			char *p = m_local_buffer.data();
			m_version = read_int8(p);
			if (m_version == SOCKS_VERSION_5)	// sock5协议.
			{
				int nmethods = read_int8(p);	// 读取客户端支持的代理方式列表.
				if (nmethods <= 0 || nmethods > 255)
				{
					std::cout << "unsupport any method!\n";
					return;
				}

				//  +----+----------+----------+
				//  |VER | NMETHODS | METHODS  |
				//  +----+----------+----------+
				//  | 1  |    1     | 1 to 255 |
				//  +----+----------+----------+
				//                  [          ]
				boost::asio::async_read(m_local_socket, boost::asio::buffer(m_local_buffer, nmethods),
					boost::asio::transfer_exactly(nmethods),
					boost::bind(&socks_session::socks_handle_connect_2, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);
			}
			else if (m_version == SOCKS_VERSION_4)	// socks4协议.
			{
				//  +----+----+----+----+----+----+----+----+----+----+....+----+
				//  | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
				//  +----+----+----+----+----+----+----+----+----+----+....+----+
				//  | 1  | 1  |    2    |         4         | variable     | 1  |
				//  +----+----+----+----+----+----+----+----+----+----+....+----+
				//            [                             ]

				m_command = read_int8(p);

				boost::asio::async_read(m_local_socket, boost::asio::buffer(m_local_buffer, 6),
					boost::asio::transfer_exactly(6),
					boost::bind(&socks_session::socks_handle_connect_2, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);
			}
			else
			{
				// std::cout << "error unknow protocol.\n";
			}
		}
	}

	void socks_handle_connect_2(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		if (!error)
		{
			if (m_version == SOCKS_VERSION_5)
			{
				// 循环读取客户端支持的代理方式.
				char *p = m_local_buffer.data();
				m_method = SOCKS5_AUTH_UNACCEPTABLE;
				bool support_auth = false;
				while (bytes_transferred != 0)
				{
					int m = read_int8(p);
					if (m == SOCKS5_AUTH_NONE || m == SOCKS5_AUTH)
						m_method = m;
					if (m == SOCKS5_AUTH)
						support_auth = true;
					bytes_transferred--;
				}

				// do_auth.js存在则表示需要认证.
				if (access("do_auth.js", 00) == 0 && !support_auth)
				{
					// 回复客户端, 不接受的代理方式.
					p = m_local_buffer.data();
					write_int8(m_version, p);
					write_int8(SOCKS5_AUTH_UNACCEPTABLE, p);
				}
				else
				{
					// 回复客户端, 选择的代理方式.
					p = m_local_buffer.data();
					write_int8(m_version, p);
					write_int8(m_method, p);
				}

				//  +----+--------+
				//  |VER | METHOD |
				//  +----+--------+
				//  | 1  |   1    |
				//  +----+--------+
				//  [             ]
				boost::asio::async_write(m_local_socket, boost::asio::buffer(m_local_buffer, 2),
					boost::asio::transfer_exactly(2),
					boost::bind(&socks_session::socks_handle_send_version, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);
			}

			if (m_version == SOCKS_VERSION_4)
			{
				char *p = m_local_buffer.data();
				m_address.port(read_uint16(p));
				m_address.address(boost::asio::ip::address_v4(read_uint32(p)));

				//  +----+----+----+----+----+----+----+----+----+----+....+----+
				//  | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
				//  +----+----+----+----+----+----+----+----+----+----+....+----+
				//  | 1  | 1  |    2    |         4         | variable     | 1  |
				//  +----+----+----+----+----+----+----+----+----+----+....+----+
				//                                          [                   ]
				boost::asio::async_read_until(m_local_socket, m_streambuf, '\0',
					boost::bind(&socks_session::socks_handle_negotiation_2, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);
			}
		}
	}

	void socks_handle_send_version(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		if (!error)
		{
			if (m_method == SOCKS5_AUTH && boost::indeterminate(m_verify_passed))	// 认证模式，未确定认证状态，开始读取用户密码信息.
			{
				//  +----+------+----------+------+----------+
				//  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
				//  +----+------+----------+------+----------+
				//  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
				//  +----+------+----------+------+----------+
				//  [           ]
				boost::asio::async_read(m_local_socket, boost::asio::buffer(m_local_buffer, 2),
					boost::asio::transfer_exactly(2),
					boost::bind(&socks_session::socks_handle_negotiation_1, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);
			}
			else if (m_method == SOCKS5_AUTH_NONE || m_verify_passed)	// 非认证模式, 或认证模式下认证已经通过, 接收socks客户端Requests.
			{
				//  +----+-----+-------+------+----------+----------+
				//  |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
				//  +----+-----+-------+------+----------+----------+
				//  | 1  |  1  | X'00' |  1   | Variable |    2     |
				//  +----+-----+-------+------+----------+----------+
				//  [                          ]
				boost::asio::async_read(m_local_socket, boost::asio::buffer(m_local_buffer, 5),
					boost::asio::transfer_exactly(5),
					boost::bind(&socks_session::socks_handle_requests_1, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);
			}
			else
			{
				close();
			}
		}
	}

	void socks_handle_negotiation_1(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		if (error)
		{
			std::cout << "socks_handle_negotiation_1, error: " << error.message() << std::endl;
			return;
		}

		if (!error)
		{
			char *p = m_local_buffer.data();
			int auth_version = read_int8(p);
			if (auth_version != 1)
			{
				std::cout << "socks_handle_negotiation_1, unsupport socks5 protocol\n";
				return;
			}
			int name_length = read_uint8(p);
			if (name_length <= 0 || name_length > 255)
			{
				std::cout << "socks_handle_negotiation_1, error unknow protocol.\n";
				return;
			}
			name_length += 1;
			//  +----+------+----------+------+----------+
			//  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
			//  +----+------+----------+------+----------+
			//  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
			//  +----+------+----------+------+----------+
			//              [                 ]
			boost::asio::async_read(m_local_socket, boost::asio::buffer(m_local_buffer, name_length),
				boost::asio::transfer_exactly(name_length),
				boost::bind(&socks_session::socks_handle_negotiation_2, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);
		}
	}

	void socks_handle_negotiation_2(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		if (error)
		{
			std::cout << "socks_handle_negotiation_2, error: " << error.message() << std::endl;
			return;
		}

		if (!error)
		{
			if (m_version == SOCKS_VERSION_5)
			{
				char *p = m_local_buffer.data();
				for (int i = 0; i < bytes_transferred - 1; i++)
					m_uname.push_back(read_int8(p));
				int passwd_len = read_uint8(p);
				if (passwd_len <= 0 || passwd_len > 255)
				{
					std::cout << "socks_handle_negotiation_2, error unknow protocol.\n";
					return;
				}
				//  +----+------+----------+------+----------+
				//  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
				//  +----+------+----------+------+----------+
				//  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
				//  +----+------+----------+------+----------+
				//                                [          ]
				boost::asio::async_read(m_local_socket, boost::asio::buffer(m_local_buffer, passwd_len),
					boost::asio::transfer_exactly(passwd_len),
					boost::bind(&socks_session::socks_handle_negotiation_3, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);
			}

			if (m_version == SOCKS_VERSION_4)
			{
				std::string userid;

				if (bytes_transferred > 1)
				{
					userid.resize(bytes_transferred - 1);
					m_streambuf.sgetn(&userid[0], bytes_transferred - 1);
				}
				m_streambuf.commit(1);

				// 检查是否需要SOCKS4认证用户, do_auth.js存在则表示需要认证.
				if (access("do_auth.js", 0) == 0)
				{
					auto endp = m_local_socket.remote_endpoint();
					auto client = endp.address().to_string();
					client += ":" + std::to_string(endp.port());

					m_verify_passed = do_auth(client, userid, "");
				}
				else
				{
					m_verify_passed = true;
				}

				if (!m_verify_passed)
				{
					//  +----+----+----+----+----+----+----+----+
					//  | VN | CD | DSTPORT |      DSTIP        |
					//  +----+----+----+----+----+----+----+----+
					//  | 1  | 1  |    2    |         4         |
					//  +----+----+----+----+----+----+----+----+
					//  [                                       ]
					char *p = m_local_buffer.data();
					write_int8(0, p);
					write_int8(SOCKS4_REQUEST_REJECTED_USER_NO_ALLOW, p);

					// 返回IP:PORT.
					write_uint16(m_address.port(), p);
					write_uint32(m_address.address().to_v4().to_ulong(), p);

					boost::asio::async_write(m_local_socket, boost::asio::buffer(m_local_buffer, 8),
						boost::asio::transfer_exactly(8),
						boost::bind(&socks_session::socks_handle_error, shared_from_this(),
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred
						)
					);

					return;
				}

				// 发起连接.
				if (m_command == SOCKS_CMD_CONNECT)
				{
					tcp::resolver::iterator endpoint_iterator;
					m_remote_socket.async_connect(m_address,
						boost::bind(&socks_session::socks_handle_connect_3,
							shared_from_this(), boost::asio::placeholders::error,
							endpoint_iterator
						)
					);
					return;
				}

				if (m_command == SOCKS_CMD_BIND)
				{
					// TODO: 实现绑定请求.
				}
			}
		}
	}

	void socks_handle_negotiation_3(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		if (error)
		{
			std::cout << "socks_handle_negotiation_3, error: " << error.message() << std::endl;
			return;
		}

		if (!error)
		{
			char *p = m_local_buffer.data();
			for (int i = 0; i < bytes_transferred; i++)
				m_passwd.push_back(read_int8(p));

			// SOCKS5验证用户和密码.
			auto endp = m_local_socket.remote_endpoint();
			auto client = endp.address().to_string();
			client += ":" + std::to_string(endp.port());

			m_verify_passed = do_auth(client, m_uname, m_passwd);

			p = m_local_buffer.data();
			write_int8(0x01, p);		// version 只能是1.
			if (m_verify_passed)
			{
				write_int8(0x00, p);		// 认证通过返回0x00, 其它值为失败.
			}
			else
			{
				write_int8(0x01, p);		// 认证返回0x01为失败.
			}

			// 返回认证状态.
			//  +----+--------+
			//  |VER | STATUS |
			//  +----+--------+
			//  | 1  |   1    |
			//  +----+--------+
			boost::asio::async_write(m_local_socket, boost::asio::buffer(m_local_buffer, 2),
				boost::asio::transfer_exactly(2),
				boost::bind(&socks_session::socks_handle_send_version, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);
		}
	}

	void socks_handle_requests_1(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		if (error)
		{
			std::cout << "socks_handle_requests_1, error: " << error.message() << std::endl;
			return;
		}

		if (!error)
		{
			char *p = m_local_buffer.data();
			if (read_int8(p) != SOCKS_VERSION_5)
			{
				std::cout << "socks_handle_requests_1, error unknow protocol.\n";
				return;
			}

			m_command = read_int8(p);		// CONNECT/BIND/UDP
			read_int8(p);				// reserved.
			m_atyp = read_int8(p);			// atyp.

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

			if (m_atyp == SOCKS5_ATYP_IPV4)
				length = 5; // 6 - 1
			else if (m_atyp == SOCKS5_ATYP_DOMAINNAME)
			{
				length = read_uint8(p) + 2;
				prefix = 0;
			}
			else if (m_atyp == SOCKS5_ATYP_IPV6)
				length = 17; // 18 - 1

			boost::asio::async_read(m_local_socket, boost::asio::buffer(m_local_buffer.begin() + prefix, length),
				boost::asio::transfer_exactly(length),
				boost::bind(&socks_session::socks_handle_requests_2, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);
		}
	}

	void socks_handle_requests_2(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		if (error)
		{
			std::cout << "socks_handle_requests_2, error: " << error.message() << std::endl;
			return;
		}

		if (!error)
		{
			if (m_version == SOCKS_VERSION_5)
			{
				char *p = m_local_buffer.data();

				if (m_atyp == SOCKS5_ATYP_IPV4)
				{
					m_address.address(boost::asio::ip::address_v4(read_uint32(p)));
					m_address.port(read_uint16(p));
					std::cout << m_local_socket.remote_endpoint() << " use ipv4: " << m_address << std::endl;
				}
				else if (m_atyp == SOCKS5_ATYP_DOMAINNAME)
				{
					for (int i = 0; i < bytes_transferred - 2; i++)
						m_domain.push_back(read_int8(p));
					m_port = read_uint16(p);
					std::cout << m_local_socket.remote_endpoint() << " use domain: " << m_domain << ":" << m_port << std::endl;
				}
				else if (m_atyp == SOCKS5_ATYP_IPV6)
				{
					boost::asio::ip::address_v6::bytes_type addr;
					for (boost::asio::ip::address_v6::bytes_type::iterator i = addr.begin();
						i != addr.end(); ++i)
					{
						*i = read_int8(p);
					}

					m_address.address(boost::asio::ip::address_v6(addr));
					m_address.port(read_uint16(p));
					std::cout << m_local_socket.remote_endpoint() << " use ipv6: " << m_address << std::endl;
				}

				// 发起连接.
				if (m_command == SOCKS_CMD_CONNECT)
				{
					if (m_atyp == SOCKS5_ATYP_IPV4 || m_atyp == SOCKS5_ATYP_IPV6)
					{
						tcp::resolver::iterator endpoint_iterator;
						m_remote_socket.async_connect(m_address,
							boost::bind(&socks_session::socks_handle_connect_3,
								shared_from_this(), boost::asio::placeholders::error,
								endpoint_iterator
							)
						);
						return;
					}
					if (m_atyp == SOCKS5_ATYP_DOMAINNAME)
					{
						std::ostringstream port_string;
						port_string << m_port;
						tcp::resolver::query query(m_domain, port_string.str());

						m_resolver.async_resolve(query,
							boost::bind(&socks_session::socks_handle_resolve,
								shared_from_this(),	boost::asio::placeholders::error,
								boost::asio::placeholders::iterator
							)
						);
						return;
					}
				}
				else if (m_command == SOCKS5_CMD_UDP || m_command == SOCKS_CMD_BIND || true)
				{
					// 实现UDP ASSOCIATE.
					if (m_command == SOCKS5_CMD_UDP)
					{
						if (m_atyp == SOCKS5_ATYP_IPV4 || m_atyp == SOCKS5_ATYP_IPV6)
						{
							// 得到客户端指定的协议类型, ipv4或ipv6, 并open udp_socket随机分配一个udp端口.
							m_client_endpoint = udp::endpoint(m_address.address(), m_address.port());
							boost::system::error_code ec;
							m_udp_socket.open(m_client_endpoint.protocol(), ec);
							if (ec)
							{
								// 打开udp socket失败.
								//  +----+-----+-------+------+----------+----------+
								//  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
								//  +----+-----+-------+------+----------+----------+
								//  | 1  |  1  | X'00' |  1   | Variable |    2     |
								//  +----+-----+-------+------+----------+----------+
								//  [                                               ]
								p = m_local_buffer.data();
								write_int8(SOCKS_VERSION_5, p);
								write_int8(SOCKS5_GENERAL_SOCKS_SERVER_FAILURE, p);
								write_int8(0x00, p);
								write_int8(1, p);
								// 没用的东西.
								for (int i = 0; i < 6; i++)
									write_int8(0, p);
								boost::asio::async_write(m_local_socket, boost::asio::buffer(m_local_buffer, 10),
									boost::asio::transfer_exactly(10),
									boost::bind(&socks_session::socks_handle_error, shared_from_this(),
										boost::asio::placeholders::error,
										boost::asio::placeholders::bytes_transferred
									)
								);
								return;
							}

							// 绑定udp端口.
							m_udp_socket.bind(udp::endpoint(m_client_endpoint.protocol(), 0),ec);
							if (ec)
							{
																// 打开udp socket失败.
								//  +----+-----+-------+------+----------+----------+
								//  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
								//  +----+-----+-------+------+----------+----------+
								//  | 1  |  1  | X'00' |  1   | Variable |    2     |
								//  +----+-----+-------+------+----------+----------+
								//  [                                               ]
								p = m_local_buffer.data();
								write_int8(SOCKS_VERSION_5, p);
								write_int8(SOCKS5_GENERAL_SOCKS_SERVER_FAILURE, p);
								write_int8(0x00, p);
								write_int8(1, p);
								// 没用的东西.
								for (int i = 0; i < 6; i++)
									write_int8(0, p);
								boost::asio::async_write(m_local_socket, boost::asio::buffer(m_local_buffer, 10),
									boost::asio::transfer_exactly(10),
									boost::bind(&socks_session::socks_handle_error, shared_from_this(),
										boost::asio::placeholders::error,
										boost::asio::placeholders::bytes_transferred
									)
								);
								return;
							}

							// 如果IP地址为空, 则使用tcp连接上的IP.
							if (m_address.address() == boost::asio::ip::address::from_string("0.0.0.0"))
							{
								boost::system::error_code err;
								tcp::endpoint endp = m_local_socket.remote_endpoint(err);
								if (err)
								{
									close();
									return;
								}

								m_client_endpoint.address(endp.address());
							}

							// 打开成功, 返回当前服务器端吕等信息给客户端.

							//  +----+-----+-------+------+----------+----------+
							//  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
							//  +----+-----+-------+------+----------+----------+
							//  | 1  |  1  | X'00' |  1   | Variable |    2     |
							//  +----+-----+-------+------+----------+----------+
							//  [                                               ]

							p = m_local_buffer.data();
							write_int8(SOCKS_VERSION_5, p);
							write_int8(SOCKS5_SUCCEEDED, p);
							write_int8(0x00, p);
							write_int8(1, p);
							// IP地址(BND.ADDR)直接写0, 客户端知道代理服务器的地址.
							for (int i = 0; i < 4; i++)
								write_int8(0, p);
							// UDP侦听的端口(BND.PORT).
							write_uint16(m_udp_socket.local_endpoint().port(), p);
							// 发送.
							boost::asio::async_write(m_local_socket, boost::asio::buffer(m_local_buffer, 10),
								boost::asio::transfer_exactly(10),
								boost::bind(&socks_session::socks_handle_succeed, shared_from_this(),
									boost::asio::placeholders::error,
									boost::asio::placeholders::bytes_transferred
								)
							);
							return;
						}
						if (m_atyp == SOCKS5_ATYP_DOMAINNAME)
						{
							// TODO: 实现客户端域名解析, 用于udp传回数据.
						}
					}

					//  +----+-----+-------+------+----------+----------+
					//  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
					//  +----+-----+-------+------+----------+----------+
					//  | 1  |  1  | X'00' |  1   | Variable |    2     |
					//  +----+-----+-------+------+----------+----------+
					//  [                                               ]
					p = m_local_buffer.data();
					write_int8(SOCKS_VERSION_5, p);
					write_int8(SOCKS5_COMMAND_NOT_SUPPORTED, p);
					write_int8(0x00, p);
					write_int8(1, p);
					// 没用的东西.
					for (int i = 0; i < 6; i++)
						write_int8(0, p);
					boost::asio::async_write(m_local_socket, boost::asio::buffer(m_local_buffer, 10),
						boost::asio::transfer_exactly(10),
						boost::bind(&socks_session::socks_handle_error, shared_from_this(),
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred
						)
					);
				}
			}
		}
	}

	void socks_handle_connect_3(const boost::system::error_code &error,
		tcp::resolver::iterator endpoint_iterator)
	{
		if (error)
		{
			if (m_version == SOCKS_VERSION_5)
			{
				// 连接目标失败!
				//  +----+-----+-------+------+----------+----------+
				//  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
				//  +----+-----+-------+------+----------+----------+
				//  | 1  |  1  | X'00' |  1   | Variable |    2     |
				//  +----+-----+-------+------+----------+----------+
				//  [                                               ]
				char *p = m_local_buffer.data();
				write_int8(SOCKS_VERSION_5, p);
				if (error == boost::asio::error::connection_refused)
					write_int8(SOCKS5_CONNECTION_REFUSED, p);
				else if (error == boost::asio::error::network_unreachable)
					write_int8(SOCKS5_NETWORK_UNREACHABLE, p);
				else
					write_int8(SOCKS5_GENERAL_SOCKS_SERVER_FAILURE, p);

				write_int8(0x00, p);

				// 返回解析出来的IP:PORT.
				if (endpoint_iterator != tcp::resolver::iterator())
				{
					auto endp = endpoint_iterator->endpoint();
					write_int8(1, p);
					write_uint32(endp.address().to_v4().to_ulong(), p);
					write_uint16(endp.port(), p);
				}
				else if (!m_domain.empty())
				{
					write_int8(0x03, p);
					write_int8(static_cast<int8_t>(m_domain.size()), p);
					write_string(m_domain, p);
					write_uint16(m_port, p);
				}
				else
				{
					write_int8(0x1, p);
					write_uint32(0, p);
					write_uint16(0, p);
				}

				boost::asio::async_write(m_local_socket, boost::asio::buffer(m_local_buffer, 10),
					boost::asio::transfer_exactly(10),
					boost::bind(&socks_session::socks_handle_error, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);

				return;
			}

			// 连接失败.
			if (m_version == SOCKS_VERSION_4)
			{
				//  +----+----+----+----+----+----+----+----+
				//  | VN | CD | DSTPORT |      DSTIP        |
				//  +----+----+----+----+----+----+----+----+
				//  | 1  | 1  |    2    |         4         |
				//  +----+----+----+----+----+----+----+----+
				//  [                                       ]
				char *p = m_local_buffer.data();
				write_int8(0, p);
				write_int8(SOCKS4_REQUEST_REJECTED_OR_FAILED, p);

				// 返回IP:PORT.
				write_uint16(m_address.port(), p);
				write_uint32(m_address.address().to_v4().to_ulong(), p);

				boost::asio::async_write(m_local_socket, boost::asio::buffer(m_local_buffer, 8),
					boost::asio::transfer_exactly(8),
					boost::bind(&socks_session::socks_handle_error, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);

				return;
			}
		}
		else
		{
			if (m_version == SOCKS_VERSION_5)
			{
				// 连接成功.
				//  +----+-----+-------+------+----------+----------+
				//  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
				//  +----+-----+-------+------+----------+----------+
				//  | 1  |  1  | X'00' |  1   | Variable |    2     |
				//  +----+-----+-------+------+----------+----------+
				//  [                                               ]

				char *p = m_local_buffer.data();
				int len = 4;
				write_int8(SOCKS_VERSION_5, p);
				write_int8(SOCKS5_SUCCEEDED, p);
				write_int8(0x00, p);
				write_int8(m_atyp, p);

				boost::system::error_code ec;
				tcp::endpoint endp = m_remote_socket.remote_endpoint(ec);
				if (ec)
				{
					close();
					return;
				}

				if (m_atyp == SOCKS5_ATYP_IPV4)
				{
					len += 6;
					write_uint32(endp.address().to_v4().to_ulong(), p);
					write_uint16(endp.port(), p);
				}
				if (m_atyp == SOCKS5_ATYP_IPV6)
				{
					len += 18;
					boost::asio::ip::address_v6::bytes_type addr;
					addr = endp.address().to_v6().to_bytes();
					for (std::size_t i = 0; i < addr.size(); i++)
						write_int8(addr[i], p);
					write_uint16(endp.port(), p);
				}
				if (m_atyp == SOCKS5_ATYP_DOMAINNAME)
				{
					len += (static_cast<int>(m_domain.size()) + 3);
					write_int8(static_cast<int>(m_domain.size()), p);
					write_string(m_domain, p);
					write_uint16(m_port, p);
				}

				// 发送回复.
				boost::asio::async_write(m_local_socket, boost::asio::buffer(m_local_buffer, len),
					boost::asio::transfer_exactly(len),
					boost::bind(&socks_session::socks_handle_succeed, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);

				// 投递一个数据接收.
				m_remote_socket.async_read_some(boost::asio::buffer(m_remote_buffer),
					boost::bind(&socks_session::socks_handle_remote_read, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);

				return;
			}

			if (m_version == SOCKS_VERSION_4)
			{
				//  +----+----+----+----+----+----+----+----+
				//  | VN | CD | DSTPORT |      DSTIP        |
				//  +----+----+----+----+----+----+----+----+
				//  | 1  | 1  |    2    |         4         |
				//  +----+----+----+----+----+----+----+----+
				//  [                                       ]
				char *p = m_local_buffer.data();
				write_int8(0, p);
				write_int8(SOCKS4_REQUEST_GRANTED, p);

				boost::system::error_code ec;
				tcp::endpoint endp = m_remote_socket.remote_endpoint(ec);
				if (ec)
				{
					close();
					return;
				}

				write_uint16(endp.port(), p);
				write_uint32(endp.address().to_v4().to_ulong(), p);

				// 回复成功.
				boost::asio::async_write(m_local_socket, boost::asio::buffer(m_local_buffer, 8),
					boost::asio::transfer_exactly(8),
					boost::bind(&socks_session::socks_handle_succeed, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);

				// 投递一个数据接收.
				m_remote_socket.async_read_some(boost::asio::buffer(m_remote_buffer),
					boost::bind(&socks_session::socks_handle_remote_read, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);

				return;
			}
		}
	}

	void socks_handle_resolve(const boost::system::error_code &error,
		tcp::resolver::iterator endpoint_iterator)
	{
		if (error)
		{
			std::cout << "socks_handle_resolve, error: " << error.message() << std::endl;
			return;
		}

		if (!error)
		{
			boost::asio::async_connect(m_remote_socket,	endpoint_iterator,
				boost::bind(&socks_session::socks_handle_connect_3,
					shared_from_this(), boost::asio::placeholders::error,
					endpoint_iterator
				)
			);
		}
	}

	void socks_handle_error(const boost::system::error_code &error, std::size_t bytes_transferred)
	{
		// 什么都不用做了, 退了.
		if (m_udp_socket.is_open())
		{
			// 如果打开了udp socket, 则先关掉不.
			boost::system::error_code ec;
			m_udp_socket.close(ec);
		}
	}

	void socks_handle_succeed(const boost::system::error_code &error, std::size_t bytes_transferred)
	{
		if (!error)
		{
			if (m_command == SOCKS5_CMD_UDP)
			{
				// 开始投递异步udp数据接收, 并开始转发数据.
				// 转发规则为:
				// 1. 任何接收到的非来自m_client_endpoint上的数据, 添加协议头后, 都将转发到m_client_endpoint.
				// 2. 接收到来自m_client_endpoint上的数据, 解析协议头, 并转发到协议中指定的endpoint.
				// 3. tcp socket断开时, 取消所有异步IO, 销毁当前session对象.
				// 4. tcp socket上任何数据传输, 处理方法如同步骤2.
				// 5. 任何socket错误, 处理方法如同步骤2.
				// 6. m_udp_timer超时(即5分钟内没任何数据传输), 处理方法如同步骤2.
				for (int i = 0; i < MAX_RECV_BUFFER_SIZE; i++)
				{
					recv_buffer& recv_buf = m_recv_buffers[i];
					boost::array<char, 2048>& buf = recv_buf.buffer;
					m_udp_socket.async_receive_from(boost::asio::buffer(buf), recv_buf.endp,
						boost::bind(&socks_session::socks_handle_udp_read, shared_from_this(),
							i,
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred
						)
					);
				}

				// 启动定时器.
				m_udp_timer.expires_from_now(boost::posix_time::seconds(1));
				m_udp_timer.async_wait(
					boost::bind(&socks_session::socks_udp_timer_handle,
						shared_from_this(),
						boost::asio::placeholders::error
					)
				);
			}
			else if (m_command == SOCKS_CMD_CONNECT)
			{
				// 投递一个数据接收.
				m_local_socket.async_read_some(boost::asio::buffer(m_local_buffer),
					boost::bind(&socks_session::socks_handle_local_read, shared_from_this(),
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);
			}
			else
			{
				// for debug.
				BOOST_ASSERT(0);
			}
		}
		else
		{
			std::cout << "socks_handle_succeed, error: " << error.message() << std::endl;
			close();
		}
	}

	void socks_handle_remote_read(const boost::system::error_code &error, std::size_t bytes_transferred)
	{
		if (!error)
		{
			// 发送到本地.
			boost::asio::async_write(m_local_socket, boost::asio::buffer(m_remote_buffer, bytes_transferred),
				boost::asio::transfer_exactly(bytes_transferred),
				boost::bind(&socks_session::socks_handle_remote_write, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);
		}
		else
		{
			if (error == boost::asio::error::eof)
				close();
			else
				close(true, true);
		}
	}

	void socks_handle_remote_write(const boost::system::error_code &error, std::size_t bytes_transferred)
	{
		if (!error)
		{
			// 从远端读取数据.
			m_remote_socket.async_read_some(boost::asio::buffer(m_remote_buffer),
				boost::bind(&socks_session::socks_handle_remote_read, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);
		}
		else
		{
			if (error == boost::asio::error::eof)
				close();
			else
				close(true, true);
		}
	}

	void socks_handle_local_read(const boost::system::error_code &error, std::size_t bytes_transferred)
	{
		if (!error)
		{
			// 发送到目标.
			boost::asio::async_write(m_remote_socket, boost::asio::buffer(m_local_buffer, bytes_transferred),
				boost::asio::transfer_exactly(bytes_transferred),
				boost::bind(&socks_session::socks_handle_local_write, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);
		}
		else
		{
			if (error == boost::asio::error::eof)
				close();
			else
				close(true, true);
		}
	}

	void socks_handle_local_write(const boost::system::error_code &error, std::size_t bytes_transferred)
	{
		if (!error)
		{
			// 从本地读取数据.
			m_local_socket.async_read_some(boost::asio::buffer(m_local_buffer),
				boost::bind(&socks_session::socks_handle_local_read, shared_from_this(),
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);
		}
		else
		{
			if (error == boost::asio::error::eof)
				close();
			else
				close(true, true);
		}
	}

	void socks_handle_udp_read(int buf_index, const boost::system::error_code &error, std::size_t bytes_transferred)
	{
		if (!error)
		{
			// 更新计时.
			m_meter = boost::posix_time::second_clock::local_time();

			// 解析数据.
			recv_buffer& recv_buf = m_recv_buffers[buf_index];
			boost::array<char, 2048>& buf = recv_buf.buffer;

			char *p = buf.data();

			// 这里进行数据转发, 如果是client发过来的数据, 则解析协议包装.
			if (recv_buf.endp == m_client_endpoint)
			{
				// 解析协议.
				//  +----+------+------+----------+----------+----------+
				//  |RSV | FRAG | ATYP | DST.ADDR | DST.PORT  |   DATA   |
				//  +----+------+------+----------+----------+----------+
				//  | 2  |  1   |  1   | Variable |    2      | Variable |
				//  +----+------+------+----------+----------+----------+
				udp::endpoint endp;

				do {
					// 字节支持小于24.
					if (bytes_transferred < 24)
						break;

					// 不是协议中的数据.
					if (read_int16(p) != 0 || read_int8(p) != 0)
						break;

					// 远程主机IP类型.
					boost::int8_t atyp = read_int8(p);
					if (atyp != 0x01 && atyp != 0x04)
						break;

					// 目标主机IP.
					boost::uint32_t ip = read_uint32(p);
					if (ip == 0)
						break;
					endp.address(boost::asio::ip::address_v4(ip));

					// 读取端口号.
					boost::uint16_t port = read_uint16(p);
					if (port == 0)
						break;
					endp.port(port);

					// 这时的指针p是指向数据了(2 + 1 + 1 + 4 + 2 = 10).
					std::string response(p, bytes_transferred - 10);

					// 转发到指定的endpoint.
					do_write(response, endp);
				} while (false);

				// 继续读取下一组udp数据.
				m_udp_socket.async_receive_from(boost::asio::buffer(buf), recv_buf.endp,
					boost::bind(&socks_session::socks_handle_udp_read, shared_from_this(),
						buf_index,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);
				return;
			}

			// 转发至客户端, 需要添加头协议.
			std::string response;
			response.resize(bytes_transferred + 10);
			char* wp = (char*)response.data();

			// 添加头信息.
			write_uint16(0, wp);	// RSV.
			write_uint8(0, wp);		// FRAG.
			write_uint8(1, wp);		// ATYP.
			write_uint32(recv_buf.endp.address().to_v4().to_ulong(), wp);	// ADDR.
			write_uint16(recv_buf.endp.port(), wp);	// PORT.
			std::memcpy(wp, p, bytes_transferred);	// DATA.

			// 转发数据.
			do_write(response, m_client_endpoint);

			// 继续读取下一组udp数据.
			m_udp_socket.async_receive_from(boost::asio::buffer(buf), recv_buf.endp,
				boost::bind(&socks_session::socks_handle_udp_read, shared_from_this(),
					buf_index,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);
		}
		else
		{
			close();
		}
	}

	void socks_handle_udp_write(const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		if (error)
			return;

		// 更新计时.
		m_meter = boost::posix_time::second_clock::local_time();

		// 弹出已经发送过的数据包.
		m_send_buffers.pop_front();
		if (!m_send_buffers.empty())
		{
			// 继续发送下一个数据包.
			m_udp_socket.async_send_to(
				boost::asio::buffer(m_send_buffers.front().buffer.data(),
					m_send_buffers.front().buffer.size()),
				m_send_buffers.front().endp,
				boost::bind(&socks_session::socks_handle_udp_write,
					this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);
		}
	}

	void socks_udp_timer_handle(const boost::system::error_code& error)
	{
		// 出错失败返回.
		if (error)
			return;

		// 超时关闭.
		if (boost::posix_time::second_clock::local_time() - m_meter >= boost::posix_time::minutes(1))
		{
			close();
			return;
		}

		// 启动定时器.
		m_udp_timer.expires_from_now(boost::posix_time::seconds(1));
		m_udp_timer.async_wait(
			boost::bind(&socks_session::socks_udp_timer_handle,
				shared_from_this(),
				boost::asio::placeholders::error
			)
		);
	}

	void do_write(const std::string& msg, const udp::endpoint& endp)
	{
		bool write_in_progress = !m_send_buffers.empty();
		if (write_in_progress)
		{
			// 超出队列大小,udp包可直接丢掉,这是符合udp协议的.
			if (m_send_buffers.size() > MAX_SEND_BUFFER_SIZE)
				return;
		}
		// 保存到发送队列.
		send_buffer buf;
		buf.buffer = msg;
		buf.endp = endp;
		m_send_buffers.push_back(buf);
		if (!write_in_progress)
		{
			m_udp_socket.async_send_to(
				boost::asio::buffer(m_send_buffers.front().buffer.data(),
					m_send_buffers.front().buffer.size()),
				m_send_buffers.front().endp,
				boost::bind(&socks_session::socks_handle_udp_write,
					this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred
				)
			);
		}
	}

	void close(bool local_force_close = false, bool remote_force_close = false)
	{
		boost::system::error_code ignored_ec;
		// 远程和本地链接都将关闭.
		if (m_local_socket.is_open())
		{
			if (local_force_close)
				m_local_socket.close(ignored_ec);
			else
				m_local_socket.shutdown(
					boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
		}
		if (m_remote_socket.is_open())
		{
			if (remote_force_close)
				m_remote_socket.close(ignored_ec);
			else
				m_remote_socket.shutdown(
					boost::asio::ip::tcp::socket::shutdown_both, ignored_ec);
		}
		m_udp_timer.cancel(ignored_ec);
		if (m_udp_socket.is_open())
		{
			m_udp_socket.close(ignored_ec);
		}
	}

	static enum v7_err js_print(struct v7 *v7, v7_val_t *res)
	{
		auto argc = v7_argc(v7);
		for (auto i = 0; i < argc; i++)
		{
			v7_val_t param = v7_arg(v7, i);
			char buf[4096], *p;
			p = v7_stringify(v7, param, buf, sizeof(buf), V7_STRINGIFY_DEFAULT);
			if (p)
				std::cout << std::string(p);
			if (p != buf && p)
				free(p);
		}
		std::cout << "\n";
		return V7_OK;
	}

	bool do_auth_js(struct v7 *v7, const std::string& client,
		const std::string& name, const std::string passwd)
	{
		v7_val_t func, result, args;

		std::string func_name = "do_auth";
		if (m_version == SOCKS_VERSION_4)
			func_name = "do_auth4";

		func = v7_get(v7, v7_get_global(v7), func_name.c_str(), func_name.size());
		args = v7_mk_array(v7);
		v7_array_push(v7, args, v7_mk_string(v7, client.c_str(), client.size(), 0));
		v7_array_push(v7, args, v7_mk_string(v7, name.c_str(), name.size(), 0));
		if (m_version != SOCKS_VERSION_4)
			v7_array_push(v7, args, v7_mk_string(v7, passwd.c_str(), passwd.size(), 0));
		auto ok = v7_apply(v7, func, v7_mk_undefined(), args, &result);
		if (ok == V7_OK)
			return v7_get_bool(v7, result);
		else
			v7_print_error(stderr, v7, func_name.c_str(), result);

		return false;
	}

	bool do_auth(const std::string& client, const std::string& name, const std::string passwd)
	{
		v7_val_t result;
		bool auth = false;

		struct v7 *v7 = v7_create();
		v7_set_method(v7, v7_get_global(v7), "print", &js_print);

		std::ifstream file("do_auth.js");
		std::string str((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		if (!str.empty())
		{
			auto rcode = v7_exec(v7, str.c_str(), &result);
			if (rcode != V7_OK)
				v7_print_error(stderr, v7, "", result);
			else
				auth = do_auth_js(v7, client, name, passwd);
		}

		v7_destroy(v7);
		return auth;
	}

private:
	boost::asio::io_service &m_io_service;
	tcp::socket m_local_socket;
	boost::array<char, 2048> m_local_buffer;
	tcp::socket m_remote_socket;
	boost::array<char, 2048> m_remote_buffer;
	boost::asio::streambuf m_streambuf;
	udp::socket m_udp_socket;
	udp::endpoint m_client_endpoint;
	// 数据接收缓冲.
	struct recv_buffer
	{
		udp::endpoint endp;
		boost::array<char, 2048> buffer;
	};
	std::map<int, recv_buffer> m_recv_buffers;
	// 用作数据包发送缓冲.
	struct send_buffer
	{
		udp::endpoint endp;
		std::string buffer;
	};
	std::deque<send_buffer> m_send_buffers;
	tcp::resolver m_resolver;
	int m_version;
	int m_method;
	std::string m_uname;
	std::string m_passwd;
	int m_command;
	int m_atyp;
	tcp::endpoint m_address;
	std::string m_domain;
	short m_port;
	boost::tribool m_verify_passed;
	boost::asio::deadline_timer m_udp_timer;
	boost::posix_time::ptime m_meter;
};

class socks_server : public boost::noncopyable
{
public:
	socks_server(boost::asio::io_service &io, unsigned short port, std::string address = "127.0.0.1")
		: m_io_service(io)
		, m_acceptor(io, tcp::endpoint(boost::asio::ip::address::from_string(address), port))
	{
		for (int i = 0; i < 32; i++)
		{
			boost::shared_ptr<socks_session> new_session(new socks_session(m_io_service));
			m_acceptor.async_accept(new_session->socket(),
				boost::bind(&socks_server::handle_accept, this, new_session,
				boost::asio::placeholders::error));
		}
	}
	~socks_server() = default;

public:

	void handle_accept(boost::shared_ptr<socks_session> new_session,
		const boost::system::error_code& error)
	{
		new_session->start();
		new_session.reset(new socks_session(m_io_service));
		m_acceptor.async_accept(new_session->socket(),
			boost::bind(&socks_server::handle_accept, this, new_session,
			boost::asio::placeholders::error));
	}

private:
	boost::asio::io_service &m_io_service;
	tcp::acceptor m_acceptor;
};

} // namespace socks

#endif // SOCKS_SERVER_HPP
