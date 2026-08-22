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
#include "proxy/proxy_util.hpp"
#include "proxy/strutil.hpp"
#include "proxy/tun_server.hpp"

#include <boost/functional/hash.hpp>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <optional>
#include <sstream>
#include <unordered_set>

#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
# include <ifaddrs.h>
# include <netinet/in.h>
# include <cstring>
#endif

#include <tinyrpc/jsonrpc.hpp>

#ifndef SO_ORIGINAL_DST
#  define SO_ORIGINAL_DST 80
#endif

namespace proxy {

//////////////////////////////////////////////////////////////////////////

// launcher 控制通道状态结构体（定义于 .cpp, 避免在头文件中暴露声明;
// 所有使用它的成员函数均实现在本文件）.
struct launcher_state
{
	// 服务启动时间（Unix 秒）与版本标识.
	uint64_t started_at_{ 0 };
	std::string server_version_;

	// 全局累计连接数与收发字节数.
	std::atomic<uint64_t> conn_total_{ 0 };
	std::atomic<uint64_t> global_rx_{ 0 };
	std::atomic<uint64_t> global_tx_{ 0 };

	// 已关闭会话的累计用户流量（user -> (rx, tx, conns)）.
	// conns 为已关闭会话累积连接数，用于状态上报 conn_total（含历史累计）.
	struct user_total
	{
		uint64_t rx{ 0 };
		uint64_t tx{ 0 };
		uint64_t conns{ 0 };
	};
	std::mutex user_mutex_;
	std::map<std::string, user_total> user_totals_;

	// launcher 记录的该用户历史已用流量（字节）。proxy_server 重启后，launcher
	// 会把这份历史用量下发回来，配额判断从“历史用量 + 新增用量”继续累计，
	// 避免重启导致用量归零.
	std::mutex usage_mutex_;
	std::map<std::string, int64_t> user_usage_;

	// 最近一次状态报告（get_status 返回用；仅 io_context 线程访问）.
	boost::json::value last_report_;

	// 控制通道停止标志与当前连接关闭标志.
	std::atomic_bool stopped_{ false };
	std::atomic_bool session_closed_{ false };

	// 当前会话的 protect 请求发送器（serve 期间有效, 会话断开后清空）.
	// 经它向 app 发起 protect 请求放行出站 socket; 为空表示无会话 (放行).
	std::function<net::awaitable<bool>(int)> call_protect_;

	// 在途请求处理协程计数（serve 结束时等待其归零, 确保会话对象存活期内
	// 所有引用会话的协程已完成, 从而无需 shared_ptr 管理会话生命周期）.
	std::atomic<int> active_requests_{ 0 };

	// TLS 客户端上下文（信任 launcher 自签证书）.
	net::ssl::context ssl_ctx_{ net::ssl::context::tls_client };
};

// launcher 控制通道请求处理错误: 由 launcher_dispatch 抛出,
// launcher_handle_request 转换为 JSON-RPC error 响应.
struct launcher_error
{
	int code{ -32000 };
	std::string message;
};

// launcher 控制通道使用的命名空间别名.
namespace json = boost::json;
using launcher_ws = beast::websocket::stream<tcp::socket>;
using launcher_wss = beast::websocket::stream<beast::ssl_stream<tcp::socket>>;

// 状态上报间隔.
inline constexpr std::chrono::milliseconds k_launcher_status_interval{ 2000 };
// 建立连接（含解析/连接/握手）超时.
inline constexpr std::chrono::milliseconds k_launcher_dial_timeout{ 10000 };
// 重连最大退避.
inline constexpr int k_launcher_max_backoff_ms = 30000;
// 未认证（匿名）连接在状态上报中的用户标识. 匿名用户不受配额限制.
inline constexpr const char* k_anon_user = "(匿名)";

namespace detail {

// 判断 websocket 底层流是否为 ssl::stream（wss）.
template <class T>
struct is_ssl_stream : std::false_type {};
template <class T>
struct is_ssl_stream<beast::ssl_stream<T>> : std::true_type {};

// 取 json 对象中的整数字段.
inline std::int64_t json_num(const json::object& obj, const char* key)
{
	auto it = obj.find(key);
	if (it == obj.end())
		return 0;
	const auto& v = it->value();
	if (v.is_int64())
		return v.as_int64();
	if (v.is_uint64())
		return static_cast<std::int64_t>(v.as_uint64());
	if (v.is_double())
		return static_cast<std::int64_t>(v.as_double());
	return 0;
}

// 取 json 对象中的字符串字段.
inline std::string json_str(const json::object& obj, const char* key)
{
	auto it = obj.find(key);
	if (it == obj.end() || !it->value().is_string())
		return {};
	return std::string(it->value().as_string());
}

// 任意值转字符串。
inline std::string to_str(const json::value& v)
{
	if (v.is_string())
		return std::string(v.as_string());
	if (v.is_bool())
		return v.as_bool() ? "true" : "false";
	if (v.is_int64())
		return std::to_string(v.as_int64());
	if (v.is_uint64())
		return std::to_string(v.as_uint64());
	if (v.is_double())
		return json::serialize(v);
	return {};
}

// 任意值转整数（兼容 int / float / string）。
inline int to_int(const json::value& v)
{
	if (v.is_int64())
		return static_cast<int>(v.as_int64());
	if (v.is_uint64())
		return static_cast<int>(v.as_uint64());
	if (v.is_double())
		return static_cast<int>(v.as_double());
	if (v.is_bool())
		return v.as_bool() ? 1 : 0;
	if (v.is_string())
		return std::atoi(std::string(v.as_string()).c_str());
	return 0;
}

// 任意值转布尔。
inline bool to_bool(const json::value& v)
{
	if (v.is_bool())
		return v.as_bool();
	if (v.is_int64())
		return v.as_int64() != 0;
	if (v.is_double())
		return v.as_double() != 0;
	if (v.is_string())
	{
		const auto& s = v.as_string();
		return s == "true" || s == "1" || s == "yes" || s == "on";
	}
	return false;
}

// 任意值转字符串列表。
inline std::vector<std::string> to_str_list(const json::value& v)
{
	std::vector<std::string> out;
	if (v.is_array())
	{
		for (const auto& e : v.as_array())
			out.push_back(to_str(e));
	}
	else if (v.is_string())
	{
		if (!v.as_string().empty())
			out.push_back(std::string(v.as_string()));
	}
	return out;
}

// 解析 server_listen 端点字符串为 (tcp::endpoint, v6only) 元组。
// 支持 ip:port、[ipv6]:port 格式，端口后可选 v6only/-v6only/ipv6only 后缀。
// 成功返回 true 并填充 endp/v6only。
inline bool parse_listen_endpoint(const std::string& str,
	tcp::endpoint& endp, bool& v6only)
{
	v6only = false;
	std::string host, port;
	size_t pos = 0;

	if (!str.empty() && str[0] == '[')
	{
		// [ipv6]:port 格式.
		auto close = str.find(']');
		if (close == std::string::npos)
			return false;
		host = str.substr(1, close - 1);
		pos = close + 1;
	}
	else
	{
		// ip:port 格式.
		auto colon = str.find(':');
		if (colon == std::string::npos)
			return false;
		host = str.substr(0, colon);
		pos = colon;
	}

	if (pos >= str.size() || str[pos] != ':')
		return false;
	pos++;

	// 解析端口.
	auto pstart = pos;
	while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9')
		pos++;
	port = str.substr(pstart, pos - pstart);
	if (port.empty())
		return false;

	// 可选后缀 v6only.
	if (pos < str.size())
	{
		auto opt = str.substr(pos);
		if (opt == "ipv6only" || opt == "-ipv6only" ||
			opt == "v6only" || opt == "-v6only")
			v6only = true;
		else
			return false;
	}

	int p;
	try { p = std::stoi(port); }
	catch (const std::exception&) { return false; }
	if (p < 0 || p > 65535)
		return false;

	boost::system::error_code ec;
	auto addr = net::ip::make_address(host, ec);
	if (ec)
		return false;

	endp = tcp::endpoint(addr, static_cast<unsigned short>(p));
	return true;
}

} // namespace detail


proxy_server::proxy_server(net::any_io_executor executor, proxy_server_option opt)
	: m_executor(executor)
	, m_option(std::move(opt))
	, m_timer(executor)
	, m_launcher_state(std::make_unique<launcher_state>())
{
	// 记录启动时间与版本标识（供 launcher 状态上报）.
	m_launcher_state->started_at_ = static_cast<uint64_t>(std::time(nullptr));
	m_launcher_state->server_version_ = "cpp-proxy";

	// 初始化 launcher 控制通道（URL 来自 m_option.launcher_url_, 为空不启用）.
	// 信任 launcher 自签证书.
	if (!m_option.launcher_url_.empty())
	{
		m_launcher_state->ssl_ctx_.set_verify_mode(net::ssl::verify_none);
		// 启用日志转发: logger_tag 钩子采集日志, 由控制通道上报时发送.
		detail::launcher_log_set_enabled(true);
	}

	if (!m_option.stdio_target_.empty())
		return;

	m_certificates.store(&m_certificate_master);

	init_ssl_context();

	boost::system::error_code ec;

	if (fs::exists(m_option.ipip_db_, ec))
	{
		try {
			m_ip_database = std::make_unique<ip_ipdb>();
			if (!m_ip_database->load(m_option.ipip_db_))
			{
				m_ip_database = std::make_unique<ip_datx>();
				if (!m_ip_database->load(m_option.ipip_db_))
					m_ip_database.reset();
			}
		} catch (const std::exception& e) {
			XLOG_WARN << "ipip database " << m_option.ipip_db_ << ", load error: " << e.what();
		}
	}

	init_acceptor();
}

proxy_server::~proxy_server()
{
	// launcher_state 为不完整类型, unique_ptr 成员在此释放.
	detail::launcher_log_set_enabled(false);
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

	std::string line;

	boost::regex re(R"(-----BEGIN\s.*\s?PRIVATE\sKEY-----)");
	boost::smatch what;

	while (std::getline(file, line))
	{
		if (line.find("-----BEGIN CERTIFICATE-----") != std::string::npos)
		{
			result.type_ = pem_type::cert;
			result.chains_++;
			continue;
		}
		else if (line.find("DH PARAMETERS-----") != std::string::npos)
		{
			result.type_ = pem_type::dhparam;
			break;
		}
		else if (boost::regex_search(line, what, re))
		{
			result.type_ = pem_type::key;
			break;
		}
	}

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
			auto pem = determine_pem_type(entry.path());
			switch (pem.type_)
			{
				case pem_type::cert:
					if (pem.chains_ > file.cert_.chains_)
						file.cert_ = pem;
					break;
				case pem_type::key:
					file.key_ = pem;
					break;
				case pem_type::dhparam:
					file.dhparam_ = pem;
					break;
				case pem_type::pwd:
					file.pwd_ = pem;
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

		// TLS 1.3 下默认启用服务端 cipher 偏好, 使下方 ciphersuites 的服务端
		// 偏好顺序生效(默认优先 AES-128-GCM, 加密最快). 该选项不破坏兼容性,
		// 服务端仅在客户端支持列表中按偏好选择; 若显式设置了
		// ssl_prefer_server_ciphers_ 则同样开启, set_options 为幂等 OR 操作.
		ssl_ctx.set_options(SSL_OP_CIPHER_SERVER_PREFERENCE);

		// 设置 ssl ciphers. (默认值已在 init_ssl_context 中设定)
		SSL_CTX_set_cipher_list(ssl_ctx.native_handle(),
			m_option.ssl_ciphers_.c_str());
#ifndef OPENSSL_IS_BORINGSSL
		// TLS 1.3 ciphersuites: 优先 AES-128-GCM(加密最快, 与常见代理一致),
		// 再 AES-256-GCM 与 CHACHA20.
		SSL_CTX_set_ciphersuites(ssl_ctx.native_handle(),
			"TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256");
#endif
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

		m_tcp_acceptors.emplace_back(
			std::make_unique<tcp_acceptor>(std::move(acceptor)));
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
	if (m_option.proxy_pass_ && m_option.transparent_ && !m_option.disable_udp_)
	{
		for (const auto& [tcp_endp, v6only] : m_option.listens_)
		{
			(void)v6only;

			if (!m_option.proxy_pass_->scheme().starts_with("socks5") &&
				!m_option.proxy_pass_->scheme().starts_with("http"))
				continue;

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

			// 设置 IP_RECVORIGDSTADDR 以接收原始目标地址.
			int opt = 1;
			if (udp_endp.protocol() == net::ip::udp::v4())
			{
				udp_sock.set_option(transparent_opt(true), ec);
				::setsockopt(udp_sock.native_handle(), IPPROTO_IP,
					IP_RECVORIGDSTADDR, &opt, sizeof(opt));
			}
			else
			{
				udp_sock.set_option(transparent6_opt(true), ec);
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

	// 默认的 ssl ciphers, 确保在 walk_certificate 之前赋值.
	const std::string ssl_ciphers = "HIGH:!aNULL:!MD5:!3DES";
	if (m_option.ssl_ciphers_.empty())
		m_option.ssl_ciphers_ = ssl_ciphers;

#ifdef OPENSSL_IS_BORINGSSL
	// BoringSSL 无独立的 TLS 1.3 ciphersuites 接口, 统一通过
	// SSL_CTX_set_cipher_list 配置, 这里将 TLS 1.3 suite 附加到默认 cipher list.
	m_option.ssl_ciphers_ += ":TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256";
#else
	// TLS 1.3 ciphersuites: 默认优先 AES-128-GCM(加密最快, 与常见代理一致),
	// 其次是 AES-256-GCM 和 CHACHA20. 若用户显式设置了 ssl_ciphers_ 则仍按
	// 其配置(TLS 1.2 部分), 此处仅确保 TLS 1.3 走最快路径.
	SSL_CTX_set_ciphersuites(m_ssl_srv_context.native_handle(),
		"TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256");
#endif
	// 启用服务端 cipher 偏好, 使上面的 ciphersuite 偏好顺序生效.
	SSL_CTX_set_options(m_ssl_srv_context.native_handle(),
		SSL_OP_CIPHER_SERVER_PREFERENCE);

	// 读取并更新证书文件.
	update_certificate(m_option.ssl_cert_path_, *m_certificates.load());

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
	auto certificates_ptr = m_certificates.load();
	if (!certificates_ptr)
		return SSL_TLSEXT_ERR_OK;

	auto& certificates = *certificates_ptr;
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

net::awaitable<std::chrono::seconds> proxy_server::certificate_check()
{
	boost::system::error_code ec;

	// 找到下次需要检查证书的时间间隔, 如果有证书过期, 返回 0 表示应尽快检查.
	// 如果所有证书都有效, 返回距最早过期的时间.

	auto now = boost::posix_time::second_clock::local_time();
	std::chrono::seconds earliest_expiry = std::chrono::hours(24) * 365;

	auto certificates_ptr = m_certificates.load();
	auto& certificates = *certificates_ptr;

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

			earliest_expiry = std::chrono::seconds::zero();
			continue;
		}

		auto remaining = std::chrono::seconds((ctx.expire_date_ - now).total_seconds());
		earliest_expiry = std::min(earliest_expiry, remaining);
	}

	if (earliest_expiry > std::chrono::seconds::zero())
		co_return earliest_expiry;

	// 热更新证书, 交替更新证书容器 master/slave.
	if (certificates_ptr == &m_certificate_master)
	{
		update_certificate(m_option.ssl_cert_path_, m_certificate_slave);
		m_certificates.store(&m_certificate_slave);
	}
	else
	{
		update_certificate(m_option.ssl_cert_path_, m_certificate_master);
		m_certificates.store(&m_certificate_master);
	}

	co_return earliest_expiry;
}

net::awaitable<void> proxy_server::tick()
{
	auto self = shared_from_this();

	boost::system::error_code ec;
	auto check_time_point = std::chrono::steady_clock::now();

	while (!m_abort)
	{
		m_timer.expires_after(std::chrono::seconds(1));
		co_await m_timer.async_wait(net_awaitable[ec]);
		if (ec)
			break;

		auto now = std::chrono::steady_clock::now();

		// 检查证书是否过期 (仅在配置了证书路径时).
		if (!m_option.ssl_cert_path_.empty() && now > check_time_point)
		{
			// 返回过期间隔期.
			auto duration = co_await certificate_check();
			// 至少 5 分钟后再检查.
			check_time_point = now + duration + std::chrono::minutes(5);
		}

#if defined(__linux__)
		if (m_option.transparent_)
		{
			// 检查 UDP TPROXY 流是否过期.
			if (!m_udp_tproxy_flows.empty())
				co_await udp_tproxy_check();

			// 检查 UDP TPROXY socks5 连接是否需要重试.
			if (m_retry_tproxy_socks5_connect)
			{
				net::co_spawn(m_executor,
					udp_tproxy_socks5_connect(), net::detached);
			}
		}
#endif
	}

	co_return;
}

//////////////////////////////////////////////////////////////////////////
// launcher 控制通道支持.

// 启动 launcher 控制通道（由 start() 调用, URL 来自 m_option.launcher_url_,
// 为空则不启动）.
void proxy_server::launcher_start() noexcept
{
	if (m_option.launcher_url_.empty())
		return;

	XLOG_DBG << "launcher control channel start: " << m_option.launcher_url_;

	auto self = shared_from_this();
	net::co_spawn(m_executor, [self]() -> net::awaitable<void> {
		co_await self->launcher_worker();
	}, net::detached);
}

// 停止 launcher 控制通道（由 close() 调用, 设置停止标志使 serve 协程退出）.
void proxy_server::launcher_stop() noexcept
{
	m_launcher_state->stopped_ = true;
	// serve 协程在状态上报循环中检查该标志后退出, 并在退出时自行关闭会话.
}

std::string proxy_server::launcher_parse_instance_id(const std::string& url)
{
	if (auto u = boost::urls::parse_uri(url); u.has_value())
	{
		auto p = u->params().find("instance");
		if (p != u->params().end())
			return std::string((*p).value);
	}
	return {};
}

// 建立 ws/wss 连接并返回 JSON-RPC 会话；失败返回 nullopt.
// 连接/握手全程受 k_launcher_dial_timeout 超时保护（超时后关闭 socket
// 使异步操作失败）. 会话对象由调用方持有, 后续均通过引用访问.
template <typename WsStream>
net::awaitable<std::optional<jsonrpc::jsonrpc_session<WsStream>>>
proxy_server::launcher_connect(const std::string& host, const std::string& port, const std::string& target)
{
	auto ex = co_await net::this_coro::executor;
	boost::system::error_code ec;

	// DNS 解析.
	net::ip::tcp::resolver resolver(ex);
	auto results = co_await resolver.async_resolve(host, port, net_awaitable[ec]);
	if (ec)
	{
		XLOG_WARN << "launcher resolve " << host << ": " << ec.message();
		co_return std::nullopt;
	}

	if constexpr (detail::is_ssl_stream<typename WsStream::next_layer_type>::value)
	{
		// wss.
		WsStream ws(ex, m_launcher_state->ssl_ctx_);
		if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str()))
			co_return std::nullopt;

		// 超时保护: 超时后关闭 socket, 使进行中的异步操作立即失败.
		net::steady_timer dial_timer(ex);
		dial_timer.expires_after(k_launcher_dial_timeout);
		auto cancel_conn = [&ws](const boost::system::error_code& tec) {
			if (tec)
				return; // 定时器被取消（连接已完成）.
			boost::system::error_code sec;
			beast::get_lowest_layer(ws).close(sec);
		};
		dial_timer.async_wait(cancel_conn);

		// TCP 连接.
		co_await net::async_connect(beast::get_lowest_layer(ws), results, net_awaitable[ec]);
		dial_timer.cancel();
		if (ec)
			co_return std::nullopt;

		// TLS 握手.
		co_await ws.next_layer().async_handshake(net::ssl::stream_base::client, net_awaitable[ec]);
		dial_timer.cancel();
		if (ec)
			co_return std::nullopt;

		// WebSocket 握手.
		co_await ws.async_handshake(host, target, net_awaitable[ec]);
		dial_timer.cancel();
		if (ec)
			co_return std::nullopt;

		co_return jsonrpc::jsonrpc_session<WsStream>(std::move(ws));
	}
	else
	{
		// ws.
		WsStream ws(ex);

		net::steady_timer dial_timer(ex);
		dial_timer.expires_after(k_launcher_dial_timeout);
		auto cancel_conn = [&ws](const boost::system::error_code& tec) {
			if (tec)
				return;
			boost::system::error_code sec;
			beast::get_lowest_layer(ws).close(sec);
		};
		dial_timer.async_wait(cancel_conn);

		// TCP 连接.
		co_await net::async_connect(beast::get_lowest_layer(ws), results, net_awaitable[ec]);
		dial_timer.cancel();
		if (ec)
			co_return std::nullopt;

		// WebSocket 握手.
		co_await ws.async_handshake(host, target, net_awaitable[ec]);
		dial_timer.cancel();
		if (ec)
			co_return std::nullopt;

		co_return jsonrpc::jsonrpc_session<WsStream>(std::move(ws));
	}
}

// 一次连接的服务流程: 注册实例信息、启动读循环、状态上报循环,
// 直到连接断开或 stop. 全程协程, 不创建线程. 会话对象由调用方
// （launcher_run_once）持有, 此处通过引用访问.
template <typename WsStream>
net::awaitable<void> proxy_server::launcher_serve(jsonrpc::jsonrpc_session<WsStream>& sess)
{
	auto ex = co_await net::this_coro::executor;

	// 注册请求/通知处理器.
	launcher_register_handlers(sess);

	m_launcher_state->session_closed_ = false;
	sess.closed_callback([self = shared_from_this()]() { self->m_launcher_state->session_closed_ = true; });

	// 先启动读循环（同 executor 上的独立协程）, 再发送通知;
	// 否则会话尚未进入运行态, 入队的写消息可能无法发出.
	sess.start();

	// 设置 protect 请求发送器: tun 出站连接经它请求 app 放行 socket.
	// 会话生命周期由 launcher_serve 管理, 发送器仅在 serve 期间有效.
	m_launcher_state->call_protect_ =
		[&sess](int fd) -> net::awaitable<bool>
		{
			json::object params;
			params["fd"] = fd;
			try
			{
				auto result = co_await sess.async_call("protect", params);
				if (result.if_contains("ok") && result.at("ok").is_bool())
					co_return result.at("ok").as_bool();
			}
			catch (...)
			{
				// 控制通道异常时放行, 避免阻塞连接建立.
				XLOG_WARN << "protect rpc failed, allow socket";
			}
			co_return true;
		};

	// 注册实例信息.
	json::object reg;
	reg["instance_id"] = launcher_parse_instance_id(m_option.launcher_url_);
	reg["pid"] = static_cast<int64_t>(::getpid());
	reg["version"] = server_version();
	reg["started_at"] = static_cast<int64_t>(started_at());
	sess.notify("register", reg);

	// 立即上报一次状态.
	m_launcher_state->last_report_ = json::value(json::object_kind);
	launcher_update_report(sess);

	// 状态上报循环: 连接断开或 stop 时退出.
	net::steady_timer timer(ex);
	boost::system::error_code sec;
	while (!m_launcher_state->stopped_ && !m_launcher_state->session_closed_)
	{
		timer.expires_after(k_launcher_status_interval);
		co_await timer.async_wait(net_awaitable[sec]);
		if (m_launcher_state->stopped_ || m_launcher_state->session_closed_)
			break;
		launcher_update_report(sess);
	}

	// 清理: 关闭会话并清空 protect 请求发送器.
	sess.stop();
	m_launcher_state->call_protect_ = {};

	// 等待所有在途请求处理完成（它们通过引用访问本会话, 必须在本协程
	// 返回前结束, 调用方才能安全销毁会话对象）. 会话已停止, 不再有新请求.
	while (m_launcher_state->active_requests_.load() > 0)
	{
		timer.expires_after(std::chrono::milliseconds(10));
		co_await timer.async_wait(net_awaitable[sec]);
	}

	co_return;
}

// 注册 launcher → proxy_server 的请求/通知处理器.
template <typename WsStream>
void proxy_server::launcher_register_handlers(jsonrpc::jsonrpc_session<WsStream>& sess)
{
	// 请求（带 id）: 协程方式处理并回复（支持错误响应）.
	// 会话对象由调用方持有, 回调捕获其指针; serve 退出前等待在途处理归零.
	sess.default_method_callback([this, &sess](json::object req) {
		auto self = shared_from_this();
		++m_launcher_state->active_requests_;
		net::co_spawn(m_executor, [self, &sess, req = std::move(req)]() mutable -> net::awaitable<void> {
			// 无论协程如何结束都递减计数, 避免 serve 退出时永久等待.
			auto guard = boost::scope::scope_exit(
				[self]() { --self->m_launcher_state->active_requests_; });
			co_await self->launcher_handle_request(sess, std::move(req));
		}, net::detached);
	});

	// 通知（无 id）: 处理 set_user_usage 等（不引用会话, 无需计数）.
	sess.notify_callback([this](json::object req) {
		auto self = shared_from_this();
		net::co_spawn(m_executor, [self, req = std::move(req)]() mutable -> net::awaitable<void> {
			co_await self->launcher_handle_notify(std::move(req));
		}, net::detached);
	});
}

// 协程方式处理一个请求, 分发到对应方法并回复（支持错误响应）.
template <typename WsStream>
net::awaitable<void> proxy_server::launcher_handle_request(jsonrpc::jsonrpc_session<WsStream>& sess, json::object req)
{
	std::string method = detail::json_str(req, "method");
	json::value params;
	if (auto it = req.if_contains("params"); it)
		params = *it;
	json::value id;
	if (auto it = req.if_contains("id"); it)
		id = *it;

	auto reply_result = [&](json::value r) { sess.reply(std::move(r), id, false); };
	auto reply_error = [&](int code, const std::string& msg) {
		json::object err;
		err["code"] = code;
		err["message"] = msg;
		sess.reply(std::move(err), id, true);
	};

	try
	{
		reply_result(launcher_dispatch(method, params));
	}
	catch (const launcher_error& e)
	{
		reply_error(e.code, e.message);
	}
	catch (const std::exception& e)
	{
		reply_error(-32000, e.what());
	}

	co_return;
}

// 处理 launcher 下发的通知（无 id 消息, 如 set_user_usage）.
net::awaitable<void> proxy_server::launcher_handle_notify(json::object req)
{
	std::string method = detail::json_str(req, "method");
	json::value params;
	if (auto it = req.if_contains("params"); it)
		params = *it;

	// launcher 在连接建立后，下发该用户的历史已用流量（用于配额续接）.
	if (method == "set_user_usage")
	{
		if (params.is_object())
		{
			auto u = params.as_object().if_contains("usage");
			if (u && u->is_object())
				set_user_usage(u->as_object());
		}
	}

	co_return;
}

// 方法分发. 返回结果 json::value；失败抛出 launcher_error.
json::value proxy_server::launcher_dispatch(const std::string& method, const json::value& params)
{
	if (method == "get_status")
	{
		if (m_launcher_state->last_report_.is_object())
			return m_launcher_state->last_report_;
		return snapshot_report();
	}

	if (method == "set_config")
	{
		if (!params.is_object())
			throw launcher_error{ -32602, "invalid set_config params" };
		auto opt = params.as_object().if_contains("options");
		if (!opt || !opt->is_object())
			throw launcher_error{ -32602, "missing options" };
		return apply_options(opt->as_object());
	}

	if (method == "add_user")
	{
		std::string user, password, addr, proxy_url;
		if (params.is_object())
		{
			user = detail::json_str(params.as_object(), "user");
			password = detail::json_str(params.as_object(), "password");
			addr = detail::json_str(params.as_object(), "addr");
			proxy_url = detail::json_str(params.as_object(), "proxy_url");
		}
		if (user.empty())
			throw launcher_error{ -32602, "user is required" };
		std::string err;
		if (!add_auth_user(user, password, addr, proxy_url, err))
			throw launcher_error{ -32000, err };
		return users_state();
	}

	if (method == "del_user")
	{
		std::string user = params.is_object() ? detail::json_str(params.as_object(), "user") : "";
		if (user.empty())
			throw launcher_error{ -32602, "user is required" };
		if (!del_auth_user(user))
			throw launcher_error{ -32000, "user not found: " + user };
		return users_state();
	}

	if (method == "set_user_password")
	{
		std::string user, password;
		if (params.is_object())
		{
			user = detail::json_str(params.as_object(), "user");
			password = detail::json_str(params.as_object(), "password");
		}
		if (user.empty())
			throw launcher_error{ -32602, "user is required" };
		if (!set_auth_user_password(user, password))
			throw launcher_error{ -32000, "user not found: " + user };
		return users_state();
	}

	if (method == "set_user_rate_limit")
	{
		std::string user;
		int rate = 0;
		if (params.is_object())
		{
			user = detail::json_str(params.as_object(), "user");
			rate = static_cast<int>(detail::json_num(params.as_object(), "rate"));
		}
		if (user.empty())
			throw launcher_error{ -32602, "user is required" };
		set_auth_user_rate_limit(user, rate);
		return users_state();
	}

	if (method == "set_user_quota")
	{
		std::string user;
		std::int64_t quota = 0;
		if (params.is_object())
		{
			user = detail::json_str(params.as_object(), "user");
			quota = detail::json_num(params.as_object(), "quota");
		}
		if (user.empty())
			throw launcher_error{ -32602, "user is required" };
		set_auth_user_quota(user, quota);
		return users_state();
	}


	if (method == "set_user_usage")
	{
		if (params.is_object())
		{
			auto u = params.as_object().if_contains("usage");
			if (u && u->is_object())
				set_user_usage(u->as_object());
		}
		return json::object{};
	}

	if (method == "set_tun_fd")
	{
		// Android VpnService 场景: app 建立 tun 后经控制通道注入 fd.
		int fd = -1;
		if (params.is_object())
		{
			auto it = params.as_object().if_contains("fd");
			if (it && it->is_int64())
				fd = static_cast<int>(it->as_int64());
		}
		if (fd < 0)
			throw launcher_error{ -32602, "invalid set_tun_fd params" };
		if (!m_tun_server)
			throw launcher_error{ -32000, "tun server not running" };
		m_tun_server->set_tun_fd(fd);
		json::object ok;
		ok["ok"] = true;
		return ok;
	}

	if (method == "shutdown")
	{
		// 延迟退出: 先让本请求的响应帧写出, launcher 才能收到关闭确认.
		// 用协程定时器实现, 不创建线程. 之后关闭 proxy_server 的所有
		// session/连接/accept/定时器, 使 io_context 无待处理工作而自然退出.
		auto self = shared_from_this();
		net::co_spawn(m_executor, [self]() -> net::awaitable<void> {
			auto ex = co_await net::this_coro::executor;
			net::steady_timer t(ex);
			t.expires_after(std::chrono::milliseconds(200));
			boost::system::error_code sec;
			co_await t.async_wait(net_awaitable[sec]);
			self->close();
		}, net::detached);
		return json::object{};
	}

	throw launcher_error{ -32601, "method not found: " + method };
}

// 采集快照、计算速率并上报.
template <typename WsStream>
void proxy_server::launcher_update_report(jsonrpc::jsonrpc_session<WsStream>& sess)
{
	json::object rep = snapshot_report();

	// 差分速率: 用相邻两次快照的差值除以时间间隔, 计数回退时钳制为 0.
	json::object rates;
	json::object user_rates;
	double rx_rate = 0, tx_rate = 0;
	if (m_launcher_state->last_report_.is_object())
	{
		auto cur_ts = rep.if_contains("ts") && rep.at("ts").is_int64() ? rep.at("ts").as_int64() : 0;
		auto prev_ts = m_launcher_state->last_report_.as_object().if_contains("ts") && m_launcher_state->last_report_.as_object().at("ts").is_int64()
			? m_launcher_state->last_report_.as_object().at("ts").as_int64() : 0;
		// 首次上报或重连后快照被清空时 prev_ts 为 0, 此时没有可做差分的
		// 上一份数据, 不计算速率, 避免用断线前的旧快照差分导致速率失真.
		if (prev_ts > 0)
		{
			double sec = static_cast<double>(cur_ts - prev_ts);
			if (sec > 0)
			{
				int64_t cur_rx = 0, cur_tx = 0, prev_rx = 0, prev_tx = 0;
				if (auto g = rep.if_contains("global"); g && g->is_object())
				{
					cur_rx = detail::json_num(g->as_object(), "rx_bytes");
					cur_tx = detail::json_num(g->as_object(), "tx_bytes");
				}
				if (auto g = m_launcher_state->last_report_.as_object().if_contains("global"); g && g->is_object())
				{
					prev_rx = detail::json_num(g->as_object(), "rx_bytes");
					prev_tx = detail::json_num(g->as_object(), "tx_bytes");
				}
				rx_rate = cur_rx > prev_rx ? (cur_rx - prev_rx) / sec : 0;
				tx_rate = cur_tx > prev_tx ? (cur_tx - prev_tx) / sec : 0;

				// 单用户速率: 对上一份报告存在且计数不回退的用户计算差分
				// （回退时钳制为 0, 如用户被删除重建）.
				std::map<std::string, std::pair<int64_t, int64_t>> prev_users;
				if (auto pu = m_launcher_state->last_report_.as_object().if_contains("users");
					pu && pu->is_array())
				{
					for (const auto& u : pu->as_array())
					{
						if (!u.is_object())
							continue;
						const auto& uo = u.as_object();
						auto it = uo.find("user");
						if (it == uo.end() || !it->value().is_string())
							continue;
						std::string name(it->value().as_string());
						prev_users[name] = {
							detail::json_num(uo, "rx_bytes"),
							detail::json_num(uo, "tx_bytes")
						};
					}
				}
				if (auto cu = rep.if_contains("users"); cu && cu->is_array())
				{
					for (const auto& u : cu->as_array())
					{
						if (!u.is_object())
							continue;
						const auto& uo = u.as_object();
						auto it = uo.find("user");
						if (it == uo.end() || !it->value().is_string())
							continue;
						std::string name(it->value().as_string());
						auto p = prev_users.find(name);
						if (p == prev_users.end())
							continue;
						int64_t cur_r = detail::json_num(uo, "rx_bytes");
						int64_t cur_t = detail::json_num(uo, "tx_bytes");
						json::object ur;
						ur["rx_rate_bps"] = cur_r > p->second.first ? (cur_r - p->second.first) / sec : 0;
						ur["tx_rate_bps"] = cur_t > p->second.second ? (cur_t - p->second.second) / sec : 0;
						user_rates[name] = std::move(ur);
					}
				}
			}
		}
	}
	rates["rx_rate_bps"] = rx_rate;
	rates["tx_rate_bps"] = tx_rate;
	rep["rates"] = std::move(rates);
	if (!user_rates.empty())
		rep["user_rates"] = std::move(user_rates);

	m_launcher_state->last_report_ = rep;
	sess.notify("status", rep);

	// 批量转发经 logger_tag 钩子采集的日志（逐条 notify 开销大）.
	auto lines = detail::launcher_log_drain();
	if (!lines.empty())
	{
		json::array arr;
		for (auto& l : lines)
			arr.emplace_back(std::move(l));
		json::object log;
		log["lines"] = std::move(arr);
		sess.notify("log", log);
	}
}

// 连接循环: 连接失败/断开后退避重连（全部协程, 不创建线程）.
// 注意: 此函数及其调用的 launcher_run_once 定义在所有模板函数之后,
// 以便模板定义在实例化点可见.
net::awaitable<void> proxy_server::launcher_worker()
{
	auto ex = co_await net::this_coro::executor;
	int backoff_ms = 1000;
	boost::system::error_code sec;

	while (!m_launcher_state->stopped_)
	{
		bool connected = co_await launcher_run_once();
		if (m_launcher_state->stopped_)
			break;
		// 建连成功（即使后来断开）: 重置退避, 避免稳定运行后一次抖动
		// 仍要等满上次退避.
		if (connected)
			backoff_ms = 1000;

		XLOG_WARN << "launcher connection lost, reconnect in "
			<< backoff_ms << "ms";

		// 分小段退避等待, 便于及时响应 launcher_stop.
		net::steady_timer timer(ex);
		int left = backoff_ms;
		while (left > 0 && !m_launcher_state->stopped_)
		{
			int chunk = (std::min)(left, 200);
			timer.expires_after(std::chrono::milliseconds(chunk));
			co_await timer.async_wait(net_awaitable[sec]);
			left -= chunk;
		}
		if (backoff_ms < k_launcher_max_backoff_ms)
			backoff_ms *= 2;
	}

	co_return;
}

// 单次连接流程. 返回 true 表示成功建立了连接（尽管之后断开）.
net::awaitable<bool> proxy_server::launcher_run_once()
{
	boost::urls::url_view u = boost::urls::parse_uri(m_option.launcher_url_).value();
	std::string scheme = std::string(u.scheme());
	std::string host = std::string(u.host());
	std::string port;
	if (u.has_port())
		port = std::string(u.port());
	else
		port = std::to_string(urls::default_port(u.scheme_id()));
	std::string target = std::string(u.encoded_target().empty() ? "/" : u.encoded_target());

	if (scheme == "wss")
	{
		auto sess = co_await launcher_connect<launcher_wss>(host, port, target);
		if (!sess)
			co_return false;
		co_await launcher_serve(*sess);
	}
	else
	{
		auto sess = co_await launcher_connect<launcher_ws>(host, port, target);
		if (!sess)
			co_return false;
		co_await launcher_serve(*sess);
	}

	co_return true;
}

// 显式实例化模板 launcher_connect
template net::awaitable<std::optional<jsonrpc::jsonrpc_session<launcher_ws>>>
proxy_server::launcher_connect<launcher_ws>(const std::string& host, const std::string& port, const std::string& target);
template net::awaitable<std::optional<jsonrpc::jsonrpc_session<launcher_wss>>>
proxy_server::launcher_connect<launcher_wss>(const std::string& host, const std::string& port, const std::string& target);

// 显式实例化模板 launcher_serve
template net::awaitable<void> proxy_server::launcher_serve<launcher_ws>(jsonrpc::jsonrpc_session<launcher_ws>& sess);
template net::awaitable<void> proxy_server::launcher_serve<launcher_wss>(jsonrpc::jsonrpc_session<launcher_wss>& sess);

// 显式实例化模板 launcher_register_handlers
template void proxy_server::launcher_register_handlers<launcher_ws>(jsonrpc::jsonrpc_session<launcher_ws>& sess);
template void proxy_server::launcher_register_handlers<launcher_wss>(jsonrpc::jsonrpc_session<launcher_wss>& sess);

// 显式实例化模板 launcher_handle_request
template net::awaitable<void> proxy_server::launcher_handle_request<launcher_ws>(jsonrpc::jsonrpc_session<launcher_ws>& sess, json::object req);
template net::awaitable<void> proxy_server::launcher_handle_request<launcher_wss>(jsonrpc::jsonrpc_session<launcher_wss>& sess, json::object req);

// 显式实例化模板 launcher_update_report
template void proxy_server::launcher_update_report<launcher_ws>(jsonrpc::jsonrpc_session<launcher_ws>& sess);
template void proxy_server::launcher_update_report<launcher_wss>(jsonrpc::jsonrpc_session<launcher_wss>& sess);

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
	if (m_option.tun_)
	{
		// 启动 TUN 设备模式（TUN2SOCKS）.
#if defined(__linux__)
		m_tun_server = tun_server::make(m_executor, m_option);
		if (m_tun_server)
		{
			// Android VpnService 场景: 出站 socket 经 launcher 请求 app protect.
			m_tun_server->set_protect_handler(
				[this](int fd) -> net::awaitable<bool>
				{
					if (m_launcher_state->call_protect_)
						co_return co_await m_launcher_state->call_protect_(fd);
					co_return true;
				});
			m_tun_server->start();
		}
#else
		XLOG_WARN << "tun proxy only support linux";
#endif
	}
	else
	{
	// 如果作为透明代理.
	if (m_option.transparent_)
	{
#if defined(__linux__)
		for (auto& acc : m_tcp_acceptors)
		{
			boost::system::error_code error;

			acc->set_option(transparent_opt(true), error);
			acc->set_option(transparent6_opt(true), error);
		}
#else
		XLOG_WARN << "transparent proxy only support linux";
#endif
		// 获取所有本机 ip 地址.
		net::co_spawn(m_executor,
			get_local_address(), net::detached);
	}

	// 同时启动32个连接协程为每个 acceptor 用于为 proxy client 提供服务.
	for (auto& acc : m_tcp_acceptors)
	{
		for (int i = 0; i < 32; i++)
		{
			net::co_spawn(m_executor,
				start_proxy_listen(*acc), net::detached);
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
	if (m_option.transparent_ && !m_option.disable_udp_)
	{
		net::co_spawn(m_executor,
			start_udp_tproxy(), net::detached);
	}
#endif // defined(__linux__)
	}

	// 启动 launcher 控制通道（URL 来自 m_option.launcher_url_, 为空不启动）.
	launcher_start();

	// 启动 UDP DNS 服务器（配置了 dns_udp_port_ 或 dns_cache_size_ 时创建）.
	// 即使未启用 UDP 监听，只要启用了 DNS 查询结果缓存，HTTP DNS 路径也
	// 需要共享该缓存实例.
	if (m_option.dns_udp_port_ > 0 || m_option.dns_cache_size_ > 0)
	{
		m_dns_server = std::make_unique<dns_server>(
			m_executor, m_backend_context, m_scheduler_locking, m_option);
		m_dns_server->start();
	}

	// 启动定时器.
	net::co_spawn(m_executor,
		tick(), net::detached);
}

// dns_query_cache 返回 DNS 查询结果缓存（未启用返回 nullptr）.
dns_response_cache* proxy_server::dns_query_cache() noexcept
{
	return m_dns_server ? m_dns_server->cache() : nullptr;
}

// dns_no_ipv6 返回是否禁用 DNS 的 IPv6 解析返回.
bool proxy_server::dns_no_ipv6() const noexcept
{
	return m_option.dns_no_ipv6_;
}

// apply_dns_options 应用 DNS 相关选项热改，在 m_option_mutex 锁内调用.
std::string proxy_server::apply_dns_options() noexcept
{
	if (!m_dns_server)
	{
		// 之前未创建（未启用任何 DNS 选项），现在需要时创建.
		if (m_option.dns_udp_port_ > 0 || m_option.dns_cache_size_ > 0)
		{
			m_dns_server = std::make_unique<dns_server>(
				m_executor, m_backend_context, m_scheduler_locking, m_option);
			m_dns_server->start();
		}
		return {};
	}

	// 已创建：热改到 dns_server（端口/缓存/no_ipv6 变化由 dns_server 内部处理）.
	return m_dns_server->apply_options(m_option);
}

void proxy_server::close() noexcept
{
	boost::system::error_code ignore_ec;
	m_abort = true;

	// 停止 launcher 控制通道: 关闭当前连接, 使 launcher 协程退出.
	m_launcher_state->call_protect_ = {};
	launcher_stop();

	// 停止 UDP DNS 服务器（关闭监听 socket 使协程退出）.
	if (m_dns_server)
		m_dns_server->close();

	// 停止 TUN 设备模式服务器（关闭设备 fd 使读包协程退出）.
	if (m_tun_server)
		m_tun_server->close();

	m_backend_context.stop();
	if (m_backend_thread && m_backend_thread->joinable())
		m_backend_thread->join();

	m_timer.cancel();

	for (auto& acc : m_tcp_acceptors)
		acc->close(ignore_ec);
	for (auto& acceptor : m_unix_acceptors)
		acceptor.close(ignore_ec);

#if defined(__linux__)

	// 先关闭 UDP 监听 socket, 使 start_udp_tproxy_listen 中的
	// 待处理 recvmsg 立即失败返回, 从而触发协程退出清理.
	for (auto& s : m_udp_tproxy_listeners)
		s.close(ignore_ec);

	// 将 flow 清理 post 到 executor, 确保与协程 scope_exit 中
	// 的 erase 在同一个执行上下文中顺序执行, 避免多线程 io_context
	// 下 swap/erase 的数据竞争.
	if (!m_udp_tproxy_flows.empty())
	{
		net::post(m_executor,
			[this, self = shared_from_this()]()
			{
				std::unordered_map<size_t, udp_tproxy_flow_ptr> tmp_flows;
				tmp_flows.swap(m_udp_tproxy_flows);

				for (auto& [_, flow] : tmp_flows)
				{
					if (flow)
					{
						flow->backend_sock_.reset();
						flow->relay_sock_.reset();
						flow->udp_http_sock_.reset();
						flow->notify_timer_.reset();
					}
				}
			});
	}

#endif // defined(__linux__)

	{
		std::lock_guard<std::mutex> lock(m_sessions_mutex);
		for (auto& [id, c] : m_sessions)
		{
			if (auto client = c.lock())
				client->close();
		}
	}
}

size_t proxy_server::num_session()
{
	std::lock_guard<std::mutex> lock(m_sessions_mutex);
	return m_sessions.size();
}

const proxy_server_option& proxy_server::option()
{
	return m_option;
}

net::ssl::context& proxy_server::ssl_context()
{
	return m_ssl_srv_context;
}

//////////////////////////////////////////////////////////////////////////
// launcher 控制通道支持

uint64_t proxy_server::started_at() const
{
	return m_launcher_state->started_at_;
}

const std::string& proxy_server::server_version() const
{
	return m_launcher_state->server_version_;
}

void proxy_server::session_closed(size_t id, uint64_t rx, uint64_t tx,
	const std::string& user)
{
	auto& stats = *m_launcher_state;

	stats.global_rx_ += rx;
	stats.global_tx_ += tx;
	{
		std::lock_guard<std::mutex> lock(stats.user_mutex_);
		// 匿名（未认证）会话以 "(匿名)" 标识计入用户明细, 便于 launcher 统一展示.
		std::string key = user.empty() ? std::string(k_anon_user) : user;
		auto& t = stats.user_totals_[key];
		t.rx += rx;
		t.tx += tx;
		t.conns++;  // 每关闭一个会话计一次累积连接（conn_total 口径）.
	}

	// 移除已关闭的会话.
	std::lock_guard<std::mutex> lock(m_sessions_mutex);
	m_sessions.erase(id);
}

boost::json::object proxy_server::snapshot_report()
{
	auto& stats = *m_launcher_state;
	uint64_t now = static_cast<uint64_t>(std::time(nullptr));

	// ------------------------------------------------------------------
	// 流量统计模型:
	//   global = 已关闭会话累计(global_rx_/global_tx_) + 当前活跃会话实时值
	//   user   = 已关闭该用户累计(user_totals_) + 活跃该用户实时值
	//            + launcher 下发的历史已用流量(user_usage_, 计入 usage_total)
	//
	// 生命周期不变量: session_closed() 在会话析构时把该会话流量并入"已关闭
	// 累计"并从 m_sessions 移除; 快照只把仍存活的会话按实时值计入"活跃".
	// 因此同一会话的流量不会同时出现在两个口径中（不重复、不遗漏）.
	//
	// 匿名（未认证, user 为空）连接以 "(匿名)" 标识计入用户明细, 便于
	// launcher 统一展示; 匿名用户不受配额限制（配额判定在会话侧对空 user
	// 直接放行）.
	//
	// usage_total = rx_bytes + tx_bytes + 续接基线(user_usage_), 为含续接
	// 基线的累计总流量, 供 launcher 持久化后跨重启续接配额.
	//
	// conn_total = 已关闭该用户会话累积连接数 + 当前活跃连接数.
	//
	// 注: 多线程 io_context 下, 会话若在"快照按活跃读取"之后、"析构按已
	// 关闭累计"之前结束, 其流量可能被重复计入一次; 该窗口极小, 且状态上报
	// 本身是近似值, 可接受.
	// ------------------------------------------------------------------

	// 1) 已关闭会话的累计流量（由 session_closed 在会话析构时累加）.
	uint64_t global_rx = stats.global_rx_.load();
	uint64_t global_tx = stats.global_tx_.load();
	int active = 0;

	// 按用户聚合:
	//   rx/tx          已关闭累计 + 当前活跃实时（本次运行期会话级）
	//   closed_conns   已关闭会话累积连接数
	//   active         当前存活连接数
	//   usage_base     launcher 下发的历史已用流量（配额续接基线）
	struct user_agg
	{
		uint64_t rx{ 0 };
		uint64_t tx{ 0 };
		uint64_t closed_conns{ 0 };
		int active{ 0 };
		int64_t usage_base{ 0 };
	};
	std::map<std::string, user_agg> users_agg;

	// 按用户聚合的活跃连接列表, 供状态上报的连接明细使用.
	std::map<std::string, boost::json::array> user_conns;
	constexpr int max_conn_per_user = 200;

	{
		std::lock_guard<std::mutex> lock(stats.user_mutex_);
		for (const auto& [user, t] : stats.user_totals_)
		{
			auto& a = users_agg[user];
			a.rx += t.rx;
			a.tx += t.tx;
			a.closed_conns += t.conns;
		}
	}

	// 2) 当前活跃会话: 实时读取其累计收发, 并入 global 与对应用户.
	{
		std::lock_guard<std::mutex> lock(m_sessions_mutex);
		for ([[maybe_unused]]auto& [id, w] : m_sessions)
		{
			auto s = w.lock();
			if (!s || !s->alive())
				continue;

			uint64_t rx = s->total_rx();
			uint64_t tx = s->total_tx();
			std::string user = s->auth_user();
			if (user.empty())
				user = k_anon_user;  // 匿名连接以 "(匿名)" 展示.

			global_rx += rx;
			global_tx += tx;
			active++;

			auto& a = users_agg[user];
			a.rx += rx;
			a.tx += tx;
			a.active++;

			// 组装连接信息（限制每用户连接数，避免大连接数时 JSON 过大）.
			auto& conns = user_conns[user];
			if (conns.size() >= max_conn_per_user)
				continue;

			boost::json::object conn;
			conn["id"] = static_cast<int64_t>(s->connection_id());
			conn["client_ip"] = s->client();
			conn["target"] = s->target();
			// region: GeoIP 地区列表（geoip() 返回空格分隔的字符串）.
			boost::json::array region;
			std::string geo = s->geoip();
			std::size_t pos = 0;
			while (pos < geo.size())
			{
				auto sp = geo.find(' ', pos);
				if (sp == std::string::npos)
				{
					region.emplace_back(geo.substr(pos));
					break;
				}
				region.emplace_back(geo.substr(pos, sp - pos));
				pos = sp + 1;
			}
			conn["region"] = std::move(region);
			conn["proto"] = s->proto();
			conn["elapsed"] = s->elapsed();
			conn["rx_bytes"] = static_cast<int64_t>(rx);
			conn["tx_bytes"] = static_cast<int64_t>(tx);
			conns.emplace_back(std::move(conn));
		}
	}

	// 3) launcher 下发的用户历史已用流量（配额续接基线; 防御负值）.
	{
		std::lock_guard<std::mutex> lock(stats.usage_mutex_);
		for (const auto& [user, base] : stats.user_usage_)
		{
			if (base > 0)
				users_agg[user].usage_base = base;
		}
	}

	// 4) 用户流量配额.
	std::map<std::string, int64_t> user_quota;
	{
		std::lock_guard<std::mutex> lock(m_option_mutex);
		for (const auto& [user, quota] : m_option.users_quota_)
			user_quota[user] = quota;
	}

	// 5) 组装全局统计.
	boost::json::object report;
	report["ts"] = static_cast<int64_t>(now);
	report["uptime"] = static_cast<int64_t>(now - stats.started_at_);

	boost::json::object global;
	global["rx_bytes"] = static_cast<int64_t>(global_rx);
	global["tx_bytes"] = static_cast<int64_t>(global_tx);
	report["global"] = std::move(global);
	report["active_connections"] = active;
	report["conn_total"] = static_cast<int64_t>(stats.conn_total_.load());

	// 6) 组装用户明细.
	//    usage_total = rx + tx + 续接基线, 为含续接基线的累计总流量,
	//    供 launcher 持久化后跨重启续接配额.
	//    过滤无任何流量与连接的空条目, 避免用户列表出现无意义占位.
	boost::json::array users;
	for (auto& [user, a] : users_agg)
	{
		int64_t conn_total = static_cast<int64_t>(a.closed_conns) + a.active;
		if (a.rx == 0 && a.tx == 0 && conn_total == 0 && a.active == 0)
			continue;

		boost::json::object u;
		u["user"] = user;
		u["rx_bytes"] = static_cast<int64_t>(a.rx);
		u["tx_bytes"] = static_cast<int64_t>(a.tx);
		u["active_connections"] = a.active;
		u["conn_total"] = conn_total;
		u["quota"] = user_quota.count(user) ? user_quota[user] : 0;
		u["usage_total"] = static_cast<int64_t>(a.rx) + static_cast<int64_t>(a.tx) + a.usage_base;

		// 活跃连接列表.
		if (auto it = user_conns.find(user); it != user_conns.end())
			u["connections"] = std::move(it->second);

		users.emplace_back(std::move(u));
	}
	report["users"] = std::move(users);

	return report;
}

boost::json::object proxy_server::apply_options(const boost::json::object& options)
{
	boost::json::array applied;
	boost::json::array needs_restart;
	boost::json::object errors;

	std::lock_guard<std::mutex> lock(m_option_mutex);
	for (const auto& [name, val] : options)
	{
		// 运行期无法生效（需重启）的选项.
		if (name == "stdio" || name == "transparent" ||
			name == "ssl_ciphers" || name == "ssl_prefer_server_ciphers")
		{
			needs_restart.emplace_back(name);
			continue;
		}
		// 本版本接受但暂不影响运行的选项.
		if (name == "http2" || name == "logs_path" ||
			name == "disable_logs" || name == "help" || name == "config")
		{
			applied.emplace_back(name);
			continue;
		}

		bool ok = false;
		// 字符串选项.
		if (name == "pam_auth") { m_option.pam_auth_ = detail::to_str(val); ok = true; }
		else if (name == "local_ip") { m_option.local_ip_ = detail::to_str(val); ok = true; }
		else if (name == "proxy_pass")
		{
			std::string s = detail::to_str(val);
			if (s.empty()) { m_option.proxy_pass_.reset(); ok = true; }
			else if (auto r = urls::parse_uri(s); r.has_value()) { m_option.proxy_pass_ = r.value(); ok = true; }
			else errors[name] = "invalid proxy_pass url";

			// 重置 ssl_client_context, 以便使用新的 proxy_pass 证书验证配置.
			m_ssl_client_context.reset();
		}
		else if (name == "ssl_sni" || name == "proxy_ssl_name") { m_option.proxy_ssl_name_ = detail::to_str(val); ok = true; }
		else if (name == "ssl_certificate_dir") { m_option.ssl_cert_path_ = detail::to_str(val); ok = true; }
		else if (name == "ssl_cacert_dir") { m_option.ssl_cacert_path_ = detail::to_str(val); ok = true; }
		else if (name == "ipip_db") { m_option.ipip_db_ = detail::to_str(val); ok = true; }
		else if (name == "http_doc") { m_option.doc_directory_ = detail::to_str(val); ok = true; }
		else if (name == "dns_upstream")
		{
			std::string s = detail::to_str(val);
			m_option.dns_upstream_ = s.empty() ? std::optional<std::string>{} : std::optional<std::string>{ s };
			ok = true;
		}
		// 布尔选项.
		else if (name == "reuse_port") { m_option.reuse_port_ = detail::to_bool(val); ok = true; }
		else if (name == "happyeyeballs") { m_option.happyeyeballs_ = detail::to_bool(val); ok = true; }
		else if (name == "v6only") { m_option.connect_v6_only_ = detail::to_bool(val); ok = true; }
		else if (name == "v4only") { m_option.connect_v4_only_ = detail::to_bool(val); ok = true; }
		else if (name == "proxy_pass_ssl") { m_option.proxy_pass_use_ssl_ = detail::to_bool(val); ok = true; }
		else if (name == "htpasswd") { m_option.htpasswd_ = detail::to_bool(val); ok = true; }
		else if (name == "autoindex") { m_option.autoindex_ = detail::to_bool(val); ok = true; }
		else if (name == "disable_http") { m_option.disable_http_ = detail::to_bool(val); ok = true; }
		else if (name == "disable_socks") { m_option.disable_socks_ = detail::to_bool(val); ok = true; }
		else if (name == "disable_udp") { m_option.disable_udp_ = detail::to_bool(val); ok = true; }
		else if (name == "disable_insecure") { m_option.disable_insecure_ = detail::to_bool(val); ok = true; }
		else if (name == "disable_check_cert") { m_option.disable_check_cert_ = detail::to_bool(val); ok = true; }
		else if (name == "scramble") { m_option.scramble_ = detail::to_bool(val); ok = true; }
		// 整数选项.
		else if (name == "so_mark")
		{
			int v = detail::to_int(val);
			if (v > 0)
				m_option.so_mark_ = static_cast<uint32_t>(v);
			else
				m_option.so_mark_.reset();
			ok = true;
		}
		else if (name == "tcp_timeout") { m_option.tcp_timeout_ = detail::to_int(val); ok = true; }
		else if (name == "udp_timeout") { m_option.udp_timeout_ = detail::to_int(val); ok = true; }
		else if (name == "rate_limit") { m_option.tcp_rate_limit_ = detail::to_int(val); ok = true; }
		else if (name == "noise_length") { m_option.noise_length_ = detail::to_int(val); ok = true; }
		// DNS 相关选项（UDP DNS 监听端口 / 查询结果缓存 / 禁用 IPv6 解析返回）.
		else if (name == "dns_udp_port")
		{
			int v = detail::to_int(val);
			if (v < 0 || v > 65535)
				errors[name] = "invalid dns_udp_port: " + std::to_string(v);
			else
			{
				m_option.dns_udp_port_ = v;
				ok = true;
			}
		}
		else if (name == "dns_cache_size")
		{
			int v = detail::to_int(val);
			if (v < 0)
				errors[name] = "invalid dns_cache_size: " + std::to_string(v);
			else
			{
				m_option.dns_cache_size_ = v;
				ok = true;
			}
		}
		else if (name == "dns_cache_ttl")
		{
			int v = detail::to_int(val);
			if (v < 0)
				errors[name] = "invalid dns_cache_ttl: " + std::to_string(v);
			else
			{
				m_option.dns_cache_ttl_ = v;
				ok = true;
			}
		}
		else if (name == "dns_no_ipv6")
		{
			m_option.dns_no_ipv6_ = detail::to_bool(val);
			ok = true;
		}
		// 列表选项.
		else if (name == "auth_users")
		{
			std::vector<proxy_server_option::auth_users> list;
			for (const auto& e : detail::to_str_list(val))
			{
				// 解析 user:password:addr:proxy_pass 格式.
				std::vector<std::string> parts;
				std::stringstream ss(e);
				std::string token;
				while (std::getline(ss, token, ':'))
					parts.push_back(token);
				std::string user, password, addr, proxy_url;
				if (parts.size() > 0) user = parts[0];
				if (parts.size() > 1) password = parts[1];
				if (parts.size() > 2) addr = parts[2];
				if (parts.size() > 3) proxy_url = parts[3];
				if (user.empty() && password.empty() && addr.empty() && proxy_url.empty())
					continue;
				std::optional<urls::url> proxy_url_result;
				if (!proxy_url.empty())
				{
					auto result = urls::parse_uri(proxy_url);
					if (result.has_value())
						proxy_url_result = result.value();
				}
				list.emplace_back(user, password, addr, proxy_url_result);
			}
			m_option.auth_users_ = std::move(list);
			ok = true;
		}
		else if (name == "users_rate_limit")
		{
			std::unordered_map<std::string, int> map;
			for (const auto& e : detail::to_str_list(val))
			{
				auto pos = e.find(':');
				if (pos == std::string::npos)
					continue;
				map[e.substr(0, pos)] = std::atoi(e.substr(pos + 1).c_str());
			}
			m_option.users_rate_limit_ = std::move(map);
			ok = true;
		}
		else if (name == "users_quota")
		{
			std::unordered_map<std::string, int64_t> map;
			for (const auto& e : detail::to_str_list(val))
			{
				auto pos = e.find(':');
				if (pos == std::string::npos)
					continue;
				try {
					map[e.substr(0, pos)] = std::stoll(e.substr(pos + 1));
				} catch (...) {}
			}
			m_option.users_quota_ = std::move(map);
			ok = true;
		}

		else if (name == "allow_region")
		{
			std::unordered_set<std::string> set;
			for (const auto& e : detail::to_str_list(val))
			{
				auto parts = strutil::split(e, '|');
				set.insert(parts.begin(), parts.end());
			}
			m_option.allow_regions_ = std::move(set);
			ok = true;
		}
		else if (name == "deny_region")
		{
			std::unordered_set<std::string> set;
			for (const auto& e : detail::to_str_list(val))
			{
				auto parts = strutil::split(e, '|');
				set.insert(parts.begin(), parts.end());
			}
			m_option.deny_regions_ = std::move(set);
			ok = true;
		}
		else if (name == "server_listen")
		{
			// 运行期热配置监听地址: 解析接收到的参数为 (endpoint, v6only) 元组,
			// 然后与 m_tcp_acceptors 双向同步, 最终保证 m_tcp_acceptors 的
			// 监听集合与接收到的 server_listen 参数一致.

			// 解析接收到的监听列表.
			std::vector<std::tuple<tcp::endpoint, bool>> new_listens;
			bool parse_ok = true;
			for (const auto& e : detail::to_str_list(val))
			{
				tcp::endpoint endp;
				bool v6only = false;
				if (!detail::parse_listen_endpoint(e, endp, v6only))
				{
					errors[name] = "invalid listen endpoint: " + e;
					parse_ok = false;
					break;
				}
				new_listens.emplace_back(endp, v6only);
			}
			if (!parse_ok)
				break;  // 解析失败, 不应用本次配置.

			// 更新配置中的监听列表.
			m_option.listens_ = new_listens;

			// 1) 关闭并移除接收参数中已不存在的 acceptor.
			// 被关闭的 acceptor 转移到 m_closed_tcp_acceptors 中保持对象存活,
			// 直到其监听协程退出, 避免引用悬空 (UAF).
			auto it = m_tcp_acceptors.begin();
			while (it != m_tcp_acceptors.end())
			{
				boost::system::error_code lec;
				auto local_ep = (*it)->local_endpoint(lec);
				bool keep = false;
				if (!lec)
				{
					for (const auto& [endp, v6only] : new_listens)
					{
						if (local_ep == endp)
						{
							keep = true;
							break;
						}
					}
				}
				if (keep)
				{
					++it;
				}
				else
				{
					(*it)->close(lec);
					XLOG_WARN << "server_listen, stop listening: " << local_ep;
					m_closed_tcp_acceptors.emplace_back(std::move(*it));
					it = m_tcp_acceptors.erase(it);
				}
			}

			// 2) 为接收参数中新增的 endpoint 创建 acceptor 并启动监听.
			for (const auto& [endp, v6only] : new_listens)
			{
				bool exists = false;
				for (auto& acc : m_tcp_acceptors)
				{
					boost::system::error_code lec;
					if (acc->local_endpoint(lec) == endp)
					{
						exists = true;
						break;
					}
				}
				if (exists)
					continue;

				tcp_acceptor acceptor(m_executor);
				boost::system::error_code ec;

				acceptor.open(endp.protocol(), ec);
				if (ec)
				{
					XLOG_WARN << "server_listen, acceptor open: " << endp
						<< ", error: " << ec.message();
					continue;
				}

				acceptor.set_option(net::socket_base::reuse_address(true), ec);
				if (ec)
				{
					XLOG_WARN << "server_listen, set_option with reuse_address: "
						<< ec.message();
				}

				if (m_option.reuse_port_)
				{
#ifdef ENABLE_REUSEPORT
					acceptor.set_option(reuse_port(true), ec);
					if (ec)
					{
						XLOG_WARN << "server_listen, set_option with SO_REUSEPORT: "
							<< ec.message();
					}
#endif
				}

				if (v6only)
				{
					acceptor.set_option(net::ip::v6_only(true), ec);
					if (ec)
					{
						XLOG_ERR << "server_listen, set v6_only failed: "
							<< ec.message();
						continue;
					}
				}

				acceptor.bind(endp, ec);
				if (ec)
				{
					XLOG_ERR << "server_listen, acceptor bind: " << endp
						<< ", error: " << ec.message();
					continue;
				}

				acceptor.listen(net::socket_base::max_listen_connections, ec);
				if (ec)
				{
					XLOG_ERR << "server_listen, acceptor listen: " << endp
						<< ", error: " << ec.message();
					continue;
				}

				m_tcp_acceptors.emplace_back(
					std::make_unique<tcp_acceptor>(std::move(acceptor)));
				auto& new_acceptor = *m_tcp_acceptors.back();

				// 启动 32 个连接协程为新的 acceptor 服务.
				start_tcp_listen(new_acceptor);

				XLOG_DBG << "server_listen, start listening: " << endp;
			}

			ok = true;
		}
		else
		{
			// 未知选项：接受但记录（避免 WebUI 提交的完整配置报错）.
			applied.emplace_back(name);
			continue;
		}

		if (ok)
			applied.emplace_back(name);
		else
			errors[name] = "apply failed";
	}

	// 应用 DNS 相关选项热改（dns_udp_port/dns_cache_size/dns_cache_ttl/
	// dns_no_ipv6 任一变化都通过 apply_dns_options 同步到 dns_server）.
	apply_dns_options();

	boost::json::object res;
	res["applied"] = std::move(applied);
	res["needs_restart"] = std::move(needs_restart);
	res["errors"] = std::move(errors);
	return res;
}

bool proxy_server::add_auth_user(const std::string& user, const std::string& password,
	const std::string& addr, const std::string& proxy_url, std::string& err)
{
	if (user.empty())
	{
		err = "user is required";
		return false;
	}
	std::optional<urls::url> proxy_url_result;
	if (!proxy_url.empty())
	{
		auto result = urls::parse_uri(proxy_url);
		if (result.has_value())
			proxy_url_result = result.value();
		else
		{
			err = "invalid proxy_url";
			return false;
		}
	}
	std::lock_guard<std::mutex> lock(m_option_mutex);
	// 替换同名用户或追加.
	auto& list = m_option.auth_users_;
	for (auto it = list.begin(); it != list.end(); ++it)
	{
		if (std::get<0>(*it) == user)
		{
			*it = { user, password, addr, proxy_url_result };
			return true;
		}
	}
	list.emplace_back(user, password, addr, proxy_url_result);
	return true;
}

bool proxy_server::del_auth_user(const std::string& user)
{
	std::lock_guard<std::mutex> lock(m_option_mutex);
	auto& list = m_option.auth_users_;
	for (auto it = list.begin(); it != list.end(); ++it)
	{
		if (std::get<0>(*it) == user)
		{
			list.erase(it);
			return true;
		}
	}
	return false;
}

bool proxy_server::set_auth_user_password(const std::string& user, const std::string& password)
{
	std::lock_guard<std::mutex> lock(m_option_mutex);
	for (auto& entry : m_option.auth_users_)
	{
		if (std::get<0>(entry) == user)
		{
			std::get<1>(entry) = password;
			return true;
		}
	}
	return false;
}

bool proxy_server::set_auth_user_rate_limit(const std::string& user, int rate)
{
	std::lock_guard<std::mutex> lock(m_option_mutex);
	if (rate <= 0)
		m_option.users_rate_limit_.erase(user);
	else
		m_option.users_rate_limit_[user] = rate;
	return true;
}

bool proxy_server::set_auth_user_quota(const std::string& user, int64_t quota)
{
	std::lock_guard<std::mutex> lock(m_option_mutex);
	if (quota <= 0)
		m_option.users_quota_.erase(user);
	else
		m_option.users_quota_[user] = quota;
	return true;
}



void proxy_server::set_user_usage(const boost::json::object& usage)
{
	auto& stats = *m_launcher_state;
	std::lock_guard<std::mutex> lock(stats.usage_mutex_);
	for (const auto& [user, v] : usage)
	{
		if (user.empty())
			continue;
		int64_t used = 0;
		if (v.is_int64())
			used = v.as_int64();
		else if (v.is_uint64())
			used = static_cast<int64_t>(v.as_uint64());
		// 只增不减: 对每个用户取 max(当前计数, 传入值), 重连/重复通知
		// 不会把已用的配额"退还".
		if (used <= 0)
			continue;
		auto& cur = stats.user_usage_[std::string(user)];
		if (used > cur)
			cur = used;
	}
}

// 查询用户下载配额（字节）；<=0 或未配置表示不限制.
std::int64_t proxy_server::user_quota(const std::string& user)
{
	std::lock_guard<std::mutex> lock(m_option_mutex);
	auto it = m_option.users_quota_.find(user);
	if (it == m_option.users_quota_.end())
		return 0;
	return it->second;
}

// 查询用户总流量（上行+下行，含 launcher 下发的历史已用流量、已关闭会话与调用方
// 当前会话），用于配额超限判断.
int64_t proxy_server::user_total_flow(const proxy_session* self)
{
	auto& stats = *m_launcher_state;
	int64_t total = 0;
	auto user = self->auth_user();

	// launcher 下发的用户历史已用流量（配额续接，计入总流量）.
	{
		std::lock_guard<std::mutex> lock(stats.usage_mutex_);
		auto it = stats.user_usage_.find(user);
		if (it != stats.user_usage_.end() && it->second > 0)
			total += it->second;
	}
	// 已关闭会话的累计流量（上行+下行）.
	{
		std::lock_guard<std::mutex> lock(stats.user_mutex_);
		auto it = stats.user_totals_.find(user);
		if (it != stats.user_totals_.end())
		{
			total += static_cast<int64_t>(it->second.rx);
			total += static_cast<int64_t>(it->second.tx);
		}
	}
	// 调用方当前会话的流量（上行+下行）；由会话直接提供，无需遍历会话表.
	{
		total += static_cast<int64_t>(self->total_rx());
		total += static_cast<int64_t>(self->total_tx());
	}

	return total;
}

boost::json::object proxy_server::users_state() const
{
	boost::json::array auth_users;
	boost::json::array users_rate_limit;
	boost::json::array users_quota;

	std::lock_guard<std::mutex> lock(m_option_mutex);
	for (const auto& [user, pwd, addr, proxy_pass] : m_option.auth_users_)
	{
		std::string e = user + ":" + pwd;
		if (!addr.empty() || proxy_pass)
		{
			e += ":" + addr;
			if (proxy_pass)
			{
				e += ":";
				e += std::string(proxy_pass->buffer());
			}
		}
		auth_users.emplace_back(e);
	}
	for (const auto& [user, rate] : m_option.users_rate_limit_)
		users_rate_limit.emplace_back(user + ":" + std::to_string(rate));
	for (const auto& [user, quota] : m_option.users_quota_)
		users_quota.emplace_back(user + ":" + std::to_string(quota));

	boost::json::object st;
	st["auth_users"] = std::move(auth_users);
	st["users_rate_limit"] = std::move(users_rate_limit);
	st["users_quota"] = std::move(users_quota);
	return st;
}

net::awaitable<std::optional<net::ip::tcp::endpoint>>
proxy_server::setup_tproxy(proxy_tcp_socket& socket, size_t connection_id) noexcept
{
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

	{
		auto ep = sockaddr_to_udp_endpoint(addr);
		if (ep.address().is_unspecified())
		{
			XLOG_WARN << "connection id: " << connection_id
				<< ", SO_ORIGINAL_DST unexpected family: " << addr.ss_family;
			co_return remote_endp;
		}
		remote_endp.emplace(ep.address(), ep.port());
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

// 切换到后端执行上下文（非锁定调度时）.
// 当 m_scheduler_locking 为 false 时, 协程会切换到后端线程池执行, 适用于需要执行
// 同步操作（如 DNS 解析）的场景. 返回当前应使用的 executor.
net::awaitable<net::any_io_executor> proxy_server::switch_to_backend_executor()
{
	co_return co_await backend_switch_to(
		m_scheduler_locking, m_backend_context, m_executor);
}

// 从后端执行上下文切换回主执行上下文.
net::awaitable<void> proxy_server::switch_from_backend_executor()
{
	co_await backend_switch_from(m_scheduler_locking, m_executor);
}

net::awaitable<void> proxy_server::get_local_address() noexcept
{
	auto self = shared_from_this();
	boost::system::error_code ec;

	// POSIX 平台直接枚举所有网络接口地址，避免依赖主机名解析：
	// OpenWrt 等嵌入式系统的 /etc/hosts 常缺少本机 IP 映射，
	// 仅靠 host_name + DNS 解析拿不到本机地址，会导致 transparent
	// 模式把访问本机监听端口的连接误判为 tproxy 转发目标。
	// 注: Android bionic 的 getifaddrs 需 API 24+, 且 transparent 模式
	// 在 Android 上不可用, 直接跳过接口枚举.
#if (defined(__linux__) || defined(__APPLE__) || defined(__unix__)) && !defined(__ANDROID__)
	struct ifaddrs* ifa = nullptr;
	if (::getifaddrs(&ifa) == 0)
	{
		for (struct ifaddrs* p = ifa; p != nullptr; p = p->ifa_next)
		{
			if (!p->ifa_addr)
				continue;
			if (p->ifa_addr->sa_family == AF_INET)
			{
				auto* sin = reinterpret_cast<struct sockaddr_in*>(p->ifa_addr);
				net::ip::address_v4::bytes_type bytes;
				std::memcpy(bytes.data(), &sin->sin_addr, bytes.size());
				m_local_addrs.insert(net::ip::make_address_v4(bytes));
			}
			else if (p->ifa_addr->sa_family == AF_INET6)
			{
				auto* sin6 = reinterpret_cast<struct sockaddr_in6*>(p->ifa_addr);
				net::ip::address_v6::bytes_type bytes;
				std::memcpy(bytes.data(), &sin6->sin6_addr, bytes.size());
				m_local_addrs.insert(net::ip::make_address_v6(bytes));
			}
		}
		::freeifaddrs(ifa);
	}
#endif

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

	// 切换到后端线程池执行 DNS 解析操作.
	auto executor = co_await switch_to_backend_executor();

	tcp::resolver resolver(executor);
	auto results = co_await resolver.async_resolve(hostname, "", net_awaitable[ec]);

	co_await switch_from_backend_executor();

	if (ec)
	{
		XLOG_WARN
			<< "get_local_address, async_resolve: "
			<< ec.message();

		co_return;
	}

	for (const auto& entry : results)
		m_local_addrs.insert(entry.endpoint().address());
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
			return iponly == ipaddr;

		auto netaddr4 = net::ip::make_network_v4(ip_cidr, ec);
		if (!ec)
		{
			auto target = net::ip::make_network_v4(ipaddr.to_v4(), netaddr4.netmask());
			return target == netaddr4;
		}

		auto netaddr6 = net::ip::make_network_v6(ip_cidr, ec);
		if (!ec)
		{
			auto target = net::ip::make_network_v6(ipaddr.to_v6(), netaddr6.prefix_length());
			return target == netaddr6;
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

// 从 msg 中提取原客户端和原目标地址.
bool proxy_server::parse_udp_tproxy_packet(struct msghdr& msg,
	udp::endpoint& client_ep, udp::endpoint& original_dest)
{
	// 提取原始目标地址, IP_ORIGDSTADDR / IPV6_ORIGDSTADDR 处理逻辑相同.
	for (auto* cmsg = CMSG_FIRSTHDR(&msg); cmsg;
		 cmsg = CMSG_NXTHDR(&msg, cmsg))
	{
		if ((cmsg->cmsg_level == IPPROTO_IP &&
			cmsg->cmsg_type == IP_ORIGDSTADDR) ||
			(cmsg->cmsg_level == IPPROTO_IPV6 &&
			cmsg->cmsg_type == IPV6_ORIGDSTADDR))
		{
			const auto& ss = *reinterpret_cast<const sockaddr_storage*>(
				CMSG_DATA(cmsg));
			original_dest = sockaddr_to_udp_endpoint(ss);
			break;
		}
	}

	if (original_dest.address().is_unspecified())
		return false;

	// 提取客户端端点.
	auto* ss = (struct sockaddr_storage*)msg.msg_name;
	client_ep = sockaddr_to_udp_endpoint(*ss);
	if (client_ep.address().is_unspecified())
		return false;

	return true;
}

// 用 (client_ep, original_dest) 计算查找 flow 的 key.
// 使用 boost::hash_combine 以获得更好的哈希分布.
size_t proxy_server::make_udp_flow_key(const udp::endpoint& client, const udp::endpoint& dest)
{
	size_t h = 0;

	auto hash_addr = [&h](const net::ip::address& addr) {
		if (addr.is_v4())
			boost::hash_combine(h, addr.to_v4().to_uint());
		else
		{
			auto b = addr.to_v6().to_bytes();
			boost::hash_combine(h, std::string_view((const char*)b.data(), b.size()));
		}
	};

	hash_addr(client.address());
	boost::hash_combine(h, client.port());
	hash_addr(dest.address());
	boost::hash_combine(h, dest.port());

	return h;
}

net::awaitable<void> proxy_server::udp_tproxy_check() noexcept
{
	std::vector<size_t> expired_keys;

	for (const auto& [key, flow] : m_udp_tproxy_flows)
	{
		if (!flow)
		{
			expired_keys.push_back(key);
			continue;
		}

		if (flow->expire_++ < m_option.udp_timeout_)
			continue;

		XLOG_DBG
			<< "tproxy flow: " << flow->flow_key_
			<< ", expired: " << flow->expire_
			<< ", client: " << flow->client_endp_
			<< ", original_dest: " << flow->original_endp_;

		flow->backend_sock_.reset();
		flow->relay_sock_.reset();
		flow->udp_http_sock_.reset();

		if (flow->notify_timer_)
			flow->notify_timer_->cancel();

		flow->notify_timer_.reset();

		expired_keys.push_back(key);
	}

	for (const auto& key : expired_keys)
	{
		m_udp_tproxy_flows.erase(key);
	}

	co_return;
}

net::awaitable<void> proxy_server::start_udp_tproxy() noexcept
{
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

	auto scheme = boost::to_lower_copy(
		std::string(m_option.proxy_pass_->scheme()));

	// 如果是 HTTP proxy_pass, 不需要 SOCKS5 UDP ASSOCIATE.
	// connect-udp 隧道将在每个 flow 创建时按需建立, 所以这里直
	// 接退出协程即可.
	if (scheme.starts_with("http"))
	{
		XLOG_DBG << "udp tproxy using RFC 9298 connect-udp via HTTP proxy: "
			<< m_option.proxy_pass_->c_str();
		co_return;
	}

	// 保持与 proxy_pass 之间的 SOCKS5 UDP ASSOCIATE 转发, 以便后续数据包转发使用.
	net::co_spawn(m_executor,
		udp_tproxy_socks5_connect(), net::detached);

	co_return;
}

net::awaitable<void> proxy_server::udp_tproxy_socks5_connect() noexcept
{
	m_retry_tproxy_socks5_connect = false;

	auto ret = co_await do_socks5_associate();
	if (!ret)
		co_return;

	// do_socks5_associate 返回 true 表示控制连接已建立, 并在循环中等待直到
	// 连接断开或服务器关闭. 仅当连接因非关闭原因断开时, 才标记需要重试.
	if (!m_abort)
		m_retry_tproxy_socks5_connect = true;

	co_return;
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
	auto ex = co_await switch_to_backend_executor();

	tcp::resolver resolver{ ex };
	auto targets = co_await resolver.async_resolve(
		proxy_host, std::to_string(proxy_port), net_awaitable[ec]);

	co_await switch_from_backend_executor();

	if (ec)
	{
		XLOG_WARN << "udp tproxy resolve: " << proxy_host
			<< ", error: " << ec.message();

		co_return std::optional<tcp::resolver::results_type>{};
	}

	co_return targets;
}

// 如果配置了 SO_MARK, 则对指定 socket 应用标记.
// 返回设置结果, 调用方负责记录日志.
boost::system::result<void>
proxy_server::apply_so_mark(int fd) noexcept
{
	return proxy::apply_so_mark(fd, m_option.so_mark_);
}

net::awaitable<boost::system::error_code>
proxy_server::connect_to_proxy(tcp::socket& remote_socket, const tcp::resolver::results_type& targets)
{
	boost::system::error_code ec;

	if (m_option.happyeyeballs_)
	{
		// 使用 Happy Eyeballs 并发连接 (RFC 8305), 加快连接建立速度.
		auto endp = co_await asio_util::async_connect(
			remote_socket,
			targets,
			[this](const auto&, auto& stream, auto& endp)
			{
				boost::system::error_code ec;

				auto interface = net::ip::make_address(m_option.local_ip_, ec);
				if (!ec)
				{
					tcp::endpoint bind_endpoint(interface, 0);
					stream.open(bind_endpoint.protocol(), ec);
					if (ec)
						return false;

					stream.bind(bind_endpoint, ec);
					if (ec)
						return false;
				}
				else
				{
					stream.open(endp.endpoint().protocol(), ec);
					if (ec)
						return false;
				}

				// 路由决策发生在 connect 时，须在连接前设置 SO_MARK.
				if (m_option.so_mark_)
				{
					auto ret = apply_so_mark(stream.native_handle());
					if (ret.has_error())
					{
						XLOG_WARN << "connect_to_proxy setsockopt SO_MARK error: "
							<< ret.error().message();
					}
				}

				return true;
			},
			net_awaitable[ec]);

		co_return ec;
	}

	// Happy Eyeballs 被禁用, 使用传统的顺序连接.
	for (const auto& endp : targets)
	{
		if (m_option.connect_v4_only_ && endp.endpoint().address().is_v6())
		{
			XLOG_DBG << "connect_v4_only: skip IPv6 endpoint "
				<< endp.endpoint();
			continue;
		}
		if (m_option.connect_v6_only_ && endp.endpoint().address().is_v4())
		{
			XLOG_DBG << "connect_v6_only: skip IPv4 endpoint "
				<< endp.endpoint();
			continue;
		}

		remote_socket.close(ec);

		if (m_option.so_mark_)
		{
			if (!remote_socket.is_open())
			{
				remote_socket.open(endp.endpoint().protocol(), ec);
				if (ec)
					continue;
			}

			// 路由决策发生在 connect 时，须在连接前设置 SO_MARK.
			auto ret = apply_so_mark(remote_socket.native_handle());
			if (ret.has_error())
			{
				XLOG_WARN << "connect_to_proxy setsockopt SO_MARK error: "
					<< ret.error().message();
			}
		}

		co_await remote_socket.async_connect(endp, net_awaitable[ec]);
		if (!ec)
			co_return ec;
	}

	co_return boost::asio::error::host_not_found;
}

net::awaitable<boost::system::result<bool>>
proxy_server::make_ssl_socket(tcp::socket& remote_socket,
	std::string_view sni,  std::optional<variant_stream_type>& ssl_sock)
{
	boost::system::error_code ec;

	if (!m_ssl_client_context)
	{
		m_ssl_client_context.emplace(net::ssl::context::sslv23_client);

		// 使用通用函数配置 SSL context (验证模式、CA 证书、主机名验证).
		ec = configure_ssl_client_ctx(*m_ssl_client_context,
			m_option.disable_check_cert_,
			std::string(sni),
			m_option.ssl_cacert_path_);
		if (ec)
		{
			// 配置失败, 重置 context, 使下次调用重新初始化.
			m_ssl_client_context.reset();
			co_return ec;
		}
	}

	// 初始化为 SSL 加密的 SOCKS5 控制连接.
	ssl_sock.emplace(init_proxy_stream(std::move(remote_socket), *m_ssl_client_context));
	auto& ssl_socket = boost::variant2::get<ssl_tcp_stream>(*ssl_sock);

	// 设置 SNI 主机名以兼容需要 SNI 的服务器.
	SSL_set_tlsext_host_name(ssl_socket.native_handle(), sni.data());

	// 进行 SSL 握手.
	co_await ssl_socket.async_handshake(
		net::ssl::stream_base::client, net_awaitable[ec]);
	if (ec)
		co_return ec;

	co_return true;
}

net::awaitable<bool> proxy_server::do_socks5_associate()
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

	XLOG_DBG << "udp tproxy connecting to proxy_pass: " << proxy_pass.c_str();

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

	// 设置 TCP Keep-Alive.
	auto ret = set_tcp_keepalive(remote_socket.native_handle());
	if (ret.has_error())
	{
		XLOG_WARN << "udp tproxy do_socks5_associate tcp_keepalive failed: "
			<< ret.error().message();
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

	endpoint_opt backend_endpoint;
	std::optional<variant_stream_type> socks5_control;

	if (use_ssl)
	{
		auto res = co_await make_ssl_socket(remote_socket, sni, socks5_control);
		if (res.has_error())
		{
			XLOG_WARN << "udp tproxy make_ssl_socket error: " << res.error().message();
			co_return false;
		}

		XLOG_DBG << "udp tproxy SSL handshake with " << sni << " succeeded";

		// 进行 SOCKS5 握手以获取 UDP 转发的目标 endpoint.
		backend_endpoint = co_await async_socks_handshake(*socks5_control, opt, net_awaitable[ec]);
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
		backend_endpoint = co_await async_socks_handshake(*socks5_control, opt, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "udp tproxy SOCKS5 handshake failed: " << ec.message();
			co_return false;
		}
	}

	if (!backend_endpoint)
	{
		XLOG_WARN << "udp tproxy failed to get backend endpoint from proxy_pass";
		co_return false;
	}

	// 保存 backend endpoint, 后续将通过这个 endpoint 将 UDP 数据包转发到 proxy_pass 服务
	// 器, 再由 proxy_pass 转发到实际目标服务器.
	m_backend_endp = *backend_endpoint;

	XLOG_DBG << "udp tproxy associate with proxy_pass succeeded, backend endpoint: " << m_backend_endp;

	if (m_backend_endp.address().is_unspecified())
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

		// 更新 backend endpoint 的地址为实际连接的远程地址, 因为某些代理服务器可能返回一个不完
		// 整的 endpoint, 只包含端口但地址为空的情况.
		if (!ec)
			m_backend_endp.address(remote_addr);
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

	XLOG_DBG << "udp tproxy control connection closed";

	co_return true;
}

net::awaitable<void> proxy_server::udp_tproxy_response_loop(udp_tproxy_flow_ptr flow)
{
	auto self = shared_from_this();

	// 退出时清理 flow, 防止 map 泄漏.
	boost::scope::scope_exit flows_guard([this, flow]()
	{
		if (!flow)
			return;

		flow->backend_sock_.reset();
		flow->relay_sock_.reset();
		flow->udp_http_sock_.reset();

		m_udp_tproxy_flows.erase(flow->flow_key_);
	});

	// 创建 backend socket 用于接收来自 proxy_pass 的 UDP 数据包.
	flow->backend_sock_.emplace(m_executor);

	auto& backend_sock = flow->backend_sock_;
	boost::system::error_code ec;

	backend_sock->open(m_backend_endp.protocol(), ec);
	if (ec)
	{
		XLOG_WARN << "tproxy flow: " << flow->flow_key_
			<< ", backend socket open error: " << ec.message();
		co_return;
	}

	// 绑定到一个随机地址.
	backend_sock->bind(udp::endpoint(backend_sock->local_endpoint().protocol(), 0), ec);
	if (ec)
	{
		XLOG_WARN << "tproxy flow: " << flow->flow_key_
			<< ", backend socket bind error: " << ec.message();
		co_return;
	}

	{
		auto ret = apply_so_mark(backend_sock->native_handle());
		if (ret.has_error())
		{
			XLOG_WARN << "tproxy flow: " << flow->flow_key_
				<< ", backend setsockopt SO_MARK error: "
				<< ret.error().message();
		}
	}

	if (!init_relay_socket(flow))
	{
		XLOG_WARN << "tproxy flow: " << flow->flow_key_
			<< ", init_relay_socket failed";
		co_return;
	}

	while (!m_abort)
	{
		char recv_buf[65535];

		auto bytes = co_await backend_sock->async_receive(
			net::buffer(recv_buf), net_awaitable[ec]);
		if (ec)
			break;

		flow->expire_ = 0; // 收到数据包重置过期计数.
		send_response_to_client(flow, recv_buf, bytes);
	}

	co_return;
}

bool proxy_server::init_relay_socket(udp_tproxy_flow_ptr flow)
{
	if (!flow)
		return false;

	// 创建 udp socket 用于与 client 进行数据转发.
	flow->relay_sock_.emplace(m_executor);

	auto& relay_sock = *flow->relay_sock_;
	boost::system::error_code ec;

	relay_sock.open(flow->original_endp_.protocol(), ec);
	if (ec)
	{
		XLOG_WARN
			<< "tproxy flow: " << flow->flow_key_
			<< ", relay socket open error: " << ec.message();
		return false;
	}

	// 设置 SO_REUSEADDR 选项以允许多个 socket 绑定到同一个地址和端口, 以便多个 flow 可以同
	// 时绑定到相同的 original 地址.
	relay_sock.set_option(net::socket_base::reuse_address(true), ec);

	// 设置 IP_TRANSPARENT 选项.
	if (flow->original_endp_.protocol() == net::ip::udp::v4())
		relay_sock.set_option(transparent_opt(true), ec);
	else
		relay_sock.set_option(transparent6_opt(true), ec);

	// 绑定到一个 original 地址, 以模拟 original server 的行为, 让客户端认为这是原
	// 始服务器的响应.
	relay_sock.bind(flow->original_endp_, ec);
	if (ec)
	{
		XLOG_WARN
			<< "tproxy flow: " << flow->flow_key_
			<< ", relay socket bind error: " << ec.message();
		return false;
	}

	return true;
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

	boost::system::error_code ec;

	// 使用 relay_sock 发送数据包回客户端.
	auto& relay_sock = *flow->relay_sock_;
	relay_sock.send_to(net::buffer(payload, payload_len), flow->client_endp_, 0, ec);
	if (ec)
	{
		XLOG_WARN << "tproxy flow: " << flow->flow_key_
			<< ", relay_sock send_to error: " << ec.message();
	}
}

void proxy_server::udp_tproxy_forward_packet(
	udp_tproxy_flow_ptr flow, const char* data, std::size_t len)
{
	if (!flow->backend_sock_)
		return;

	// SOCKS5 UDP: RSV(2) + FRAG(1) + ATYP(1) + addr + port
	// 最大 IPv6 地址长度: 2+1+1+16+2 = 22
	char header[22];
	char* hp = header;

	write<uint16_t>(0x0, hp);
	write<uint8_t>(0x0, hp);

	auto& backend_sock = *flow->backend_sock_;
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

	size_t total_len = header_len + len;

	// UDP 数据包通常不超过 65535 字节.
	if (total_len > 65535)
	{
		XLOG_WARN << "tproxy flow: " << flow->flow_key_
			<< ", packet too large: " << total_len;
		return;
	}

	char buf[65535];
	char* buf_ptr = &buf[0];

	std::memcpy(buf_ptr, header, header_len);
	std::memcpy(buf_ptr + header_len, data, len);

	backend_sock.send_to(net::buffer(buf_ptr, total_len), m_backend_endp, 0, ec);
	if (ec)
	{
		flow->backend_sock_.reset();

		XLOG_WARN
			<< "tproxy flow: " << flow->flow_key_
			<< ", forward error: " << ec.message()
			<< ", client: " << flow->client_endp_
			<< ", dest: " << flow->original_endp_
			<< ", backend endp: " << m_backend_endp;

		m_udp_tproxy_flows.erase(flow->flow_key_);
	}
}

net::awaitable<void> proxy_server::start_udp_tproxy_listen(udp::socket& udp_sock) noexcept
{
	auto self = shared_from_this();

	XLOG_DBG << "udp tproxy listener started on: " << udp_sock.local_endpoint();

	boost::system::error_code ec;

	auto scheme = boost::to_lower_copy(
	std::string(m_option.proxy_pass_->scheme()));

	// 如果上游代理是 HTTP 代理, 则通过 connect-udp 协议进行 udp 代理.
	bool using_connect_udp = scheme.starts_with("http");

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
		if (!parse_udp_tproxy_packet(msg, client_ep, original_dest))
			continue;

		// 计算 flow 表的 key.
		size_t flow_key = make_udp_flow_key(client_ep, original_dest);

		std::shared_ptr<udp_tproxy_flow> flow;

		auto make_tproxy_flow = [&]() mutable
		{
			auto it = m_udp_tproxy_flows.find(flow_key);
			if (it != m_udp_tproxy_flows.end())
			{
				flow = it->second;
				return false;
			}

			flow = std::make_shared<udp_tproxy_flow>(
				client_ep, original_dest, flow_key);

			m_udp_tproxy_flows[flow_key] = flow;

			return true;
		};

		// 创建 flow 对象的 shared_ptr 指针, make_tproxy_flow 返回 true 则表示新建 flow 连接.
		if (make_tproxy_flow())
		{
			BOOST_ASSERT(flow && "flow pointer is null");

			if (using_connect_udp)
			{
				flow->using_connect_udp_ = true;

				net::co_spawn(m_executor,
					udp_tproxy_http_udp_loop(flow), net::detached);
			}
			else
			{
				// 创建 backend socket 后就启动一个协程循环等待 backend socket 上的响应数
				// 据, 并转发回客户端.
				net::co_spawn(m_executor,
					udp_tproxy_response_loop(flow), net::detached);
			}

			XLOG_DBG << "tproxy flow: " << flow_key
				<< ", client: " << client_ep
				<< ", original dest: " << original_dest
				<< ", flow key: " << flow_key
				<< ", mode: " << (using_connect_udp ? "connect-udp" : "socks5");
		}

		BOOST_ASSERT(flow && "udp tproxy flow is null");
		flow->expire_ = 0; // 收到数据包重置过期计数.

		if (using_connect_udp)
			udp_tproxy_forward_packet_http(flow, data, static_cast<size_t>(ret));
		else
			udp_tproxy_forward_packet(flow, data, static_cast<size_t>(ret));
	}

	XLOG_DBG << "udp tproxy listener stopped";
}

// UDP 透传 connect-udp 转发: 将客户端数据封装成 capsule 并推入发送队列, 由串行发送协程处理.
// UDP 透传 connect-udp 转发: 将客户端数据封装成 capsule 后推入发送队列,
// 并通知发送协程处理. 发送队列确保对 udp_http_sock_ 的 TCP 写入是串行的.
void proxy_server::udp_tproxy_forward_packet_http(
	udp_tproxy_flow_ptr flow, const char* data, std::size_t len)
{
	// 构建 DATAGRAM capsule (RFC 9297):
	//   capsule type (varint) = 0x00
	//   capsule length (varint) = len + 1  # 这里 + 1 是因为 context ID 为 0 占用一字节
	//   HTTP Datagram Payload (RFC 9298):
	//     context ID (varint) = 0
	//     UDP payload

	uint8_t buf[65536];
	size_t pos = 0;

	pos += varint_int_encode(udp_proxy_capsule_type, buf + pos); // capsule type
	pos += varint_int_encode(1 + len, buf + pos);				 // capsule length
	pos += varint_int_encode(0, buf + pos);						 // context ID

	if (pos + len > sizeof(buf))
	{
		XLOG_WARN << "tproxy flow: " << flow->flow_key_
			<< ", capsule too large: " << (pos + len);
		return;
	}

	std::memcpy(buf + pos, data, len);							 // UDP payload
	pos += len;

	// 推入发送队列并通知发送协程.
	{
		flow->send_queue_.emplace_back(buf, buf + pos);

		// 通知发送协程有新数据到达.
		if (flow->notify_timer_)
			flow->notify_timer_->cancel();
	}
}

// UDP TPROXY connect-udp 循环: 连接上游 HTTP 代理, 建立 RFC 9298 隧道,
// 然后从 TCP 连接接收 capsule, 提取 UDP payload 并通过 relay_sock_ 送回客户端.
net::awaitable<void> proxy_server::udp_tproxy_http_udp_loop(udp_tproxy_flow_ptr flow)
{
	auto self = shared_from_this();

	// 退出时清理.
	boost::scope::scope_exit flows_guard([this, flow]()
	{
		if (!flow)
			return;

		flow->udp_http_sock_.reset();
		flow->backend_sock_.reset();
		flow->notify_timer_.reset();

		auto flow_key = make_udp_flow_key(
			flow->client_endp_, flow->original_endp_);

		m_udp_tproxy_flows.erase(flow_key);
	});

	if (!init_relay_socket(flow))
	{
		XLOG_WARN << "tproxy flow: " << flow->flow_key_
			<< ", init_relay_socket failed";
		co_return;
	}

	auto proxy_pass = *m_option.proxy_pass_;

	// 解析 proxy_pass 地址和端口.
	auto proxy_host = std::string(proxy_pass.host());
	auto proxy_port = proxy_pass.port_number();
	if (proxy_port == 0)
		proxy_port = urls::default_port(proxy_pass.scheme_id());

	auto targets = co_await resolve_proxy_pass(proxy_pass);
	if (!targets || targets->empty())
		co_return;

	// 连接到上游 HTTP 代理.
	tcp::socket remote_socket(m_executor);
	boost::system::error_code ec;

	ec = co_await connect_to_proxy(remote_socket, *targets);
	if (ec)
	{
		XLOG_WARN << "tproxy flow: " << flow->flow_key_
			<< ", connect to http proxy failed: " << ec.message();
		co_return;
	}

	XLOG_DBG << "tproxy flow: " << flow->flow_key_
		<< ", connected to http proxy: " << proxy_host << ":" << proxy_port;

	// 检查是否使用 SSL.
	bool use_ssl = m_option.proxy_pass_use_ssl_;
	if (proxy_pass.scheme().ends_with("s"))
		use_ssl = true;

	// 保存 HTTP socket 的对象引用.
	std::optional<variant_stream_type>& http_sock_opt = flow->udp_http_sock_;

	if (use_ssl)
	{
		auto sni = m_option.proxy_ssl_name_.empty() ? proxy_host : m_option.proxy_ssl_name_;
		auto res = co_await make_ssl_socket(remote_socket, sni, http_sock_opt);
		if (res.has_error())
		{
			XLOG_WARN << "tproxy flow: " << flow->flow_key_
				<< ", make_ssl_socket failed: " << res.error().message();
			co_return;
		}

		XLOG_DBG << "tproxy flow: " << flow->flow_key_
			<< ", udp tproxy SSL handshake with " << sni << " succeeded";
	}
	else
	{
		http_sock_opt.emplace(init_proxy_stream(std::move(remote_socket)));
	}

	// 构建 RFC 9298 connect-udp 请求, 使用 absolute-form URI.
	auto& original_dest = flow->original_endp_;
	std::string target_host = original_dest.address().to_string();
	uint16_t target_port = original_dest.port();

	// 对 IPv6 地址中的冒号进行百分号编码.
	if (original_dest.address().is_v6())
		boost::replace_all(target_host, ":", "%3A");

	// 使用 boost.beast 构建 HTTP 请求.
	std::string target =
		"https://" + proxy_host + ":" + std::to_string(proxy_port) +
		"/.well-known/masque/udp/" + target_host + "/" +
		std::to_string(target_port) + "/";

	http::request<http::empty_body> req{http::verb::get, target, 11};
	req.set(http::field::host, proxy_host + ":" + std::to_string(proxy_port));
	req.set(http::field::connection, "Upgrade");
	req.set(http::field::upgrade, "connect-udp");
	req.set("Capsule-Protocol", "?1");

	// 如果 proxy_pass 需要认证, 添加 Proxy-Authorization 头.
	if (!proxy_pass.user().empty())
	{
		const auto userinfo =
			std::string(proxy_pass.user()) + ":" +
			std::string(proxy_pass.password());
		req.set(http::field::proxy_authorization,
			"Basic " + strutil::base64_encode(userinfo));
	}

	beast::flat_buffer resp_buf;
	http::response_parser<http::empty_body> resp_parser;
	resp_parser.skip(true);

	// 获取 HTTP socket 的引用.
	auto& http_sock = *http_sock_opt;

	// 使用 boost.beast 的序列化器发送 HTTP 请求.
	co_await http::async_write(http_sock, req, net_awaitable[ec]);
	if (ec)
	{
		XLOG_WARN << "tproxy flow: " << flow->flow_key_
			<< ", send connect-udp request failed: " << ec.message();
		co_return;
	}

	// 读取响应 headers.
	co_await http::async_read_header(
		http_sock, resp_buf, resp_parser, net_awaitable[ec]);
	if (ec)
	{
		XLOG_WARN << "tproxy flow: " << flow->flow_key_
			<< ", read connect-udp response failed: " << ec.message();
		co_return;
	}

	auto resp = resp_parser.release();
	if (resp.result() != http::status::switching_protocols)
	{
		XLOG_WARN << "tproxy flow: " << flow->flow_key_
			<< ", connect-udp rejected: "
			<< static_cast<int>(resp.result());
		co_return;
	}

	XLOG_DBG << "tproxy flow: " << flow->flow_key_
		<< ", connect-udp tunnel established to target: "
		<< original_dest;

	// 创建通知定时器并启动发送协程, 用于串行发送队列中的 UDP 数据包.
	flow->notify_timer_.emplace(m_executor);

	net::co_spawn(m_executor, [this, self, flow]() -> net::awaitable<void>
	{
		boost::system::error_code ec;

		while (!self->m_abort)
		{
			std::vector<char> item;

			if (!flow->send_queue_.empty())
			{
				item = std::move(flow->send_queue_.front());
				flow->send_queue_.pop_front();
			}

			// 检查 socket 或 timer 是否仍有效.
			if (!flow->udp_http_sock_ || !flow->notify_timer_)
				break;

			if (item.empty())
			{
				// 队列为空, 等待通知.
				flow->notify_timer_->expires_after(std::chrono::seconds(1));
				co_await flow->notify_timer_->async_wait(net_awaitable[ec]);
				continue;
			}

			auto& http_sock = *flow->udp_http_sock_;

			co_await net::async_write(http_sock, net::buffer(item), net_awaitable[ec]);
			if (ec)
			{
				XLOG_WARN
					<< "tproxy flow: " << flow->flow_key_
					<< ", connect-udp forward error: " << ec.message()
					<< ", client: " << flow->client_endp_
					<< ", dest: " << flow->original_endp_;
				break;
			}
		}
	}, net::detached);

	// 隧道建立成功, 进入响应读取循环: 从 http 接收 capsule, 提取 UDP payload,
	// 通过 relay_sock_ 发送回客户端.
	while (!m_abort)
	{
		// 读取 capsule type.
		auto capsule_type = co_await read_varint_from_stream(http_sock, ec);
		if (ec)
			break;

		// 读取 capsule length.
		auto capsule_length = co_await read_varint_from_stream(http_sock, ec);
		if (ec)
			break;

		if (capsule_length > 65535)
		{
			XLOG_WARN << "tproxy flow: " << flow->flow_key_
				<< ", capsule too large: " << capsule_length;
			break;
		}

		// 读取 capsule value.
		std::string capsule_value(
			static_cast<size_t>(capsule_length), '\0');
		if (capsule_length > 0)
		{
			co_await net::async_read(
				http_sock, net::buffer(capsule_value),
				net_awaitable[ec]);
			if (ec) break;
		}

		// 仅处理 DATAGRAM capsule (RFC 9297).
		if (capsule_type != udp_proxy_capsule_type)
		{
			XLOG_DBG << "tproxy flow: " << flow->flow_key_
				<< ", unknown capsule type: " << capsule_type;
			continue;
		}

		// 解析 context ID.
		auto val_data = reinterpret_cast<const uint8_t*>(capsule_value.data());
		size_t val_len = capsule_value.size();

		if (val_len == 0)
			continue;

		auto [ctx_id_len, ctx_id] = varint_int_decode(val_data);
		if (ctx_id != 0)
		{
			XLOG_DBG << "tproxy flow: " << flow->flow_key_
				<< ", unknown context ID: " << ctx_id;
			continue;
		}

		// UDP payload.
		size_t udp_len = val_len - ctx_id_len;
		if (udp_len == 0) continue;

		flow->expire_ = 0;

		auto& relay_sock = *flow->relay_sock_;
		relay_sock.send_to(
			net::buffer(val_data + ctx_id_len, udp_len), flow->client_endp_, 0, ec);
		if (ec)
		{
			XLOG_WARN << "tproxy flow: " << flow->flow_key_
				<< ", relay_sock send_to error: " << ec.message();
			break;
		}
	}

	XLOG_DBG << "tproxy flow: " << flow->flow_key_
		<< ", connect-udp loop ended";

	co_return;
}


#endif // defined(__linux__)

//////////////////////////////////////////////////////////////////////////
// start_accept 模板实现

template <typename T, typename S>
net::awaitable<void> proxy_server::start_accept(T& acceptor, S& socket)
{
	boost::system::error_code error;
	net::ip::tcp::no_delay no_delay_opt(true);
	net::ip::tcp::no_delay delay_opt(false);

	auto self = shared_from_this();

	co_await acceptor.async_accept(socket.lowest_layer(), net_awaitable[error]);
	if (error)
	{
		if (error == net::error::operation_aborted || m_abort)
			co_return;

		XLOG_WARN << "start_proxy_listen, async_accept: " << error.message();

		// 添加退避延迟，避免 accept 失败时快速循环消耗 CPU，使用 50ms 延迟作为简单的退避策略.
		net::steady_timer timer(co_await net::this_coro::executor);
		timer.expires_after(std::chrono::milliseconds(50));
		co_await timer.async_wait(net_awaitable[error]);
		co_return;
	}

	// 将 IPv4 映射的 IPv6 地址转换为 IPv4 字符串表示, 以便在日志中显示.
	auto address_to_string = [](const net::ip::address& addr) -> std::string
	{
		if (addr.is_v6() && addr.to_v6().is_v4_mapped())
		{
			auto v4_addr = net::ip::make_address_v4(net::ip::v4_mapped, addr.to_v6());
			return v4_addr.to_string();
		}
		return addr.to_string();
	};

	static std::atomic_size_t id{ 1 };
	size_t connection_id = id++;

	std::vector<std::string> local_info;
	std::string client;
	std::string client_addr;
	// client_ep 客户端地址（host:port），写入会话供状态上报连接明细展示.
	std::string client_ep;
	std::string region;

	if constexpr (std::same_as<S, proxy_tcp_socket>)
	{
		auto endp = tcp_remote_endpoint(socket);
		client_addr = address_to_string(endp.address());
		local_info.push_back(client_addr);
		client = client_addr + ":" + std::to_string(endp.port());
		client_ep = client;

		if (m_ip_database)
		{
			try {
				auto [ret, isp] = m_ip_database->lookup(endp.address());
				if (!ret.empty())
				{
					for (auto& c : ret)
					{
						client += " " + c;
						if (!region.empty())
							region += " ";
						region += c;
					}

					local_info.insert(local_info.end(), ret.begin(), ret.end());
				}
			}
			catch (const std::exception&)
			{}
		}
	}
	else if constexpr (std::same_as<S, proxy_uds_socket>)
	{
		auto endp = uds_remote_endpoint(socket);
		client = endp.path();
		client_addr = endp.path();
		client_ep = client_addr;
	}

	XLOG_DBG << "connection id: " << connection_id
		<< ", start client incoming: " << client;

	if (!region_filter(local_info))
	{
		XLOG_WARN << "connection id: " << connection_id
			<< ", region filter: " << client;
		co_return;
	}

	// 设置 TCP keepalive.
	set_tcp_keepalive(socket.lowest_layer().native_handle());

	// 设置 TCP_NODELAY (非 scramble 模式默认已设) 和 TCP_QUICKACK.
	if (!m_option.scramble_)
	{
#if defined(__linux__)
		// 启用 TCP_QUICKACK, 减少 ACK 延迟, 提升交互式代理场景的响应速率.
		auto ret = set_tcp_quickack(socket.lowest_layer().native_handle(), true);
		if (ret.has_error())
		{
			XLOG_WARN << "connection id: " << connection_id
				<< ", tcp quickack error: " << ret.error().message();
		}
#endif
	}

#if defined (__linux__)
	std::optional<net::ip::tcp::endpoint> tproxy_endpoint;
	// 是否启用透明代理.
	if constexpr (std::same_as<S, proxy_tcp_socket>)
	{
		if (m_option.transparent_)
			tproxy_endpoint = co_await setup_tproxy(socket, connection_id);
	}
#endif

	// 在启用 scramble 时, 刻意开启 Nagle's algorithm 以尽量保证数据包
	// 被重组, 尽最大可能避免观察者通过观察 ip 数据包大小的规律来分析 tcp
	// 数据发送调用, 从而增加噪声加扰的强度.
	if (m_option.scramble_)
		socket.set_option(delay_opt, error);
	else
		socket.set_option(no_delay_opt, error);

	// 创建 proxy_session 对象.
	auto new_session =
		std::make_shared<proxy_session>(
			m_executor,
			m_backend_context,
			m_scheduler_locking,
			m_dns_cache,
			init_proxy_stream(std::move(socket)),
			connection_id,
			self);

	// 在会话发布到 m_sessions 之前写入客户端信息（之后只读，无锁）.
	// client_ep 为 host:port（UDS 场景为路径），供状态上报连接明细展示.
	new_session->set_client_info(client_ep, region);

	// 保存 proxy_session 对象到 m_sessions 中.
	{
		std::lock_guard<std::mutex> lock(m_sessions_mutex);
		m_sessions[connection_id] = new_session;
	}

	// 累计连接数（供 launcher 状态上报）.
	m_launcher_state->conn_total_++;

#if defined (__linux__)
	if constexpr (std::same_as<S, proxy_tcp_socket>)
	{
		if (tproxy_endpoint)
			new_session->setup_tproxy(*tproxy_endpoint);
	}
#endif

	// 启动 proxy_session 对象.
	new_session->start();

	co_return;
}

// start_proxy_listen 模板实现

template <typename T>
net::awaitable<void> proxy_server::start_proxy_listen(T& acceptor) noexcept
{
	auto self = shared_from_this();

	while (!m_abort)
	{
		// acceptor 可能被运行时 server_listen 热配置关闭删除, 关闭后
		// 退出监听协程, 避免对已关闭的 acceptor 反复 accept 造成忙循环.
		if (!acceptor.is_open())
		{
			XLOG_WARN << "start_proxy_listen exit (acceptor closed) ...";
			break;
		}

		if constexpr (std::same_as<std::decay_t<T>, tcp_acceptor>)
		{
			proxy_tcp_socket socket(m_executor);
			co_await start_accept(acceptor, socket);
		}
		else if constexpr (std::same_as<std::decay_t<T>, unix_acceptor>)
		{
			proxy_uds_socket socket(m_executor);
			co_await start_accept(acceptor, socket);
		}
	}

	XLOG_WARN << "start_proxy_listen exit ...";
	co_return;
}

// 显式实例化模板 start_proxy_listen
template net::awaitable<void> proxy_server::start_proxy_listen<tcp_acceptor>(
	tcp_acceptor& acceptor) noexcept;
template net::awaitable<void> proxy_server::start_proxy_listen<unix_acceptor>(
	unix_acceptor& acceptor) noexcept;

// 显式实例化模板 start_accept
template net::awaitable<void> proxy_server::start_accept<tcp_acceptor, proxy_tcp_socket>(
	tcp_acceptor& acceptor, proxy_tcp_socket& socket);
template net::awaitable<void> proxy_server::start_accept<unix_acceptor, proxy_uds_socket>(
	unix_acceptor& acceptor, proxy_uds_socket& socket);

// 为指定 TCP acceptor 启动 32 个监听协程（供 apply_options 运行时
// server_listen 热配置调用；非模板函数, 定义于模板之后, 便于实例化）.
void proxy_server::start_tcp_listen(tcp_acceptor& acceptor)
{
	for (int i = 0; i < 32; i++)
	{
		net::co_spawn(m_executor,
			start_proxy_listen(acceptor), net::detached);
	}
}

} // namespace proxy