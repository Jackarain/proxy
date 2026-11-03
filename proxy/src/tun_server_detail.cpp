//
// tun_server_detail.cpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tun_server_detail.hpp"
#include "proxy/proxy_websocket_client.hpp"
#include "proxy/dns_server.hpp"
#include "proxy/http_proxy_client.hpp"
#include "proxy/logging.hpp"
#include "proxy/proxy_util.hpp"
#include "proxy/socks_client.hpp"
#include "proxy/ssl_connect.hpp"
#include "proxy/network_order.hpp"

#if defined(__linux__)
# include <arpa/inet.h>
# include <net/if.h>
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstring>

namespace proxy::tun_detail {

		int udp_idle_timeout(const proxy_server_option& opt) noexcept
		{
			return opt.udp_timeout_ > 0 ? opt.udp_timeout_ : 300;
		}

#if defined(__linux__)
		// 查询 tun 设备当前 IPv4 地址（外部 ip addr add 配置），
		// 供环路防护识别本地地址.
		void query_tun_device_ip(const std::string& dev, std::string& ip)
		{
			const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
			if (fd < 0)
				return;
			struct ifreq ifr {};
			std::strncpy(ifr.ifr_name, dev.c_str(), IFNAMSIZ - 1);
			if (::ioctl(fd, SIOCGIFADDR, &ifr) == 0)
			{
				const auto* sin = reinterpret_cast<const sockaddr_in*>(
					&ifr.ifr_addr);
				char buf[INET_ADDRSTRLEN] = {};
				if (::inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf)))
					ip = buf;
			}
			::close(fd);
		}

		// 判断客户端地址是否为 tun 设备自身地址：tun 接管路由后，未绕过
		// tun 的出站连接会以本地地址为源回环进 tun（环路风暴的根源）.
		// 每次调用查询设备地址（ioctl 开销极小），1 秒内缓存避免高频查询.
		bool is_tun_loopback_source(const net::ip::address& addr,
			const std::string& dev) noexcept
		{
			static std::chrono::steady_clock::time_point last;
			static std::string cached;
			const auto now = std::chrono::steady_clock::now();
			if (now - last > std::chrono::seconds(1))
			{
				cached.clear();
				query_tun_device_ip(dev, cached);
				last = now;
			}
			if (cached.empty())
				return false;
			boost::system::error_code ec;
			const auto local = net::ip::make_address(cached, ec);
			return !ec && local == addr;
		}
#endif

		// 构造 SOCKS5 UDP 请求/应答头（RSV + FRAG + ATYP + 地址 + 端口）.
		std::string build_socks5_udp_header(
			const net::ip::udp::endpoint& target)
		{
			std::string header;

			if (target.address().is_v4())
			{
				header.resize(10);
				char* hp = header.data();
				io_util::write<uint16_t>(0, hp);  // RSV.
				*hp++ = 0;                        // FRAG.
				*hp++ = SOCKS5_ATYP_IPV4;
				auto addr = target.address().to_v4().to_bytes();
				std::memcpy(hp, addr.data(), 4);
				hp += 4;
				io_util::write<uint16_t>(target.port(), hp);
			}
			else
			{
				header.resize(22);
				char* hp = header.data();
				io_util::write<uint16_t>(0, hp);  // RSV.
				*hp++ = 0;                        // FRAG.
				*hp++ = SOCKS5_ATYP_IPV6;
				auto addr = target.address().to_v6().to_bytes();
				std::memcpy(hp, addr.data(), 16);
				hp += 16;
				io_util::write<uint16_t>(target.port(), hp);
			}

			return header;
		}

		// 解析 SOCKS5 UDP 应答头，返回头部长度；非法包头返回 0.
		size_t socks5_udp_header_size(const char* data, size_t len) noexcept
		{
			if (len < 4 || static_cast<uint8_t>(data[2]) != 0)
				return 0;  // 不支持分片（FRAG 非 0）.

			size_t header_size = 0;
			switch (static_cast<uint8_t>(data[3]))
			{
			case SOCKS5_ATYP_IPV4:
				header_size = 10;
				break;
			case SOCKS5_ATYP_DOMAINNAME:
				if (len < 5)
					return 0;
				header_size = 7 + static_cast<uint8_t>(data[4]);
				break;
			case SOCKS5_ATYP_IPV6:
				header_size = 22;
				break;
			default:
				return 0;
			}

			return header_size <= len ? header_size : 0;
		}

		// 解析并连接目标（直连），成功返回已连接的 socket；失败返回未打开的 socket.
		net::awaitable<tcp::socket> connect_direct(
			net::any_io_executor executor,
			const proxy_server_option& opt,
			const net::ip::address& dst, uint16_t port,
			const std::function<net::awaitable<bool>(int)>& protect)
		{
			boost::system::error_code ec;

			tcp::endpoint endp(dst, port);

			// 建连总超时（黑洞网络下 connect/protect 可能无限挂起）:
			// 超时关闭在途 socket 使 async_connect 立即失败.
			auto sock = std::make_shared<tcp::socket>(executor);
			net::steady_timer conn_timer(executor);
			conn_timer.expires_after(std::chrono::seconds(15));
			conn_timer.async_wait(
				[sock](const boost::system::error_code& tec)
				{
					if (tec)
						return;
					boost::system::error_code sec;
					sock->close(sec);
				});

			sock->open(endp.protocol(), ec);
			if (!ec)
			{
				apply_so_mark_if(*sock, opt);

				// Android VpnService 场景必须先放行再 connect：否则 SYN 会按
				// 全隧道路由回环进 tun，被当成新连接再次转发，形成环路.
				if (protect && !co_await protect(sock->native_handle()))
				{
					XLOG_WARN << "tun connect direct not protected: "
						<< dst.to_string() << ":" << port;
					co_return tcp::socket(executor);
				}

				co_await sock->async_connect(endp, net_awaitable[ec]);
			}
			conn_timer.cancel();
			if (ec)
			{
				XLOG_WARN << "tun connect direct: " << dst.to_string()
					<< ":" << port << ", error: " << ec.message();
				co_return tcp::socket(executor);
			}

			co_return std::move(*sock);
		}

		// 构造 RFC 9298 CONNECT-UDP 请求（absolute-form URI）.
		http::request<http::empty_body> build_connect_udp_request(
			const urls::url& proxy_url,
			const net::ip::udp::endpoint& target,
			bool use_ssl)
		{
			std::string proxy_host(proxy_url.encoded_host());
			uint16_t proxy_port = proxy_url.port_number();
			if (proxy_port == 0)
				proxy_port = proxy_pass_default_port(proxy_url);

			std::string uri =
				std::string(use_ssl ? "https://" : "http://") +
				proxy_host + ":" + std::to_string(proxy_port) +
				format_connect_udp_path(
					target.address().to_string(), target.port());

			http::request<http::empty_body> req{ http::verb::get, uri, 11 };
			req.set(http::field::host, proxy_host + ":" + std::to_string(proxy_port));
			req.set(http::field::connection, "Upgrade");
			req.set(http::field::upgrade, "connect-udp");
			req.set("Capsule-Protocol", "?1");

			// 需要认证时添加 Proxy-Authorization 头.
			if (!proxy_url.user().empty())
			{
				const auto userinfo =
					std::string(proxy_url.user()) + ":" +
					std::string(proxy_url.password());
				req.set(http::field::proxy_authorization,
					"Basic " + strutil::base64_encode(userinfo));
			}

			return req;
		}

		// 从 DNS 服务器列表中选择与 target 同地址族的条目（端口 53），
		// 无同族条目时取首个可解析地址，全部失败返回空.
		std::optional<net::ip::udp::endpoint> pick_dns_server(
			const std::vector<std::string>& dns_list,
			const net::ip::udp::endpoint& target) noexcept
		{
			for (const auto& s : dns_list)
			{
				boost::system::error_code ec;
				auto addr = net::ip::make_address(s, ec);
				if (ec)
					continue;
				if (addr.is_v4() == target.address().is_v4())
					return net::ip::udp::endpoint(addr, 53);
			}
			for (const auto& s : dns_list)
			{
				boost::system::error_code ec;
				auto addr = net::ip::make_address(s, ec);
				if (!ec)
					return net::ip::udp::endpoint(addr, 53);
			}
			return std::nullopt;
		}

		// parse_dns_question 解析问题区，取第一个问题域名填充 qname，
		// 成功返回解析后的偏移位置，失败返回 nullptr.
		const char* parse_dns_question(const char* p, const char* end,
			const char* msg_start, uint16_t qdcount, std::string& qname)
		{
			for (uint16_t i = 0; i < qdcount; ++i)
			{
				auto [name, np] = dns_parse_name(p, end, msg_start);
				if (!np || np + 4 > end)
					return nullptr;

				if (i == 0)
					qname = name;
				p = np + 4;
			}
			if (qdcount == 0 || qname.empty())
				return nullptr;

			// 去掉末尾的 '.'.
			if (!qname.empty() && qname.back() == '.')
				qname.pop_back();
			return p;
		}

		// parse_dns_answer 解析单条 Answer 记录，A/AAAA 记录加入 answers，
		// 成功返回下一条记录的偏移位置，失败返回 nullptr.
		const char* parse_dns_answer(const char* p, const char* end,
			const char* msg_start,
			std::vector<std::pair<net::ip::address, uint32_t>>& answers)
		{
			if (p + 10 > end)
				return nullptr;

			auto [name, np] = dns_parse_name(p, end, msg_start);
			if (!np || np + 10 > end)
				return nullptr;

			const char* type_p = np;
			const char* ttl_p = np + 4;
			const char* rdl_p = np + 8;
			uint16_t type = io_util::read<uint16_t>(type_p);
			uint32_t ttl = io_util::read<uint32_t>(ttl_p);
			uint16_t rdlength = io_util::read<uint16_t>(rdl_p);
			const char* rdata = np + 10;
			if (rdata + rdlength > end)
				return nullptr;

			if (type == 1 && rdlength == 4)
			{
				net::ip::address_v4::bytes_type bytes;
				std::memcpy(bytes.data(), rdata, 4);
				answers.emplace_back(net::ip::address_v4(bytes), ttl);
			}
			else if (type == 28 && rdlength == 16)
			{
				net::ip::address_v6::bytes_type bytes;
				std::memcpy(bytes.data(), rdata, 16);
				answers.emplace_back(net::ip::address_v6(bytes), ttl);
			}

			return rdata + rdlength;
		}

		// parse_dns_response 解析 DNS 响应报文中的问题域名与 A/AAAA 答案.
		// 成功返回 true，qname/answers 为解析结果（answers 元素为 IP 与 TTL）.
		bool parse_dns_response(const char* data, size_t len,
			std::string& qname,
			std::vector<std::pair<net::ip::address, uint32_t>>& answers)
		{
			if (len < 12)
				return false;

			const char* msg_start = data;
			const char* end = data + len;

			const char* qd_p = data + 4;
			const char* an_p = data + 6;
			uint16_t qdcount = io_util::read<uint16_t>(qd_p);
			uint16_t ancount = io_util::read<uint16_t>(an_p);

			// 解析问题区，取第一个问题域名.
			const char* p = parse_dns_question(
				data + 12, end, msg_start, qdcount, qname);
			if (!p)
				return false;

			// 遍历 Answer 区提取 A/AAAA 记录.
			for (uint16_t i = 0; i < ancount; ++i)
			{
				p = parse_dns_answer(p, end, msg_start, answers);
				if (!p)
					break;
			}

			return !answers.empty();
		}

} // namespace proxy::tun_detail
