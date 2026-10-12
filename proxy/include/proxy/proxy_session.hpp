//
// proxy_session.hpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2023_10_18__PROXY_SESSION_HPP
#define INCLUDE__2023_10_18__PROXY_SESSION_HPP


#include "proxy/use_awaitable.hpp"
#include "proxy/async_connect.hpp"
#include "proxy/logging.hpp"
#include "proxy/launcher_log.hpp"
#include "proxy/variant_stream.hpp"
#include "proxy/default_cert.hpp"
#include "proxy/fileop.hpp"
#include "proxy/strutil.hpp"
#include "proxy/ip_database.hpp"

#include "proxy/socks_enums.hpp"
#include "proxy/socks_client.hpp"
#include "proxy/http_proxy_client.hpp"
#include "proxy/socks_io.hpp"

#include "proxy/xxhash.hpp"
#include "proxy/scramble.hpp"

#include "proxy/proxy_stream.hpp"
#include "proxy/url_info.hpp"
#include "proxy/dns_cache.hpp"
#include "proxy/dns_response_cache.hpp"
#include "proxy/proxy_util.hpp"
#include "proxy/proxy_config.hpp"


#include <fmt/xchar.h>
#include <fmt/format.h>


#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/network_v4.hpp>
#include <boost/asio/ip/network_v6.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/stream_file.hpp>
#include <boost/asio/local/stream_protocol.hpp>

#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4702)
#endif // _MSC_VER

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/core/detail/base64.hpp>

#ifdef _MSC_VER
# pragma warning(pop)
#endif

#include <boost/url.hpp>
#include <boost/regex.hpp>

#include <boost/nowide/convert.hpp>
#include <boost/nowide/fstream.hpp>

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4819)
#endif

#include <boost/json.hpp>

#ifdef _MSC_VER
# pragma warning(pop)
#endif

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/scope/scope_exit.hpp>

#ifdef USE_BOOST_FILESYSTEM
// 为避免低版本(gcc-12.3 以下)的 libstdc++ 中 filesystem 的
// bug( https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95048 )
// 可以使用 boost.filesystem 来规避这个 bug
# include <boost/filesystem.hpp>
#else
# include <filesystem>
#endif // USE_BOOST_FILESYSTEM

#include <cstdlib>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <cstdint>
#include <span>
#include <sstream>
#include <string>
#include <array>
#include <thread>
#include <type_traits>
#include <vector>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#ifdef USE_PAM_AUTH
# include <security/pam_appl.h>
# include <security/pam_misc.h>
#endif // USE_PAM_AUTH

#ifdef __linux__
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/ip.h>
# include <arpa/inet.h>
#endif // __linux__


namespace proxy {

	namespace net = boost::asio;

	using namespace net::experimental::awaitable_operators;
	using namespace util;

	using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
	using udp = net::ip::udp;               // from <boost/asio/ip/udp.hpp>

	namespace beast = boost::beast;			// from <boost/beast.hpp>
	namespace http = beast::http;           // from <boost/beast/http.hpp>

	namespace urls = boost::urls;			// form <boost/url.hpp>

#ifdef USE_BOOST_FILESYSTEM
	namespace fs = boost::filesystem;
#else
	namespace fs = std::filesystem;
#endif

	using string_body = http::string_body;
	using dynamic_body = http::dynamic_body;
	using buffer_body = http::buffer_body;

	using dynamic_request = http::request<dynamic_body>;
	using string_request = http::request<string_body>;

	using string_response = http::response<string_body>;
	using buffer_response = http::response<buffer_body>;

	using request_parser = http::request_parser<string_request::body_type>;

	using response_serializer = http::response_serializer<buffer_body, http::fields>;
	using string_response_serializer = http::response_serializer<string_body, http::fields>;

	//////////////////////////////////////////////////////////////////////////

	// noise_injection_max_len 用于指定噪声注入的最大长度, 单位为字节.
	inline const uint16_t noise_injection_max_len = 0x0fff;

	// global_known_proto 用于指定全局已知的协议, 用于噪声注入时避免生成已知的协议头.
	inline const std::set<uint8_t> global_known_proto =
		{
			0x04, // socks4
			0x05, // socks5
			0x47, // 'G'
			0x50, // 'P'
			0x48, // 'H'
			0x44, // 'D'
			0x4F, // 'O'
			0x54, // 'T'
			0x43, // 'C'
			0x16, // ssl
		};

	//////////////////////////////////////////////////////////////////////////

	inline const char* version_string = R"x*x*x(nginx/1.20.2)x*x*x";

	//////////////////////////////////////////////////////////////////////////



	//////////////////////////////////////////////////////////////////////////

	// proxy_session 前向声明.
	class proxy_session;

	// proxy server 虚基类, 任何 proxy server 的实现, 必须基于这个基类.
	// 这样 proxy_session 才能通过虚基类指针访问 proxy server 的具体实
	// 现以及虚函数方法.
	class proxy_server_base {
	public:
		virtual ~proxy_server_base() {}
		virtual size_t num_session() = 0;
		virtual const proxy_server_option& option() = 0;
		virtual net::ssl::context& ssl_context() = 0;

		// 会话结束时上报累计流量（供 launcher 状态按用户/全局统计）。
		// 默认空实现，不要求所有 proxy_server 都实现统计。
		virtual void session_closed(
			size_t id, uint64_t rx, uint64_t tx, const std::string& user) = 0;

		// 查询用户流量配额（字节，按上行+下行总和计算）；<=0 或未配置表示不限制. 线程安全.
		virtual int64_t user_quota(const std::string& user) = 0;

		// 查询用户总流量（上行+下行，含 launcher 下发的历史已用流量、已关闭会话与调用方
		// 当前会话），用于配额超限判断；未配置返回 0. 线程安全.
		// self 为发起检查的会话指针（可为空，为空则仅统计历史与已关闭部分）.
		virtual int64_t user_total_flow(const proxy_session* self) = 0;

		// 申请一个用户连接计数槽位（用于用户连接数限制）；返回 false 表示
		// 当前连接数已达限制. user 为空（匿名）时直接返回 true. 线程安全.
		virtual bool acquire_user_connection(const std::string& user) = 0;

		// 释放一个用户连接计数槽位（与 acquire_user_connection 配对）.
		// user 为空时不做任何处理. 线程安全.
		virtual void release_user_connection(const std::string& user) = 0;

		// 获取 DNS 查询结果缓存（UDP DNS 服务器与 HTTP DNS 路径共享）。
		// 未启用缓存时返回 nullptr. 默认空实现.
		virtual dns_response_cache* dns_query_cache() noexcept { return nullptr; }

		// 是否禁用 DNS 的 IPv6 解析返回（AAAA 查询返回空应答）. 默认关闭.
		virtual bool dns_no_ipv6() const noexcept { return false; }
	};


	//////////////////////////////////////////////////////////////////////////

	// proxy session 虚基类.
	class proxy_session_base {
	public:
		virtual ~proxy_session_base() {}
		virtual void start() = 0;
		virtual void close() = 0;
		virtual void setup_tproxy(const net::ip::tcp::endpoint&) = 0;
		virtual size_t connection_id() const = 0;
	};


	//////////////////////////////////////////////////////////////////////////

	// proxy_session 用于处理代理服务器的连接, 一个 proxy_session 对应一个
	// 客户端连接, 用于处理客户端的请求, 并将请求转发到目标服务器.
	class proxy_session
		: public proxy_session_base
		, public std::enable_shared_from_this<proxy_session>
	{
		proxy_session(const proxy_session&) = delete;
		proxy_session& operator=(const proxy_session&) = delete;

		struct http_context
		{
			// 在 http 请求时, 保存正则表达式命中时匹配的结果列表.
			std::vector<std::string> regex_results_;

			// 保存 http 客户端的请求信息.
			string_request& request_;

			// 保存 http 客户端请求的原始目标.
			// 指向 request 内部的字符串, 不拷贝, 生命周期与 request 一致.
			std::string_view target_;

			// 保存 http 客户端请求目标的具体路径, 即: doc 目录 + target_ 组成的路径.
			fs::path target_path_;
		};

		//////////////////////////////////////////////////////////////////////////
		// 日志与工具函数

		// http 认证错误代码对应的错误信息.
		static std::string pauth_error_message(int code) noexcept;

		// session 的连接日志输出函数, 用于输出连接 id 相关的日志信息, 简化重复
		// 的 connection id 这些信息的代码.
		inline auto log_conn_error() const noexcept
		{
			return std::move(XLOG_FERR("connection id: {}", m_connection_id));
		}

		inline auto log_conn_warning() const noexcept
		{
			return std::move(XLOG_FWARN("connection id: {}", m_connection_id));
		}

		inline auto log_conn_debug() const noexcept
		{
			return std::move(XLOG_FDBG("connection id: {}", m_connection_id));
		}

		// 更新 session 发起连接时使用的本地绑定地址.
		void update_bind_interface(const std::string& addr) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// PAM 认证

#ifdef USE_PAM_AUTH
		// PAM 认证回调函数, 用于与 PAM 模块交互.
		static int pam_conv_func(int num_msg, const struct pam_message **msg,
			struct pam_response **resp, void *appdata_ptr);

		// 同步执行 PAM 用户认证.
		bool pam_authenticate_user(const char *service, const char *username, const char *password) noexcept;

		// 异步执行 PAM 用户认证, 在后台线程中执行以避免阻塞.
		template <typename CompletionToken>
		auto async_pam_auth(const std::string& username, const std::string& passwd,
			const std::string& service_name, CompletionToken&& token) noexcept;
#endif // USE_PAM_AUTH

		//////////////////////////////////////////////////////////////////////////
		// 认证与授权

		// 检查是否需要认证.
		bool auth_required() const noexcept;

		// 检查用户密码等相关配置信息, 包括更新绑定的本地网络地址和限速等信息.
		net::awaitable<bool> check_userpasswd(
			const std::string& username,
			const std::string& passwd, bool skip_passwd = false) noexcept;

		//////////////////////////////////////////////////////////////////////////

	public:
		proxy_session(
			net::any_io_executor executor,
			net::io_context& backend_context,
			bool scheduler_locking,
			dns_cache& cache,
			variant_stream_type&& socket,
			size_t id,
			std::weak_ptr<proxy_server_base> server
		);

		~proxy_session();

	public:
		// 启动 session, 开始处理客户端连接.
		void start() noexcept override;

		// 关闭当前 session 连接.
		void close() noexcept override;

		// 设置透明代理 (TPROXY) 的目标端点.
		void setup_tproxy(const net::ip::tcp::endpoint& tproxy_remote) noexcept override;

		// 返回当前连接的唯一标识符.
		size_t connection_id() const noexcept override;

		//////////////////////////////////////////////////////////////////////////
		// 会话信息查询（供 launcher 状态上报）

		// 会话是否仍存活（未中止）。
		bool alive() const noexcept;

		// 本会话累计接收字节数（客户端 → 本服务端）。
		uint64_t total_rx() const noexcept;

		// 本会话累计发送字节数（本服务端 → 客户端，即客户端下载量）。
		uint64_t total_tx() const noexcept;

		// 本会话认证成功的用户名（匿名/未认证为空串）。
		std::string auth_user() const noexcept;

		// 返回当前会话的代理目标。
		std::string target() const noexcept;

		// 返回当前会话的客户端信息（host:port）。
		std::string client() const noexcept;

		// 返回当前会话的地区信息（GeoIP），若无法获取则返回空字符串。
		std::string geoip() const noexcept;

		// 返回当前会话的协议类型（socks4/socks5/http/https/tproxy/stdio），未识别为空串。
		std::string proto() const noexcept;

		// 返回当前会话连接已持续秒数（连接建立于启动时，之后只读）。
		int64_t elapsed() const noexcept;

		// 设置客户端地址（host:port）与地区信息，由 proxy_server 在连接建立时调用，
		// 仅在会话发布到 m_sessions 之前写入，之后只读（无锁）。
		void set_client_info(const std::string& client, const std::string& region) noexcept;

		// 记录当前会话的协议类型（运行期可变，内部加锁）。
		void set_proto(std::string proto) noexcept;

		// 记录当前会话的代理目标 host:port（运行期可变，内部加锁）。
		void set_target(std::string target) noexcept;

	private:

		// http 认证结果代码.
		enum {
			PROXY_AUTH_SUCCESS = 0,
			PROXY_AUTH_FAILED,
			PROXY_AUTH_NONE,
			PROXY_AUTH_ILLEGAL,
		};

		//////////////////////////////////////////////////////////////////////////
		// 专用代理模式

		// stdio 代理模式, 将 stdin/stdout 作为一条逻辑连接进行转发.
		net::awaitable<void> stdio_proxy() noexcept;

		// 透明代理模式, 用于处理透明代理转发的流量.
		net::awaitable<void> transparent_proxy() noexcept;

		//////////////////////////////////////////////////////////////////////////
		// 混淆与协议侦测

		// 与客户端进行噪声(noise)握手, 协商后续数据加密的密钥.
		template <typename T>
		net::awaitable<bool>
		noise_handshake(T& socket, std::array<uint8_t, 16>& inkey, std::array<uint8_t, 16>& outkey) noexcept;

		// 协议侦测协程.
		template <typename T>
		net::awaitable<void> proto_detect(bool handshake_before = true) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// 代理协议处理

		// 启动代理协议处理, 根据检测到的协议类型进入相应的处理流程.
		net::awaitable<void> start_proxy() noexcept;

		// SOCKS5 代理连接处理.
		net::awaitable<void> socks_connect_v5() noexcept;

		// 在 udp client 和 server 之间转发 UDP 数据包.
		net::awaitable<void> forward_udp(udp::socket& client, udp::socket& server) noexcept;

		// SOCKS4 代理连接处理.
		net::awaitable<void> socks_connect_v4() noexcept;

		// HTTP 代理认证处理, 解析 Proxy-Authorization 头并验证.
		net::awaitable<int> http_authorization(std::string_view pa) noexcept;

		// HTTP GET 代理请求处理.
		net::awaitable<bool> http_proxy_get() noexcept;

		// HTTP CONNECT 隧道代理请求处理.
		net::awaitable<bool> http_proxy_connect() noexcept;

		// HTTP CONNECT-UDP (RFC 9298) 代理请求处理.
		net::awaitable<void> http_proxy_connect_udp(
			const http::request<http::string_body>& req,
			const std::string& target_host,
			uint16_t target_port) noexcept;

		// HTTP CONNECT-UDP 通过代理转发(有 proxy_pass 的情况下)处理.
		net::awaitable<void> http_proxy_udp_proxy_pass(
			const http::request<http::string_body>& req,
			const std::string& target_host,
			uint16_t target_port
		);

		// 解析 HTTP CONNECT-UDP 请求的目标地址.
		bool parse_connect_udp_target(std::string_view path,
			std::string& target_host, uint16_t& target_port);

		// SOCKS5 用户名/密码认证处理.
		net::awaitable<bool> socks_auth() noexcept;

		//////////////////////////////////////////////////////////////////////////
		// 数据传输

		// 在两个流之间并发传输数据, 支持限速和超时控制.
		// count_rx 为 true 时该方向数据计入会话接收量（RX），
		// 否则计入发送量（TX），供 launcher 状态按用户/全局统计。
		template<typename S1, typename S2>
		net::awaitable<void>
		transfer(S1& from, S2& to, size_t& bytes_transferred, bool count_rx) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// 连接相关，对外发起连接和对上游代理服务器的连接

		// 连接到上游代理服务器.
		net::awaitable<boost::system::error_code>
		connect_proxy_pass(tcp::socket& remote_socket, tcp::resolver::results_type targets) noexcept;

		// 与上游代理服务器进行协议握手 (SOCKS/HTTP).
		net::awaitable<boost::system::error_code>
		proxy_pass_handshake(tcp::socket& remote_socket,
			const std::string& target_host, uint16_t target_port,
			const urls::url& proxy_url, int command = SOCKS_CMD_CONNECT);

		// 开始连接到目标主机, 支持域名解析和代理级联.
		net::awaitable<boost::system::error_code> start_connect_host(
			std::string target_host,
			uint16_t target_port,
			bool resolved = false,
			int command = SOCKS_CMD_CONNECT) noexcept;

		// is_crypto_stream 判断当前连接是否为加密连接.
		bool is_crypto_stream() const noexcept;

		//////////////////////////////////////////////////////////////////////////
		// HTTP 静态服务

		// 处理 HTTP 静态文件服务请求, 返回本地文档目录中的文件.
		net::awaitable<void>
		static_web_server(http::request<http::string_body>& req, std::optional<request_parser>& parser) noexcept;

		// 格式化目录下的文件列表, 返回每个文件/目录的名称.
		std::vector<std::wstring>
		format_path_list(const fs::path& path, boost::system::error_code& ec) const;

		// HTTP JSON 目录列表实现的模板函数, 支持迭代器遍历目录.
		template <typename DirIter>
		net::awaitable<void> on_http_json_impl(const http_context& hctx) noexcept;

		// 处理 HTTP JSON 格式的完整目录列表请求.
		net::awaitable<void> on_http_all_json(const http_context& hctx) noexcept;

		// 处理 HTTP JSON 格式的目录列表请求.
		net::awaitable<void> on_http_json(const http_context& hctx) noexcept;

		// 处理 HTTP HTML 格式的目录列表请求.
		net::awaitable<void> on_http_dir(const http_context& hctx) noexcept;

		// 处理 HTTP GET 文件请求, 返回静态文件内容.
		net::awaitable<void> on_http_get(const http_context& hctx) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// DNS 查询相关实现

	public:
		// 处理 HTTP DNS 查询请求 (支持 DNS-over-HTTPS 和 UDP).
		net::awaitable<void> on_http_dns_query(const http_context& hctx) noexcept;

		// udp_dns_query 通过传统 UDP 协议转发 DNS 查询到上游服务器.
		// output 非空时，把收到的响应报文写入其中（供缓存写回）.
		net::awaitable<void> udp_dns_query(
			const string_request& request, const std::string& dns_query,
			std::string* output = nullptr) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// DNS 查询处理

		// log_dns_result 从 DNS wire-format 查询和响应中提取关键信息并输出日志.
		// query_msg: 原始 wire-format 查询 (用于提取问题域名).
		// response_msg: wire-format 响应 (用于提取答案记录).
		// json_body: 可选的 JSON 响应体 (非空时优先从中提取答案).
		void log_dns_result(
			const std::string& query_msg,
			const std::string& response_msg,
			const std::string& json_body = {}) const noexcept;

		// dns_json_query 处理 application/dns-json 格式的 DNS 查询.
		net::awaitable<void> dns_json_query(
			const string_request& request,
			const std::string& question_name,
			uint16_t question_type,
			bool cd_flag,
			bool do_flag) noexcept;

		// udp_query_raw 通过 UDP 上游发送 wire-format DNS 查询并返回响应.
		// 结果通过 output 参数返回, 返回 true 表示成功.
		net::awaitable<bool> udp_query_raw(
			const std::string& dns_query, std::string& output) noexcept;

		// doh_query_raw 通过 DoH 上游发送 wire-format DNS 查询并返回响应.
		// 结果通过 output 参数返回, 返回 true 表示成功.
		net::awaitable<bool> doh_query_raw(
			const std::string& dns_query, std::string& output) noexcept;

		// doh_dns_query 通过 DoH (DNS over HTTPS) 转发 DNS 查询到上游服务器.
		// output 非空时，把收到的响应报文写入其中（供缓存写回）.
		net::awaitable<void> doh_dns_query(
			const string_request& request, const std::string& dns_query,
			std::string* output = nullptr) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// HTTP 响应辅助

	private:

		// 生成 HTTP 响应中的 Date 头字符串.
		static std::string server_date_string() noexcept;

		// 返回默认 HTTP 响应 (指定状态码和消息体).
		net::awaitable<void> default_http_route(
			const string_request& request, std::string response, http::status status) noexcept;

		// 返回 HTTP 重定向响应 (Location).
		net::awaitable<void> location_http_route(
			const string_request& request, const std::string& path) noexcept;

		// 返回 HTTP 403 Forbidden 响应.
		net::awaitable<void> forbidden_http_route(const string_request& request) noexcept;

		// 返回 HTTP 405 Method Not Allowed 响应 (含 Allow 头).
		net::awaitable<void> method_not_allowed_http_route(const string_request& request) noexcept;

		// 返回 HTTP 401 Unauthorized 响应.
		net::awaitable<void> unauthorized_http_route(const string_request& request) noexcept;

		// 发送 HTTP 字符串响应, 自动设置常用头部 (server, date, content-type, keep-alive).
		net::awaitable<void> send_http_string_response(
			const string_request& request,
			http::status status,
			std::string content_type,
			std::string body,
			bool keep_alive = true) noexcept;

		// 根据 If-None-Match / If-Modified-Since 判断是否应返回 304 Not Modified.
		static bool should_send_not_modified(
			const string_request& request,
			const std::string& etag,
			std::time_t mtime,
			bool mtime_valid) noexcept;

		// 根据 If-Range 判断 range 请求是否仍然有效(无效则忽略 Range 返回完整内容).
		static bool if_range_matches(
			std::string_view value,
			const std::string& etag,
			std::time_t mtime,
			bool mtime_valid) noexcept;

		// 发送 304 Not Modified 响应(带 ETag 与 Last-Modified).
		net::awaitable<void> send_not_modified_http_route(
			const string_request& request,
			const std::string& etag,
			const std::string& last_modified) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// 流控制工具

		// 配置指定用户的速率限制.
		void user_rate_limit_config(const std::string& user) noexcept;

		// 登记用户连接计数（用于用户连接数限制）；同一连接内重复认证同一
		// 用户不重复计数，返回 false 表示已达连接数限制.
		bool user_conn_acquire(const std::string& user) noexcept;

		// 检查当前认证用户是否已超出流量配额（上行+下行总和，含历史已用流量与其他会话流量）.
		bool quota_exceeded() const noexcept;

		// 取消流超时设置, 使其永不超时.
		static void stream_expires_never(variant_stream_type& stream) noexcept;

		// 设置流超时时间.
		static void stream_expires_after(
			variant_stream_type& stream, net::steady_timer::duration expiry_time) noexcept;

		// 关闭流, 用于超时 watchdog 中止挂起的 I/O 操作.
		static void stream_cancel(variant_stream_type& stream) noexcept;

		// 设置流速率限制.
		static void stream_rate_limit(variant_stream_type& stream, int rate) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// DNS 解析与缓存

		// 从 DNS 缓存中获取主机名解析结果.
		tcp::resolver::results_type
		get_resolver_from_cache(const std::string& hostname, uint16_t port) noexcept;

		// 异步解析目标主机地址.
		net::awaitable<tcp::resolver::results_type>
		resolve_targets(const std::string& hostname, uint16_t port_number) noexcept;

		// 异步解析上游代理服务器地址.
		net::awaitable<tcp::resolver::results_type>
		resolve_proxy_pass_targets() noexcept;

		// 切换到后端执行上下文（非锁定调度时）.
		net::awaitable<net::any_io_executor> switch_to_backend_executor();

		// 从后端执行上下文切换回主执行上下文.
		net::awaitable<void> switch_from_backend_executor();

		//////////////////////////////////////////////////////////////////////////

		// 异步连接到多个目标地址 (支持 Happy Eyeballs).
		net::awaitable<boost::system::error_code>
		async_connect_targets(tcp::socket& socket, tcp::resolver::results_type& targets) noexcept;

		// 实例化上游代理连接, 可选择是否使用 SSL 加密.
		net::awaitable<variant_stream_type>
		instantiate_proxy_pass(tcp::socket remote_socket, bool proxy_pass_use_ssl) noexcept;

		// 并发转发本地和远程之间的双向数据.
		net::awaitable<void> concurrent_transfer();

		// 空闲超时检测协程, 用于并发传输中检测整体连接的空闲状态.
		// 参数 s1/s2 为两个传输方向的流, 当 idle_timeout 检测到超时时会关闭它们.
		// 当任何一个方向有数据传输时, 超时计时器会被重置.
		net::awaitable<void> idle_timeout(
			variant_stream_type& s1, variant_stream_type& s2) noexcept;

	private:
		// m_executor 保存当前 io_context 的 executor.
		net::any_io_executor m_executor;

		// backend resolver 用于解析域名.
		net::io_context& m_backend_context;

		// 记录 asio 调度器是否启用锁标识.
		bool m_scheduler_locking;

		// 作为中继桥接的时候, 下游代理服务器解析的地址缓存.
		dns_cache& m_dns_cache;

		// m_local_socket 本地 socket, 即客户端连接的 socket.
		variant_stream_type m_local_socket;

		// m_remote_socket 远程 socket, 即连接远程代理服务端或远程服务的 socket.
		variant_stream_type m_remote_socket;

		// 用于优雅关闭时, 取消所有超时的异步传输操作.
		std::atomic<int> m_transfer_timeout{ 0 };

		// m_bind_interface 用于向外发起连接时, 指定的 bind 地址.
		std::optional<net::ip::address> m_bind_interface;

		// m_local_udp_endpoint 用于保存 udp 通信时, 本地的地址.
		udp::endpoint m_local_udp_endpoint;
		// m_remote_udp_endpoint 用于保存 udp 通信时, 远程的地址, 用于作为 UDP 代理桥接时使用.
		udp::endpoint m_remote_udp_endpoint;

		// m_connection_id 当前连接的 id, 用于日志输出.
		size_t m_connection_id;

		// m_tproxy_remote tproxy 模式下, 客户端期望请求远程地址.
		std::optional<net::ip::tcp::endpoint> m_tproxy_remote;

		// m_local_buffer 本地缓冲区, 用于接收客户端的数据的 buffer.
		net::streambuf m_local_buffer;

		// m_server_rx_key 用于身份为服务端时, 解密接收到的混淆数据的 key.
		std::array<uint8_t, 16> m_server_rx_key{};
		// m_server_tx_key 用于身份为服务端时, 加密发送的混淆数据的 key.
		std::array<uint8_t, 16> m_server_tx_key{};

		// m_client_rx_key 用于身份为客户端时, 与下游代理服务器加密通信时, 解密接收到
		// 下游代理服务器混淆数据的 key.
		std::array<uint8_t, 16> m_client_rx_key{};
		// m_client_tx_key 用于身份为客户端时, 与下游代理服务器加密通信时, 加密给下
		// 游代理服务器发送的混淆数据的 key.
		std::array<uint8_t, 16> m_client_tx_key{};

		// m_proxy_server 当前代理服务器对象的弱引用.
		std::weak_ptr<proxy_server_base> m_proxy_server;

		// m_option 当前代理服务器的配置选项.
		proxy_server_option m_option;

		// m_proxy_pass 作为中继桥接的时候, 下游代理服务器的地址.
		std::optional<urls::url> m_proxy_pass;

		// m_last_activity 用于双向超时检测, 在代理传输过程中, 只要有任何一个
		// 方向上有数据传输，都不应触发超时。
		std::atomic<int64_t> m_last_activity{ 0 };

		// m_total_rx / m_total_tx 本会话累计收发字节数（供 launcher 状态统计）。
		std::atomic<uint64_t> m_total_rx{ 0 };
		std::atomic<uint64_t> m_total_tx{ 0 };

		// m_auth_user 认证成功的用户名（匿名/未认证为空串）。
		std::string m_auth_user;

		// m_conn_user 已登记用户连接计数的用户名（空串表示未登记）。
		// HTTP keep-alive 同一连接内多次认证时避免重复计数.
		std::string m_conn_user;

		// m_conn_mutex 保护 m_target/m_proto（运行期可变字段），
		// 避免与 launcher 状态上报并发读取产生数据竞争.
		mutable std::mutex m_conn_mutex;

		// m_target 当前会话的代理目标（host:port），由各协议处理路径设置.
		std::string m_target;

		// m_proto 协议类型（socks4/socks5/http/https/tproxy/stdio），
		// 由协议分发路径设置.
		std::string m_proto;

		// m_client_addr 客户端地址（host:port），连接建立时由 proxy_server
		// 设置，之后只读（发布到 m_sessions 前写入）.
		std::string m_client_addr;

		// m_region 客户端地区信息（GeoIP 查询结果），连接建立时由
		// proxy_server 设置，之后只读（同上）.
		std::string m_region;

		// m_started_at 连接建立时间（unix 秒），之后只读.
		int64_t m_started_at{ 0 };

		// 当前 session 是否被中止的状态.
		std::atomic_bool m_abort{ false };
	};
}

#endif // INCLUDE__2023_10_18__PROXY_SESSION_HPP
