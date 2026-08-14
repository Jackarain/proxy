//
// http_server.cpp
// ~~~~~~~~~~~~~~~
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "http_server.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

#ifdef _WIN32
# include <winsock2.h>
#else
# include <sys/socket.h>
#endif

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/filesystem.hpp>
#include <boost/scope/scope_exit.hpp>

#include <openssl/asn1.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

#include <tinyrpc/jsonrpc.hpp>

#include "datetime.hpp"
#include "options.hpp"
#include "webui_embedded.hpp"

namespace launcher {

namespace net = boost::asio;
namespace ssl = net::ssl;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace fs = boost::filesystem;
namespace json = boost::json;

using tcp = net::ip::tcp;

namespace {

using response = http::response<http::string_body>;

// ---- 小工具 ----

// 拆分路径为段。
std::vector<std::string> split_path(const std::string& p)
{
	std::vector<std::string> out;
	std::string cur;
	for (char c : p) {
		if (c == '/') {
			if (!cur.empty()) {
				out.push_back(cur);
				cur.clear();
			}
		} else {
			cur += c;
		}
	}
	if (!cur.empty())
		out.push_back(cur);
	return out;
}

// URL 解码。
std::string url_decode(const std::string& s)
{
	std::string out;
	out.reserve(s.size());
	for (std::size_t i = 0; i < s.size(); i++) {
		if (s[i] == '%' && i + 2 < s.size()) {
			auto hex = [](char c) -> int {
				if (c >= '0' && c <= '9') return c - '0';
				if (c >= 'a' && c <= 'f') return c - 'a' + 10;
				if (c >= 'A' && c <= 'F') return c - 'A' + 10;
				return -1;
			};
			int hi = hex(s[i + 1]);
			int lo = hex(s[i + 2]);
			if (hi >= 0 && lo >= 0) {
				out += static_cast<char>((hi << 4) | lo);
				i += 2;
				continue;
			}
		}
		out += s[i];
	}
	return out;
}

// 解析查询参数。
std::map<std::string, std::string> parse_query(const std::string& target)
{
	std::map<std::string, std::string> out;
	auto q = target.find('?');
	if (q == std::string::npos)
		return out;
	std::string query = target.substr(q + 1);
	std::size_t pos = 0;
	while (pos < query.size()) {
		auto amp = query.find('&', pos);
		std::string pair = query.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
		auto eq = pair.find('=');
		if (eq != std::string::npos)
			out[url_decode(pair.substr(0, eq))] = url_decode(pair.substr(eq + 1));
		else
			out[url_decode(pair)] = "";
		if (amp == std::string::npos)
			break;
		pos = amp + 1;
	}
	return out;
}

// Base64 解码。
std::string base64_decode(const std::string& in)
{
	static const char* tbl =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	int lut[256];
	std::fill(std::begin(lut), std::end(lut), -1);
	for (int i = 0; i < 64; i++)
		lut[static_cast<unsigned char>(tbl[i])] = i;

	std::string out;
	int buf = 0;
	int bits = 0;
	for (unsigned char c : in) {
		if (c == '=' || c == '\n' || c == '\r')
			continue;
		int v = lut[c];
		if (v < 0)
			continue;
		buf = (buf << 6) | v;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			out += static_cast<char>((buf >> bits) & 0xff);
		}
	}
	return out;
}

std::string mime_type(const std::string& path)
{
	auto dot = path.rfind('.');
	if (dot == std::string::npos)
		return "application/octet-stream";
	std::string ext = path.substr(dot);
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
	if (ext == ".js" || ext == ".mjs")
		return "application/javascript";
	if (ext == ".css")
		return "text/css";
	if (ext == ".html" || ext == ".htm")
		return "text/html";
	if (ext == ".json")
		return "application/json";
	if (ext == ".ico")
		return "image/x-icon";
	if (ext == ".png")
		return "image/png";
	if (ext == ".svg")
		return "image/svg+xml";
	if (ext == ".woff")
		return "font/woff";
	if (ext == ".woff2")
		return "font/woff2";
	if (ext == ".txt")
		return "text/plain";
	return "application/octet-stream";
}

// ---- 响应构造 ----

response make_json_response(http::status status, const json::value& v)
{
	response res{ status, 11 };
	res.set(http::field::content_type, "application/json; charset=utf-8");
	res.body() = json::serialize(v);
	res.prepare_payload();
	return res;
}

response make_text_response(http::status status, const std::string& body)
{
	response res{ status, 11 };
	res.set(http::field::content_type, "text/plain; charset=utf-8");
	res.set(http::field::x_content_type_options, "nosniff");
	res.body() = body;
	res.prepare_payload();
	return res;
}

response make_error(http::status status, const std::string& msg)
{
	return make_text_response(status, msg + "\n");
}

// 任意值转 int64（供 handler 使用）。
std::int64_t as_int(const json::value& v)
{
	if (v.is_int64())
		return v.as_int64();
	if (v.is_uint64())
		return static_cast<std::int64_t>(v.as_uint64());
	if (v.is_double())
		return static_cast<std::int64_t>(v.as_double());
	if (v.is_bool())
		return v.as_bool() ? 1 : 0;
	if (v.is_string()) {
		try {
			return std::stoll(std::string(v.as_string()));
		} catch (...) {}
	}
	return 0;
}

std::string as_str(const json::value& v)
{
	if (v.is_string())
		return std::string(v.as_string());
	return {};
}

// ---- 证书加载 ----

bool looks_like_private_key(const std::string& data)
{
	return data.find("PRIVATE KEY") != std::string::npos ||
		data.find("ENCRYPTED") != std::string::npos;
}

void extract_cert_names(X509* cert, std::string& cn, std::vector<std::string>& sans)
{
	// 提取 CN（subject CommonName）。
	auto* subj = X509_get_subject_name(cert);
	int idx = subj ? X509_NAME_get_index_by_NID(subj, NID_commonName, -1) : -1;
	if (idx >= 0) {
		auto* entry = X509_NAME_get_entry(subj, idx);
		auto* data = entry ? X509_NAME_ENTRY_get_data(entry) : nullptr;
		if (data && ASN1_STRING_length(data) > 0)
			cn.assign(reinterpret_cast<const char*>(ASN1_STRING_get0_data(data)),
				static_cast<std::size_t>(ASN1_STRING_length(data)));
	}

	// SAN（subjectAltName）。
	std::unique_ptr<GENERAL_NAMES, decltype(&GENERAL_NAMES_free)> names{
		static_cast<GENERAL_NAMES*>(X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr)),
		&GENERAL_NAMES_free
	};
	if (names) {
		int n = sk_GENERAL_NAME_num(names.get());
		for (int i = 0; i < n; i++) {
			GENERAL_NAME* gn = sk_GENERAL_NAME_value(names.get(), i);
			if (gn->type == GEN_DNS && gn->d.dNSName) {
				auto* s = gn->d.dNSName;
				if (ASN1_STRING_type(s) == V_ASN1_IA5STRING) {
					const unsigned char* data = ASN1_STRING_get0_data(s);
					int len = ASN1_STRING_length(s);
					if (data && len > 0)
						sans.emplace_back(reinterpret_cast<const char*>(data), static_cast<std::size_t>(len));
				}
			}
		}
	}
}

// 读取 PEM 证书链。
std::vector<X509*> read_cert_chain(const std::string& path)
{
	std::vector<X509*> chain;
	FILE* f = std::fopen(path.c_str(), "rb");
	if (!f)
		return chain;
	X509* cert = nullptr;
	while ((cert = PEM_read_X509(f, nullptr, nullptr, nullptr)) != nullptr)
		chain.push_back(cert);
	std::fclose(f);
	return chain;
}

} // namespace

bool load_certificates(const std::string& dir, std::vector<cert_entry>& entries, std::string& err)
{
	boost::system::error_code ec;
	if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
		err = "certificate directory not found: " + dir;
		return false;
	}

	// 收集候选证书文件。
	std::vector<fs::path> cert_files;
	for (fs::recursive_directory_iterator it(dir, ec), end; it != end && !ec; it.increment(ec)) {
		if (ec)
			break;
		if (fs::is_regular_file(it->path(), ec)) {
			std::string name = it->path().filename().string();
			std::string lower = name;
			std::transform(lower.begin(), lower.end(), lower.begin(),
				[](unsigned char c) { return std::tolower(c); });
			auto dot = lower.rfind('.');
			std::string ext = dot == std::string::npos ? "" : lower.substr(dot);
			bool is_cert = ext == ".crt" || ext == ".cer" || ext == ".pem" || ext == ".cert";
			if (!is_cert)
				continue;
			std::string data;
			std::ifstream ifs(it->path().string(), std::ios::binary);
			std::stringstream ss;
			ss << ifs.rdbuf();
			data = ss.str();
			if (looks_like_private_key(data))
				continue;
			cert_files.push_back(it->path());
		}
	}

	for (const auto& cf : cert_files) {
		std::string dir_path = cf.parent_path().string();
		std::string base = cf.stem().string();
		std::vector<std::string> key_candidates = {
			(fs::path(dir_path) / (base + ".key")).string(),
			(fs::path(dir_path) / (base + ".pem")).string(),
			(fs::path(dir_path) / "key.pem").string(),
			(fs::path(dir_path) / "private.key").string(),
			(fs::path(dir_path) / "privkey.pem").string(),
		};
		// 目录下仅有一个 *.key 时也作为候选。
		std::vector<fs::path> key_files;
		for (fs::directory_iterator it(dir_path), end; it != end; it++) {
			if (fs::is_regular_file(it->path(), ec)) {
				std::string lower = it->path().filename().string();
				std::transform(lower.begin(), lower.end(), lower.begin(),
					[](unsigned char c) { return std::tolower(c); });
				if (lower.size() > 4 && lower.substr(lower.size() - 4) == ".key")
					key_files.push_back(it->path());
			}
		}
		if (key_files.size() == 1)
			key_candidates.push_back(key_files[0].string());

		// 验证候选 key 文件确实是私钥。
		std::string key_path;
		for (const auto& kc : key_candidates) {
			if (kc == cf.string())
				continue;
			std::ifstream ifs(kc, std::ios::binary);
			std::stringstream ss;
			ss << ifs.rdbuf();
			if (looks_like_private_key(ss.str())) {
				key_path = kc;
				break;
			}
		}
		if (key_path.empty())
			continue;

		auto chain = read_cert_chain(cf.string());
		if (chain.empty())
			continue;
		// 校验 key 可加载。
		FILE* kf = std::fopen(key_path.c_str(), "rb");
		if (!kf) {
			for (auto* c : chain)
				X509_free(c);
			continue;
		}
		EVP_PKEY* pkey = PEM_read_PrivateKey(kf, nullptr, nullptr, nullptr);
		std::fclose(kf);
		if (!pkey) {
			for (auto* c : chain)
				X509_free(c);
			continue;
		}
		// 校验 key 与证书匹配。
		if (X509_check_private_key(chain[0], pkey) != 1) {
			for (auto* c : chain)
				X509_free(c);
			EVP_PKEY_free(pkey);
			continue;
		}

		cert_entry e;
		e.cert_file_ = cf.string();
		e.key_file_ = key_path;
		extract_cert_names(chain[0], e.domain_, e.sans_);
		entries.push_back(std::move(e));

		for (std::size_t i = 1; i < chain.size(); i++)
			X509_free(chain[i]);
		X509_free(chain[0]);
		EVP_PKEY_free(pkey);
	}

	if (entries.empty()) {
		err = "no certificate found in " + dir;
		return false;
	}
	return true;
}

namespace {

// 通配符主机名匹配。
bool match_hostname(const std::string& name, const std::string& domain,
	const std::vector<std::string>& sans)
{
	std::vector<std::string> patterns = sans;
	if (!domain.empty())
		patterns.push_back(domain);
	std::string n = name;
	std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return std::tolower(c); });
	if (!n.empty() && n.back() == '.')
		n.pop_back();
	for (auto p : patterns) {
		std::transform(p.begin(), p.end(), p.begin(), [](unsigned char c) { return std::tolower(c); });
		if (!p.empty() && p.back() == '.')
			p.pop_back();
		if (p == n)
			return true;
		if (p.size() > 2 && p.substr(0, 2) == "*.") {
			std::string suffix = p.substr(1);
			if (n.size() > suffix.size() && n.compare(n.size() - suffix.size(), suffix.size(), suffix) == 0)
				return true;
		}
	}
	return false;
}

struct sni_ctx {
	std::vector<std::vector<X509*>> chains;
	std::vector<EVP_PKEY*> keys;
	std::vector<std::string> domains;
	std::vector<std::vector<std::string>> sans;

	// 释放本对象持有的证书与私钥引用。
	// 注意：首条链的额外证书 (i>=1) 已通过 SSL_CTX_add_extra_chain_cert
	// 转移所有权给 SSL_CTX，由 SSL_CTX 析构时释放，不能在此重复释放。
	~sni_ctx()
	{
		for (std::size_t i = 0; i < chains.size(); ++i)
		{
			if (i == 0)
			{
				if (!chains[i].empty())
					X509_free(chains[i][0]);
				continue;
			}
			for (auto* c : chains[i])
				X509_free(c);
		}
		for (auto* k : keys)
			EVP_PKEY_free(k);
	}
};

int sni_callback(SSL* ssl, int* /*al*/, void* arg)
{
	auto* ctx = static_cast<sni_ctx*>(arg);
	if (ctx->chains.empty())
		return SSL_TLSEXT_ERR_ALERT_FATAL;
	const char* name = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	std::size_t idx = 0;
	if (name && *name) {
		std::string host(name);
		for (std::size_t i = 0; i < ctx->domains.size(); i++) {
			if (match_hostname(host, ctx->domains[i], ctx->sans[i])) {
				idx = i;
				break;
			}
		}
	}
	SSL_use_certificate(ssl, ctx->chains[idx][0]);
	SSL_use_PrivateKey(ssl, ctx->keys[idx]);
#ifdef BOOST_ASIO_USE_WOLFSSL
	// wolfSSL 未提供 SSL_clear_chain_certs（仅 SSL_CTX 级别），
	// 且其 use_certificate 会整体替换证书链，无需显式清空。
#else
	SSL_clear_chain_certs(ssl);
#endif
	for (std::size_t i = 1; i < ctx->chains[idx].size(); i++)
		SSL_add1_chain_cert(ssl, ctx->chains[idx][i]);
	return SSL_TLSEXT_ERR_OK;
}

} // namespace

std::shared_ptr<net::ssl::context> build_ssl_context(const std::vector<cert_entry>& entries)
{
	// 先收集证书与私钥：SNI 回调按主机名动态选择证书，
	// 数据须在握手期间存活，故由返回的 context 的 deleter 持有。
	auto sni = std::make_shared<sni_ctx>();
	for (const auto& e : entries) {
		auto chain = read_cert_chain(e.cert_file_);
		if (chain.empty())
			continue;
		FILE* kf = std::fopen(e.key_file_.c_str(), "rb");
		if (!kf) {
			for (auto* c : chain)
				X509_free(c);
			continue;
		}
		EVP_PKEY* pkey = PEM_read_PrivateKey(kf, nullptr, nullptr, nullptr);
		std::fclose(kf);
		if (!pkey) {
			for (auto* c : chain)
				X509_free(c);
			continue;
		}
		if (X509_check_private_key(chain[0], pkey) != 1) {
			for (auto* c : chain)
				X509_free(c);
			EVP_PKEY_free(pkey);
			continue;
		}
		sni->chains.push_back(std::move(chain));
		sni->keys.push_back(pkey);
		sni->domains.push_back(e.domain_);
		sni->sans.push_back(e.sans_);
	}

	// 自定义 deleter：先释放 SSL_CTX（归还 add_extra_chain_cert 已转移
	// 所有权的首条链额外证书），再释放 sni 持有的其余证书数据。
	std::shared_ptr<net::ssl::context> ctx(
		new net::ssl::context(net::ssl::context::tls_server),
		[sni](net::ssl::context* c) { delete c; });
	net::ssl::context::options opts = net::ssl::context::default_workarounds |
		net::ssl::context::no_sslv2 | net::ssl::context::no_sslv3 |
		net::ssl::context::no_tlsv1 | net::ssl::context::no_tlsv1_1;
	ctx->set_options(opts);
	SSL_CTX_set_min_proto_version(ctx->native_handle(), TLS1_2_VERSION);

	// 首条作为默认证书。
	if (sni->chains.empty())
		return ctx;
	SSL_CTX_use_certificate(ctx->native_handle(), sni->chains[0][0]);
	for (std::size_t i = 1; i < sni->chains[0].size(); i++)
		SSL_CTX_add_extra_chain_cert(ctx->native_handle(), sni->chains[0][i]);
	SSL_CTX_use_PrivateKey(ctx->native_handle(), sni->keys[0]);

	if (sni->chains.size() > 1) {
		// 多证书：SNI 回调按主机名匹配。
		SSL_CTX_set_tlsext_servername_callback(ctx->native_handle(), sni_callback);
		SSL_CTX_set_tlsext_servername_arg(ctx->native_handle(), sni.get());
	}
	return ctx;
}

// =====================================================================

http_server::http_server(std::shared_ptr<manager> mgr, net::io_context& ioc,
	std::string webui_user, std::string webui_password, std::string version)
	: m_mgr_(std::move(mgr))
	, m_ioc_(ioc)
	, m_webui_user_(std::move(webui_user))
	, m_webui_password_(std::move(webui_password))
	, m_version_(std::move(version))
{}

http_server::~http_server()
{
	stop();
}

bool http_server::start(const std::string& listen_addr, bool https,
	const std::string& ssl_dir, std::string& err)
{
	// 解析 host:port。
	auto colon = listen_addr.rfind(':');
	if (colon == std::string::npos) {
		err = "invalid --listen address: " + listen_addr;
		return false;
	}
	std::string host = listen_addr.substr(0, colon);
	std::string port_str = listen_addr.substr(colon + 1);
	int port = 0;
	try {
		port = std::stoi(port_str);
	} catch (...) {}
	if (port <= 0 || port > 65535) {
		err = "invalid --listen port: " + port_str;
		return false;
	}

	boost::system::error_code ec;
	net::ip::address addr;
	if (host.empty() || host == "*")
		addr = net::ip::address_v6::any();
	else if (host == "0.0.0.0")
		addr = net::ip::address_v4::any();
	else if (host == "::")
		addr = net::ip::address_v6::any();
	else
		addr = net::ip::make_address(host, ec);
	if (ec) {
		err = "invalid --listen address: " + listen_addr;
		return false;
	}

	if (https) {
		std::vector<cert_entry> entries;
		std::string cerr;
		if (!load_certificates(ssl_dir, entries, cerr)) {
			err = "load ssl certificates failed: " + cerr;
			return false;
		}
		m_ssl_ctx_ = build_ssl_context(entries);
	}

	m_acceptor_ = std::make_unique<tcp::acceptor>(m_ioc_.get_executor());
	m_acceptor_->open(addr.is_v6() ? tcp::v6() : tcp::v4(), ec);
	if (ec) {
		err = "open listener failed: " + ec.message();
		return false;
	}
	m_acceptor_->set_option(net::socket_base::reuse_address(true), ec);
	m_acceptor_->bind(tcp::endpoint(addr, static_cast<unsigned short>(port)), ec);
	if (ec) {
		err = "listen " + listen_addr + " failed: " + ec.message();
		return false;
	}
	m_acceptor_->listen(net::socket_base::max_listen_connections, ec);
	if (ec) {
		err = "listen " + listen_addr + " failed: " + ec.message();
		return false;
	}

	m_stopped_ = false;
	// 协程化的 accept 循环（挂在共享 io_context 上）。
	net::co_spawn(m_ioc_, accept_loop(), net::detached);
	return true;
}

void http_server::stop()
{
	bool expected = false;
	if (!m_stopped_.compare_exchange_strong(expected, true))
		return;
	if (m_acceptor_) {
		boost::system::error_code ec;
		m_acceptor_->close(ec);
	}
	// 唤醒所有活动连接，使挂起的协程以错误完成并退出（不停止 ioc）。
	std::lock_guard<std::mutex> lock(m_conn_mu_);
	for (auto h : m_conns_) {
#ifdef _WIN32
		::shutdown(h, SD_BOTH);
#else
		::shutdown(h, SHUT_RDWR);
#endif
	}
}

void http_server::add_conn(net::ip::tcp::socket::native_handle_type h)
{
	std::lock_guard<std::mutex> lock(m_conn_mu_);
	m_conns_.insert(h);
}

void http_server::remove_conn(net::ip::tcp::socket::native_handle_type h)
{
	std::lock_guard<std::mutex> lock(m_conn_mu_);
	m_conns_.erase(h);
}

net::awaitable<void> http_server::accept_loop()
{
	// 纯 Asio 协程化 accept：async_accept 挂在 io_context 上，
	// 无需 poll 轮询停止标志——stop() 关闭 acceptor 后挂起中的
	// async_accept 以 operation_aborted 完成，循环随即退出。
	auto ex = co_await net::this_coro::executor;
	while (!m_stopped_.load()) {
		tcp::socket sock(ex);
		boost::system::error_code ec;
		co_await m_acceptor_->async_accept(sock, net::redirect_error(net::use_awaitable, ec));
		if (m_stopped_.load())
			break;
		if (ec) {
			// 监听器被关闭（stop）或出致命错误。
			if (ec == net::error::operation_aborted)
				break;
			// 瞬时错误（如 EMFILE）：短暂停顿后重试。
			net::steady_timer timer(ex);
			timer.expires_after(std::chrono::milliseconds(200));
			co_await timer.async_wait(net::use_awaitable);
			continue;
		}
		boost::system::error_code sec;
		sock.set_option(net::socket_base::keep_alive(true), sec);
		// 每个连接一个协程，挂在共享 io_context 上处理。
		net::co_spawn(ex, handle_connection(std::move(sock)), net::detached);
	}
}

// ---- 连接处理 ----

// 处理一个连接（plain / TLS）：全部在共享 io_context 的线程池上协程化。
net::awaitable<void> http_server::handle_connection(tcp::socket sock)
{
	boost::system::error_code ec;
	if (m_ssl_ctx_) {
		try {
			beast::ssl_stream<tcp::socket> stream(std::move(sock), *m_ssl_ctx_);
			co_await stream.async_handshake(ssl::stream_base::server,
				net::redirect_error(net::use_awaitable, ec));
			if (ec)
				co_return;
			co_await serve_http(stream);
		} catch (...) {}
	} else {
		try {
			co_await serve_http(sock);
		} catch (...) {}
	}
}

template <class Stream>
net::awaitable<void> http_server::serve_http(Stream& stream)
{
	// 登记活动连接（stop 时 shutdown 使挂起协程退出）。
	auto conn = stream.lowest_layer().native_handle();
	add_conn(conn);
	auto conn_guard = boost::scope::scope_exit([this, conn] { remove_conn(conn); });

	for (;;) {
		beast::flat_buffer buffer;
		http::request<http::string_body> req;
		boost::system::error_code ec;
		co_await http::async_read(stream, buffer, req,
			net::redirect_error(net::use_awaitable, ec));
		if (ec == http::error::end_of_stream)
			break;
		if (ec)
			break;

		std::string target(req.target());
		auto q = target.find('?');
		std::string path = q == std::string::npos ? target : target.substr(0, q);
		bool upgrade = req[http::field::upgrade] == "websocket";

		if (path == "/rpc" && upgrade) {
			auto params = parse_query(target);
			auto in = m_mgr_->ws_auth(params["instance"], params["token"]);
			if (!in) {
				auto resp = make_error(http::status::unauthorized, "unauthorized");
				co_await http::async_write(stream, resp,
					net::redirect_error(net::use_awaitable, ec));
				break;
			}
			// 升级为 WebSocket 并运行 JSON-RPC 控制通道。
			websocket::stream<Stream> ws(std::move(stream));
			co_await ws.async_accept(req, net::redirect_error(net::use_awaitable, ec));
			if (ec)
				break;
			ws.read_message_max(16 * 1024 * 1024);
			ws.binary(true);

			using ws_stream = websocket::stream<Stream>;
			auto sess = std::make_shared<jsonrpc::jsonrpc_session<ws_stream>>(std::move(ws));
			jsonrpc_session ch(sess);
			std::uint64_t gen = in->chan_gen_.fetch_add(1) + 1;
			auto closed = std::make_shared<std::atomic<bool>>(false);

			// 通知（register/status/log）转发到 manager。
			sess->notify_callback([mgr = m_mgr_, in](json::object obj) {
				std::string method = json_str(obj, "method");
				json::value params;
				if (auto p = obj.if_contains("params"); p)
					params = *p;
				mgr->handle_notify(in, method, params);
			});
			// 未绑定方法：回复 -32601。
			sess->default_method_callback([sess](json::object obj) {
				json::object err;
				err["code"] = kCodeMethod;
				err["message"] = "method not found";
				auto id = obj.if_contains("id") ? obj.at("id") : json::value();
				sess->reply(std::move(err), id, true);
			});
			sess->closed_callback([closed]() { closed->store(true); });

			sess->start();
			m_mgr_->ws_attached(in, std::move(ch));

			// 等待连接关闭（协程，运行在共享 io_context 上）。
			auto ex = co_await net::this_coro::executor;
			net::steady_timer timer(ex);
			while (!closed->load() && !m_stopped_.load()) {
				timer.expires_after(std::chrono::milliseconds(50));
				co_await timer.async_wait(net::use_awaitable);
			}
			m_mgr_->ws_detached(in, gen);
			// default_method_callback 捕获了 sess 自身, 形成
			// 会话 -> 回调 -> 会话 的引用循环; 连接关闭后必须清除,
			// 否则会话及其持有的 socket 资源永远不会被释放.
			sess->default_method_callback();
			break;
		}

		// WebUI Basic 鉴权（/rpc 已在上方处理，不受影响）。
		if (!m_webui_user_.empty()) {
			bool ok = false;
			auto auth = req[http::field::authorization];
			if (auth.size() > 6 && auth.substr(0, 6) == "Basic ") {
				std::string decoded = base64_decode(std::string(auth.substr(6)));
				auto colon = decoded.find(':');
				if (colon != std::string::npos) {
					std::string user = decoded.substr(0, colon);
					std::string pass = decoded.substr(colon + 1);
					ok = user == m_webui_user_ && pass == m_webui_password_;
				}
			}
			if (!ok) {
				auto resp = make_error(http::status::unauthorized, "unauthorized");
				resp.set(http::field::www_authenticate, "Basic realm=\"launcher\"");
				co_await http::async_write(stream, resp,
					net::redirect_error(net::use_awaitable, ec));
				break;
			}
		}

		// 路由处理（协程：涉及 RPC 的操作异步等待 proxy_server 响应）。
		auto resp = co_await route(req);
		co_await http::async_write(stream, resp,
			net::redirect_error(net::use_awaitable, ec));
		if (ec)
			break;
		if (!resp.keep_alive() || !req.keep_alive())
			break;
	}
}

// ---- 路由 ----

net::awaitable<response> http_server::route(const http::request<http::string_body>& req)
{
	std::string target(req.target());
	auto q = target.find('?');
	std::string path = q == std::string::npos ? target : target.substr(0, q);

	// /api/version
	if (path == "/api/version") {
		if (req.method() != http::verb::get)
			co_return make_error(http::status::method_not_allowed, "method not allowed");
		json::object o;
		o["version"] = m_version_;
		co_return make_json_response(http::status::ok, o);
	}

	// /api/options
	if (path == "/api/options") {
		if (req.method() != http::verb::get)
			co_return make_error(http::status::method_not_allowed, "method not allowed");
		json::array out;
		for (const auto& o : all_options()) {
			if (o.hidden_)
				continue;
			json::object it;
			it["name"] = o.name_;
			it["kind"] = kind_type_name(o.kind_);
			it["category"] = o.category_;
			it["help"] = o.help_;
			if (!o.hint_.empty())
				it["hint"] = o.hint_;
			if (o.has_default_) {
				switch (o.kind_) {
				case option_kind::boolean:
					it["default"] = o.def_bool_;
					break;
				case option_kind::integer:
					it["default"] = o.def_int_;
					break;
				case option_kind::string_list: {
					json::array arr;
					for (const auto& s : o.def_list_)
						arr.emplace_back(s);
					it["default"] = std::move(arr);
					break;
				}
				default:
					it["default"] = o.def_str_;
					break;
				}
			}
			it["restart_only"] = o.restart_only_;
			it["common"] = o.common_;
			out.emplace_back(std::move(it));
		}
		co_return make_json_response(http::status::ok, out);
	}

	// /api/instances
	if (path == "/api/instances") {
		if (req.method() == http::verb::get)
			co_return make_json_response(http::status::ok, m_mgr_->summaries());
		if (req.method() == http::verb::post) {
			boost::system::error_code ec;
			auto jv = json::parse(req.body(), ec);
			if (ec || !jv.is_object()) {
				co_return make_error(http::status::bad_request, "invalid body: " + ec.message());
			}
			const auto& obj = jv.as_object();
			std::string name = as_str(obj.if_contains("name") ? obj.at("name") : json::value());
			json::object config;
			if (auto c = obj.if_contains("config"); c && c->is_object())
				config = c->as_object();
			std::string err;
			auto in = m_mgr_->create(name, std::move(config), err);
			if (!in) {
				co_return make_error(http::status::bad_request, err);
			}
			json::object o;
			o["id"] = in->id_;
			o["name"] = in->name_;
			co_return make_json_response(http::status::ok, o);
		}
		co_return make_error(http::status::method_not_allowed, "method not allowed");
	}

	// /api/instances/...
	if (path.rfind("/api/instances/", 0) == 0) {
		std::string rest = path.substr(std::string("/api/instances/").size());
		auto parts = split_path(rest);
		if (parts.empty()) {
			co_return make_error(http::status::not_found, "not found");
		}
		const std::string& id = parts[0];

		if (parts.size() == 1) {
			if (req.method() == http::verb::get) {
				view v;
				if (!m_mgr_->view(id, v))
					co_return make_error(http::status::not_found, "not found");
				json::object o;
				o["id"] = v.id_;
				o["name"] = v.name_;
				o["state"] = v.state_;
				o["online"] = v.online_;
				o["pid"] = v.pid_;
				o["autostart"] = v.autostart_;
				o["config"] = json::value(v.config_);
				o["created_at"] = rfc3339_format(v.created_at_);
				co_return make_json_response(http::status::ok, o);
			}
			if (req.method() == http::verb::delete_) {
				std::string err;
				if (!co_await m_mgr_->del(id, err))
					co_return make_error(http::status::not_found, err);
				json::object o;
				o["ok"] = true;
				co_return make_json_response(http::status::ok, o);
			}
			if (req.method() == http::verb::put) {
				boost::system::error_code ec;
				auto jv = json::parse(req.body(), ec);
				if (ec || !jv.is_object())
					co_return make_error(http::status::bad_request, "invalid body: " + ec.message());
				const auto& obj = jv.as_object();
				std::string name = as_str(obj.if_contains("name") ? obj.at("name") : json::value());
				std::optional<bool> autostart;
				if (auto a = obj.if_contains("autostart"); a && a->is_bool())
					autostart = a->as_bool();
				if (name.empty() && !autostart)
					co_return make_error(http::status::bad_request, "nothing to update");
				std::string err;
				if (!m_mgr_->update(id, name, autostart, err))
					co_return make_error(http::status::not_found, err);
				json::object o;
				o["ok"] = true;
				co_return make_json_response(http::status::ok, o);
			}
			co_return make_error(http::status::method_not_allowed, "method not allowed");
		}

		const std::string& action = parts[1];

		if (action == "start" || action == "stop" || action == "restart") {
			if (req.method() != http::verb::post)
				co_return make_error(http::status::method_not_allowed, "method not allowed");
			std::string err;
			bool ok = false;
			if (action == "start")
				ok = m_mgr_->start(id, err);
			else if (action == "stop")
				ok = co_await m_mgr_->stop(id, err);
			else
				ok = co_await m_mgr_->restart(id, err);
			if (!ok)
				co_return make_error(http::status::bad_request, err);
			json::object o;
			o["ok"] = true;
			co_return make_json_response(http::status::ok, o);
		}

		if (action == "status") {
			if (req.method() != http::verb::get)
				co_return make_error(http::status::method_not_allowed, "method not allowed");
			json::value out;
			if (!m_mgr_->status_view(id, out))
				co_return make_error(http::status::not_found, "not found");
			co_return make_json_response(http::status::ok, out);
		}

		if (action == "logs") {
			if (req.method() != http::verb::get)
				co_return make_error(http::status::method_not_allowed, "method not allowed");
			std::int64_t since = 0;
			auto qp = parse_query(target);
			if (auto it = qp.find("since"); it != qp.end()) {
				try {
					since = std::stoll(it->second);
				} catch (...) {}
			}
			json::value out;
			if (!m_mgr_->logs(id, since, out))
				co_return make_error(http::status::not_found, "not found");
			co_return make_json_response(http::status::ok, out);
		}

		if (action == "config") {
			if (req.method() != http::verb::put)
				co_return make_error(http::status::method_not_allowed, "method not allowed");
			boost::system::error_code ec;
			auto jv = json::parse(req.body(), ec);
			if (ec || !jv.is_object())
				co_return make_error(http::status::bad_request, "invalid body: " + ec.message());
			const auto& obj = jv.as_object();
			auto c = obj.if_contains("config");
			if (!c || !c->is_object())
				co_return make_error(http::status::bad_request, "config is required");
			json::value result;
			std::string err;
			if (!co_await m_mgr_->apply_config(id, c->as_object(), result, err))
				co_return make_error(http::status::bad_request, err);
			co_return make_json_response(http::status::ok, result);
		}

		if (action == "users") {
			if (req.method() == http::verb::post) {
				boost::system::error_code ec;
				auto jv = json::parse(req.body(), ec);
				if (ec || !jv.is_object())
					co_return make_error(http::status::bad_request, "invalid body: " + ec.message());
				json::value result;
				std::string err;
				if (!co_await m_mgr_->add_user(id, jv.as_object(), result, err))
					co_return make_error(http::status::bad_request, err);
				co_return make_json_response(http::status::ok, result);
			}
			if (parts.size() < 3)
				co_return make_error(http::status::not_found, "not found");
			const std::string& user = parts[2];
			if (req.method() == http::verb::delete_) {
				json::value result;
				std::string err;
				if (!co_await m_mgr_->del_user(id, user, result, err))
					co_return make_error(http::status::bad_request, err);
				co_return make_json_response(http::status::ok, result);
			}
			if (req.method() == http::verb::put) {
				boost::system::error_code ec;
				auto jv = json::parse(req.body(), ec);
				if (ec || !jv.is_object())
					co_return make_error(http::status::bad_request, "invalid body: " + ec.message());
				const auto& obj = jv.as_object();
				json::value result;
				std::string err;
				if (parts.size() >= 4 && parts[3] == "rate") {
					int rate = static_cast<int>(as_int(obj.if_contains("rate") ? obj.at("rate") : json::value()));
					if (!co_await m_mgr_->set_user_rate_limit(id, user, rate, result, err))
						co_return make_error(http::status::bad_request, err);
					co_return make_json_response(http::status::ok, result);
				}
				if (parts.size() >= 4 && parts[3] == "quota") {
					std::int64_t quota = as_int(obj.if_contains("quota") ? obj.at("quota") : json::value());
					if (!co_await m_mgr_->set_user_quota(id, user, quota, result, err))
						co_return make_error(http::status::bad_request, err);
					co_return make_json_response(http::status::ok, result);
				}
				std::string password = as_str(obj.if_contains("password") ? obj.at("password") : json::value());
				if (password.empty())
					co_return make_error(http::status::bad_request, "password is required");
				if (!co_await m_mgr_->set_user_password(id, user, password, result, err))
					co_return make_error(http::status::bad_request, err);
				co_return make_json_response(http::status::ok, result);
			}
			co_return make_error(http::status::method_not_allowed, "method not allowed");
		}

		co_return make_error(http::status::not_found, "not found");
	}

	// 静态资源与 index.html（内嵌于可执行文件）。
	if (path == "/" || path == "/index.html") {
		// 渲染后的 index（含版本参数）。
		const embedded_file* f = find_embedded_file("index.html");
		if (!f)
			co_return make_error(http::status::not_found, "404 page not found");
		std::string html(reinterpret_cast<const char*>(f->data), f->size);
		// 替换 {{.Version}} 为构建 git hash。
		auto pos = html.find("{{.Version}}");
		if (pos != std::string::npos)
			html.replace(pos, std::string("{{.Version}}").size(), m_version_);
		response res{ http::status::ok, 11 };
		res.set(http::field::content_type, "text/html; charset=utf-8");
		res.set(http::field::cache_control, "no-store, max-age=0");
		res.body() = std::move(html);
		res.prepare_payload();
		co_return res;
	}

	// 静态文件（内嵌，no-store 禁用缓存）。
	std::string rel = path;
	if (!rel.empty() && rel.front() == '/')
		rel = rel.substr(1);
	// 防目录穿越。
	if (rel.find("..") != std::string::npos)
		co_return make_error(http::status::not_found, "404 page not found");
	const embedded_file* f = find_embedded_file(rel);
	if (!f)
		co_return make_error(http::status::not_found, "404 page not found");
	response res{ http::status::ok, 11 };
	res.set(http::field::content_type, mime_type(rel));
	res.set(http::field::cache_control, "no-store, max-age=0");
	res.body().assign(reinterpret_cast<const char*>(f->data), f->size);
	res.prepare_payload();
	co_return res;
}

} // namespace launcher
