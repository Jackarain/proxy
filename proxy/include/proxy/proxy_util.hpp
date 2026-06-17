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


#include "proxy/default_cert.hpp"
#include "proxy/use_awaitable.hpp"

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/udp.hpp>

#include <boost/system/error_code.hpp>
#include <boost/system/result.hpp>
#include <boost/config.hpp>

#ifdef USE_BOOST_FILESYSTEM
// 为避免低版本(gcc-12.3 以下)的 libstdc++ 中 filesystem 的
// bug( https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95048 )
// 可以使用 boost.filesystem 来规避这个 bug
# include <boost/filesystem.hpp>
#else
# include <filesystem>
#endif // USE_BOOST_FILESYSTEM

#include <random>
#include <string>
#include <string_view>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#if defined(__linux__) || defined(__APPLE__)
# include <sys/socket.h>
# if defined(__APPLE__)
#  include <netinet/tcp.h>
# endif
#elif defined(BOOST_WINDOWS)
# include <mstcpip.h>
#endif


namespace proxy {

    namespace net = boost::asio;
	using udp = net::ip::udp;               // from <boost/asio/ip/udp.hpp>

#ifdef USE_BOOST_FILESYSTEM
	namespace fs = boost::filesystem;
#else
	namespace fs = std::filesystem;
#endif


#if defined(__linux__)
#  if !defined(IP_TRANSPARENT)
#    define IP_TRANSPARENT 19
#  endif
#  if !defined(IPV6_TRANSPARENT)
#    define IPV6_TRANSPARENT 75
#  endif
#  if !defined(IP_RECVORIGDSTADDR)
#    define IP_RECVORIGDSTADDR 20
#  endif
#  if !defined(IP_ORIGDSTADDR)
#    define IP_ORIGDSTADDR 20
#  endif
#  if !defined(IPV6_RECVORIGDSTADDR)
#    define IPV6_RECVORIGDSTADDR 74
#  endif
#  if !defined(IPV6_ORIGDSTADDR)
#    define IPV6_ORIGDSTADDR 74
#  endif

	// 设置 IP_TRANSPARENT 和 IPV6_TRANSPARENT 选项.
	using transparent_opt = net::detail::socket_option::boolean<
		IPPROTO_IP, IP_TRANSPARENT>;
	using transparent6_opt = net::detail::socket_option::boolean<
		IPPROTO_IPV6, IPV6_TRANSPARENT>;

#  ifdef ENABLE_REUSEPORT
#    if !defined(SO_REUSEPORT)
#      define SO_REUSEPORT 15
#    endif
	// 设置 SO_REUSEPORT 选项.
	using reuse_port = net::detail::socket_option::boolean<
		SOL_SOCKET, SO_REUSEPORT>;
#endif

#endif // defined(__linux__)


	static constexpr uint64_t udp_proxy_capsule_type = 0x00; // RFC 9297 DATAGRAM


	// 将错误代码设置为系统错误.
	inline void make_error_code(boost::system::error_code& ec, bool err) noexcept
	{
		if (!err)
		{
			ec = {};
		}
		else
		{
	#if defined(BOOST_WINDOWS)
			ec = boost::system::error_code(WSAGetLastError(), net::error::get_system_category());
	#else
			ec = boost::system::error_code(errno, net::error::get_system_category());
	#endif
		}
	}

	// 检测 host 是否是域名或主机名, 如果是域名则返回 true, 否则返回 false.
	inline bool is_hostname(std::string_view host) noexcept
	{
		boost::system::error_code ec;
		net::ip::make_address(host, ec);
		if (ec)
			return true;
		return false;
	}

	// 全局随机数生成器, 用于生成随机数据.
	inline std::random_device& global_random_device() noexcept
	{
		static std::random_device rd;
		return rd;
	}

#if defined(BOOST_WINDOWS)
	// 启用 TCP Keep-Alive 选项.
	inline boost::system::result<bool> set_tcp_keepalive(SOCKET fd) noexcept
	{
		boost::system::error_code ec;
		int ret = 0;

		struct tcp_keepalive alive;
		alive.onoff = 1;
		alive.keepalivetime = 30000;  		// 30 seconds
		alive.keepaliveinterval = 15000; 	// 15 seconds
		ret = ::WSAIoctl(fd, SIO_KEEPALIVE_VALS, &alive, sizeof(alive), NULL, 0, NULL, NULL, NULL);
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;
		return true;
	}
#elif defined(__APPLE__)
	// macOS 使用 TCP_KEEPALIVE 替代 TCP_KEEPIDLE.
	inline boost::system::result<bool> set_tcp_keepalive(int fd) noexcept
	{
		boost::system::error_code ec;
		int ret = 0;
		int keepalive = 1;

		ret = ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		int idle_time = 30;     // 30 seconds
		ret = ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &idle_time, sizeof(idle_time));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		int interval = 15;      // 15 seconds
		ret = ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		int maxpkt = 3;         // 3 probes
		ret = ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(maxpkt));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		return true;
	}
#else
	// 启用 TCP Keep-Alive 选项.
	inline boost::system::result<bool> set_tcp_keepalive(int fd) noexcept
	{
		boost::system::error_code ec;
		int ret = 0;
		int keepalive = 1;

		ret = ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		int idle_time = 30;     // 30 seconds
		ret = ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle_time, sizeof(idle_time));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		int interval = 15;      // 15 seconds
		ret = ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		int maxpkt = 3;         // 3 probes
		ret = ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(maxpkt));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;

		return true;
	}
#endif

	// 设置 socket 的 mark 标志.
	inline boost::system::result<bool> set_socket_mark(int fd, uint32_t mark) noexcept
	{
		boost::system::error_code ec;
#if defined(__linux__)
		int ret = ::setsockopt(fd, SOL_SOCKET, SO_MARK, &mark, sizeof(uint32_t));
		make_error_code(ec, ret != 0);
		if (ret != 0)
			return ec;
#endif
		return true;
	}

	// 在配置了 SO_MARK 时, 对指定 fd 应用 mark 标记.
	// 返回设置结果, 调用方负责记录日志.
	inline boost::system::result<void>
	apply_so_mark(int fd, const std::optional<uint32_t>& so_mark) noexcept
	{
#if defined(__linux__)
		if (so_mark)
		{
			auto r = set_socket_mark(fd, so_mark.value());
			if (r.has_error())
				return r.error();
		}
#endif
		return {};
	}

	//////////////////////////////////////////////////////////////////////////
	// QUIC 可变长度整数编码/解码 (RFC 9298 capsule 协议)

	// 获取 QUIC 变长整数编码后的实际字节数
	constexpr size_t varint_encoded_length(uint64_t val)
	{
		if (val <= 0x3F)       return 1;
		if (val <= 0x3FFF)     return 2;
		if (val <= 0x3FFFFFFF) return 4;
		return 8;
	}

	// 将 value 编码为 QUIC 变长整数, 写入 buf, 返回写入的字节数.
	inline size_t varint_int_encode(uint64_t value, uint8_t* buf) noexcept
	{
		if (value < 0x40)
		{
			buf[0] = static_cast<uint8_t>(value);
			return 1;
		}
		else if (value < 0x4000)
		{
			value |= 0x4000;
			buf[0] = static_cast<uint8_t>(value >> 8);
			buf[1] = static_cast<uint8_t>(value);
			return 2;
		}
		else if (value < 0x40000000)
		{
			value |= 0x80000000;
			buf[0] = static_cast<uint8_t>(value >> 24);
			buf[1] = static_cast<uint8_t>(value >> 16);
			buf[2] = static_cast<uint8_t>(value >> 8);
			buf[3] = static_cast<uint8_t>(value);
			return 4;
		}
		else
		{
			value |= 0xC000000000000000ULL;
			buf[0] = static_cast<uint8_t>(value >> 56);
			buf[1] = static_cast<uint8_t>(value >> 48);
			buf[2] = static_cast<uint8_t>(value >> 40);
			buf[3] = static_cast<uint8_t>(value >> 32);
			buf[4] = static_cast<uint8_t>(value >> 24);
			buf[5] = static_cast<uint8_t>(value >> 16);
			buf[6] = static_cast<uint8_t>(value >> 8);
			buf[7] = static_cast<uint8_t>(value);
			return 8;
		}
	}

	// 从 buf 解码一个 QUIC 变长整数, 返回 {字节数, 值}.
	inline std::pair<size_t, uint64_t> varint_int_decode(const uint8_t* buf) noexcept
	{
		uint8_t prefix = buf[0] >> 6;
		if (prefix == 0)
			return { 1, buf[0] };
		else if (prefix == 1)
		{
			uint64_t v = (static_cast<uint64_t>(buf[0]) << 8) | buf[1];
			return { 2, v & 0x3FFF };
		}
		else if (prefix == 2)
		{
			uint64_t v = (static_cast<uint64_t>(buf[0]) << 24) |
				(static_cast<uint64_t>(buf[1]) << 16) |
				(static_cast<uint64_t>(buf[2]) << 8) |
				buf[3];
			return { 4, v & 0x3FFFFFFF };
		}
		else
		{
			uint64_t v = (static_cast<uint64_t>(buf[0]) << 56) |
				(static_cast<uint64_t>(buf[1]) << 48) |
				(static_cast<uint64_t>(buf[2]) << 40) |
				(static_cast<uint64_t>(buf[3]) << 32) |
				(static_cast<uint64_t>(buf[4]) << 24) |
				(static_cast<uint64_t>(buf[5]) << 16) |
				(static_cast<uint64_t>(buf[6]) << 8) |
				buf[7];
			return { 8, v & 0x3FFFFFFFFFFFFFFF };
		}
	}

	// 从流中读取一个 QUIC 变长整数 (varint), 用于 RFC 9298 capsule 协议解析.
	// 注意: 需要调用方已包含 proxy_stream.hpp 和 use_awaitable.hpp.
	// 返回读取到的值, ec 非零表示读取失败.
	template <typename Stream>
	inline net::awaitable<uint64_t>
	read_varint_from_stream(Stream& stream, boost::system::error_code& ec)
	{
		uint8_t buf[8];

		co_await net::async_read(stream, net::buffer(buf, 1), net_awaitable[ec]);
		if (ec)
			co_return 0;

		uint8_t prefix = buf[0] >> 6;
		size_t len = 1 << prefix;

		if (len > 1)
		{
			co_await net::async_read(stream, net::buffer(buf + 1, len - 1),
				net_awaitable[ec]);
			if (ec)
				co_return 0;
		}

		auto [consumed, value] = varint_int_decode(buf);
		(void)consumed;

		co_return value;
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
	// SSL 客户端通用工具函数
	// 将 proxy_server 和 proxy_session 中重复的 SSL 客户端连接代码抽象到此.

	// 配置 SSL 客户端 context 的通用设置.
	// 包括设置验证模式、加载 CA 根证书、以及配置主机名验证回调.
	// 使用模板以避免在 proxy_util.hpp 中引入 SSL 相关头文件依赖，
	// 函数体在实例化时需保证以下符号可用:
	//   net::ssl::context, net::ssl::verify_peer, net::ssl::verify_none,
	//   net::ssl::host_name_verification, net::buffer, default_root_certificates()
	// 参数:
	//   ctx - SSL context 引用
	//   disable_check_cert - 是否禁用证书验证
	//   hostname - 用于证书验证的主机名
	//   cacert_path - 可选的自定义 CA 证书目录路径
	inline boost::system::error_code
	configure_ssl_client_ctx(net::ssl::context& ctx,
		bool disable_check_cert,
		const std::string& hostname,
		const std::string& cacert_path = {}) noexcept
	{
		boost::system::error_code ec;

		if (!cacert_path.empty() && fs::exists(cacert_path, ec))
		{
			ctx.add_verify_path(cacert_path, ec);
			if (ec)
				return ec;
		}

		auto verify = net::ssl::verify_peer;
		if (disable_check_cert)
			verify = net::ssl::verify_none;

		ctx.set_verify_mode(verify);

		auto certs = default_root_certificates();
		ctx.add_certificate_authority(
			net::buffer(certs.data(), certs.size()), ec);
		if (ec) return ec;

		ctx.set_verify_callback(
			net::ssl::host_name_verification(hostname), ec);

		return ec;
	}

	// 将路径转换为 UNC 路径 (Windows 下处理长路径).
	template<typename Path>
	static fs::path make_unc_path(const Path& path) noexcept
	{
#ifndef WIN32
		return path;
#else
		auto ret = path.string();
		if (ret.size() > MAX_PATH)
		{
			boost::replace_all(ret, "/", "\\");
			return "\\\\?\\" + ret;
		}
		return ret;
#endif
	}

	// 获取文件的最后修改时间和 UNC 路径.
	inline std::tuple<std::string, fs::path> file_last_write_time(const fs::path& file) noexcept
	{
		std::string time_string;
		fs::path returned_path; // 仅在 Windows 长路径处理时使用
		std::time_t time_c = 0;
		bool success = false;

#ifdef WIN32
		WIN32_FILE_ATTRIBUTE_DATA file_attr;
		std::wstring wpath = file.wstring();

		// 如果路径超长且没有 UNC 前缀，尝试转换
		if (wpath.size() > MAX_PATH && wpath.compare(0, 4, L"\\\\?\\") != 0)
		{
			returned_path = make_unc_path(file);
			wpath = returned_path.wstring();
		}

		if (GetFileAttributesExW(wpath.c_str(), GetFileExInfoStandard, &file_attr))
		{
			// 将 Windows 的 FILETIME (100纳秒为单位) 转换为 Unix 的 time_t (秒为单位)
			ULARGE_INTEGER ull;
			ull.LowPart = file_attr.ftLastWriteTime.dwLowDateTime;
			ull.HighPart = file_attr.ftLastWriteTime.dwHighDateTime;

			// Windows Epoch 是 1601年1月1日，Unix Epoch 是 1970年1月1日，相差 11644473600 秒
			time_c = static_cast<std::time_t>((ull.QuadPart / 10000000ULL) - 11644473600ULL);
			success = true;
		}
#else
		struct stat file_stat;
		if (::stat(file.c_str(), &file_stat) == 0)
		{
#if defined(__APPLE__)
			time_c = file_stat.st_mtimespec.tv_sec;
#else
			time_c = file_stat.st_mtim.tv_sec; // 现代 Linux 的纳秒级结构体中的秒部分
#endif
			success = true;
		}
#endif

		if (success)
		{
			struct tm tm_buf{};
#ifdef WIN32
			localtime_s(&tm_buf, &time_c);
#else
			localtime_r(&time_c, &tm_buf);
#endif

			char tmbuf[64] = { 0 };
			std::strftime(tmbuf, sizeof(tmbuf), "%m-%d-%Y %H:%M", &tm_buf);
			time_string = tmbuf;
		}

		return { time_string, returned_path };
	}

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

#endif // INCLUDE__2026_06_08__PROXY_UTIL_HPP
