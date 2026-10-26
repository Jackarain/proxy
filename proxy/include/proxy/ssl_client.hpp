//
// ssl_client.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_10_13__SSL_CLIENT_HPP
#define INCLUDE__2026_10_13__SSL_CLIENT_HPP


#include "proxy/default_cert.hpp"

#include <boost/asio/ssl.hpp>
#include <boost/system/result.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

// ASN1_STRING_get_length() 仅在 OpenSSL 4.1+ 和 BoringSSL 中提供.
#if !defined(ASN1_STRING_get_length) && \
	(defined(BOOST_ASIO_USE_WOLFSSL) || \
	(defined(OPENSSL_VERSION_MAJOR) && \
		(OPENSSL_VERSION_MAJOR < 4 || \
		(OPENSSL_VERSION_MAJOR == 4 && OPENSSL_VERSION_MINOR < 1))))
# define ASN1_STRING_get_length(x) ASN1_STRING_length(x)
#endif

#ifdef USE_BOOST_FILESYSTEM
# include <boost/filesystem.hpp>
#else
# include <filesystem>
#endif


namespace proxy {

	namespace net = boost::asio;

#ifdef USE_BOOST_FILESYSTEM
	namespace fs = boost::filesystem;
#else
	namespace fs = std::filesystem;
#endif

	// 设置客户端 SNI. OpenSSL 接口要求空结尾字符串.
	inline bool set_client_sni(SSL* ssl, std::string_view sni)
	{
		std::string terminated_sni(sni);
		return SSL_set_tlsext_host_name(
			ssl, terminated_sni.c_str()) == 1;
	}

	//////////////////////////////////////////////////////////////////////////

	// 配置 SSL 客户端 context 的通用设置.
	// 包括设置验证模式、加载 CA 根证书、以及配置主机名验证回调.
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

	// 进程内共享的 SSL 客户端 context 缓存.
	//
	// 创建 SSL context 并加载内置 CA 根证书 bundle(含数百张证书) 开销很大, 若为每个
	// 连接/请求都创建 context 并重复加载, 会成为 CPU 热点. 这里按 tag 缓存并复用:
	// tag 相同的调用共享同一个 context, setup 仅在首次创建时执行.
	// setup 接收 context 引用并返回错误码, 返回错误时不缓存该 context.
	// tag 必须唯一标识一组相同的 setup 与配置参数. 共享的 SSL_CTX 可被多个连接和
	// 线程安全地并发使用.
	//
	// 缓存的存储与互斥锁提取到下面的非模板函数中, 保证不同 setup(不同 lambda)的
	// 调用方共享同一份缓存; 若直接写在模板函数内, 每个 setup 类型都会生成各自独立
	// 的静态缓存, 相同 tag 会缓存出多份 context.
	inline std::unordered_map<std::string,
		std::shared_ptr<net::ssl::context>>&
	ssl_client_ctx_cache() noexcept
	{
		static std::unordered_map<std::string,
			std::shared_ptr<net::ssl::context>> cache;
		return cache;
	}

	inline std::mutex& ssl_client_ctx_cache_mutex() noexcept
	{
		static std::mutex mutex;
		return mutex;
	}

	template <typename Setup>
	inline boost::system::result<std::shared_ptr<net::ssl::context>>
	get_cached_ssl_client_ctx(const std::string& tag, Setup&& setup) noexcept
	{
		auto& cache = ssl_client_ctx_cache();

		std::lock_guard<std::mutex> lock(ssl_client_ctx_cache_mutex());

		auto it = cache.find(tag);
		if (it != cache.end())
			return it->second;

		auto ctx = std::make_shared<net::ssl::context>(
			net::ssl::context::sslv23_client);

		auto ec = setup(*ctx);
		if (ec)
			return ec;

		cache.emplace(tag, ctx);

		return ctx;
	}

	// 返回进程内共享的 SSL 客户端 context, 按 (是否校验证书, 主机名, 自定义 CA 路径)
	// 缓存并复用. 由于主机名校验回调与 context 绑定, 这里以主机名作为缓存键.
	inline boost::system::result<std::shared_ptr<net::ssl::context>>
	get_shared_ssl_client_ctx(bool disable_check_cert,
		const std::string& hostname, const std::string& cacert_path = {}) noexcept
	{
		std::string key;
		key.reserve(hostname.size() + cacert_path.size() + 3);
		key += "client\n";
		key += disable_check_cert ? '1' : '0';
		key += '\n';
		key += hostname;
		key += '\n';
		key += cacert_path;

		return get_cached_ssl_client_ctx(key,
			[&](net::ssl::context& ctx) noexcept
			{
				return configure_ssl_client_ctx(
					ctx, disable_check_cert, hostname, cacert_path);
			});
	}

}


#endif // INCLUDE__2026_10_13__SSL_CLIENT_HPP
