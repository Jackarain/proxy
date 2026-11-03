//
// proxy_session_connect.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_session.hpp"
#include "proxy/http_error_body.hpp"
#include "proxy/proxy_websocket_client.hpp"

namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// 连接相关

	net::awaitable<boost::system::error_code>
	proxy_session::connect_proxy_pass(tcp::socket& remote_socket, tcp::resolver::results_type targets) noexcept
	{
		auto proxy_hostname = m_proxy_pass->encoded_origin();

		log_conn_debug()
			<< ", starting connection to next proxy: "
			<< proxy_hostname;

		auto ec = co_await async_connect_targets(remote_socket, targets);
		if (ec)
		{
			log_conn_warning()
				<< ", connect to next proxy: "
				<< proxy_hostname
				<< ", error: "
				<< ec.message();

			co_return ec;
		}

		log_conn_debug()
			<< ", connect to next proxy: '"
			<< proxy_hostname
			<< "' successfully";

		co_return ec;
	}

	net::awaitable<boost::system::error_code>
	proxy_session::proxy_pass_handshake(tcp::socket& remote_socket,
		const std::string& target_host, uint16_t target_port,
		const urls::url& proxy_url, int command)
	{
		auto proxy_hostname = proxy_url.encoded_origin();

		// 如果启用了 noise, 则在向上游代理服务器发起 tcp 连接成功后, 发送 noise
		// 数据以及接收 noise 数据进行握手.
		if (m_option.scramble_)
		{
			if (!co_await noise_handshake(remote_socket, m_client_rx_key, m_client_tx_key))
				co_return make_error_code(boost::system::errc::io_error);

			log_conn_debug()
				<< ", next proxy: " << proxy_hostname
				<< ", with upstream noise completed";
		}

		auto proxy_pass_use_ssl = m_option.proxy_pass_use_ssl_;
		auto scheme = boost::to_lower_copy(std::string(proxy_url.scheme()));

		// 判断是否使用 ssl 加密与下一级代理通信.
		// 这里仅当 scheme 为 socks协议后辍是 s 时启用 ssl 加密.
		// 其它 scheme 仍按 scheme 含义中是否包括 ssl 加密来处理.
		if (proxy_scheme_use_ssl(scheme))
			proxy_pass_use_ssl = true;

		// 初始化 m_remote_socket 为 variant_stream_type 类型, 若是 ssl/tls 加
		// 密, 则初始化为 ssl_tcp_stream.
		m_remote_socket = std::move(
			co_await instantiate_proxy_pass(std::move(remote_socket), proxy_pass_use_ssl));

		log_conn_debug()
			<< ", next proxy: "
			<< proxy_hostname
			<< ", handshake with "
			<< std::string(scheme)
			<< ", target: "
			<< target_host
			<< ":"
			<< target_port;

		boost::system::error_code ec;

		// 根据 scheme 向 proxy_pass 进行代理握手相关实现.
		if (scheme.starts_with("socks"))
		{
			socks_client_option opt;

			opt.target_host = target_host;
			opt.target_port = target_port;
			opt.proxy_hostname = is_hostname(target_host);
			opt.command = command;
			opt.username = std::string(proxy_url.user());
			opt.password = std::string(proxy_url.password());

			if (scheme.starts_with("socks4a"))
				opt.version = socks4a_version;
			else if (scheme.starts_with("socks4"))
				opt.version = socks4_version;

			auto endpoint = co_await async_socks_handshake(
				m_remote_socket,
				opt,
				net_awaitable[ec]);
			if (endpoint)
			{
				m_remote_udp_endpoint = *endpoint;
				if (m_remote_udp_endpoint.address() == net::ip::make_address_v4("0.0.0.0") ||
					m_remote_udp_endpoint.address() == net::ip::make_address_v6("::0"))
				{
					m_remote_udp_endpoint.address(tcp_remote_endpoint(m_remote_socket).address());
				}
			}
		}
		else if (scheme.starts_with("http"))
		{
			http_proxy_client_option opt;

			opt.target_host = target_host;
			opt.target_port = target_port;
			opt.username = std::string(proxy_url.user());
			opt.password = std::string(proxy_url.password());

			if (!m_option.scramble_ && (m_option.noise_length_ >= 16 && m_option.noise_length_ <= 64 * 1024))
			{
				// 如果配置了 noise_length_ 则启用 http 代理握手中的 padding, 以迷惑扫描者无法通过
				// tcp 数据发送长度特征来识别出是 http 代理握手数据.
				std::mt19937 gen(global_random_device()());
				std::uniform_int_distribution<int64_t> dis(16, m_option.noise_length_);

				auto padding = generate_noise(global_random_device(), static_cast<uint16_t>(dis(gen)));
				auto len = boost::beast::detail::base64::encoded_size(padding.size());
				std::string padding_value(len, 0);
				len = beast::detail::base64::encode(padding_value.data(), padding.data(), padding.size());
				padding_value.resize(len);

				opt.extra_headers.insert("Proxy-Padding", padding_value);
			}

			if (command == SOCKS5_CMD_UDP)
			{
				ec = net::error::operation_not_supported;
			}
			else
			{
				co_await async_http_proxy_handshake(
					m_remote_socket,
					opt,
					net_awaitable[ec],
					&m_remote_buffer);
			}
		}
		else if (scheme == "ws" || scheme == "wss")
		{
			if (command == SOCKS5_CMD_UDP)
			{
				ec = net::error::operation_not_supported;
			}
			else
			{
				auto options = make_websocket_handshake_options(
					proxy_url, target_host, target_port);
				ec = co_await async_websocket_proxy_handshake(
					m_remote_socket, std::move(options));
			}
		}

		if (ec)
		{
			log_conn_warning()
				<< ", next proxy: "
				<< proxy_hostname
				<< ", handshake error: "
				<< ec.message();

			co_return ec;
		}

		co_return ec;
	}


	net::awaitable<variant_stream_type>
	proxy_session::instantiate_proxy_pass(tcp::socket remote_socket, bool proxy_pass_use_ssl) noexcept
	{
		boost::system::error_code ec;

		if (!proxy_pass_use_ssl)
		{
			// 非 ssl 加密, 直接使用 tcp socket 实例化成 variant_stream_type 类型的对象.
			auto sock_stream = init_proxy_stream(std::move(remote_socket));
			auto& sock = boost::variant2::get<proxy_tcp_socket>(sock_stream);

			if (!m_option.scramble_)
				co_return sock_stream;

			// 若是 scramble 混淆, 则配置相应的 key.
			sock.set_scramble_key(m_client_tx_key);
			sock.set_unscramble_key(m_client_rx_key);

			co_return sock_stream;
		}

		// 获取 proxy_pass 的主机名称.
		auto proxy_host = std::string(m_proxy_pass->host_address());

		// 若用户配置了 SNI, 则使用用户配置的 SNI 否则使用默认 hostname.
		std::string sni = m_option.proxy_ssl_name_.empty() ? proxy_host : m_option.proxy_ssl_name_;

		// 获取进程内共享的 SSL context (按 是否校验证书/SNI/自定义 CA 路径 缓存),
		// 避免每个会话都重新创建 context 并加载 CA 根证书; 该操作开销很大, 在连接
		// 频繁时会成为 CPU 热点.
		std::string ctx_tag;
		ctx_tag.reserve(sni.size() + m_option.ssl_cacert_path_.size() + 16);
		ctx_tag += "proxy_pass\n";
		ctx_tag += m_option.disable_check_cert_ ? '1' : '0';
		ctx_tag += '\n';
		ctx_tag += sni;
		ctx_tag += '\n';
		ctx_tag += m_option.ssl_cacert_path_;

		auto cli_ctx = get_cached_ssl_client_ctx(ctx_tag,
			[this, &sni](net::ssl::context& ctx) noexcept
				-> boost::system::error_code
			{
				// 使用通用函数配置 SSL context (验证模式、CA 证书、主机名验证).
				boost::system::error_code cec = configure_ssl_client_ctx(ctx,
					m_option.disable_check_cert_,
					sni,
					m_option.ssl_cacert_path_);
				if (cec)
				{
					log_conn_warning()
						<< ", configure ssl context error: " << cec.message();
				}

				// 额外的 SSL 上下文配置: DH 参数和 ALPN 协议.
				ctx.use_tmp_dh(net::buffer(default_dh_param()), cec);
				SSL_CTX_set_alpn_protos(ctx.native_handle(),
					(const unsigned char *)"\x08http/1.1", 9);

				return {};
			});

		if (cli_ctx.has_error())
		{
			log_conn_warning()
				<< ", get ssl context error: " << cli_ctx.error().message();
			co_return init_proxy_stream(std::move(remote_socket));
		}

		// 生成 ssl_tcp_stream 的 variant_stream_type 对象.
		auto sock_stream = init_proxy_stream(
			std::move(remote_socket), *cli_ctx.value());

		// get origin ssl stream type.
		ssl_tcp_stream& ssl_socket = boost::variant2::get<ssl_tcp_stream>(sock_stream);

		if (m_option.scramble_)
		{
			// 若是 scramble 混淆, 则配置相应的 key.
			auto& next_layer = ssl_socket.next_layer();

			next_layer.set_scramble_key(m_client_tx_key);
			next_layer.set_unscramble_key(m_client_rx_key);
		}

		// Set SNI Hostname.
		if (!SSL_set_tlsext_host_name(ssl_socket.native_handle(), sni.c_str()))
		{
			log_conn_warning()
				<< ", set ssl sni name: "
				<< sni
				<< " failed, error: "
				<< ::ERR_get_error();
		}

		log_conn_debug() << ", performing TLS handshake with " << sni << "...";

		// do async handshake.
		co_await ssl_socket.async_handshake(net::ssl::stream_base::client, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", TLS handshake failed with " << proxy_host
				<< ", error: " << ec.message();
		}

		log_conn_debug() << ", TLS handshake with " << proxy_host << " completed successfully";

		co_return sock_stream;
	}

} // namespace proxy
