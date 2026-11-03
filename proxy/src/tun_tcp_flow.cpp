//
// tun_tcp_flow.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/tun_server.hpp"
#include "proxy/proxy_websocket_client.hpp"
#include "proxy/dns_server.hpp"
#include "proxy/http_proxy_client.hpp"
#include "proxy/logging.hpp"
#include "proxy/proxy_util.hpp"
#include "proxy/socks_client.hpp"
#include "proxy/ssl_connect.hpp"
#include "proxy/network_order.hpp"
#include "proxy/wintun_install.hpp"
#include "tun_server_detail.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/functional/hash.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <future>
#include <string>
#include <vector>

namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// tun_tcp_flow

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

	tun_tcp_flow::tun_tcp_flow(net::any_io_executor executor,
		const std::shared_ptr<tun_server>& owner,
		const proxy_server_option& opt,
		tcp_flow_key key,
		tunio::tun_tcp_socket stream)
		: m_executor(std::move(executor))
		, m_owner(owner)
		, m_option(opt)
		, m_key(std::move(key))
		, m_conn_id(owner ? owner->next_conn_id() : 0)
		, m_client(stream.remote_endpoint())
		, m_target(stream.original_destination())
		, m_started(std::chrono::steady_clock::now())
		, m_proto(static_cast<uint8_t>(tun_conn_proto::tcp))
		, m_client_stream(std::move(stream))
		, m_upstream(init_proxy_stream(m_executor))
	{}

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
				return do_connect();
			}, net::detached);
	}

	net::awaitable<void> tun_tcp_flow::do_connect()
	{
		auto self = shared_from_this();

		const std::string target_host = m_target.address().to_string();
		const uint16_t target_port = m_target.port();

		// 建连总超时: 黑洞网络下代理握手等阶段可能无限挂起, 超时关闭
		// 整个 flow (客户端流与上游) 使在途建连操作失败, 避免协程在
		// 停止后仍残留挂起点 (io_context 析构需逐个清理这些帧).
		net::steady_timer conn_timer(m_executor);
		conn_timer.expires_after(std::chrono::seconds(15));
		conn_timer.async_wait(
			[self](const boost::system::error_code& tec)
			{
				if (tec)
					return;
				self->close();
			});

		// 分流判定：命中 proxy_cidr_ 或代理域名解析缓存走上游代理，否则直连.
		const bool use_proxy = m_owner && m_owner->ip_match_proxy(m_target.address());

		XLOG_DBG << "tun tcp connect " << target_host << ":" << target_port
			<< (use_proxy ? " via proxy" : " direct")
			<< ", cidr_size=" << m_option.proxy_cidr_.size();

		if (!co_await establish_upstream(target_host, target_port, use_proxy))
		{
			// 后端连接失败：向客户端发送 RST 并关闭.
			conn_timer.cancel();
			m_client_stream.reset();
			close();
			co_return;
		}

		if (m_upstream_buffer.size() > 0)
		{
			boost::system::error_code ec;
			auto bytes = m_upstream_buffer.size();
			co_await net::async_write(m_client_stream,
				m_upstream_buffer.data(), net_awaitable[ec]);
			m_upstream_buffer.consume(bytes);
			if (ec)
			{
				XLOG_WARN << "tun write buffered upstream data: "
					<< ec.message();
				conn_timer.cancel();
				close();
				co_return;
			}

			m_tx_bytes += static_cast<uint64_t>(bytes);
		}

		conn_timer.cancel();

		m_connected = true;

		XLOG_DBG << "tun tcp established " << target_host << ":" << target_port;

		// 启动双向数据搬运协程.
		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				return tx_loop();
			}, net::detached);

		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				return rx_loop();
			}, net::detached);
	}

	// establish_upstream 依据分流结果经上游代理或直连建立连接，
	// 成功后设置 m_upstream 并返回 true.
	net::awaitable<bool> tun_tcp_flow::establish_upstream(
		const std::string& target_host, uint16_t target_port, bool use_proxy)
	{
		const auto protect = m_owner ?
			std::function<net::awaitable<bool>(int)>(
				[owner = m_owner](int fd) { return owner->protect_socket(fd); }) :
			std::function<net::awaitable<bool>(int)>();

		if (use_proxy && m_option.proxy_pass_)
			co_return co_await establish_via_proxy(
				target_host, target_port, protect);

		co_return co_await establish_direct(target_port, protect);
	}

	// establish_via_proxy 经上游代理建立连接：按 scheme 记录协议类型，
	// 连接 proxy_pass 后视 SSL 代理先建 TLS，再完成代理协议握手.
	net::awaitable<bool> tun_tcp_flow::establish_via_proxy(
		const std::string& target_host, uint16_t target_port,
		const std::function<net::awaitable<bool>(int)>& protect)
	{
		const auto& proxy_url = *m_option.proxy_pass_;
		auto scheme = boost::to_lower_copy(
			std::string(proxy_url.scheme()));
		if (scheme.starts_with("socks"))
			m_proto.store(static_cast<uint8_t>(tun_conn_proto::socks5));
		else if (scheme == "wss")
			m_proto.store(static_cast<uint8_t>(tun_conn_proto::wss));
		else if (scheme == "ws")
			m_proto.store(static_cast<uint8_t>(tun_conn_proto::ws));
		else if (proxy_use_ssl(proxy_url, m_option))
			m_proto.store(static_cast<uint8_t>(tun_conn_proto::https));
		else
			m_proto.store(static_cast<uint8_t>(tun_conn_proto::http));

		// 优先从 proxy_pass 预选连接池获取已建立的连接（TCP/TLS 已完成）.
		std::shared_ptr<proxy_pass_pool> pool;
		if (m_option.proxy_pass_pool_size_ > 0 && m_owner)
			pool = m_owner->proxy_conn_pool();
		if (pool)
		{
			auto conn = co_await pool->acquire();
			if (conn && conn->is_open())
			{
				m_upstream = std::move(*conn);
				if (co_await do_proxy_handshake(proxy_url))
				{
					XLOG_DBG << "tun tcp proxy from pool: "
						<< target_host << ":" << target_port;
					co_return true;
				}

				// 池中连接握手失败（代理端可能已断开），丢弃并回退新建连接.
				XLOG_WARN << "tun tcp proxy pool handshake failed, "
					"fallback connect: " << target_host << ":"
					<< target_port;
				m_upstream = init_proxy_stream(m_executor);
			}
			else
			{
				XLOG_WARN << "tun tcp proxy pool acquire failed, "
					"fallback connect: " << target_host << ":"
					<< target_port;
			}
		}

		auto sock = co_await connect_proxy_pass(
			m_executor, m_option, proxy_url, protect);
		if (!sock.is_open())
		{
			XLOG_WARN << "tun tcp connect proxy_pass failed: "
				<< target_host << ":" << target_port;
			co_return false;
		}

		XLOG_DBG << "tun tcp proxy connected: "
			<< target_host << ":" << target_port;

		// https/wss 等 SSL 代理需要先建立 TLS 再执行代理握手.
		if (proxy_use_ssl(proxy_url, m_option))
		{
			if (!m_owner)
				co_return false;

			std::string proxy_host(proxy_url.host_address());
			auto sni = m_option.proxy_ssl_name_.empty() ?
				proxy_host : m_option.proxy_ssl_name_;
			std::optional<variant_stream_type> ssl_stream;
			auto res = co_await m_owner->make_ssl_socket(
				sock, sni, ssl_stream);
			if (res.has_error())
			{
				XLOG_WARN << "tun tcp make_ssl_socket: "
					<< res.error().message();
				co_return false;
			}
			m_upstream = std::move(*ssl_stream);
		}
		else
		{
			m_upstream = init_proxy_stream(std::move(sock));
		}

		co_return co_await do_proxy_handshake(proxy_url);
	}

	// establish_direct 直连目标建立连接，成功设置 m_upstream 并返回 true.
	net::awaitable<bool> tun_tcp_flow::establish_direct(
		uint16_t target_port,
		const std::function<net::awaitable<bool>(int)>& protect)
	{
		auto sock = co_await tun_detail::connect_direct(
			m_executor, m_option, m_target.address(), target_port, protect);
		if (!sock.is_open())
			co_return false;

		m_upstream = init_proxy_stream(std::move(sock));
		co_return true;
	}

	net::awaitable<bool> tun_tcp_flow::do_proxy_handshake(const urls::url& proxy_url)
	{
		boost::system::error_code ec;

		const std::string target_host = m_target.address().to_string();
		const uint16_t target_port = m_target.port();
		auto scheme = boost::to_lower_copy(std::string(proxy_url.scheme()));

		if (scheme.starts_with("socks"))
		{
			socks_client_option opt;
			opt.target_host = target_host;
			opt.target_port = target_port;
			// IP 目标直接以地址类型发送，域名目标交由代理解析.
			opt.proxy_hostname = !m_target.address().is_v4() &&
				!m_target.address().is_v6();
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

			co_await async_http_proxy_handshake(
				m_upstream, opt, net_awaitable[ec], &m_upstream_buffer);
		}
		else if (scheme == "ws" || scheme == "wss")
		{
			auto options = make_websocket_handshake_options(
				proxy_url, target_host, target_port);
			ec = co_await async_websocket_proxy_handshake(
				m_upstream, std::move(options));
		}
		else
		{
			XLOG_WARN << "tun unsupported proxy_pass scheme: " << scheme;
			co_return false;
		}

		if (ec)
		{
			XLOG_WARN << "tun proxy_pass handshake: "
				<< std::string(proxy_url.encoded_host())
				<< ", error: " << ec.message();
			co_return false;
		}

		co_return true;
	}

	// tx_loop 读取客户端（tun_tcp_socket）数据并转发到上游.
	net::awaitable<void> tun_tcp_flow::tx_loop()
	{
		using namespace net::experimental::awaitable_operators;

		constexpr size_t buf_size = 65536;

		std::unique_ptr<char, decltype(&std::free)> buf0(
			(char*)std::malloc(buf_size), &std::free);
		std::unique_ptr<char, decltype(&std::free)> buf1(
			(char*)std::malloc(buf_size), &std::free);

		// 分别使用主从缓冲指针用于并发读写.
		auto primary_buf = buf0.get();
		auto secondary_buf = buf1.get();

		boost::system::error_code read_ec;
		boost::system::error_code write_ec;

		// 首先读取客户端数据作为预备, 以用于后面的交替读写逻辑.
		size_t bytes = co_await m_client_stream.async_read_some(
			net::buffer(primary_buf, buf_size), net_awaitable[read_ec]);
		if (read_ec)
		{
			// 客户端流异常（RST/关闭）：整体关闭连接.
			close();
			co_return;
		}

		if (bytes == 0)
		{
			// 客户端 FIN：半关闭上游写方向，等待上游 EOF 后整体关闭.
			co_await handle_client_fin();
			co_return;
		}

		m_rx_bytes += static_cast<uint64_t>(bytes);
		if (m_owner)
			m_owner->add_traffic(bytes, 0);

		for (; !m_closed;)
		{
			// 并发读写, 将上次接收到的客户端数据 primary_buf 转发给上游,
			// 同时异步读取客户端的数据到 secondary_buf 中.
			auto [write_bytes, read_bytes] = co_await(
				net::async_write(m_upstream,
					net::buffer(primary_buf, bytes),
					[bytes](const boost::system::error_code& ec, std::size_t)
						-> std::size_t {
							return ec ? 0 : bytes;
						}, net_awaitable[write_ec])
				&&
				m_client_stream.async_read_some(
					net::buffer(secondary_buf, buf_size),
					net_awaitable[read_ec])
			);
			(void)write_bytes;

			// 交换主从缓冲区.
			std::swap(primary_buf, secondary_buf);

			// 保存接收到的数据大小用于转发给上游, 以及计算整个传输数据量.
			bytes = read_bytes;

			// 客户端流异常（RST/关闭）：整体关闭连接.
			if (read_ec)
			{
				close();
				co_return;
			}

			// 客户端 FIN：半关闭上游写方向，等待上游 EOF 后整体关闭.
			if (bytes == 0)
			{
				co_await handle_client_fin();
				co_return;
			}

			// 上游写入失败：EOF 向客户端发 FIN，异常发送 RST 并关闭.
			if (write_ec)
			{
				if (write_ec == net::error::eof)
					handle_upstream_eof(write_ec, 0);
				else
					on_upstream_closed();
				co_return;
			}

			m_rx_bytes += static_cast<uint64_t>(bytes);
			if (m_owner)
				m_owner->add_traffic(bytes, 0);
		}

		co_return;
	}

	// rx_loop 读取上游数据并转发到客户端（tun_tcp_socket）.
	net::awaitable<void> tun_tcp_flow::rx_loop()
	{
		using namespace net::experimental::awaitable_operators;

		constexpr size_t buf_size = 65536;

		std::unique_ptr<char, decltype(&std::free)> buf0(
			(char*)std::malloc(buf_size), &std::free);
		std::unique_ptr<char, decltype(&std::free)> buf1(
			(char*)std::malloc(buf_size), &std::free);

		// 分别使用主从缓冲指针用于并发读写.
		auto primary_buf = buf0.get();
		auto secondary_buf = buf1.get();

		boost::system::error_code read_ec;
		boost::system::error_code write_ec;

		// 首先读取上游数据作为预备, 以用于后面的交替读写逻辑.
		size_t bytes = co_await m_upstream.async_read_some(
			net::buffer(primary_buf, buf_size), net_awaitable[read_ec]);
		if (read_ec || bytes == 0)
		{
			// 上游读取结束：EOF 向客户端发 FIN，异常发送 RST 并关闭.
			handle_upstream_eof(read_ec, bytes);
			co_return;
		}

		m_tx_bytes += static_cast<uint64_t>(bytes);
		if (m_owner)
			m_owner->add_traffic(0, bytes);

		for (; !m_closed;)
		{
			// 并发读写, 将上次接收到的上游数据 primary_buf 写给客户端,
			// 同时异步读取上游的数据到 secondary_buf 中.
			auto [write_bytes, read_bytes] = co_await(
				net::async_write(m_client_stream,
					net::buffer(primary_buf, bytes),
					[bytes](const boost::system::error_code& ec, std::size_t)
						-> std::size_t {
							return ec ? 0 : bytes;
						}, net_awaitable[write_ec])
				&&
				m_upstream.async_read_some(
					net::buffer(secondary_buf, buf_size),
					net_awaitable[read_ec])
			);
			(void)write_bytes;

			// 交换主从缓冲区.
			std::swap(primary_buf, secondary_buf);

			// 保存接收到的数据大小用于转发给客户端, 以及计算整个传输数据量.
			bytes = read_bytes;

			// 上游读取结束：EOF 向客户端发 FIN，异常发送 RST 并关闭.
			if (read_ec || bytes == 0)
			{
				handle_upstream_eof(read_ec, bytes);
				co_return;
			}

			// 客户端流不可写（已关闭/RST）：整体关闭连接.
			if (write_ec)
			{
				close();
				co_return;
			}

			m_tx_bytes += static_cast<uint64_t>(bytes);
			if (m_owner)
				m_owner->add_traffic(0, bytes);
		}

		co_return;
	}

	// handle_client_fin 处理客户端 FIN：半关闭上游写方向，等待上游 EOF
	// 后整体关闭.
	net::awaitable<void> tun_tcp_flow::handle_client_fin()
	{
		m_client_fin = true;
		if (m_upstream_eof)
		{
			close();
			co_return;
		}

		boost::system::error_code ec;
		if (is_websocket_tunnel(m_upstream))
		{
			co_await async_shutdown(m_upstream, net_awaitable[ec]);
		}
		else
		{
			net_tcp_socket(m_upstream).shutdown(tcp::socket::shutdown_send, ec);
		}
		co_return;
	}

	// handle_upstream_eof 处理上游读取结束：EOF 向客户端发 FIN，
	// 异常向客户端发 RST 并关闭.
	void tun_tcp_flow::handle_upstream_eof(boost::system::error_code ec, size_t n)
	{
		if (ec == net::error::eof || n == 0)
		{
			// 上游关闭：向客户端发送 FIN（半关闭发送侧）.
			if (!m_upstream_eof)
			{
				m_upstream_eof = true;

				boost::system::error_code sec;
				m_client_stream.shutdown(
					net::ip::tcp::socket::shutdown_send, sec);
			}
			if (m_client_fin)
				close();
		}
		else
		{
			// 上游异常：向客户端发送 RST 并关闭.
			m_client_stream.reset();
			close();
		}
	}

	// on_upstream_closed 处理上游连接异常/关闭.
	void tun_tcp_flow::on_upstream_closed()
	{
		if (m_closed)
			return;

		m_client_stream.reset();
		close();
	}

	void tun_tcp_flow::close()
	{
		if (m_closed)
			return;

		m_closed = true;

		boost::system::error_code ec;
		net_tcp_socket(m_upstream).close(ec);
		m_client_stream.close();

		if (m_owner)
			m_owner->remove_tcp_flow(m_key, this);
	}


#endif // defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

} // namespace proxy
