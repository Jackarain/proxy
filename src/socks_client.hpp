#ifndef SOCKS_CLIENT_HPP
#define SOCKS_CLIENT_HPP

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

#include <boost/utility/string_view.hpp>

#include <boost/logic/tribool.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/array.hpp>
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/system/system_error.hpp>
#include <boost/system/error_code.hpp>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include "io.hpp"


namespace socks {

	//////////////////////////////////////////////////////////////////////////

	class error_category_impl;

	template<class error_category>
	const boost::system::error_category& error_category_single()
	{
		static error_category error_category_instance;
		return reinterpret_cast<const boost::system::error_category&>(error_category_instance);
	}

	inline const boost::system::error_category& error_category()
	{
		return error_category_single<socks::error_category_impl>();
	}

	namespace errc {
		enum errc_t
		{
			/// SOCKS unsupported version.
			socks_unsupported_version = 1000,

			/// SOCKS username required.
			socks_username_required,

			/// SOCKS unsupported authentication version.
			socks_unsupported_authentication_version,

			/// SOCKS authentication error.
			socks_authentication_error,

			/// SOCKS general failure.
			socks_general_failure,

			/// SOCKS command not supported.
			socks_command_not_supported,

			/// SOCKS no identd running.
			socks_no_identd,

			/// SOCKS no identd running.
			socks_identd_error,
		};

		inline boost::system::error_code make_error_code(errc_t e)
		{
			return boost::system::error_code(static_cast<int>(e), socks::error_category());
		}
	}

	class error_category_impl
		: public boost::system::error_category
	{
		virtual const char* name() const BOOST_SYSTEM_NOEXCEPT
		{
			return "SOCKS";
		}

		virtual std::string message(int e) const
		{
			switch (e)
			{
			case errc::socks_unsupported_version:
				return "SOCKS unsupported version";
			case errc::socks_username_required:
				return "SOCKS username required";
			case errc::socks_unsupported_authentication_version:
				return "SOCKS unsupported authentication version";
			case errc::socks_authentication_error:
				return "SOCKS authentication error";
			case errc::socks_general_failure:
				return "SOCKS general failure";
			case errc::socks_command_not_supported:
				return "SOCKS command not supported";
			case errc::socks_no_identd:
				return "SOCKS no identd running";
			case errc::socks_identd_error:
				return "SOCKS no identd running";
			default:
				return "Unknown PROXY error";
			}
		}
	};
}

namespace boost {
	namespace system {

		template <>
		struct is_error_code_enum<socks::errc::errc_t>
		{
			static const bool value = true;
		};

	} // namespace system
} // namespace boost


namespace socks {

	//////////////////////////////////////////////////////////////////////////

	// 解析uri格式
	// scheme:[//[user[:password]@]host[:port]][/path][?query][#fragment]
	struct socks_address
	{
		std::string scheme;
		std::string host;
		std::string port;
		std::string path;
		std::string query;
		std::string fragment;
		std::string username;
		std::string password;
	};

#define is_scheme_char(c) \
	(isalpha(c) || isdigit(c) || c == '+' || c == '-' || c == '.')

	inline bool parse_url(const std::string& url, socks_address& result)
	{
		// Read scheme
		auto it = std::find(url.begin(), url.end(), ':');
		if (it == url.end())
			return false;

		for (auto first = url.begin(); first != it; first++)
		{
			auto& c = *first;
			if (!is_scheme_char(c))
				return false;
			result.scheme.push_back(std::tolower(c));
		}

		// Skip ':'
		it++;

		// Eat "//"
		for (int i = 0; i < 2; i++)
		{
			if (*it++ != '/')
				return false;
		}

		// Check if the user (and password) are specified.
		auto tmp_it = std::find(it, url.end(), '@');
		if (tmp_it != url.end())
		{
			int userpwd_flag = 0;
			for (auto first = it; first != tmp_it; first++)
			{
				auto& c = *first;
				if (c == ':')
				{
					userpwd_flag = 1;
					continue;
				}
				if (userpwd_flag == 0)
					result.username.push_back(c);
				else if (userpwd_flag == 1)
					result.password.push_back(c);
				else
					return false;
			}

			std::advance(it, tmp_it - it);
			// Eat '@'
			it++;
		}

		bool is_ipv6_address = *it == '[';
		if (is_ipv6_address)
		{
			tmp_it = std::find(it, url.end(), ']');
			if (tmp_it == url.end())
				return false;

			// Eat '['
			it++;

			for (auto first = it; first != tmp_it; first++)
				result.host.push_back(*first);

			// Skip host.
			std::advance(it, tmp_it - it);
			// Eat ']'
			it++;
		}
		else
		{
			tmp_it = std::find(it, url.end(), ':');
			if (tmp_it != url.end())
			{
				for (auto first = it; first != tmp_it; first++)
					result.host.push_back(*first);
			}
			else
			{
				tmp_it = std::find(it, url.end(), '/');
				for (auto first = it; first != tmp_it; first++)
					result.host.push_back(*first);
				if (tmp_it == url.end())
					return true;
			}

			// Skip host.
			std::advance(it, tmp_it - it);
		}

		// Port number is specified.
		if (*it == ':')
		{
			// Eat ':'
			it++;

			tmp_it = std::find(it, url.end(), '/');
			for (auto first = it; first != tmp_it; first++)
			{
				auto& c = *first;
				if (!isdigit(c))
					return false;
				result.port.push_back(*first);
			}
			if (tmp_it == url.end())
				return true;

			// Skip port.
			std::advance(it, tmp_it - it);
		}

		// Parse path.
		tmp_it = std::find(it, url.end(), '?');
		if (tmp_it != url.end())
		{
			// Read path.
			for (auto first = it; first != tmp_it; first++)
				result.path.push_back(*first);

			// Skip path.
			std::advance(it, tmp_it - it);

			// Skip '?'
			it++;
		}
		else
		{
			tmp_it = std::find(it, url.end(), '#');
			if (tmp_it != url.end())
			{
				// Read path.
				for (auto first = it; first != tmp_it; first++)
					result.path.push_back(*first);

				// Skip path.
				std::advance(it, tmp_it - it);

				// Skip '#'
				it++;

				// Read fragment.
				for (auto first = it; first != url.end(); first++)
					result.fragment.push_back(*first);

				return true;
			}
			else
			{
				// Read path.
				for (auto first = it; first != url.end(); first++)
					result.path.push_back(*first);

				return true;
			}
		}

		// Read query.
		tmp_it = std::find(it, url.end(), '#');
		if (tmp_it != url.end())
		{
			// Read query.
			for (auto first = it; first != tmp_it; first++)
				result.query.push_back(*first);

			// Skip query.
			std::advance(it, tmp_it - it);

			// Skip '#'
			it++;

			// Read fragment.
			for (auto first = it; first != url.end(); first++)
				result.fragment.push_back(*first);

			return true;
		}

		// Read query.
		for (auto first = it; first != url.end(); first++)
			result.query.push_back(*first);

		return true;
	}





	//////////////////////////////////////////////////////////////////////////

	using boost::asio::ip::tcp;
	using boost::asio::ip::udp;

	class socks_client
		: public boost::enable_shared_from_this<socks_client>
	{
	public:
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
		explicit socks_client(tcp::socket& socket, bool proxy_hostname = true)
			: m_socket(socket)
			, m_proxy_hostname(true)
		{}

		template <typename Handler>
		bool async_do_proxy(socks_address& content,
			const std::string& address, const std::string& port, Handler handler)
		{
			m_socks_address = content;
			m_address = address;
			m_port = port;

			if (content.scheme == "socks5")
			{
				auto self = shared_from_this();
				boost::asio::spawn(m_socket.get_io_service(), [this, self, handler]
				(boost::asio::yield_context yield)
				{
					do_socks5<Handler>(handler, yield);
				});
			}
			else if (content.scheme == "socks4")
			{
				auto self = shared_from_this();
				boost::asio::spawn(m_socket.get_io_service(), [this, self, handler]
				(boost::asio::yield_context yield)
				{
					do_socks4<Handler>(handler, yield);
				});
			}
			else
			{
				return false;
			}

			return true;
		}

	private:
		template <typename Handler>
		void do_socks5(Handler handler, boost::asio::yield_context yield)
		{
			std::size_t bytes_to_write = m_socks_address.username.empty() ? 3 : 4;
			boost::asio::streambuf request;
			boost::asio::mutable_buffer b = request.prepare(bytes_to_write);
			char* p = boost::asio::buffer_cast<char*>(b);

			write_uint8(5, p); // SOCKS VERSION 5.
			if (m_socks_address.username.empty())
			{
				write_uint8(1, p); // 1 authentication method (no auth)
				write_uint8(0, p); // no authentication
			}
			else
			{
				write_uint8(2, p); // 2 authentication methods
				write_uint8(0, p); // no authentication
				write_uint8(2, p); // username/password
			}

			request.commit(bytes_to_write);

			boost::system::error_code ec;
			boost::asio::async_write(m_socket, request, boost::asio::transfer_exactly(bytes_to_write), yield[ec]);
			if (ec)
			{
				handler(ec);
				return;
			}

			boost::asio::streambuf response;
			boost::asio::async_read(m_socket, response, boost::asio::transfer_exactly(2), yield[ec]);
			if (ec)
			{
				handler(ec);
				return;
			}

			int method;
			bool authed = false;

			{
				int version;

				boost::asio::const_buffer b = response.data();
				const char* p = boost::asio::buffer_cast<const char*>(b);
				version = read_uint8(p);
				method = read_uint8(p);
				if (version != 5)	// 版本不等于5, 不支持socks5.
				{
					ec = socks::errc::socks_unsupported_version;
					handler(ec);
					return;
				}
			}

			if (method == 2)
			{
				if (m_socks_address.username.empty())
				{
					ec = socks::errc::socks_username_required;
					handler(ec);
					return;
				}

				// start sub-negotiation.
				request.consume(request.size());

				std::size_t bytes_to_write = m_socks_address.username.size() + m_socks_address.password.size() + 3;
				boost::asio::mutable_buffer mb = request.prepare(bytes_to_write);
				char* mp = boost::asio::buffer_cast<char*>(mb);

				write_uint8(1, mp);
				write_uint8(static_cast<int8_t>(m_socks_address.username.size()), mp);
				write_string(m_socks_address.username, mp);
				write_uint8(static_cast<int8_t>(m_socks_address.password.size()), mp);
				write_string(m_socks_address.password, mp);
				request.commit(bytes_to_write);

				// 发送用户密码信息.
				boost::asio::async_write(m_socket, request, boost::asio::transfer_exactly(bytes_to_write), yield[ec]);
				if (ec)
				{
					handler(ec);
					return;
				}

				// 读取状态.
				response.consume(response.size());
				boost::asio::async_read(m_socket, response, boost::asio::transfer_exactly(2), yield[ec]);
				if (ec)
				{
					handler(ec);
					return;
				}

				// 读取版本状态.
				boost::asio::const_buffer cb = response.data();
				const char* cp = boost::asio::buffer_cast<const char*>(cb);

				int version = read_uint8(cp);
				int status = read_uint8(cp);

				// 不支持的认证版本.
				if (version != 1)
				{
					ec = errc::socks_unsupported_authentication_version;
					handler(ec);
					return;
				}

				// 认证错误.
				if (status != 0)
				{
					ec = errc::socks_authentication_error;
					handler(ec);
					return;
				}

				authed = true;
			}

			if (method == 0 || authed)
			{
				request.consume(request.size());
				std::size_t bytes_to_write = 7 + m_address.size();
				boost::asio::mutable_buffer mb = request.prepare(std::max<std::size_t>(bytes_to_write, 22));
				char* wp = boost::asio::buffer_cast<char*>(mb);

				// 发送socks5连接命令.
				write_uint8(5, wp); // SOCKS VERSION 5.
				write_uint8(1, wp); // CONNECT command.
				write_uint8(0, wp); // reserved.

				if (m_proxy_hostname)
				{
					write_uint8(3, wp); // atyp, domain name.
					BOOST_ASSERT(m_address.size() <= 255);
					write_uint8(static_cast<int8_t>(m_address.size()), wp);	// domainname size.
					std::copy(m_address.begin(), m_address.end(), wp);		// domainname.
					wp += m_address.size();
					write_uint16(atoi(m_port.c_str()), wp);					// port.
				}
				else
				{
					auto endp = boost::asio::ip::address::from_string(m_address);
					if (endp.is_v4())
					{
						write_uint8(1, wp); // ipv4.
						write_uint32(endp.to_v4().to_ulong(), wp);
						write_uint16(atoi(m_port.c_str()), wp);
						bytes_to_write = 10;
					}
					else
					{
						write_uint8(4, wp); // ipv6.
						auto bytes = endp.to_v6().to_bytes();
						std::copy(bytes.begin(), bytes.end(), wp);
						wp += 16;
						write_uint16(atoi(m_port.c_str()), wp);
						bytes_to_write = 22;
					}
				}

				request.commit(bytes_to_write);
				boost::asio::async_write(m_socket, request, boost::asio::transfer_exactly(bytes_to_write), yield[ec]);
				if (ec)
				{
					handler(ec);
					return;
				}

				std::size_t bytes_to_read = 10;
				response.consume(response.size());
				boost::asio::async_read(m_socket, response,
					boost::asio::transfer_exactly(bytes_to_read), yield[ec]);
				if (ec)
				{
					handler(ec);
					return;
				}
				boost::asio::const_buffer cb = response.data();
				const char* rp = boost::asio::buffer_cast<const char*>(cb);
				int version = read_uint8(rp);
				int resp = read_uint8(rp);
				read_uint8(rp);	// skip RSV.
				int atyp = read_uint8(rp);

				if (atyp == 1) // ADDR.PORT
				{
					m_remote_endp.address(boost::asio::ip::address_v4(read_uint32(rp)));
					m_remote_endp.port(read_uint16(rp));

					std::cout << "* SOCKS remote host: " << m_remote_endp.address().to_string()
						<< ":" << m_remote_endp.port() << "\n";
				}
				else if (atyp == 3) // DOMAIN
				{
					auto domain_length = read_uint8(rp);

					boost::asio::async_read(m_socket, response,
						boost::asio::transfer_exactly(domain_length - 3), yield[ec]);
					if (ec)
					{
						handler(ec);
						return;
					}

					boost::asio::const_buffer cb = response.data();
					rp = boost::asio::buffer_cast<const char*>(cb) + 5;

					std::string domain;
					for (int i = 0; i < domain_length; i++)
						domain.push_back(read_uint8(rp));
					auto port = read_uint16(rp);

					std::cout << "* SOCKS remote host: " << domain << ":" << port << "\n";
				}
				else
				{
					ec = errc::socks_general_failure;
					handler(ec);
					return;
				}

				if (version != 5)
				{
					ec = errc::socks_unsupported_version;
					handler(ec);
					return;
				}

				if (resp != 0)
				{
					ec = errc::socks_general_failure;
					// 得到更详细的错误信息.
					switch (resp)
					{
					case 2: ec = boost::asio::error::no_permission; break;
					case 3: ec = boost::asio::error::network_unreachable; break;
					case 4: ec = boost::asio::error::host_unreachable; break;
					case 5: ec = boost::asio::error::connection_refused; break;
					case 6: ec = boost::asio::error::timed_out; break;
					case 7: ec = errc::socks_command_not_supported; break;
					case 8: ec = boost::asio::error::address_family_not_supported; break;
					}

					handler(ec);
					return;
				}

				ec = boost::system::error_code();	// 没有发生错误, 返回.
				handler(ec);
				return;
			}

			ec = boost::asio::error::address_family_not_supported;
			handler(ec);
			return;
		}

		template <typename Handler>
		void do_socks4(Handler handler, boost::asio::yield_context yield)
		{
			boost::system::error_code ec = errc::socks_unsupported_version;
			handler(ec);
			return;
		}

	private:
		tcp::socket& m_socket;
		bool m_proxy_hostname;
		socks_address m_socks_address;
		std::string m_address;
		std::string m_port;
		tcp::endpoint m_remote_endp;
	};
}

#endif // SOCKS_CLIENT_HPP
