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

#include "proxy/http_proxy_client.hpp"
#include "proxy/logging.hpp"
#include "proxy/proxy_util.hpp"
#include "proxy/socks_client.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/functional/hash.hpp>

#include <cstring>
#include <random>

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

		// 写入大端序 16 位整数.
		inline void write_be16(char* p, uint16_t v) noexcept
		{
			p[0] = static_cast<char>(v >> 8);
			p[1] = static_cast<char>(v & 0xff);
		}

		// 读取大端序 32 位整数.
		inline uint32_t read_be32(const char* p) noexcept
		{
			return (static_cast<uint32_t>(static_cast<uint8_t>(p[0])) << 24) |
				(static_cast<uint32_t>(static_cast<uint8_t>(p[1])) << 16) |
				(static_cast<uint32_t>(static_cast<uint8_t>(p[2])) << 8) |
				static_cast<uint32_t>(static_cast<uint8_t>(p[3]));
		}

		// 写入大端序 32 位整数.
		inline void write_be32(char* p, uint32_t v) noexcept
		{
			p[0] = static_cast<char>(v >> 24);
			p[1] = static_cast<char>((v >> 16) & 0xff);
			p[2] = static_cast<char>((v >> 8) & 0xff);
			p[3] = static_cast<char>(v & 0xff);
		}

		// 计算校验和（RFC 1071）.
		uint16_t checksum(const char* data, size_t len, uint32_t sum = 0) noexcept
		{
			while (len > 1)
			{
				sum += read_be16(data);
				data += 2;
				len -= 2;
			}
			if (len)
				sum += static_cast<uint8_t>(*data);

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

		// 构造 TCP 段并封装为 IPv4 包（含 TCP 校验和）.
		// src/dst 必须为 IPv4 地址；with_mss 非 0 时在 TCP 头附加 MSS 选项.
		std::string build_tcp_segment(
			const net::ip::address& src, const net::ip::address& dst,
			uint16_t src_port, uint16_t dst_port,
			uint32_t seq, uint32_t ack, uint8_t flags,
			const char* payload, size_t payload_len,
			uint16_t with_mss = 0) noexcept
		{
			if (!src.is_v4() || !dst.is_v4())
				return {};

			size_t tcp_hdr_len = 20 + (with_mss ? 4 : 0);
			std::string tcp(tcp_hdr_len + payload_len, '\0');
			char* p = tcp.data();

			write_be16(p, src_port);
			write_be16(p + 2, dst_port);
			write_be32(p + 4, seq);
			write_be32(p + 8, ack);
			p[12] = static_cast<char>((tcp_hdr_len / 4) << 4);
			p[13] = static_cast<char>(flags);
			write_be16(p + 14, 0xffff);  // window.
			write_be16(p + 18, 0);       // urgent pointer.

			if (with_mss)
			{
				p[20] = 2;              // kind.
				p[21] = 4;              // length.
				write_be16(p + 22, with_mss);
			}

			if (payload_len)
				std::memcpy(p + tcp_hdr_len, payload, payload_len);

			auto s4 = src.to_v4().to_bytes();
			auto d4 = dst.to_v4().to_bytes();

			// 计算 TCP 校验和（含 IPv4 伪头）.
			std::string pseudo;
			pseudo.reserve(12 + tcp.size());
			pseudo.append(reinterpret_cast<const char*>(s4.data()), 4);
			pseudo.append(reinterpret_cast<const char*>(d4.data()), 4);
			pseudo.push_back(0);
			pseudo.push_back(ip_proto_tcp);
			pseudo.push_back(0);  // TCP 长度占位.
			pseudo.push_back(0);
			write_be16(pseudo.data() + 10, static_cast<uint16_t>(tcp.size()));
			pseudo.append(tcp);

			uint16_t sum = checksum(pseudo.data(), pseudo.size());
			write_be16(p + 16, sum);

			return build_ip_packet(src, dst, ip_proto_tcp, tcp);
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

			// 分片包（fragment offset 非 0 或 MF 置位）直接丢弃.
			// 注意 flags 中的 DF（不分片）位不影响判断.
			uint16_t frag = read_be16(data + 6);
			if ((frag & 0x1fff) != 0 || (frag & 0x2000) != 0)
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

		uint16_t sum = checksum(p, 20);
		p[10] = static_cast<char>(sum >> 8);
		p[11] = static_cast<char>(sum & 0xff);

		if (!payload.empty())
			std::memcpy(p + 20, payload.data(), payload.size());

		return out;
	}

	//////////////////////////////////////////////////////////////////////////
	// tun_tcp_flow

#if defined(__linux__)

	tun_tcp_flow::tun_tcp_flow(net::any_io_executor executor,
		const std::shared_ptr<tun_server>& owner,
		const proxy_server_option& opt,
		tcp_flow_key key,
		const ip_packet& syn)
		: m_executor(std::move(executor))
		, m_owner(owner)
		, m_option(opt)
		, m_key(std::move(key))
		, m_upstream(init_proxy_stream(m_executor))
		, m_client_isn(syn.seq)
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		m_server_isn = gen();

		m_client_next_seq = m_client_isn + 1;
		m_client_ack_seq = m_client_isn + 1;
		m_server_next_seq = m_server_isn;

		m_tx_signal.emplace(m_executor);
	}

	tun_tcp_flow::~tun_tcp_flow()
	{
		close();
	}

	void tun_tcp_flow::start()
	{
		auto self = shared_from_this();

		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				co_await do_connect();
				co_return;
			}, net::detached);
	}

	void tun_tcp_flow::handle_packet(const ip_packet& pkt)
	{
		if (m_closed)
			return;

		auto flags = pkt.flags;

		if (flags & tcp_flag_rst)
		{
			close();
			return;
		}

		if (flags & tcp_flag_fin)
		{
			m_client_fin = true;

			if (pkt.payload_len > 0 && m_connected && pkt.seq == m_client_next_seq)
			{
				m_client_next_seq += static_cast<uint32_t>(pkt.payload_len);
				m_client_ack_seq = m_client_next_seq;
				push_tx(std::string(pkt.payload, pkt.payload_len));
			}

			// FIN 消耗一个序号.
			m_client_ack_seq = m_client_next_seq + 1;

			// 通知发送协程在队列清空后半关闭上游写方向.
			{
				std::lock_guard<std::mutex> lk(m_tx_mutex);
				m_tx_fin = true;
			}
			if (m_tx_signal)
				m_tx_signal->cancel();

			if (m_connected)
				send_ack();

			// 上游已关闭时，客户端 FIN 到达则完成四路挥手.
			if (m_upstream_eof)
				close();

			return;
		}

		if (!m_connected)
			return;

		if (pkt.payload_len > 0)
		{
			if (pkt.seq != m_client_next_seq)
			{
				// 乱序或重传：通告期望序号，丢弃数据（简化实现不做缓存重排）.
				if (pkt.seq > m_client_next_seq)
					send_ack();
				return;
			}

			m_client_next_seq += static_cast<uint32_t>(pkt.payload_len);
			m_client_ack_seq = m_client_next_seq;

			push_tx(std::string(pkt.payload, pkt.payload_len));
			send_ack();
		}
	}

	void tun_tcp_flow::close()
	{
		if (m_closed)
			return;

		m_closed = true;

		boost::system::error_code ec;
		net_tcp_socket(m_upstream).close(ec);

		if (m_tx_signal)
			m_tx_signal->cancel();

		if (m_owner)
			m_owner->remove_tcp_flow(m_key);
	}

	net::awaitable<void> tun_tcp_flow::do_connect()
	{
		auto self = shared_from_this();
		boost::system::error_code ec;

		const std::string target_host = m_key.dst.to_string();
		const uint16_t target_port = m_key.dst_port;

		// 分流判定：命中 proxy_cidr_ 走上游代理，否则直连.
		const bool use_proxy = m_owner && m_owner->cidr_match(m_key.dst);

		XLOG_DBG << "tun tcp connect " << target_host << ":" << target_port
			<< (use_proxy ? " via proxy" : " direct")
			<< ", cidr_size=" << m_option.proxy_cidr_.size();

		if (use_proxy && m_option.proxy_pass_)
		{
			const auto& proxy_url = *m_option.proxy_pass_;

			// 解析上游代理服务器地址.
			std::string proxy_host(proxy_url.encoded_host());
			std::string port_str = proxy_url.port();
			uint16_t proxy_port = static_cast<uint16_t>(
				port_str.empty() ? 1080 : std::atoi(port_str.c_str()));

			tcp::resolver resolver(m_executor);
			auto targets = co_await resolver.async_resolve(
				proxy_host, std::to_string(proxy_port), net_awaitable[ec]);
			if (ec || targets.empty())
			{
				XLOG_WARN << "tun resolve proxy_pass: " << proxy_host
					<< ", error: " << ec.message();
				send_rst();
				close();
				co_return;
			}

			// 依次尝试连接.
			tcp::socket sock(m_executor);
			bool connected = false;
			for (const auto& t : targets)
			{
				sock = tcp::socket(m_executor);
				sock.open(t.endpoint().protocol(), ec);
				if (ec)
					continue;

				// 设置 SO_MARK（配合策略路由排除代理自身流量，防止环路）.
				if (m_option.so_mark_)
				{
					auto ret = apply_so_mark(sock.native_handle(), m_option.so_mark_);
					if (ret.has_error())
						XLOG_WARN << "tun set socket mark: " << ret.error().message();
				}

				co_await sock.async_connect(t.endpoint(), net_awaitable[ec]);
				if (!ec)
				{
					connected = true;
					break;
				}
			}

			if (!connected)
			{
				XLOG_WARN << "tun connect proxy_pass: " << proxy_host
					<< ", error: " << ec.message();
				send_rst();
				close();
				co_return;
			}

			m_upstream = init_proxy_stream(std::move(sock));

			auto scheme = boost::to_lower_copy(std::string(proxy_url.scheme()));

			if (scheme.starts_with("socks"))
			{
				socks_client_option opt;
				opt.target_host = target_host;
				opt.target_port = target_port;
				opt.proxy_hostname = true;
				opt.username = std::string(proxy_url.user());
				opt.password = std::string(proxy_url.password());

				co_await async_socks_handshake(m_upstream, opt, net_awaitable[ec]);
			}
			else if (scheme.starts_with("http"))
			{
				http_proxy_client_option opt;
				opt.target_host = target_host;
				opt.target_port = target_port;
				opt.username = std::string(proxy_url.user());
				opt.password = std::string(proxy_url.password());

				co_await async_http_proxy_handshake(m_upstream, opt, net_awaitable[ec]);
			}
			else
			{
				XLOG_WARN << "tun unsupported proxy_pass scheme: " << scheme;
				send_rst();
				close();
				co_return;
			}

			if (ec)
			{
				XLOG_WARN << "tun proxy_pass handshake: " << proxy_host
					<< ", error: " << ec.message();
				send_rst();
				close();
				co_return;
			}
		}
		else
		{
			// 直连目标.
			tcp::socket sock(m_executor);
			tcp::endpoint endp(net::ip::make_address(target_host), target_port);

			sock.open(endp.protocol(), ec);
			if (!ec)
			{
				// 设置 SO_MARK（配合策略路由排除代理自身流量，防止环路）.
				if (m_option.so_mark_)
				{
					auto ret = apply_so_mark(sock.native_handle(), m_option.so_mark_);
					if (ret.has_error())
						XLOG_WARN << "tun set socket mark: " << ret.error().message();
				}

				co_await sock.async_connect(endp, net_awaitable[ec]);
			}
			if (ec)
			{
				send_rst();
				close();
				co_return;
			}

			m_upstream = init_proxy_stream(std::move(sock));
		}

		m_connected = true;

		XLOG_DBG << "tun tcp established " << target_host << ":" << target_port;

		// 回 SYN-ACK，通告 MSS 以减少分片.
		uint16_t mss = static_cast<uint16_t>(
			std::max(536, m_option.tun_mtu_ > 0 ? m_option.tun_mtu_ - 40 : 1460));
		send_tcp(m_server_isn, m_client_isn + 1,
			tcp_flag_syn | tcp_flag_ack, nullptr, 0, true, mss);

		// SYN 消耗一个序号，后续数据段从 server_isn + 1 开始.
		m_server_next_seq = m_server_isn + 1;

		// 启动双向数据搬运协程.
		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				co_await tx_loop();
				co_return;
			}, net::detached);

		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				co_await rx_loop();
				co_return;
			}, net::detached);
	}

	net::awaitable<void> tun_tcp_flow::tx_loop()
	{
		for (; !m_closed;)
		{
			std::string data;
			bool fin = false;

			{
				std::lock_guard<std::mutex> lk(m_tx_mutex);
				if (!m_tx_queue.empty())
				{
					data = std::move(m_tx_queue.front());
					m_tx_queue.pop_front();
				}
				fin = m_tx_fin && m_tx_queue.empty();
			}

			if (data.empty() && !fin)
			{
				boost::system::error_code ec;
				m_tx_signal->expires_at(net::steady_timer::time_point::max());
				co_await m_tx_signal->async_wait(net_awaitable[ec]);
				continue;
			}

			if (!data.empty())
			{
				boost::system::error_code ec;
				co_await net::async_write(m_upstream, net::buffer(data), net_awaitable[ec]);
				if (ec)
				{
					on_upstream_closed();
					co_return;
				}
			}

			if (fin)
			{
				// 客户端已发送 FIN，半关闭上游写方向（只执行一次）.
				m_tx_fin = false;

				boost::system::error_code ec;
				net_tcp_socket(m_upstream).shutdown(
					tcp::socket::shutdown_send, ec);
				if (ec)
				{
					on_upstream_closed();
					co_return;
				}
			}
		}

		co_return;
	}

	net::awaitable<void> tun_tcp_flow::rx_loop()
	{
		uint16_t mss = static_cast<uint16_t>(
			std::max(536, m_option.tun_mtu_ > 0 ? m_option.tun_mtu_ - 40 : 1460));

		char buffer[8192];

		for (; !m_closed;)
		{
			boost::system::error_code ec;
			size_t n = co_await m_upstream.async_read_some(
				net::buffer(buffer), net_awaitable[ec]);

			if (ec || n == 0)
			{
				if (ec == net::error::eof || n == 0)
				{
					// 上游关闭：向客户端发送 FIN.
					if (!m_upstream_eof)
					{
						m_upstream_eof = true;
						send_fin();
					}
					if (m_client_fin)
						close();
				}
				else
				{
					// 异常：直接关闭.
					close();
				}
				co_return;
			}

			// 按 MSS 切片发送给客户端.
			size_t off = 0;
			while (off < n)
			{
				size_t chunk = (std::min)(n - off, static_cast<size_t>(mss));
				send_tcp(m_server_next_seq, m_client_ack_seq,
					tcp_flag_ack | tcp_flag_psh,
					buffer + off, chunk);
				m_server_next_seq += static_cast<uint32_t>(chunk);
				off += chunk;
			}
		}

		co_return;
	}

	void tun_tcp_flow::send_tcp(uint32_t seq, uint32_t ack, uint8_t flags,
		const char* payload, size_t payload_len, bool with_mss, uint16_t mss)
	{
		if (!m_owner)
			return;

		auto packet = build_tcp_segment(
			m_key.dst, m_key.src,
			m_key.dst_port, m_key.src_port,
			seq, ack, flags, payload, payload_len,
			with_mss ? mss : 0);

		if (!packet.empty())
			m_owner->write_packet(std::move(packet));
	}

	void tun_tcp_flow::send_ack()
	{
		send_tcp(m_server_next_seq, m_client_ack_seq, tcp_flag_ack, nullptr, 0);
	}

	void tun_tcp_flow::send_fin()
	{
		send_tcp(m_server_next_seq, m_client_ack_seq,
			tcp_flag_fin | tcp_flag_ack, nullptr, 0);
		m_server_next_seq += 1;
	}

	void tun_tcp_flow::send_rst()
	{
		send_tcp(m_server_next_seq, m_client_ack_seq, tcp_flag_rst | tcp_flag_ack,
			nullptr, 0);
	}

	void tun_tcp_flow::push_tx(std::string data)
	{
		if (m_closed)
			return;

		{
			std::lock_guard<std::mutex> lk(m_tx_mutex);
			m_tx_queue.push_back(std::move(data));
		}

		if (m_tx_signal)
			m_tx_signal->cancel();
	}

	void tun_tcp_flow::on_upstream_closed()
	{
		if (m_closed)
			return;

		// 上游异常关闭：向客户端发送 RST 并关闭.
		send_rst();
		close();
	}

	//////////////////////////////////////////////////////////////////////////
	// tun_server

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

		// 关闭所有 TCP flow.
		{
			std::lock_guard<std::mutex> lk(m_flows_mutex);
			m_tcp_flows.clear();
		}

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
		else
			XLOG_DBG << "tun proto " << static_cast<int>(pkt.proto);
	}

	void tun_server::write_packet(std::string packet)
	{
		if (m_abort || !m_stream)
			return;

		bool need_start = false;

		{
			std::lock_guard<std::mutex> lk(m_write_mutex);
			m_write_queue.push_back(std::move(packet));
			need_start = !m_writing;
			if (need_start)
				m_writing = true;
		}

		if (need_start)
		{
			auto self = shared_from_this();
			net::post(m_executor,
				[this, self]() mutable
				{
					do_write();
				});
		}
	}

	void tun_server::do_write()
	{
		if (m_abort || !m_stream)
		{
			std::lock_guard<std::mutex> lk(m_write_mutex);
			m_writing = false;
			return;
		}

		auto buf = std::make_shared<std::string>();

		{
			std::lock_guard<std::mutex> lk(m_write_mutex);
			if (m_write_queue.empty())
			{
				m_writing = false;
				return;
			}
			*buf = std::move(m_write_queue.front());
			m_write_queue.pop_front();
		}

		auto self = shared_from_this();
		m_stream->async_write_some(net::buffer(*buf),
			[this, self, buf](const boost::system::error_code& ec, size_t)
			{
				if (ec)
				{
					XLOG_WARN << "tun write: " << ec.message();

					// 写失败后复位写状态，避免后续包永久卡在队列.
					std::lock_guard<std::mutex> lk(m_write_mutex);
					m_writing = false;
					return;
				}
				do_write();
			});
	}

	void tun_server::handle_tcp_packet(ip_packet& pkt) noexcept
	{
		// 目前仅支持 IPv4 转发.
		if (!pkt.src.is_v4() || !pkt.dst.is_v4())
			return;

		tcp_flow_key key{
			pkt.src,
			pkt.src_port,
			pkt.dst,
			pkt.dst_port
		};

		std::shared_ptr<tun_tcp_flow> flow;
		bool created = false;

		{
			std::lock_guard<std::mutex> lk(m_flows_mutex);
			auto it = m_tcp_flows.find(key);
			if (it != m_tcp_flows.end())
			{
				flow = it->second;
			}
			else
			{
				if (!(pkt.flags & tcp_flag_syn))
					return;

				flow = std::make_shared<tun_tcp_flow>(
					m_executor, shared_from_this(), m_option, key, pkt);
				m_tcp_flows.emplace(key, flow);
				created = true;
			}
		}

		flow->handle_packet(pkt);

		// 新创建的 flow 启动建连（SYN 重传不重复建连）.
		if (created)
			flow->start();
	}

	void tun_server::remove_tcp_flow(const tcp_flow_key& key)
	{
		std::lock_guard<std::mutex> lk(m_flows_mutex);
		m_tcp_flows.erase(key);
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
					if ((addr.to_v4().to_uint() & netaddr4.netmask().to_uint()) ==
						netaddr4.network().to_uint())
						return true;
					continue;
				}

				ec.clear();
				auto netaddr6 = net::ip::make_network_v6(ip_cidr, ec);
				if (!ec)
				{
					auto net6 = net::ip::make_network_v6(addr.to_v6(),
						netaddr6.prefix_length());
					if (net6.canonical() == netaddr6.canonical())
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
