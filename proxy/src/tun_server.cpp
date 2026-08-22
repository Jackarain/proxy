//
// tun_server.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/tun_server.hpp"

#include "proxy/logging.hpp"

#include <cstring>

#if defined(__linux__)
# include <fcntl.h>
# include <linux/if.h>
# include <linux/if_tun.h>
# include <netinet/in.h>
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <unistd.h>
#endif

namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// tun_device

#if defined(__linux__)

	tun_device::~tun_device()
	{
		close();
	}

	boost::system::error_code tun_device::open(const std::string& name, int mtu) noexcept
	{
		close();

		boost::system::error_code ec;

		m_fd = ::open("/dev/net/tun", O_RDWR | O_CLOEXEC);
		if (m_fd < 0)
		{
			ec = boost::system::error_code(errno, boost::system::generic_category());
			return ec;
		}

		struct ifreq ifr {};
		ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
		if (!name.empty())
			std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);

		if (::ioctl(m_fd, TUNSETIFF, &ifr) < 0)
		{
			ec = boost::system::error_code(errno, boost::system::generic_category());
			close();
			return ec;
		}

		m_name = ifr.ifr_name;

		if (mtu > 0)
		{
			int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
			if (sock >= 0)
			{
				struct ifreq mtu_ifr {};
				std::strncpy(mtu_ifr.ifr_name, m_name.c_str(), IFNAMSIZ - 1);
				mtu_ifr.ifr_mtu = mtu;
				if (::ioctl(sock, SIOCSIFMTU, &mtu_ifr) < 0)
					ec = boost::system::error_code(errno, boost::system::generic_category());
				::close(sock);
			}
		}

		m_mtu = mtu > 0 ? mtu : 1500;

		return ec;
	}

	void tun_device::close() noexcept
	{
		if (m_fd >= 0)
		{
			::close(m_fd);
			m_fd = -1;
		}
		m_name.clear();
	}

#endif // defined(__linux__)

	//////////////////////////////////////////////////////////////////////////
	// IP 包解析

	namespace {

		// 读取大端序 16 位整数.
		inline uint16_t read_be16(const char* p) noexcept
		{
			return static_cast<uint16_t>(
				(static_cast<uint8_t>(p[0]) << 8) | static_cast<uint8_t>(p[1]));
		}

		// 读取大端序 32 位整数.
		inline uint32_t read_be32(const char* p) noexcept
		{
			return (static_cast<uint32_t>(static_cast<uint8_t>(p[0])) << 24) |
				(static_cast<uint32_t>(static_cast<uint8_t>(p[1])) << 16) |
				(static_cast<uint32_t>(static_cast<uint8_t>(p[2])) << 8) |
				static_cast<uint32_t>(static_cast<uint8_t>(p[3]));
		}

		// 计算 IPv4 头校验和.
		uint16_t ipv4_checksum(const char* data, size_t len) noexcept
		{
			uint32_t sum = 0;
			const uint16_t* p = reinterpret_cast<const uint16_t*>(data);
			while (len > 1)
			{
				sum += *p++;
				len -= 2;
			}
			if (len)
				sum += static_cast<uint8_t>(*reinterpret_cast<const char*>(p));

			while (sum >> 16)
				sum = (sum & 0xffff) + (sum >> 16);

			return static_cast<uint16_t>(~sum);
		}

		// 从 IPv6 头部偏移处跳过扩展头, 返回传输层协议号与偏移.
		// 分片/认证/封装安全载荷等不支持的头返回 false.
		bool ipv6_next_header(const char* data, size_t len,
			size_t& off, uint8_t& proto) noexcept
		{
			off = 40;
			proto = static_cast<uint8_t>(data[6]);

			for (;;)
			{
				switch (proto)
				{
				case 0:    // Hop-by-Hop
				case 43:   // Routing
				case 60:   // Destination Options
					if (off + 2 > len)
						return false;
					{
						uint8_t next = static_cast<uint8_t>(data[off]);
						uint8_t hdr_ext_len = static_cast<uint8_t>(data[off + 1]);
						off += (static_cast<size_t>(hdr_ext_len) + 1) * 8;
						if (off > len)
							return false;
						proto = next;
					}
					break;
				case 44:   // Fragment
				case 51:   // AH
				case 50:   // ESP
					return false;
				default:
					return true;
				}
			}
		}

	} // namespace

	bool parse_ip_packet(const char* data, size_t len, ip_packet& pkt) noexcept
	{
		if (!data || len < 20)
			return false;

		uint8_t version = static_cast<uint8_t>(data[0]) >> 4;
		size_t l4_off = 0;
		size_t total_len = 0;

		if (version == 4)
		{
			uint8_t ihl = (static_cast<uint8_t>(data[0]) & 0x0f) * 4;
			if (ihl < 20 || len < ihl)
				return false;

			total_len = read_be16(data + 2);
			if (total_len < ihl)
				return false;
			if (total_len > len)
				total_len = len;

			// 分片包（非首片或 MF 置位）直接丢弃.
			uint16_t frag = read_be16(data + 6);
			if (frag != 0)
				return false;

			pkt.proto = static_cast<uint8_t>(data[9]);
			pkt.src = net::ip::make_address_v4(
				net::ip::address_v4::bytes_type{
					static_cast<uint8_t>(data[12]),
					static_cast<uint8_t>(data[13]),
					static_cast<uint8_t>(data[14]),
					static_cast<uint8_t>(data[15]) });
			pkt.dst = net::ip::make_address_v4(
				net::ip::address_v4::bytes_type{
					static_cast<uint8_t>(data[16]),
					static_cast<uint8_t>(data[17]),
					static_cast<uint8_t>(data[18]),
					static_cast<uint8_t>(data[19]) });

			l4_off = ihl;
		}
		else if (version == 6)
		{
			if (len < 40)
				return false;

			uint8_t proto = 0;
			size_t off = 0;
			if (!ipv6_next_header(data, len, off, proto))
				return false;

			pkt.proto = proto;
			net::ip::address_v6::bytes_type s6{};
			net::ip::address_v6::bytes_type d6{};
			std::memcpy(s6.data(), data + 8, 16);
			std::memcpy(d6.data(), data + 24, 16);
			pkt.src = net::ip::make_address_v6(s6);
			pkt.dst = net::ip::make_address_v6(d6);

			uint16_t payload_len = read_be16(data + 4);
			total_len = static_cast<size_t>(payload_len) + 40;
			if (total_len > len)
				total_len = len;

			l4_off = off;
		}
		else
		{
			return false;
		}

		if (total_len < l4_off)
			return false;

		const char* l4 = data + l4_off;
		size_t l4_len = total_len - l4_off;

		if (pkt.proto == ip_proto_tcp)
		{
			if (l4_len < 20)
				return false;

			pkt.src_port = read_be16(l4);
			pkt.dst_port = read_be16(l4 + 2);
			pkt.seq = read_be32(l4 + 4);
			pkt.ack = read_be32(l4 + 8);

			uint16_t tcp_hdr_len = (static_cast<uint8_t>(l4[12]) >> 4) * 4;
			if (tcp_hdr_len < 20 || tcp_hdr_len > l4_len)
				return false;

			pkt.tcp_hdr_len = tcp_hdr_len;
			pkt.flags = static_cast<uint8_t>(l4[13]) & 0x3f;
			pkt.payload = l4 + tcp_hdr_len;
			pkt.payload_len = l4_len - tcp_hdr_len;
		}
		else if (pkt.proto == ip_proto_udp)
		{
			if (l4_len < 8)
				return false;

			pkt.src_port = read_be16(l4);
			pkt.dst_port = read_be16(l4 + 2);
			pkt.payload = l4 + 8;
			pkt.payload_len = l4_len - 8;
		}
		else
		{
			return false;
		}

		pkt.raw = data;
		pkt.raw_len = total_len;

		return true;
	}

	std::string build_ip_packet(
		const net::ip::address& src, const net::ip::address& dst,
		uint8_t proto, const std::string& payload) noexcept
	{
		if (!src.is_v4() || !dst.is_v4())
			return {};

		auto s4 = src.to_v4().to_bytes();
		auto d4 = dst.to_v4().to_bytes();

		std::string out;
		out.resize(20 + payload.size());

		char* p = out.data();
		p[0] = 0x45;
		p[1] = 0;

		uint16_t total = static_cast<uint16_t>(out.size());
		p[2] = static_cast<char>(total >> 8);
		p[3] = static_cast<char>(total & 0xff);

		p[4] = p[5] = 0;  // identification.
		p[6] = p[7] = 0;  // flags/fragment offset.
		p[8] = 64;        // TTL.
		p[9] = static_cast<char>(proto);
		p[10] = p[11] = 0; // checksum 占位.

		std::memcpy(p + 12, s4.data(), 4);
		std::memcpy(p + 16, d4.data(), 4);

		uint16_t sum = ipv4_checksum(p, 20);
		p[10] = static_cast<char>(sum >> 8);
		p[11] = static_cast<char>(sum & 0xff);

		if (!payload.empty())
			std::memcpy(p + 20, payload.data(), payload.size());

		return out;
	}

	//////////////////////////////////////////////////////////////////////////
	// tun_server

#if defined(__linux__)

	tun_server::tun_server(net::any_io_executor executor, proxy_server_option opt)
		: m_executor(std::move(executor))
		, m_option(std::move(opt))
		, m_tun(std::make_unique<tun_device>())
	{}

	std::shared_ptr<tun_server>
	tun_server::make(net::any_io_executor executor, proxy_server_option opt)
	{
		return std::shared_ptr<tun_server>(
			new tun_server(std::move(executor), std::move(opt)));
	}

	tun_server::~tun_server()
	{
		close();
	}

	void tun_server::start() noexcept
	{
		if (!m_tun->is_open())
		{
			boost::system::error_code ec = m_tun->open(m_option.tun_name_, m_option.tun_mtu_);
			if (ec)
			{
				XLOG_ERR << "tun open device failed: " << ec.message();
				return;
			}

			XLOG_INFO << "tun device: " << m_tun->name()
				<< ", mtu: " << m_tun->mtu();
		}

		if (!m_stream)
			m_stream.emplace(m_executor, m_tun->native_handle());

		auto self = shared_from_this();
		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				co_await run();
				co_return;
			}, net::detached);
	}

	void tun_server::close() noexcept
	{
		m_abort = true;

		if (m_stream)
		{
			boost::system::error_code ec;
			m_stream->close(ec);
		}

		if (m_tun)
			m_tun->close();
	}

	net::awaitable<void> tun_server::run()
	{
		// TUN 设备一次 read 返回一个完整 IP 包，缓冲取 64K 上限.
		char buffer[65536];

		for (; !m_abort;)
		{
			boost::system::error_code ec;
			size_t n = co_await m_stream->async_read_some(
				net::buffer(buffer), net_awaitable[ec]);
			if (ec)
			{
				XLOG_WARN << "tun read: " << ec.message();
				break;
			}

			handle_packet(buffer, n);
		}

		co_return;
	}

	void tun_server::handle_packet(const char* data, size_t len) noexcept
	{
		ip_packet pkt;

		if (!parse_ip_packet(data, len, pkt))
			return;

		if (pkt.proto == ip_proto_tcp)
			handle_tcp_packet(pkt);
		else if (pkt.proto == ip_proto_udp)
			handle_udp_packet(pkt);
	}

	bool tun_server::cidr_match(const net::ip::address& addr) const noexcept
	{
		boost::system::error_code ec;

		for (const auto& ip_cidr : m_option.proxy_cidr_)
		{
			if (ip_cidr.empty())
				continue;

			try
			{
				auto iponly = net::ip::make_address(ip_cidr, ec);
				if (!ec)
				{
					if (iponly == addr)
						return true;
					continue;
				}

				ec.clear();
				auto netaddr4 = net::ip::make_network_v4(ip_cidr, ec);
				if (!ec)
				{
					auto target = net::ip::make_network_v4(addr.to_v4(), netaddr4.netmask());
					if (target == netaddr4)
						return true;
					continue;
				}

				ec.clear();
				auto netaddr6 = net::ip::make_network_v6(ip_cidr, ec);
				if (!ec)
				{
					auto target = net::ip::make_network_v6(addr.to_v6(), netaddr6.prefix_length());
					if (target == netaddr6)
						return true;
				}
			}
			catch (const std::exception&)
			{}
		}

		return false;
	}

	bool tun_server::domain_match(const std::string& domain) const noexcept
	{
		for (const auto& d : m_option.proxy_domains_)
		{
			if (d.empty() || domain.size() < d.size())
				continue;

			if (domain == d)
				return true;

			if (domain.size() > d.size() &&
				domain.ends_with(d) &&
				domain[domain.size() - d.size() - 1] == '.')
				return true;
		}

		return false;
	}

	void tun_server::handle_tcp_packet(ip_packet& pkt) noexcept
	{
		(void)pkt;
		// TCP 状态机与转发在后续实现.
	}

	void tun_server::handle_udp_packet(ip_packet& pkt) noexcept
	{
		(void)pkt;
		// UDP 会话转发在后续实现.
	}

#else // !defined(__linux__)

	std::shared_ptr<tun_server>
	tun_server::make(net::any_io_executor executor, proxy_server_option opt)
	{
		(void)executor;
		(void)opt;
		return nullptr;
	}

#endif // defined(__linux__)

} // namespace proxy
