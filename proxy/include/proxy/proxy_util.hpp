//
// proxy_util.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_06_08__PROXY_UTIL_HPP
#define INCLUDE__2026_06_08__PROXY_UTIL_HPP


#include "proxy/http_date.hpp"
#include "proxy/net_socket_option.hpp"
#include "proxy/path_util.hpp"
#include "proxy/rfc9298_capsule.hpp"
#include "proxy/ssl_client.hpp"
#include "proxy/use_awaitable.hpp"

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/network_v4.hpp>
#include <boost/asio/ip/network_v6.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/post.hpp>
#include <boost/system/result.hpp>
#include <boost/config.hpp>
#include <boost/nowide/convert.hpp>
#include <boost/url.hpp>

#include <random>
#include <string>
#include <string_view>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>

#ifdef USE_BOOST_FILESYSTEM
# include <boost/filesystem.hpp>
#else
# include <filesystem>
#endif

#if defined(__linux__) || defined(__APPLE__)
# include <sys/socket.h>
# if defined(__APPLE__)
#  include <netinet/tcp.h>
# endif
#elif defined(BOOST_WINDOWS)
# include <mstcpip.h>
#endif


// 通用工具头文件.
//
// 具体领域工具已拆分到独立头文件, 这里聚合以便既有调用方继续包含:
//   net_socket_option.hpp  socket 选项
//   rfc9298_capsule.hpp    RFC 9298 connect-udp capsule 与 QUIC varint
//   ssl_client.hpp         SSL 客户端 context 配置与共享缓存
//   http_date.hpp          HTTP 日期与 ETag 校验
//   path_util.hpp          路径与文件时间工具

namespace proxy {

    namespace net = boost::asio;
	using udp = net::ip::udp;               // from <boost/asio/ip/udp.hpp>

	namespace urls = boost::urls;			// form <boost/url.hpp>

#ifdef USE_BOOST_FILESYSTEM
	namespace fs = boost::filesystem;
#else
	namespace fs = std::filesystem;
#endif

	//////////////////////////////////////////////////////////////////////////

	// cidr_rule 保存一条预解析后的 IP/CIDR 规则.
	//
	// 规则字符串只在配置加载/变更时解析一次, 命中判断阶段只做地址比较,
	// 避免在按包/按连接的匹配路径上重复调用 make_address/
	// make_network_v4/make_network_v6. 解析顺序与原实现保持一致:
	// 单地址 -> IPv4 网段 -> IPv6 网段; 无法解析时三者皆为空, 永不命中.
	struct cidr_rule
	{
		// 规则原文 (地区名等非 IP 规则用作文本匹配).
		std::string text;

		// 三种形态互斥.
		std::optional<net::ip::address> single;
		std::optional<net::ip::network_v4> v4;
		std::optional<net::ip::network_v6> v6;
	};

	inline cidr_rule parse_cidr_rule(std::string_view text)
	{
		cidr_rule rule;
		rule.text.assign(text);

		boost::system::error_code ec;
		if (auto addr = net::ip::make_address(rule.text, ec); !ec)
		{
			rule.single = addr;
			return rule;
		}

		ec.clear();
		if (auto v4 = net::ip::make_network_v4(rule.text, ec); !ec)
		{
			rule.v4 = v4;
			return rule;
		}

		ec.clear();
		if (auto v6 = net::ip::make_network_v6(rule.text, ec); !ec)
			rule.v6 = v6;

		return rule;
	}

	//////////////////////////////////////////////////////////////////////////

	// 检测 host 是否是域名或主机名, 如果是域名则返回 true, 否则返回 false.
	inline bool is_hostname(std::string_view host) noexcept
	{
		boost::system::error_code ec;
		net::ip::make_address(host, ec);
		if (ec)
			return true;
		return false;
	}

	// 计算 proxy_pass 未显式指定端口时的默认端口: 已知 scheme (http/https/ws/wss)
	// 使用标准默认端口; 未定义默认端口的 scheme 中, 以 's' 结尾(ssl 加密)默认 443,
	// 其余 (socks4/socks5 等) 默认 1080.
	inline uint16_t proxy_pass_default_port(const urls::url& url) noexcept
	{
		auto port = urls::default_port(url.scheme_id());
		if (port != 0)
			return port;
		auto scheme = boost::to_lower_copy(std::string(url.scheme()));
		if (scheme.ends_with("s"))
			return 443;
		return 1080;
	}

	// 解析十进制数字字符串为 int64_t, 失败或溢出时返回空 optional.
	// 仅接受非负十进制数字, 不进行动态内存分配, 用于解析 HTTP 头等场景.
	inline std::optional<int64_t> parse_num(std::string_view s) noexcept
	{
		if (s.empty())
			return std::nullopt;

		int64_t v = 0;
		for (char c : s)
		{
			if (c < '0' || c > '9')
				return std::nullopt;

			int64_t d = c - '0';
			if (v > (std::numeric_limits<int64_t>::max() - d) / 10)
				return std::nullopt;
			v = v * 10 + d;
		}
		return v;
	}

	// 判定是否为 ASCII 空白字符 (空格, \t, \n, \v, \f, \r).
	// 避免使用 std::isspace: 该函数依赖 locale 且通常无法内联, 在解析热路径上开销较大.
	inline bool is_ascii_space(char c) noexcept
	{
		return c == ' ' || (c >= '\t' && c <= '\r');
	}

	// 全局随机数生成器, 用于生成随机数据.
	inline std::random_device& global_random_device() noexcept
	{
		static std::random_device rd;
		return rd;
	}

	//////////////////////////////////////////////////////////////////////////

	// 将 sockaddr_storage 转换为 udp::endpoint.
	inline udp::endpoint sockaddr_to_udp_endpoint(const sockaddr_storage& addr) noexcept
	{
		if (addr.ss_family == AF_INET)
		{
			const auto& addr4 = reinterpret_cast<const sockaddr_in&>(addr);
			return udp::endpoint(
				net::ip::address_v4(ntohl(addr4.sin_addr.s_addr)),
				ntohs(addr4.sin_port));
		}
		if (addr.ss_family == AF_INET6)
		{
			const auto& addr6 = reinterpret_cast<const sockaddr_in6&>(addr);
			net::ip::address_v6::bytes_type bt;
			std::memcpy(bt.data(), &addr6.sin6_addr, 16);
			return udp::endpoint(
				net::ip::make_address_v6(bt),
				ntohs(addr6.sin6_port));
		}

		return {};
	}

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// 执行上下文切换工具
	// 从 proxy_server 和 proxy_session 中提取公共代码消除重复.
	// 当 m_scheduler_locking 为 false 时, 协程会切换到后端线程池执行, 适用于需要执行
	// 同步操作（如 DNS 解析）的场景. 返回当前应使用的 executor.

	// 切换到后端执行上下文（非锁定调度时）.
	inline net::awaitable<net::any_io_executor>
	backend_switch_to(bool scheduler_locking,
		net::io_context& backend_context, net::any_io_executor executor)
	{
		if (!scheduler_locking)
		{
			co_await net::post(
				net::bind_executor(backend_context.get_executor(), net::use_awaitable));
			co_return backend_context.get_executor();
		}
		co_return executor;
	}

	// 从后端执行上下文切换回主执行上下文.
	inline net::awaitable<void>
	backend_switch_from(bool scheduler_locking, net::any_io_executor executor)
	{
		if (!scheduler_locking)
			co_await net::post(
				net::bind_executor(executor, net::use_awaitable));
	}

}

constexpr std::size_t custom_default_transfer_size = 1024 * 1024;

// 用于单次向系统提交最大的发送数据量, 以提高效率.
inline std::size_t custom_write_all(const boost::system::error_code& ec, std::size_t) noexcept
{
    return ec ? 0 : (std::numeric_limits<std::size_t>::max)();
}

#endif // INCLUDE__2026_06_08__PROXY_UTIL_HPP

