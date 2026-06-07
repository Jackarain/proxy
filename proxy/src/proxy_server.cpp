//
// proxy_server.cpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_server.hpp"

namespace proxy {

//////////////////////////////////////////////////////////////////////////

proxy_server::proxy_server(net::any_io_executor executor, proxy_server_option opt)
	: m_executor(executor)
	, m_option(std::move(opt))
	, m_timer(executor)
{
	if (!m_option.stdio_target_.empty())
		return;

	m_certificates = &m_certificate_master;

	init_ssl_context();

	boost::system::error_code ec;

	if (fs::exists(m_option.ipip_db_, ec))
	{
		try {
			m_ipdb = std::make_unique<ip_ipdb>();
			if (!m_ipdb->load(m_option.ipip_db_))
			{
				m_ipdb = std::make_unique<ip_datx>();
				if (!m_ipdb->load(m_option.ipip_db_))
					m_ipdb.reset();
			}
		} catch (const std::exception& e) {
			XLOG_WARN << "ipip database " << m_option.ipip_db_ << ", load error: " << e.what();
		}
	}

	init_acceptor();
}

std::shared_ptr<proxy_server>
proxy_server::make(net::any_io_executor executor, proxy_server_option opt)
{
	return std::shared_ptr<proxy_server>(new
		proxy_server(executor, opt));
}

bool proxy_server::rfc2818_verification_match_pattern(
	const char* pattern, std::size_t pattern_length, const char* host)
{
	const char* p = pattern;
	const char* p_end = p + pattern_length;
	const char* h = host;

	while (p != p_end && *h)
	{
		if (*p == '*')
		{
			++p;
			while (*h && *h != '.')
			{
				if (rfc2818_verification_match_pattern(p, p_end - p, h++))
					return true;
			}
		}
		else if (std::tolower(*p) == std::tolower(*h))
		{
			++p;
			++h;
		}
		else
		{
			return false;
		}
	}

	return p == p_end && !*h;
}

pem_file proxy_server::determine_pem_type(const fs::path& filepath) noexcept
{
	pem_file result{ filepath, pem_type::none };

	boost::system::error_code ec;

	// 文件过大跳过, ssl 证书及密钥相关文件通常不可能超过1M大小.
	auto filesize = fs::file_size(filepath, ec);
	if (filesize > 1 * 1024 * 1024 || ec)
		return result;

	boost::nowide::fstream file(filepath, std::ios::in | std::ios::binary);
	if (!file.is_open())
		return result;

	if (filepath.filename() == "password.txt" ||
		filepath.filename() == "passwd.txt" ||
		filepath.filename() == "passwd" ||
		filepath.filename() == "password" ||
		filepath.filename() == "passphrase" ||
		filepath.filename() == "passphrase.txt")
	{
		result.type_ = pem_type::pwd;
		return result;
	}

	proxy::pem_type type = pem_type::none;
	std::string line;

	boost::regex re(R"(-----BEGIN\s.*\s?PRIVATE\sKEY-----)");
	boost::smatch what;

	while (std::getline(file, line))
	{
		if (line.find("-----BEGIN CERTIFICATE-----") != std::string::npos)
		{
			type = pem_type::cert;
			result.chains_++;
			continue;
		}
		else if (line.find("DH PARAMETERS-----") != std::string::npos)
		{
			type = pem_type::dhparam;
			break;
		}
		else if (boost::regex_search(line, what, re))
		{
			type = pem_type::key;
			break;
		}
	}
	result.type_ = type;

	return result;
}

void proxy_server::walk_certificate(
	const fs::path& directory, std::vector<certificate_file>& certificates) noexcept
{
	if (!fs::exists(directory) || !fs::is_directory(directory))
	{
		XLOG_WARN << "Path is not a directory or doesn't exist: " << directory;
		return;
	}

	certificate_file file;

	for (const auto& entry : fs::directory_iterator(directory, fs::directory_options::skip_permission_denied))
	{
		if (entry.is_directory())
		{
			walk_certificate(entry.path(), certificates);
			continue;
		}

		if (entry.is_regular_file())
		{
			// 读取文件, 并判断文件类型.
			auto type = determine_pem_type(entry.path());
			switch (type.type_)
			{
				case pem_type::cert:
					if (type.chains_ > file.cert_.chains_)
						file.cert_ = type;
					break;
				case pem_type::key:
					file.key_ = type;
					break;
				case pem_type::dhparam:
					file.dhparam_ = type;
					break;
				case pem_type::pwd:
					file.pwd_ = type;
					break;
				default:
					break;
			}
		}
	}

	// 如果找到了证书文件, 创建一个证书文件对象.
	if (file.cert_.type_ != pem_type::none &&
		file.key_.type_ != pem_type::none)
	{
		// 创建 ssl context 对象.
		file.ssl_context_.emplace(net::ssl::context::sslv23);

		auto& ssl_ctx = file.ssl_context_.value();

		// 设置 ssl context 选项.
		ssl_ctx.set_options(
			net::ssl::context::default_workarounds
			| net::ssl::context::no_sslv2
			| net::ssl::context::no_sslv3
			| net::ssl::context::no_tlsv1
			| net::ssl::context::no_tlsv1_1
			| net::ssl::context::single_dh_use
		);

		// 如果设置了 ssl_prefer_server_ciphers_ 则设置 SSL_OP_CIPHER_SERVER_PREFERENCE.
		if (m_option.ssl_prefer_server_ciphers_)
			ssl_ctx.set_options(SSL_OP_CIPHER_SERVER_PREFERENCE);

		// 默认的 ssl ciphers.
		const std::string ssl_ciphers = "HIGH:!aNULL:!MD5:!3DES";
		if (m_option.ssl_ciphers_.empty())
			m_option.ssl_ciphers_ = ssl_ciphers;

		// 设置 ssl ciphers.
		SSL_CTX_set_cipher_list(ssl_ctx.native_handle(),
			m_option.ssl_ciphers_.c_str());
		// 设置 alpn 协议.
		SSL_CTX_set_alpn_select_cb(ssl_ctx.native_handle(),
			alpn_select_proto_cb, (void*)this);

		// 设置证书文件.
		boost::system::error_code ec;
		ssl_ctx.use_certificate_chain_file(file.cert_.filepath_.string(), ec);
		if (ec)
		{
			XLOG_WARN << "use_certificate_chain_file: "
				<< file.cert_.filepath_
				<< ", error: "
				<< ec.message();
			return;
		}

		// 设置 password 文件, 如果存在的话.
		if (file.pwd_.type_ != pem_type::none && fs::exists(file.pwd_.filepath_))
		{
			auto pwd = file.pwd_.filepath_;

			ssl_ctx.set_password_callback(
				[pwd]([[maybe_unused]] auto... args) {
					std::string password;
					fileop::read(pwd, password);
					return password;
				}
			);
		}

		// 设置私钥文件.
		ssl_ctx.use_private_key_file(
			file.key_.filepath_.string(),
			net::ssl::context::pem, ec);
		if (ec)
		{
			XLOG_WARN << "use_private_key_file: "
				<< file.key_.filepath_
				<< ", error: "
				<< ec.message();
			return;
		}

		// 设置 dhparam 文件, 如果存在的话.
		if (file.dhparam_.type_ != pem_type::none && fs::exists(file.dhparam_.filepath_))
		{
			ssl_ctx.use_tmp_dh_file(file.dhparam_.filepath_.string(), ec);
			if (ec)
			{
				XLOG_WARN << "use_tmp_dh_file: "
					<< file.dhparam_.filepath_
					<< ", error: "
					<< ec.message();
				return;
			}
		}

		// 设置证书过期时间和域名.
		X509* x509_cert = SSL_CTX_get0_certificate(ssl_ctx.native_handle());
		const auto expire_date = X509_getm_notAfter(x509_cert);

#ifdef OPENSSL_IS_BORINGSSL
		std::time_t expiration_time;
		ASN1_TIME_to_time_t(expire_date, &expiration_time);
		file.expire_date_ = boost::posix_time::from_time_t(expiration_time);
#else
		std::tm expire_date_tm;
		ASN1_TIME_to_tm(expire_date, &expire_date_tm);
		file.expire_date_ = boost::posix_time::ptime_from_tm(expire_date_tm);
#endif
		std::unique_ptr<GENERAL_NAMES, decltype(&GENERAL_NAMES_free)> general_names{
			static_cast<GENERAL_NAMES*>(X509_get_ext_d2i(x509_cert, NID_subject_alt_name, 0, 0)),
			&GENERAL_NAMES_free
		};

		if (general_names)
		{
			for (int i = 0; i < sk_GENERAL_NAME_num(general_names.get()); i++)
			{
				GENERAL_NAME* gen = sk_GENERAL_NAME_value(general_names.get(), i);
				if (gen->type == GEN_DNS)
				{
					const ASN1_IA5STRING* domain = gen->d.dNSName;
					auto* non_const_domain = const_cast<ASN1_STRING*>(domain);

					if (ASN1_STRING_type(non_const_domain) == V_ASN1_IA5STRING &&
						ASN1_STRING_get0_data(non_const_domain) &&
						ASN1_STRING_length(non_const_domain))
					{
						file.subject_alt_name_.emplace_back(
							(const char*)(ASN1_STRING_get0_data(non_const_domain)),
							ASN1_STRING_length(non_const_domain)
						);
					}
				}
			}
		}
		else
		{
			XLOG_DBG << "No subject alternative name, will use Common Name as fallback.";
		}

		char cert_cname[256] = { 0 };
		{
			auto* x509_name = X509_get_subject_name(x509_cert);
			int idx = X509_NAME_get_index_by_NID(x509_name, NID_commonName, -1);
			if (idx >= 0)
			{
				auto* entry = X509_NAME_get_entry(x509_name, idx);
				if (entry)
				{
					auto* data = X509_NAME_ENTRY_get_data(entry);
					if (data && ASN1_STRING_length(data) > 0)
					{
						int copy_len = (std::min)(ASN1_STRING_length(data), (int)(sizeof(cert_cname) - 1));
						memcpy(cert_cname, ASN1_STRING_get0_data(data), copy_len);
						cert_cname[copy_len] = '\0';
					}
				}
			}
		}
		file.domain_ = cert_cname;

		// 保存到 certificates 中.
		certificates.emplace_back(std::move(file));
	}
}

void proxy_server::init_acceptor() noexcept
{
	auto& endps = m_option.listens_;

	for (const auto& [endp, v6only] : endps)
	{
		tcp_acceptor acceptor(m_executor);
		boost::system::error_code ec;

		acceptor.open(endp.protocol(), ec);
		if (ec)
		{
			XLOG_WARN << "acceptor open: " << endp
				<< ", error: " << ec.message();
			continue;
		}

		acceptor.set_option(net::socket_base::reuse_address(true), ec);
		if (ec)
		{
			XLOG_WARN << "acceptor set_option with reuse_address: "
				<< ec.message();
		}

		if (m_option.reuse_port_)
		{
#ifdef ENABLE_REUSEPORT
			using net::detail::socket_option::boolean;
			using reuse_port = boolean<SOL_SOCKET, SO_REUSEPORT>;

			acceptor.set_option(reuse_port(true), ec);
			if (ec)
			{
				XLOG_WARN << "acceptor set_option with SO_REUSEPORT: "
					<< ec.message();
			}
#endif
		}

		if (v6only)
		{
			acceptor.set_option(net::ip::v6_only(true), ec);
			if (ec)
			{
				XLOG_ERR << "TCP server accept "
					<< "set v6_only failed: " << ec.message();
				continue;
			}
		}

		acceptor.bind(endp, ec);
		if (ec)
		{
			XLOG_ERR << "acceptor bind: " << endp
				<< ", error: " << ec.message();
			continue;
		}

		acceptor.listen(net::socket_base::max_listen_connections, ec);
		if (ec)
		{
			XLOG_ERR << "acceptor listen: " << endp
				<< ", error: " << ec.message();
			continue;
		}

		m_tcp_acceptors.emplace_back(std::move(acceptor));
	}

	auto& uds_endps = m_option.uds_listens_;

	for (const auto& endp : uds_endps)
	{
		try
		{
			m_unix_acceptors.emplace_back(m_executor, endp, false);
		}
		catch (const std::exception& e)
		{
			XLOG_ERR << "unix domain socket acceptor listen: " << endp.path()
				<< ", error: " << e.what();
			continue;
		}
	}

#if defined(__linux__) && defined(IP_TRANSPARENT)
	// 创建 UDP TPROXY 透明代理 sockets，用于接收被重定向的 UDP 数据包.
	if (m_option.proxy_pass_ && m_option.transparent_)
	{
		for (const auto& [tcp_endp, v6only] : m_option.listens_)
		{
			(void)v6only;
			net::ip::udp::socket udp_sock(m_executor);
			boost::system::error_code ec;

			// 从 TCP endpoint 构造对应的 UDP endpoint.
			net::ip::udp::endpoint udp_endp(
				tcp_endp.address(), tcp_endp.port());

			udp_sock.open(udp_endp.protocol(), ec);
			if (ec)
			{
				XLOG_WARN << "udp tproxy open: "
					<< udp_endp << ", error: " << ec.message();
				continue;
			}

			udp_sock.set_option(
				net::socket_base::reuse_address(true), ec);

			// 设置 IP_TRANSPARENT 选项.
			using transparent_udp = net::detail::socket_option::boolean<
				IPPROTO_IP, IP_TRANSPARENT>;
			using transparent6_udp = net::detail::socket_option::boolean<
				IPPROTO_IPV6, IPV6_TRANSPARENT>;

			// 设置 IP_RECVORIGDSTADDR 以接收原始目标地址.
			int opt = 1;
			if (udp_endp.protocol() == net::ip::udp::v4())
			{
				udp_sock.set_option(transparent_udp(true), ec);
				::setsockopt(udp_sock.native_handle(), IPPROTO_IP,
					IP_RECVORIGDSTADDR, &opt, sizeof(opt));
			}
			else
			{
				udp_sock.set_option(transparent6_udp(true), ec);
				::setsockopt(udp_sock.native_handle(), IPPROTO_IPV6,
					IPV6_RECVORIGDSTADDR, &opt, sizeof(opt));
			}

			udp_sock.bind(udp_endp, ec);
			if (ec)
			{
				XLOG_ERR << "udp tproxy bind: " << udp_endp
					<< ", error: " << ec.message();
				continue;
			}

			XLOG_DBG << "udp tproxy listen on: " << udp_endp;
			m_udp_tproxy_listeners.push_back(std::move(udp_sock));
		}
	}
#endif // defined(__linux__) && defined(IP_TRANSPARENT)
}

void proxy_server::update_certificate(
	const fs::path& directory, std::vector<certificate_file>& certificates) noexcept
{
	// 清空现有证书.
	certificates.clear();

	// 扫描证书文件.
	walk_certificate(directory, certificates);

	// 按过期时间排序.
	std::stable_sort(certificates.begin(), certificates.end(),
		[](const certificate_file& a, const certificate_file& b) {
			return a.expire_date_ < b.expire_date_;
		});

	auto print_path = [](const std::string& prefix, const fs::path path)
	{
		return path.empty() ? "" : prefix + path.string();
	};

	for (const auto& ctx : certificates)
	{
		XLOG_DBG << "domain: '" << ctx.domain_
			<< "', expire: '" << ctx.expire_date_
			<< print_path("', cert: '", ctx.cert_.filepath_)
			<< print_path("', key: '", ctx.key_.filepath_)
			<< print_path("', dhparam: '", ctx.dhparam_.filepath_)
			<< print_path("', pwd: '", ctx.pwd_.filepath_);
	}
}

void proxy_server::init_ssl_context() noexcept
{
	// 如果没有设置证书文件, 则直接返回.
	if (m_option.ssl_cert_path_.empty())
		return;

	// 读取并更新证书文件.
	update_certificate(m_option.ssl_cert_path_, *m_certificates);

	// 设置 SNI 回调函数.
	SSL_CTX_set_tlsext_servername_callback(
		m_ssl_srv_context.native_handle(), proxy_server::ssl_sni_callback);
	SSL_CTX_set_tlsext_servername_arg(m_ssl_srv_context.native_handle(), this);

	// 设置 ALPN 回调函数.
	SSL_CTX_set_alpn_select_cb(m_ssl_srv_context.native_handle(),
		alpn_select_proto_cb, (void*)this);
}

int proxy_server::alpn_select_proto_cb(SSL *ssl, const unsigned char **out,
                                unsigned char *outlen, const unsigned char *in,
                                unsigned int inlen, void *arg)
{
	proxy_server* self = (proxy_server*)arg;
	return self->alpn_select_proto(ssl, out, outlen, in, inlen);
}

int proxy_server::alpn_select_proto(SSL *ssl, const unsigned char **out,
	unsigned char *outlen, const unsigned char *in,
	unsigned int inlen) noexcept
{
	(void)ssl;
	int ret = SSL_select_next_proto((unsigned char **)out, outlen,
									in, inlen,
									(const unsigned char *)"\x8http/1.1", 9);
	if (ret == OPENSSL_NPN_NEGOTIATED)
		return SSL_TLSEXT_ERR_OK;

	XLOG_DBG << "ALPN negotiation failed: "
		<< inlen << " " << std::string((const char*)in, inlen);

	return SSL_TLSEXT_ERR_ALERT_FATAL;
}

int proxy_server::ssl_sni_callback(SSL *ssl, int *ad, void *arg)
{
	proxy_server* self = (proxy_server*)arg;
	return self->sni_callback(ssl, ad);
}

int proxy_server::sni_callback(SSL *ssl, [[maybe_unused]] int *ad) noexcept
{
	if (!m_certificates)
		return SSL_TLSEXT_ERR_OK;

	auto& certificates = *m_certificates;
	if (certificates.empty())
		return SSL_TLSEXT_ERR_OK;

	certificate_file* default_ctx = nullptr;
	for (auto& c : certificates)
	{
		if (c.ssl_context_.has_value())
		{
			default_ctx = &c;
			break;
		}
	}

	if (!default_ctx)
		return SSL_TLSEXT_ERR_OK;

	const char *servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	if (!servername)
	{
		SSL_set_SSL_CTX(ssl, default_ctx->ssl_context_->native_handle());
		return SSL_TLSEXT_ERR_OK;
	}

	for (auto& ctx : certificates)
	{
		if (!ctx.ssl_context_.has_value())
			continue;

		if (!ctx.domain_.empty() &&
			rfc2818_verification_match_pattern(ctx.domain_.c_str(), ctx.domain_.size(), servername))
		{
			SSL_set_SSL_CTX(ssl, ctx.ssl_context_->native_handle());
			return SSL_TLSEXT_ERR_OK;
		}

		for (auto& alt_name : ctx.subject_alt_name_)
		{
			if (rfc2818_verification_match_pattern(alt_name.c_str(), alt_name.length(), servername))
			{
				SSL_set_SSL_CTX(ssl, ctx.ssl_context_->native_handle());
				return SSL_TLSEXT_ERR_OK;
			}
		}
	}

	SSL_set_SSL_CTX(ssl, default_ctx->ssl_context_->native_handle());

	return SSL_TLSEXT_ERR_OK;
}

net::awaitable<void> proxy_server::certificate_check_timer()
{
	auto self = shared_from_this();
	boost::system::error_code ec;

	// 定时检查证书是否有过期, 如果有过期证书, 则5分钟检查一次证书信息.
	while (!m_abort)
	{
		auto now = boost::posix_time::second_clock::local_time();
		std::chrono::seconds duration(std::chrono::years(1));

		auto& certificates = *m_certificates;

		for (const auto& ctx : certificates)
		{
			if (now > ctx.expire_date_)
			{
				XLOG_WARN << "domain: '" << ctx.domain_
					<< "', cert: '" << ctx.cert_.filepath_.string()
					<< "', key: '" << ctx.key_.filepath_.string()
					<< "', dhparam: '" << ctx.dhparam_.filepath_.string()
					<< "', pwd: '" << ctx.pwd_.filepath_.string()
					<< "', expired: '" << ctx.expire_date_ << "'";

				// 有证书过期, 设定为5分钟检查一次证书目录.
				duration = std::chrono::minutes(5);
				continue;
			}

			// 设置为过期时检查证书.
			auto expire_date = std::chrono::seconds((ctx.expire_date_ - now).total_seconds());
			duration = std::min(duration, expire_date);
		}

		// 每隔 duration 检查一次证书是否过期.
		m_timer.expires_after(duration);
		co_await m_timer.async_wait(net_awaitable[ec]);
		if (ec)
			break;

		// 热更新证书, 交替更新证书容器 master/slave.
		if (m_certificates == &m_certificate_master)
		{
			update_certificate(m_option.ssl_cert_path_, m_certificate_slave);
			m_certificates = &m_certificate_slave;
		}
		else
		{
			update_certificate(m_option.ssl_cert_path_, m_certificate_master);
			m_certificates = &m_certificate_master;
		}
	}

	co_return;
}

void proxy_server::start() noexcept
{
	m_scheduler_locking = net::config(m_executor.context()).get("scheduler", "locking", true);

	// 运行后端任务线程.
	if (!m_scheduler_locking)
	{
		auto self = shared_from_this();
		m_backend_thread = std::make_unique<std::thread>([this, self]() mutable
			{
				backend_thread_run();
			});
	}

	// 如果是 stdio 模式, 则直接启动 stdio 监听协程.
	if (!m_option.stdio_target_.empty())
	{
		auto self = shared_from_this();

		net::co_spawn(m_executor, [this, self]() -> net::awaitable<void>
			{
				try
				{
					// 使用 stdio socket 初始化 proxy session.
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
					net::posix::stream_descriptor stream_in(m_executor, ::dup(STDIN_FILENO));
					stdio_stream stream(std::move(stream_in));
#else
					std::shared_ptr<net::io_context> in_ctx = std::make_shared<net::io_context>(1);
					std::thread([in_ctx]() mutable
						{
							auto work_guard = net::make_work_guard(*in_ctx);

							try
							{
								in_ctx->run();
							}
							catch (const std::exception&)
							{}

							XLOG_DBG << "stdio input context thread exit";

						}).detach();


					stdio_stream stream(in_ctx->get_executor(), m_executor);
#endif
					// 创建 proxy session 对象.
					auto new_session =
						std::make_shared<proxy_session>(
							m_executor,
							m_backend_context,
							m_scheduler_locking,
							m_dns_cache,
							init_proxy_stream(std::move(stream)),
							0,
							self);

					// 启动 proxy_session 对象.
					new_session->start();
				}
				catch (const std::exception& e)
				{
					XLOG_ERR << "stdio proxy exception: " << e.what();
				}

				co_return;
			}, net::detached);

		return;
	}

	// 如果作为透明代理.
	if (m_option.transparent_)
	{
#if defined(__linux__)

#  if defined (IP_TRANSPARENT) && defined (IPV6_TRANSPARENT)
		// 设置 acceptor 为透明代理模式.
		using transparent = net::detail::socket_option::boolean<
			IPPROTO_IP, IP_TRANSPARENT>;
		using transparent6 = net::detail::socket_option::boolean<
			IPPROTO_IPV6, IPV6_TRANSPARENT>;

		for (auto& acceptor : m_tcp_acceptors)
		{
			boost::system::error_code error;

			acceptor.set_option(transparent(true), error);
			acceptor.set_option(transparent6(true), error);
		}
#  endif

#else
		XLOG_WARN << "transparent proxy only support linux";
#endif
		// 获取所有本机 ip 地址.
		net::co_spawn(m_executor,
			get_local_address(), net::detached);
	}

	// 同时启动32个连接协程为每个 acceptor 用于为 proxy client 提供服务.
	for (auto& acceptor : m_tcp_acceptors)
	{
		for (int i = 0; i < 32; i++)
		{
			net::co_spawn(m_executor,
				start_proxy_listen(acceptor), net::detached);
		}
	}

	// 同时启动32个连接协程为每个 acceptor 用于为 proxy client 提供服务.
	for (auto& acceptor : m_unix_acceptors)
	{
		for (int i = 0; i < 32; i++)
		{
			net::co_spawn(m_executor,
				start_proxy_listen(acceptor), net::detached);
		}
	}

#if defined(__linux__)
	if (m_option.transparent_)
	{
		net::co_spawn(m_executor,
			start_udp_tproxy(), net::detached);
	}
#endif // defined(__linux__)

	// 启动 证书检查定时器.
	net::co_spawn(m_executor,
		certificate_check_timer(), net::detached);
}

void proxy_server::close() noexcept
{
	boost::system::error_code ignore_ec;
	m_abort = true;

	m_backend_context.stop();
	if (m_backend_thread && m_backend_thread->joinable())
		m_backend_thread->join();

	m_timer.cancel();

	for (auto& acceptor : m_tcp_acceptors)
		acceptor.close(ignore_ec);
	for (auto& acceptor : m_unix_acceptors)
		acceptor.close(ignore_ec);

#if defined(__linux__)
	// 关闭 UDP TPROXY 相关资源.
	{
		std::lock_guard<std::mutex> lock(m_udp_flows_mutex);
		for (auto& [key, flow] : m_udp_tproxy_flows)
		{
			if (flow)
				flow->relay_sock_.reset();
		}
		m_udp_tproxy_flows.clear();
	}
	for (auto& s : m_udp_tproxy_listeners)
		s.close(ignore_ec);
#endif // defined(__linux__)

	for (auto& [id, c] : m_clients)
	{
		if (auto client = c.lock())
			client->close();
	}
}

void proxy_server::remove_session(size_t id)
{
	m_clients.erase(id);
}

size_t proxy_server::num_session()
{
	return m_clients.size();
}

const proxy_server_option& proxy_server::option()
{
	return m_option;
}

net::ssl::context& proxy_server::ssl_context()
{
	return m_ssl_srv_context;
}

net::awaitable<std::optional<net::ip::tcp::endpoint>>
proxy_server::setup_tproxy(proxy_tcp_socket& socket, size_t connection_id) noexcept
{
#ifndef SO_ORIGINAL_DST
#  define SO_ORIGINAL_DST 80
#endif
	auto sockfd = socket.native_handle();
	std::optional<net::ip::tcp::endpoint> remote_endp;

	sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);

	if (::getsockopt(sockfd, IPPROTO_IP, SO_ORIGINAL_DST, (char*)&addr, &addrlen) < 0)
	{
		XLOG_WARN << "connection id: " << connection_id
			<< ", getsockopt: " << (int)sockfd
			<< ", SO_ORIGINAL_DST: " << strerror(errno);
		co_return remote_endp;
	}

	if (addr.ss_family == AF_INET)
	{
		remote_endp.emplace();

		auto addr4 = reinterpret_cast<sockaddr_in*>(&addr);
		auto port = ntohs(addr4->sin_port);

		remote_endp->address(net::ip::address_v4(htonl(addr4->sin_addr.s_addr)));
		remote_endp->port(port);
	}
	else if (addr.ss_family == AF_INET6)
	{
		remote_endp.emplace();

		auto addr6 = reinterpret_cast<sockaddr_in6*>(&addr);
		auto port = ntohs(addr6->sin6_port);

		net::ip::address_v6::bytes_type bt;

		std::copy(std::begin(addr6->sin6_addr.s6_addr),
			std::end(addr6->sin6_addr.s6_addr), std::begin(bt));

		remote_endp->address(net::ip::make_address_v6(bt));
		remote_endp->port(port);
	}
	else
	{
		XLOG_WARN << "connection id: " << connection_id
			<< ", SO_ORIGINAL_DST unexpected family: " << addr.ss_family;
		co_return remote_endp;
	}

	XLOG_DBG << "connection id: " << connection_id << ", tproxy, remote: " << *remote_endp;

	// 请求的是本机的回环连接, 而不是 TPROXY 代理.
	if (remote_endp->address().is_loopback())
	{
		remote_endp.reset();
		co_return remote_endp;
	}

	// 如果 original dst 是本机地址 => 这不是 tproxy 转发目标
	if (m_local_addrs.find(remote_endp->address()) != m_local_addrs.end())
		remote_endp.reset();

	co_return remote_endp;
}

net::awaitable<void> proxy_server::get_local_address() noexcept
{
	auto self = shared_from_this();
	boost::system::error_code ec;

	auto hostname = net::ip::host_name(ec);
	if (ec)
	{
		XLOG_WARN
			<< "get_local_address, host_name: "
			<< ec.message();

		co_return;
	}

	if (!is_hostname(hostname))
	{
		m_local_addrs.insert(net::ip::make_address(hostname, ec));
		co_return;
	}

	// 切换线程到后端线程池中执行同步 dns 解析操作.
	auto executor = m_executor;
	if (!m_scheduler_locking)
	{
		co_await net::post(net::bind_executor(m_backend_context.get_executor(), net::use_awaitable));
		executor = m_backend_context.get_executor();
	}

	tcp::resolver resolver(executor);

	auto results = co_await resolver.async_resolve(hostname, "", net_awaitable[ec]);

	if (!m_scheduler_locking)
		co_await net::post(net::bind_executor(m_executor, net::use_awaitable));

	if (ec)
	{
		XLOG_WARN
			<< "get_local_address, async_resolve: "
			<< ec.message();

		co_return;
	}

	auto it = results.begin();
	while (it != results.end())
	{
		tcp::endpoint ep = *it++;
		m_local_addrs.insert(ep.address());
	}
}

// 判断 IP 地址是否在指定的 CIDR 范围.
bool proxy_server::ip_filter(const std::string& ip_cidr, const std::string& ip) const noexcept
{
	if (ip_cidr.empty() || ip.empty())
		return false;

	boost::system::error_code ec;

	auto ipaddr = net::ip::make_address(ip, ec);
	if (ec)
		return false;

	try
	{
		auto iponly = net::ip::make_address(ip_cidr, ec);
		if (!ec)
		{
			if (iponly == ipaddr)
				return true;
			return false;
		}

		auto netaddr4 = net::ip::make_network_v4(ip_cidr, ec);
		if (!ec)
		{
			auto target = net::ip::make_network_v4(ipaddr.to_v4(), netaddr4.netmask());
			if (target == netaddr4)
				return true;
			return false;
		}

		auto netaddr6 = net::ip::make_network_v6(ip_cidr, ec);
		if (!ec)
		{
			auto target = net::ip::make_network_v6(ipaddr.to_v6(), netaddr6.prefix_length());
			if (target == netaddr6)
				return true;
			return false;
		}
	}
	catch (const std::exception&)
	{}

	return false;
}

bool proxy_server::region_filter(const std::vector<std::string>& local_info) const noexcept
{
	const auto& deny_region = m_option.deny_regions_;
	const auto& allow_region = m_option.allow_regions_;

	if (deny_region.empty() && allow_region.empty())
		return true;

	auto rule_hit = [&](const std::string& rule) -> bool
		{
			for (const auto& item : local_info)
			{
				if (item == rule)
					return true;

				if (ip_filter(rule, item))
					return true;
			}
			return false;
		};

	for (const auto& rule : deny_region)
	{
		if (rule_hit(rule))
			return false;
	}

	if (!allow_region.empty())
	{
		for (const auto& rule : allow_region)
		{
			if (rule_hit(rule))
				return true;
		}
		return false;
	}

	return true;
}

void proxy_server::backend_thread_run() noexcept
{
	auto work = net::make_work_guard(m_backend_context);

	try
	{
		m_backend_context.run();
	}
	catch (const std::exception&)
	{}
}

#if defined(__linux__)

// msg 不能为 const，CMSG_NXTHDR 需要非 const msghdr*.
bool proxy_server::parse_udp_tproxy_packet(struct msghdr& msg, ssize_t ret,
	udp::endpoint& client_ep, udp::endpoint& original_dest)
{
	if (ret < 0) return false;

	// 提取原始目标地址.
	for (auto* cmsg = CMSG_FIRSTHDR(&msg); cmsg;
		 cmsg = CMSG_NXTHDR(&msg, cmsg))
	{
		if (cmsg->cmsg_level == IPPROTO_IP &&
			cmsg->cmsg_type == IP_ORIGDSTADDR)
		{
			auto* sin = (struct sockaddr_in*)CMSG_DATA(cmsg);
			original_dest.address(
				net::ip::address_v4(ntohl(sin->sin_addr.s_addr)));
			original_dest.port(ntohs(sin->sin_port));
			break;
		}
		if (cmsg->cmsg_level == IPPROTO_IPV6 &&
			cmsg->cmsg_type == IPV6_ORIGDSTADDR)
		{
			auto* sin6 = (struct sockaddr_in6*)CMSG_DATA(cmsg);
			net::ip::address_v6::bytes_type bt;
			std::memcpy(bt.data(), &sin6->sin6_addr, 16);
			original_dest.address(net::ip::make_address_v6(bt));
			original_dest.port(ntohs(sin6->sin6_port));
			break;
		}
	}

	if (original_dest.address().is_unspecified())
		return false;

	// 提取客户端端点.
	auto* ss = (struct sockaddr_storage*)msg.msg_name;
	if (ss->ss_family == AF_INET)
	{
		auto* sin = (struct sockaddr_in*)ss;
		client_ep.address(
			net::ip::address_v4(ntohl(sin->sin_addr.s_addr)));
		client_ep.port(ntohs(sin->sin_port));
	}
	else if (ss->ss_family == AF_INET6)
	{
		auto* sin6 = (struct sockaddr_in6*)ss;
		net::ip::address_v6::bytes_type bt;
		std::memcpy(bt.data(), &sin6->sin6_addr, 16);
		client_ep.address(net::ip::make_address_v6(bt));
		client_ep.port(ntohs(sin6->sin6_port));
	}
	else
	{
		return false;
	}

	return true;
}

// 为 (client_ep, original_dest) 生成查找 key.
size_t proxy_server::make_udp_flow_key(const udp::endpoint& client, const udp::endpoint& dest)
{
	size_t h = 0;
	auto combine = [&h](size_t v) {
		h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2);
	};

	if (client.address().is_v4())
	{
		combine(std::hash<uint32_t>{}(client.address().to_v4().to_uint()));
	}
	else
	{
		auto b = client.address().to_v6().to_bytes();
		combine(std::hash<std::string_view>{}(std::string_view((const char*)b.data(), b.size())));
	}
	combine(std::hash<uint16_t>{}(client.port()));

	if (dest.address().is_v4())
	{
		combine(std::hash<uint32_t>{}(dest.address().to_v4().to_uint()));
	}
	else
	{
		auto b = dest.address().to_v6().to_bytes();
		combine(std::hash<std::string_view>{}(std::string_view((const char*)b.data(), b.size())));
	}
	combine(std::hash<uint16_t>{}(dest.port()));

	return h;
}

net::awaitable<void> proxy_server::udp_tproxy_check() noexcept
{
	auto self = shared_from_this();

	boost::system::error_code ec;
	net::steady_timer timer(m_executor);

	while (!m_abort)
	{
		timer.expires_after(std::chrono::seconds(1));
		co_await timer.async_wait(net_awaitable[ec]);
		if (ec)
			break;

		std::lock_guard<std::mutex> lock(m_udp_flows_mutex);
		std::vector<size_t> expired_keys;
		for (auto [key, flow] : m_udp_tproxy_flows)
		{
			if (!flow)
			{
				expired_keys.push_back(key);
				continue;
			}

			if (flow->expire_++ > m_option.udp_timeout_)
			{
				XLOG_DBG << "udp tproxy flow expired: "
					<< flow->expire_ << ", key: "
					<< key << ", client: "
					<< flow->client_endp_ << ", dest: "
					<< flow->original_endp_;

				flow->relay_sock_.reset();
				expired_keys.push_back(key);
			}
		}

		for (auto key : expired_keys)
		{
			m_udp_tproxy_flows.erase(key);
		}
	}
}

net::awaitable<void> proxy_server::start_udp_tproxy() noexcept
{
	auto self = shared_from_this();

	// 检查配置项，UDP TPROXY 模式必须配置 proxy_pass 以转发数据包.
	if (!m_option.proxy_pass_)
	{
		XLOG_ERR << "udp tproxy requires a proxy_pass";
		co_return;
	}

	// 启动 UDP TPROXY 监听协程.
	for (auto& udp_sock : m_udp_tproxy_listeners)
	{
		net::co_spawn(m_executor,
			start_udp_tproxy_listen(udp_sock), net::detached);
	}

	// 启动 UDP TPROXY 检查协程, 定时检查是否有新的数据包需要处理.
	net::co_spawn(m_executor,
		udp_tproxy_check(), net::detached);

	boost::system::error_code ec;
	int retry_time_count = 1;

	// 保持与 proxy_pass 之间的 SOCKS5 UDP ASSOCIATE 转发, 以便后续数据包转发使用.
	while (!m_abort)
	{
		auto ret = co_await do_sock5_associate();
		if (!ret)
			co_return;

		// 启动定时器延时一会会，继续尝试.
		net::steady_timer timer(m_executor);

		timer.expires_after(std::chrono::seconds(retry_time_count));
		co_await timer.async_wait(net_awaitable[ec]);

		// 指数退避增加重试时间, 最多退避到 32 秒, 然后重置为 2 秒继续尝试.
		retry_time_count *= 2;
		if (retry_time_count > 32)
			retry_time_count = 2;
	}
}

// 解析 proxy_pass 地址并返回 endpoints.
net::awaitable<std::optional<tcp::resolver::results_type>>
proxy_server::resolve_proxy_pass(const boost::urls::url& proxy_pass)
{
	auto proxy_host = std::string(proxy_pass.host());
	uint16_t proxy_port = 0;

	if (proxy_pass.port_number() == 0)
		proxy_port = urls::default_port(proxy_pass.scheme_id());
	else
		proxy_port = proxy_pass.port_number();

	// IP 地址无需 DNS 解析.
	boost::system::error_code ec;
	if (!is_hostname(proxy_host))
	{
		tcp::endpoint endp(net::ip::make_address(proxy_host), proxy_port);
		co_return tcp::resolver::results_type::create(
			endp, proxy_host, proxy_pass.scheme());
	}

	// DNS 解析.
	auto ex = m_executor;
	if (!m_scheduler_locking)
	{
		co_await net::post(net::bind_executor(
				m_backend_context.get_executor(), net::use_awaitable));
		ex = m_backend_context.get_executor();
	}

	tcp::resolver resolver{ ex };
	auto targets = co_await resolver.async_resolve(
		proxy_host, std::to_string(proxy_port), net_awaitable[ec]);

	if (!m_scheduler_locking)
		co_await net::post(
			net::bind_executor(m_executor, net::use_awaitable));

	if (ec)
	{
		XLOG_WARN << "udp tproxy resolve: " << proxy_host
			<< ", error: " << ec.message();

		co_return std::optional<tcp::resolver::results_type>{};
	}

	co_return targets;
}

net::awaitable<boost::system::error_code>
proxy_server::connect_to_proxy(tcp::socket& remote_socket, const tcp::resolver::results_type& targets)
{
	boost::system::error_code ec;

	for (auto endp : targets)
	{
		remote_socket.close(ec);

		co_await remote_socket.async_connect(endp, net_awaitable[ec]);
		if (!ec)
		{
#if defined(__linux__)
			if (m_option.so_mark_)
			{
				uint32_t mark = m_option.so_mark_.value();
				if (::setsockopt(remote_socket.native_handle(), SOL_SOCKET, SO_MARK, &mark, sizeof(mark)) < 0)
				{
					XLOG_WARN << "connect_to_proxy setsockopt SO_MARK error: "
						<< strerror(errno);
				}
			}
#endif
			co_return ec;
		}
	}
	co_return boost::asio::error::host_not_found;
}

net::awaitable<bool> proxy_server::do_sock5_associate()
{
	auto proxy_pass = *m_option.proxy_pass_;

	auto proxy_host = std::string(proxy_pass.host());
	auto sni = m_option.proxy_ssl_name_.empty() ? proxy_host : m_option.proxy_ssl_name_;
	auto scheme = boost::to_lower_copy(std::string(proxy_pass.scheme()));

	// 目前仅支持 SOCKS 协议的 proxy_pass 转发, 因为 HTTP 代理不支持 UDP 转发.
	if (!scheme.starts_with("socks"))
	{
		XLOG_WARN << "udp tproxy only supports socks protocol, got: " << scheme;
		co_return false;
	}

	// 解析 proxy_pass 地址并连接到代理服务器. 这里如果 proxy_pass 是一个域名, 则会进
	// 行 DNS 解析以获取 IP 地址列表, 然后尝试连接列表中的每个地址直到成功连接为止.
	tcp::socket remote_socket(m_executor);
	boost::system::error_code ec;

	auto targets = co_await resolve_proxy_pass(proxy_pass);
	if (!targets || targets->empty())
		co_return false;

	// 连接到 proxy_pass 的 SOCKS5 服务器.
	ec = co_await connect_to_proxy(remote_socket, *targets);
	if (ec)
	{
		XLOG_WARN << "udp tproxy connect to proxy_pass failed: "
			<< ec.message();
		co_return true;
	}

	// 启动与 proxy_pass 的连接和协商以获取关联的 udp endpoint.
	socks_client_option opt;

	opt.target_host = "0.0.0.0";
	opt.target_port = 21;
	opt.proxy_hostname = true;
	opt.command = SOCKS5_CMD_UDP;
	opt.username = std::string(proxy_pass.user());
	opt.password = std::string(proxy_pass.password());

	if (scheme.starts_with("socks4a"))
		opt.version = socks4a_version;
	else if (scheme.starts_with("socks4"))
		opt.version = socks4_version;

	bool use_ssl = m_option.proxy_pass_use_ssl_;
	if (scheme.ends_with("s"))
		use_ssl = true;

	endpoint_opt endpoint;
	std::optional<variant_stream_type> socks5_control;

	if (use_ssl)
	{
		net::ssl::context cli_ctx(net::ssl::context::sslv23_client);
		if (fs::exists(m_option.ssl_cacert_path_))
		{
			cli_ctx.add_verify_path(m_option.ssl_cacert_path_, ec);
			if (ec)
			{
				XLOG_WARN << "udp tproxy add_verify_path error: " << ec.message();
				co_return false;
			}
		}

		auto verify = net::ssl::verify_peer;
		if (m_option.disable_check_cert_)
		{
			XLOG_DBG << "udp tproxy certificate verification will be disabled";
			verify = net::ssl::verify_none;
		}

		cli_ctx.set_verify_mode(verify);
		auto certs = default_root_certificates();

		cli_ctx.add_certificate_authority(net::buffer(certs.data(), certs.size()), ec);
		if (ec)
		{
			XLOG_WARN << "udp tproxy add_certificate_authority error: " << ec.message();
			co_return false;
		}
		cli_ctx.set_verify_callback(net::ssl::host_name_verification(sni), ec);
		if (ec)
		{
			XLOG_WARN << "udp tproxy set_verify_callback error: " << ec.message();
			co_return false;
		}

		// 初始化为 SSL 加密的 SOCKS5 控制连接.
		socks5_control.emplace(init_proxy_stream(std::move(remote_socket), cli_ctx));
		auto& ssl_sock = boost::variant2::get<ssl_tcp_stream>(*socks5_control);

		// 设置 SNI 主机名以兼容需要 SNI 的服务器.
		if (!SSL_set_tlsext_host_name(ssl_sock.native_handle(), sni.c_str()))
			XLOG_WARN << "udp tproxy set sni failed";

		// 进行 SSL 握手.
		co_await ssl_sock.async_handshake(
			net::ssl::stream_base::client, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "udp tproxy SSL handshake failed: " << ec.message();
			co_return false;
		}

		XLOG_DBG << "udp tproxy SSL handshake with " << sni << " succeeded";

		// 进行 SOCKS5 握手以获取 UDP 转发的目标 endpoint.
		endpoint = co_await async_socks_handshake(*socks5_control, opt, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "udp tproxy SOCKS5 handshake with ssl failed: " << ec.message();
			co_return false;
		}
	}
	else
	{
		socks5_control.emplace(init_proxy_stream(std::move(remote_socket)));

		// 进行 SOCKS5 握手以获取 UDP 转发的目标 endpoint.
		endpoint = co_await async_socks_handshake(*socks5_control, opt, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "udp tproxy SOCKS5 handshake failed: " << ec.message();
			co_return false;
		}
	}

	if (!endpoint)
	{
		XLOG_WARN << "udp tproxy failed to get relay endpoint from proxy_pass";
		co_return false;
	}

	// 保存 relay endpoint, 后续将通过这个 endpoint 将 UDP 数据包转发到 proxy_pass 服务
	// 器, 再由 proxy_pass 转发到实际目标服务器.
	m_relay_endp = *endpoint;

	if (m_relay_endp.address().is_unspecified())
	{
		net::ip::address remote_addr;
		if (use_ssl)
		{
			auto& ssl_sock = boost::variant2::get<ssl_tcp_stream>(*socks5_control);
			remote_addr = ssl_sock.lowest_layer().remote_endpoint(ec).address();
		}
		else
		{
			auto& tcp_sock = boost::variant2::get<proxy_tcp_socket>(*socks5_control);
			remote_addr = tcp_sock.lowest_layer().remote_endpoint(ec).address();
		}

		// 更新 relay endpoint 的地址为实际连接的远程地址, 因为某些代理服务器可能返回一个不完
		// 整的 endpoint, 只包含端口但地址为空的情况.
		if (!ec)
			m_relay_endp.address(remote_addr);
	}

	// 在这里循环等待，保持与 proxy_pass 的连接以维持 UDP ASSOCIATE 会话, 直到服务器关闭或发生错误.
	while (!m_abort)
	{
		char bufs[64];
		co_await socks5_control->async_read_some(
			net::buffer(bufs, sizeof(bufs)), net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "udp tproxy control connection error: " << ec.message();
			break;
		}
	}

	co_return true;
}

net::awaitable<void> proxy_server::udp_tproxy_response_loop(udp_tproxy_flow_ptr flow)
{
	auto self = shared_from_this();

	if (!flow)
		co_return;

	// 退出时清理 flow, 防止 map 泄漏.
	boost::scope::scope_exit flows_guard([this, flow]()
	{
		if (!flow)
			return;

		flow->relay_sock_.reset();

		auto flow_key = make_udp_flow_key(
			flow->client_endp_, flow->original_endp_);

		std::lock_guard<std::mutex> lock(m_udp_flows_mutex);
		m_udp_tproxy_flows.erase(flow_key);
	});

	if (!flow->relay_sock_)
		flow->relay_sock_.emplace(m_executor);

	auto& relay_sock = flow->relay_sock_;
	boost::system::error_code ec;

	relay_sock->open(m_relay_endp.protocol(), ec);
	if (ec)
	{
		XLOG_WARN << "udp tproxy relay socket open error: " << ec.message();
		co_return;
	}

	// 绑定到一个随机地址.
	relay_sock->bind(udp::endpoint(relay_sock->local_endpoint().protocol(), 0), ec);
	if (ec)
	{
		XLOG_WARN << "udp tproxy relay socket bind error: " << ec.message();
		co_return;
	}

#if defined(__linux__)
	if (m_option.so_mark_)
	{
		uint32_t mark = m_option.so_mark_.value();
		if (::setsockopt(relay_sock->native_handle(), SOL_SOCKET, SO_MARK, &mark, sizeof(mark)) < 0)
		{
			XLOG_WARN << "udp tproxy relay setsockopt SO_MARK error: "
				<< strerror(errno);
		}
	}
#endif

	while (!m_abort)
	{
		char recv_buf[65535];

		auto bytes = co_await relay_sock->async_receive(
			net::buffer(recv_buf), net_awaitable[ec]);
		if (ec)
			break;

		flow->expire_ = 0; // 收到数据包重置过期计数.
		send_response_to_client(flow, recv_buf, bytes);
	}

	co_return;
}

// 去掉 SOCKS5 UDP 头, 然后使用 sendmsg + IP_PKTINFO 将原始数据送回客户端.
void proxy_server::send_response_to_client(udp_tproxy_flow_ptr flow, const char* data, std::size_t len)
{
	if (len < 6)
		return;

	//  +----+------+------+----------+-----------+----------+
	//  |RSV | FRAG | ATYP | DST.ADDR | DST.PORT  |   DATA   |
	//  +----+------+------+----------+-----------+----------+
	//  | 2  |  1   |  1   | Variable |    2      | Variable |
	//  +----+------+------+----------+-----------+----------+

	const char* p = data;

	// 跳过 RSV(2) + FRAG(1)
	read<uint16_t>(p);
	if (read<uint8_t>(p) != 0)
		return; // 不支持分片

	// 跳过 ATYP + DST.ADDR + DST.PORT 计算 payload 偏移.
	size_t header_size;
	switch (read<uint8_t>(p))
	{
	case proxy::SOCKS5_ATYP_IPV4:
		p += 4; header_size = 10; break; // 4+2+4
	case proxy::SOCKS5_ATYP_DOMAINNAME:
		header_size = 7 + static_cast<uint8_t>(*p);
		p += 1 + static_cast<uint8_t>(*p); break; // len(1)+domain
	case proxy::SOCKS5_ATYP_IPV6:
		p += 16; header_size = 22; break; // 4+2+16
	default:
		return; // 未知地址类型
	}

	if (len <= header_size)
		return;

	auto payload = data + header_size;
	auto payload_len = len - header_size;

	auto& client_endp = flow->client_endp_;
	auto& original_dest = flow->original_endp_;

	struct sockaddr_storage ss;
	auto addr_data = client_endp.data();
	std::memcpy(&ss, addr_data, client_endp.size());

	struct iovec iov = {
		.iov_base = const_cast<char*>(payload),
		.iov_len = payload_len
	};

	constexpr size_t ctrl_size_v4 = CMSG_SPACE(sizeof(struct in_pktinfo));
	constexpr size_t ctrl_size_v6 = CMSG_SPACE(sizeof(struct in6_pktinfo));
	constexpr size_t ctrl_max = ctrl_size_v4 > ctrl_size_v6
		? ctrl_size_v4 : ctrl_size_v6;
	alignas(struct cmsghdr) char ctrl[ctrl_max];

	struct msghdr msg = {};
	msg.msg_name = &ss;
	msg.msg_namelen = client_endp.size();
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = ctrl;
	msg.msg_controllen = sizeof(ctrl);

	auto* cmsg = CMSG_FIRSTHDR(&msg);
	if (!cmsg)
		return;
	if (original_dest.address().is_v4())
	{
		cmsg->cmsg_level = IPPROTO_IP;
		cmsg->cmsg_type = IP_PKTINFO;
		cmsg->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));

		auto* pi = (struct in_pktinfo*)CMSG_DATA(cmsg);
		std::memset(pi, 0, sizeof(*pi));
		pi->ipi_ifindex = 0;
		pi->ipi_spec_dst.s_addr = htonl(
			original_dest.address().to_v4().to_uint());

		msg.msg_controllen = CMSG_SPACE(sizeof(struct in_pktinfo));
	}
	else
	{
		cmsg->cmsg_level = IPPROTO_IPV6;
		cmsg->cmsg_type = IPV6_PKTINFO;
		cmsg->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));

		auto* pi6 = (struct in6_pktinfo*)CMSG_DATA(cmsg);
		auto ab = original_dest.address().to_v6().to_bytes();
		std::memcpy(&pi6->ipi6_addr, ab.data(), 16);
		pi6->ipi6_ifindex = 0;
		msg.msg_controllen = CMSG_SPACE(sizeof(struct in6_pktinfo));
	}

	// 通过 TPROXY socket 使用 sendmsg 将数据包发送回客户端.
	auto sent = ::sendmsg(flow->tproxy_sock_.native_handle(), &msg, 0);
	if (sent < 0)
	{
		XLOG_WARN << "udp tproxy sendmsg error: " << strerror(errno);
	}
}

void proxy_server::udp_tproxy_forward_packet(
	udp_tproxy_flow_ptr flow, const char* data, std::size_t len)
{
	if (!flow)
		return;

	if (!flow->relay_sock_)
		return;

	// SOCKS5 UDP: RSV(2) + FRAG(1) + ATYP(1) + addr + port
	// 最大 IPv6 地址长度: 2+1+1+16+2 = 22
	char header[22];
	char* hp = header;

	write<uint16_t>(0x0, hp);
	write<uint8_t>(0x0, hp);

	auto& relay_sock = *flow->relay_sock_;
	auto& original_dest = flow->original_endp_;

	if (original_dest.address().is_v4())
	{
		write<uint8_t>(proxy::SOCKS5_ATYP_IPV4, hp);
		// write 已按大端序输出, 不需要 htonl.
		write<uint32_t>(
			original_dest.address().to_v4().to_uint(), hp);
		write<uint16_t>(original_dest.port(), hp);
	}
	else // IPv6
	{
		write<uint8_t>(proxy::SOCKS5_ATYP_IPV6, hp);
		auto addr_bytes = original_dest.address().to_v6().to_bytes();
		for (auto b : addr_bytes)
			write<uint8_t>(b, hp);
		write<uint16_t>(original_dest.port(), hp);
	}

	size_t header_len = hp - header;
	boost::system::error_code ec;

	std::vector<char> buf(header_len + len);
	std::memcpy(buf.data(), header, header_len);
	std::memcpy(buf.data() + header_len, data, len);

	relay_sock.send_to(net::buffer(buf.data(), buf.size()), m_relay_endp, 0, ec);
	if (ec)
	{
		auto flow_key = make_udp_flow_key(flow->client_endp_, flow->original_endp_);
		flow->relay_sock_.reset();

		XLOG_WARN << "udp tproxy forward error: " << ec.message()
			<< ", closing flow: " << flow_key
			<< ", client: " << flow->client_endp_
			<< ", dest: " << flow->original_endp_;

		std::lock_guard<std::mutex> lock(m_udp_flows_mutex);
		m_udp_tproxy_flows.erase(flow_key);
	}
}

net::awaitable<void> proxy_server::start_udp_tproxy_listen(udp::socket& udp_sock) noexcept
{
	auto self = shared_from_this();

	XLOG_DBG << "udp tproxy listener started on: " << udp_sock.local_endpoint();

	boost::system::error_code ec;

	while (!m_abort)
	{
		co_await udp_sock.async_wait(net::socket_base::wait_read, net_awaitable[ec]);
		if (ec || m_abort)
			break;

		// 使用 recvmsg 接收并获取原始目标地址.
		char data[65535];

		struct sockaddr_storage client_ss = {};
		struct iovec iov = { data, sizeof(data) };
		char ancillary[1024];
		struct msghdr msg = {};

		msg.msg_name = &client_ss;
		msg.msg_namelen = sizeof(client_ss);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = ancillary;
		msg.msg_controllen = sizeof(ancillary);

		ssize_t ret = ::recvmsg(udp_sock.native_handle(), &msg, 0);
		if (ret < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;

			XLOG_WARN << "udp tproxy recvmsg error: " << strerror(errno);
			continue;
		}

		// 解析 UDP TPROXY 数据包，提取客户端地址和原始目标地址.
		udp::endpoint client_ep, original_dest;
		if (!parse_udp_tproxy_packet(msg, ret, client_ep, original_dest))
			continue;

		size_t flow_key = make_udp_flow_key(client_ep, original_dest);

		// 查找或创建 flow.
		std::shared_ptr<udp_tproxy_flow> flow;
		{
			std::lock_guard<std::mutex> lock(m_udp_flows_mutex);

			auto it = m_udp_tproxy_flows.find(flow_key);
			if (it != m_udp_tproxy_flows.end())
				flow = it->second;
		}

		if (!flow)
		{
			// 创建一个新的 flow 来处理这个客户端和原始目标地址的通信.
			flow = std::make_shared<udp_tproxy_flow>(client_ep, original_dest, udp_sock);

			{
				std::lock_guard<std::mutex> lock(m_udp_flows_mutex);
				m_udp_tproxy_flows[flow_key] = flow;
			}

			// 创建 relay socket 后就启动一个协程循环等待 relay socket 上的响应数据, 并转发回客户端.
			net::co_spawn(m_executor, udp_tproxy_response_loop(flow), net::detached);

			XLOG_DBG << "new udp tproxy flow, client: " << client_ep
				<< ", original dest: " << original_dest
				<< ", flow key: " << flow_key;
		}

		BOOST_ASSERT(flow && "udp tproxy flow is null");
		flow->expire_ = 0; // 收到数据包重置过期计数.
		udp_tproxy_forward_packet(flow, data, static_cast<size_t>(ret));
	}

	XLOG_DBG << "udp tproxy listener stopped";
}

#endif // defined(__linux__)

} // namespace proxy