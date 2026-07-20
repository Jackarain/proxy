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
#include "proxy/proxy_util.hpp"


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

	// tcp_session_expired_time 用于指定 tcp session 的默认过期时间, 单位为秒.
	inline const int tcp_session_expired_time = 60;

	// udp_session_expire_time 用于指定 udp session 的过期时间, 单位为秒.
	inline const int udp_session_expired_time = 60;

	// noise_injection_max_len 用于指定噪声注入的最大长度, 单位为字节.
	inline const uint16_t noise_injection_max_len = 0x0fff;

	// global_known_proto 用于指定全局已知的协议, 用于噪声注入时避免生成已知的协议头.
	inline const std::set<uint8_t> global_known_proto =
		{
			0x04, // socks4
			0x05, // socks5
			0x47, // 'G'
			0x50, // 'P'
			0x43, // 'C'
			0x16, // ssl
		};

	//////////////////////////////////////////////////////////////////////////

	inline const char* version_string = R"x*x*x(nginx/1.20.2)x*x*x";

	//////////////////////////////////////////////////////////////////////////

	// proxy server 参数选项, 用于指定 proxy server 的各种参数.
	struct proxy_server_option
	{
		// 代理服务端 TCP 监听端口列表。
		//
		// - 支持同时监听多个 endpoint（多地址/多端口）。
		// - tuple 的第 1 个元素为 tcp::endpoint；第 2 个元素仅在 endpoint 为 IPv6 地址时生效：
		//   * true  : 表示设置 IPV6_V6ONLY，仅接受 IPv6 连接；
		//   * false : 表示允许 IPv4-mapped/双栈行为（具体由系统与 socket 选项决定）。
		std::vector<std::tuple<tcp::endpoint, bool>> listens_;

		// 代理服务端 Unix Domain Socket（UDS）监听端点列表。
		//
		// - 支持同时监听多个 UDS 路径。
		// - 适用于同机进程间通信、在容/本机环境中提供更安全/更低开销的入口。
		std::vector<net::local::stream_protocol::endpoint> uds_listens_;

		// 通过 proxy_pass 转发/桥接当前进程的 stdio 到指定目标服务器。
		//
		// - 典型用途：将 stdin/stdout 作为一条逻辑“连接”转发到远端（例如作为守护进程的 stdio 入口）。
		// - 为空表示不启用该功能。
		std::string stdio_target_;

		// 监控 URL 用于指定 proxy server 的监控接口 URL。
		//
		// - 为空表示不启用监控功能。
		// - 否则，将使用指定的 URL 进行监控。
		std::string monitor_url_;

		// PAM 认证服务配置名称。
		//
		// - 为空表示不启用 PAM 认证。
		// - 否则，将使用指定的 PAM 服务配置进行认证。
		std::string pam_auth_;

		// 授权（认证）用户列表。
		//
		// auth_users 的含义：
		// - 第 1 个元素：用户名
		// - 第 2 个元素：密码
		// - 第 3 个元素：该用户绑定的网络出口 IP / 本地接口（用于对外发起连接时的绑定，或作为策略匹配字段）
		// - 第 4 个元素：该用户专属的 proxy_pass（可选），用于让不同用户走不同上游/链路
		//
		// 约定：
		// - auth_users_ 为空：表示不需要认证（匿名访问）。
		// - 支持多个用户，例如：
		//   { {"user1", "passwd1", "", std::nullopt}, {"user2", "passwd2", "1.2.3.4", url} }。
		using auth_users = std::tuple<std::string, std::string, std::string, std::optional<urls::url>>;
		std::vector<auth_users> auth_users_;

		// 用户限速配置（按用户名粒度）。
		//
		// - key   ：用户名
		// - value ：速率（单位建议与实现保持一致；若实现为 bytes/s，则此处即 bytes/second）
		// - 未配置某用户时：表示不对该用户单独限速（由全局 tcp_rate_limit_ 或默认策略决定）。
		std::unordered_map<std::string, int> users_rate_limit_;

		// 允许访问的地区集合（白名单）。
		//
		// - 例如：{ "中国", "香港", "台湾" }
		// - 为空：表示不启用地区限制（允许所有地区）。
		// - 生效前提：必须正确配置 ipip_db_，并且地区解析能够命中。
		std::unordered_set<std::string> allow_regions_;

		// 拒绝访问的地区集合（黑名单）。
		//
		// - 例如：{ "美国", "日本" }
		// - 为空：表示不启用地区限制（不拒绝任何地区）。
		// - 若其中地址与 allow_regions_ 同时设置时，由 deny_regions_ 决定。
		// - 生效前提：必须正确配置 ipip_db_。
		std::unordered_set<std::string> deny_regions_;

		// IPIP 数据库文件路径（用于根据客户端 IP 解析地区信息，进而做 allow/deny 判断）。
		//
		// - 数据库可从 https://www.ipip.net 下载或该项目 Release 页面下载。
		// - 为空：表示不启用地区解析与地区限制功能（allow/deny 不生效）。
		std::string ipip_db_;

		// 上游代理（多层代理 / 级联）配置：当前代理把流量转发给下一个代理或直接目标。
		//
		// - 对客户端无感：客户端只连接当前服务端，当前服务端再决定如何转发到下一跳。
		// - 典型示例：
		//   socks5://user:passwd@proxy.server.com:1080
		//   https://user:passwd@proxy.server.com:1080
		//
		// DNS 行为说明：
		// - 当 proxy_pass_ 为 socks5 时，使用 hostname 模式（即域名解析在远端/上游执行）。
		std::optional<urls::url> proxy_pass_;

		// 在多层代理模式下，与“下一个代理服务器”之间是否使用 TLS/SSL 加密。
		//
		// - 通常用于 proxy_pass_ 指向 socks 代理时使用该选项（用于决定 socks 连接的外层是否包 TLS）。
		// - 若 proxy_pass_ 为 HTTP/HTTPS proxy，是否加密同时也由 URL scheme 决定（http vs https）。
		bool proxy_pass_use_ssl_{ false };

		// 指定本代理在向外（上游/目标）发起连接时绑定的本地源地址。
		//
		// - 适用于多网卡/多出口场景，通过绑定不同 local_ip_ 控制出站链路。
		// - 为空：不显式绑定，由操作系统路由表/策略自动选择源地址。
		std::string local_ip_;

		// 是否启用 TCP 端口重用（SO_REUSEPORT）。
		//
		// - 仅 Linux kernel 3.9+ 支持。
		// - 典型用途：多进程/多线程同时 bind 同一端口以提升 accept 性能或做负载均衡。
		bool reuse_port_{ false };

		// 是否启用 Happy Eyeballs 连接算法（IPv6/IPv4 并行探测以降低连接延迟）。
		//
		// - true ：启用（默认），通常可改善双栈环境下的首包延迟与可用性。
		// - false：禁用，按系统/解析顺序串行尝试。
		bool happyeyeballs_{ true };

		// 是否仅使用 IPv4 发起出站连接。
		//
		// - true  ：只使用 IPv4
		// - false ：IPv4/IPv6 都可能使用（受解析结果、happyeyeballs_ 等影响）
		bool connect_v4_only_{ false };

		// 是否仅使用 IPv6 发起出站连接。
		//
		// - true  ：只使用 IPv6
		// - false ：IPv4/IPv6 都可能使用（受解析结果、happyeyeballs_ 等影响）
		bool connect_v6_only_{ false };

		// 是否作为透明代理（Transparent Proxy）。
		//
		// - 仅 Linux 场景可用（通常依赖 TPROXY / iptables / nft 等路由策略配套配置）。
		// - 启用后通常意味着需要特殊的 socket 选项与策略路由，才能“伪装”源地址或接管流量。
		bool transparent_{ false };

		// 向外连接发起时使用的 SO_MARK（Linux）标记。
		//
		// - 设置后，所有由本代理发起的 TCP/UDP 出站连接都会带上此标记，
		//   可用于策略路由/防火墙匹配。
		// - 未设置表示不打 mark。
		std::optional<uint32_t> so_mark_;

		// TCP 会话/连接超时时间（秒）。
		//
		// - 用于控制连接建立/读写等阶段的超时。
		// - 默认值为 tcp_session_expired_time, 小于等于0表示禁用超时检查。
		int tcp_timeout_{ tcp_session_expired_time };

		// UDP 会话/流过期时间（秒）。
		//
		// - 用于控制 UDP 会话的生命周期，尤其在透明代理模式下用于清理过期的流。
		// - 默认值为 60 秒（可根据实际需求调整）。
		int udp_timeout_{ udp_session_expired_time };

		// TCP 连接速率限制（全局），单位：bytes/second。
		//
		// - -1 表示不限制。
		// - 若 users_rate_limit_ 中对某用户单独设置，通常应以“用户限速优先或覆盖全局”为准（由实现定义）。
		int tcp_rate_limit_{ -1 };

		// 作为服务端时：SSL 证书目录。
		//
		// - 会自动搜索子目录；每个子目录保存一个域名对应的一组证书文件。
		// - 若证书为加密私钥，则需要 password.txt 存储解密密码（按实现约定读取）。
		std::string ssl_cert_path_;

		// 作为客户端时：CA 证书目录或 CA bundle 路径（用于校验上游 TLS 证书）。
		//
		// - 若不指定：默认使用 curl 官方提供的 CA bundle（见 https://curl.se/docs/caextract.html）。
		// - 具体支持目录还是文件取决于 TLS 实现（OpenSSL 等）。
		std::string ssl_cacert_path_;

		// 指定上游代理/服务器的 SNI（Server Name Indication）名字。
		//
		// - 当上游使用“多域名证书”且需要指定匹配域名时，通过该字段明确 SNI。
		// - 为空：通常使用 proxy_pass_ 中的 host 作为默认 SNI（若实现如此）。
		std::string proxy_ssl_name_;

		// 指定允许使用的 TLS 加密套件（cipher suites）。
		//
		// - 格式取决于 TLS 库（例如 OpenSSL 的 cipher list 格式）。
		// - 为空：使用库默认配置。
		std::string ssl_ciphers_;

		// 是否优先使用服务端指定的加密套件顺序（server cipher preference）。
		//
		// - true ：优先服务端顺序
		// - false：可能由客户端偏好决定
		bool ssl_prefer_server_ciphers_;

		// HTTP 文档根目录：用于将代理伪装为 Web 站点（静态文件服务）。
		//
		// - 为空：不启用静态站点功能；遇到 HTTP/HTTPS 文件请求时返回错误信息。
		// - 非空：对该目录提供文件访问能力（需注意安全，可启用 htpasswd_ 确保访问需要认证）。
		std::string doc_directory_;

		// DNS 上游服务器地址，用于 DoH (DNS over HTTPS) 查询转发。
		//
		// - 格式: "ip:port"，如 "8.8.8.8:53"。
		std::optional<std::string> dns_upstream_;

		// 是否启用目录列表（类似 nginx 的 autoindex）。
		//
		// - 仅在 doc_directory_ 非空、启用了静态站点功能时生效。
		// - true ：访问目录时返回目录下文件列表。
		// - false：访问目录时通常返回错误或默认首页。
		bool autoindex_;

		// 是否启用 HTTP Basic Auth（类似 htpasswd 的认证方式）。
		//
		// - 默认 false：不启用。
		// - 若启用：通常需要配置 auth_users_ 用于用户信息认证。
		bool htpasswd_{ false };

		// 是否禁用 HTTP 相关服务入口。
		//
		// - true ：客户端无法使用 HTTP 与服务端通信，包含 HTTP 代理功能。
		bool disable_http_{ false };

		// 是否禁用 SOCKS 代理服务入口。
		//
		// - true ：不提供 SOCKS4/5 代理服务。
		// - false：提供 SOCKS 服务。
		bool disable_socks_{ false };

		// 是否禁止不安全（明文）连接。
		//
		// - true ：禁止所有非加密连接。
		// - false：允许明文与加密并存，由客户端与服务端协商确定是否加密。
		bool disable_insecure_{ false };


		// 是否禁用 TLS 证书校验（不安全，通常用于测试或特定环境）。
		//
		// - true ：不校验 TLS 证书，允许连接到证书无效/自签的服务器。
		// - false：启用 TLS 证书校验，确保连接安全。
		bool disable_check_cert_{ false };

		// 是否禁用 UDP 代理服务。
		//
		// - true ：不提供 UDP 转发/代理能力（如 UDP ASSOCIATE 等）。
		// - false：提供 UDP 相关服务。
		bool disable_udp_{ false };

		// 是否启用“噪声注入 / 混淆”（scramble）以干扰流量分析。
		//
		// 生效条件：
		// - 必须 server/client 两端同时启用才有效。
		//
		// 工作机制：
		// - TLS 握手完成后，双方互相发送一段随机长度的随机数据作为“噪声”；
		// - 双方收到对方噪声后对整段噪声做 hash，得到后续数据的加密密钥；
		// - 后续数据使用简单异或加密；密钥用完后通过 hash(hash) 派生新密钥；
		// - hash 算法采用 xxhash；密钥长度固定为 16 字节以兼顾性能。
		//
		// 取舍：
		// - 可降低被动流量分析的有效性，但会增加额外流量与时延；
		// - 默认不启用，建议仅在明确存在分析/干扰风险时开启。
		bool scramble_{ false };

		// 噪声注入的最大长度（单位：字节）。
		//
		// - 允许范围：[16, 64K]。
		int64_t noise_length_{ -1 };
	};


	//////////////////////////////////////////////////////////////////////////

	// proxy server 虚基类, 任何 proxy server 的实现, 必须基于这个基类.
	// 这样 proxy_session 才能通过虚基类指针访问 proxy server 的具体实
	// 现以及虚函数方法.
	class proxy_server_base {
	public:
		virtual ~proxy_server_base() {}
		virtual void remove_session(size_t id) = 0;
		virtual size_t num_session() = 0;
		virtual const proxy_server_option& option() = 0;
		virtual net::ssl::context& ssl_context() = 0;
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
			std::string target_;

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
			const std::string& service_name, CompletionToken&& token) noexcept
		{
			return net::async_initiate<CompletionToken,
				void(boost::system::error_code, bool)>
				([this, username, passwd, service_name](auto&& handler) mutable
				{
					// 这里不需要保持对 proxy_session 的 shared_ptr 引用, 因为 PAM 认证完成后无论结果如
					// 何都不需要访问 proxy_session 的成员变量或方法了, 另外 async_pam_auth 总是运行在
					// 协程中，协程在 proxy_session 启动时就已经保存了 shared_ptr 自身.
					std::thread([this, username = std::move(username), passwd = std::move(passwd),
						service_name = std::move(service_name), handler = std::move(handler)]() mutable
						{
							bool result = pam_authenticate_user(
								service_name.c_str(),
								username.c_str(),
								passwd.c_str());

							auto executor = net::get_associated_executor(handler);
							net::post(executor,
								[result, handler = std::move(handler)]() mutable
								{
									boost::system::error_code ec;
									handler(ec, result);
								});
						}
					).detach();
				}, token);
		}
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

	private:

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
		inline net::awaitable<bool>
		noise_handshake(T& socket, std::array<uint8_t, 16>& inkey, std::array<uint8_t, 16>& outkey) noexcept
		{
			boost::system::error_code error;

			// 工作流程:
			// 1. 生成一段随机长度的随机数据(最大长度由配置文件中的参数 noise_length 指定), 用于发送给对方.
			// 2. 根据这些随机数据计算发送数据的 key, 这个 key 将会用于后续的代理时数据的加密.
			// 3. 发送随机数据.
			// 4. 对方在接收到随机数据后, 同样会发送噪声随机数据(包含随机数长度本身, 在前16字节中的最后一位,
			//    组成一个 16 位的整数表示长度).
			// 5. 计算接收随机数据的 key 用于后续的接收到的数据的解密.

			// 生成要发送的噪声数据.
			int noise_length = static_cast<int>(m_option.noise_length_);

			if (noise_length < 16 ||
				(noise_length > std::numeric_limits<uint16_t>::max() / 2))
				noise_length = noise_injection_max_len;

			std::vector<uint8_t> noise =
				generate_noise(global_random_device(), static_cast<uint16_t>(noise_length), global_known_proto);

			// 计算数据发送 key.
			outkey = compute_key(noise);

			log_conn_debug()
				<< ", send noise, length: "
				<< noise.size();

			// 发送 noise 消息.
			co_await net::async_write(
				socket,
				net::buffer(noise),
				net_awaitable[error]);
			if (error)
			{
				log_conn_warning()
					<< ", noise write error: "
					<< error.message();

				co_return false;
			}

			noise.resize(16);

			// 接收对方发过来的 noise 回应消息.
			co_await net::async_read(
				socket,
				net::buffer(noise, 16),
				net_awaitable[error]);

			if (error)
			{
				log_conn_warning()
					<< ", noise read header error: "
					<< error.message();

				co_return false;
			}

			noise_length = extract_noise_length(noise);

			// 计算要接收的剩余数据大小.
			int remainder = static_cast<int>(noise_length) - 16;
			if (remainder < 0 || remainder >= std::numeric_limits<uint16_t>::max())
			{
				log_conn_debug()
					<< ", noise length: "
					<< noise_length
					<< ", is invalid, noise size: "
					<< noise.size();

				co_return false;
			}

			noise.resize(noise_length);
			co_await net::async_read(
					socket,
					net::buffer(noise.data() + 16, remainder),
					net_awaitable[error]);

			if (error)
			{
				log_conn_warning()
					<< ", noise read body error: "
					<< error.message();

				co_return false;
			}

			log_conn_debug()
				<< ", recv noise, length: "
				<< noise.size();

			// 计算接收数据key.
			inkey = compute_key(noise);

			co_return true;
		}

		// 协议侦测协程.
		template <typename T>
		inline net::awaitable<void> proto_detect(bool handshake_before = true) noexcept
		{
			// 如果 server 对象已经撤销, 说明服务已经关闭则直接退出这个 session 连接不再
			// 进行任何处理.
			auto server = m_proxy_server.lock();
			if (!server)
				co_return;

			auto self = shared_from_this();

			// 从 m_local_socket 中获取 tcp::socket 对象的引用.
			auto& socket = boost::variant2::get<T>(m_local_socket);

			boost::system::error_code error;

			// 等待 read 事件以确保下面 recv 偷看数据时能有数据.
			co_await async_wait(m_local_socket, net_awaitable[error]);
			if (error)
			{
				log_conn_warning() << ", socket.async_wait error: " << error.message();
				co_return;
			}

			auto scramble_setup = [this](auto& sock) mutable
			{
				if (!m_option.scramble_)
					return;

				using Stream = std::decay_t<decltype(sock)>;

				if constexpr (std::same_as<Stream, tcp::socket> ||
					std::same_as<Stream, net::local::stream_protocol::socket>)
					return;

				if constexpr (std::same_as<Stream, proxy_tcp_socket> ||
					std::same_as<Stream, proxy_uds_socket>)
				{
					sock.set_scramble_key(m_server_tx_key);
					sock.set_unscramble_key(m_server_rx_key);
				}
			};

			// handshake_before 在调用 proto_detect 时第1次为 true, 第2次调用 proto_detect
			// 时 handshake_before 为 false, 此时就表示已经完成了 scramble 握手并协
			// 商好了 scramble 加解密用的 key, 则此时应该为 socket 配置好加解密用的 key.

			if (!handshake_before)
			{
				// 为 socket 设置 scramble key.
				scramble_setup(socket);
			}

			// 检查协议.
			auto fd = socket.native_handle();
			uint8_t detect[5] = { 0 };

#if defined(WIN32) || defined(__APPLE__)
			auto ret = recv(fd, (char*)detect, sizeof(detect), MSG_PEEK);
#else
			auto ret = recv(fd, (void*)detect, sizeof(detect), MSG_PEEK | MSG_NOSIGNAL | MSG_DONTWAIT);
#endif
			if (ret <= 0)
			{
				log_conn_warning() << ", peek message return: " << ret;
				co_return;
			}

			// detect 中的数据只有下面几种情况, 它是 http/socks4/5/ssl 协议固定的头
			// 几个字节, 如若不是, 在启用 scramble 的情况下, 则是 scramble 协议头
			// 此时应该进入 scramble 协商密钥, 协商密钥之后则重新进入 proto_detect
			// 以检测在 scramble 加密后的真实协议头.
			// 如果没启用 scramble, 接受到 http/socks4/5/ssl 协议固定的头之外的数据
			// 则视为未知协议退出.

			// scramble_peek 用于解密 peek 数据.
			auto scramble_peek = [this](auto& sock, std::span<uint8_t> detect) mutable
			{
				if (!m_option.scramble_)
					return;

				using Stream = std::decay_t<decltype(sock)>;

				if constexpr (std::same_as<Stream, tcp::socket> ||
					std::same_as<Stream, net::local::stream_protocol::socket>)
					return;

				if constexpr (std::same_as<Stream, proxy_tcp_socket> ||
					std::same_as<Stream, proxy_uds_socket>)
				{
					auto& unscramble = sock.unscramble();
					unscramble.peek_data(detect);
				}
			};

			if (!handshake_before)
			{
				// peek 方式解密混淆的数据, 用于检测加密混淆的数据的代理协议. 在双方启用
				// scramble 的情况下, 上面 recv 接收到的数据则会为 scramble 加密后的
				// 数据, 要像未启用 scramble 时那样探测协议, 就必须将上面 recv 中
				// peek 得到的数据：detect 临时解密(因为 proxy_stream 的加密为流式加
				// 密, 非临时解密则会对整个数据流产生错误解密), 从而得到具体的协议字节
				// 用于后面探测逻辑.
				scramble_peek(socket, detect);
			}

			// 保存第一个字节用于协议类型甄别.
			const uint8_t proto_byte = detect[0];

			// 非安全连接检查.
			if (m_option.disable_insecure_)
			{
				bool noise_proto = false;

				// 如果启用了 scramble, 则也认为是安全连接.
				if ((proto_byte != 0x05 &&
					proto_byte != 0x04 &&
					proto_byte != 0x47 &&
					proto_byte != 0x50 &&
					proto_byte != 0x43) ||
					!handshake_before)
				{
					noise_proto = true;
				}

				if (detect[0] != 0x16 && !noise_proto)
				{
					log_conn_debug() << ", insecure protocol disabled";
					co_return;
				}
			}

			// plain socks4/5 protocol.
			if (detect[0] == 0x05 || detect[0] == 0x04)
			{
				if (m_option.disable_socks_)
				{
					log_conn_debug() << ", socks protocol disabled";
					co_return;
				}

				log_conn_debug() << ", plain socks4/5 protocol";

				// 开始启动代理协议.
				co_await start_proxy();
			}
			else if (detect[0] == 0x16) // http/socks proxy with ssl crypto protocol.
			{
				log_conn_debug() << ", ssl protocol";

				auto& srv_ssl_context = server->ssl_context();

				// instantiate socks stream with ssl context.
				auto ssl_socks_stream = init_proxy_stream(std::move(socket), srv_ssl_context);

				// get origin ssl stream type.
				ssl_tcp_stream& ssl_socket = boost::variant2::get<ssl_tcp_stream>(ssl_socks_stream);

				// do async ssl handshake.
				co_await ssl_socket.async_handshake(net::ssl::stream_base::server, net_awaitable[error]);
				if (error)
				{
					log_conn_debug() << ", ssl server protocol handshake error: " << error.message();
					co_return;
				}

				// 使用 ssl_socks_stream 替换 m_local_socket.
				m_local_socket = std::move(ssl_socks_stream);

				// 开始启动代理协议.
				co_await start_proxy();
			}								// plain http protocol.
			else if (detect[0] == 0x47 ||	// 'G'
				detect[0] == 0x50 ||		// 'P'
				detect[0] == 0x43)			// 'C'
			{
				if (m_option.disable_http_)
				{
					log_conn_debug() << ", http protocol disabled";
					co_return;
				}

				log_conn_debug() << ", plain http protocol";

				// 开始启动代理协议.
				co_await start_proxy();
			}
			else if (handshake_before && m_option.scramble_)
			{
				// 进入噪声握手协议, 即: 返回一段噪声给客户端, 并等待客户端返回噪声.
				log_conn_debug() << ", noise protocol";

				if constexpr (std::same_as<T, proxy_tcp_socket>)
				{
					if (!co_await noise_handshake<tcp::socket>(
						net_tcp_socket(socket), m_server_rx_key, m_server_tx_key))
						co_return;
				}
				else if constexpr (std::same_as<T, proxy_uds_socket>)
				{
					if (!co_await noise_handshake<net::local::stream_protocol::socket>(
						net_uds_socket(socket), m_server_rx_key, m_server_tx_key))
						co_return;
				}

				// 在完成 noise 握手后, 重新检测被混淆之前的代理协议.
				co_await proto_detect<T>(false);
			}
			else
			{
				log_conn_debug() << ", unknown protocol";
			}

			co_return;
		}

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
		template<typename S1, typename S2>
		net::awaitable<void>
		transfer(S1& from, S2& to, size_t& bytes_transferred) noexcept
		{
			bytes_transferred = 0;

			// 记录 from 和 to 的 endpoint 信息.
			auto from_endpoint = remote_endpoint_string(from);
			auto to_endpoint = remote_endpoint_string(to);

			constexpr auto buf_size = 512 * 1024;

			std::unique_ptr<char, decltype(&std::free)> buf0((char*)std::malloc(buf_size), &std::free);
			std::unique_ptr<char, decltype(&std::free)> buf1((char*)std::malloc(buf_size), &std::free);

			// 分别使用主从缓冲指针用于并发读写.
			auto primary_buf = buf0.get();
			auto secondary_buf = buf1.get();

			boost::system::error_code from_ec;
			boost::system::error_code to_ec;

			// 设置传输限速.
			stream_rate_limit(from, m_option.tcp_rate_limit_);
			stream_rate_limit(to, m_option.tcp_rate_limit_);

			// 首先读取第一个数据作为预备, 以用于后面的交替读写逻辑.
			auto bytes = co_await from.async_read_some(net::buffer(primary_buf, buf_size), net_awaitable[from_ec]);
			if (from_ec || m_abort)
			{
				if (from_ec != net::error::eof)
				{
					log_conn_warning()
						<< ", read from endpoint: " << from_endpoint
						<< ", error: " << from_ec.message();
				}
				log_conn_warning()
					<< ", shutdown to endpoint: " << to_endpoint;
				co_await async_shutdown(to, net_awaitable[to_ec]);
				co_return;
			}

			// 重置最后活跃计数.
			m_last_activity.store(0, std::memory_order_relaxed);

			for (; !m_abort;)
			{
				// 并发读写, 将上次接收到的数据 primary_buf 发送给 to, 同时异步读取 from 的数
				// 据到 secondary_buf 中.
				auto [write_bytes, read_bytes] =
					co_await(
						net::async_write(to,
							net::buffer(primary_buf, bytes), net_awaitable[to_ec])
						&&
						from.async_read_some(
							net::buffer(secondary_buf, buf_size), net_awaitable[from_ec])
					);
				(void)write_bytes;

				// 交换主从缓冲区.
				std::swap(primary_buf, secondary_buf);

				// 保存接收到的数据大小用于转发给 to 端, 以及计算整个传输数据量.
				bytes = read_bytes;
				bytes_transferred += bytes;

				// 重置最后活跃计数.
				m_last_activity.store(0, std::memory_order_relaxed);

				// 如果 async_write 失败, 则也无需要再读取数据, 如果 async_read_some 失败, 则
				// 也无数据可用于写, 所以无论哪一种情况都可以直接退出.
				if (from_ec || to_ec)
				{
					if (from_ec && from_ec != net::error::eof)
					{
						log_conn_warning()
							<< ", read from endpoint: " << from_endpoint
							<< ", error: " << from_ec.message();
					}
					if (to_ec && to_ec != net::error::eof)
					{
						log_conn_warning()
							<< ", write to endpoint: " << to_endpoint
							<< ", error: " << to_ec.message();
					}

					// shutdown 当前连接, 只关闭非 TLS 连接, 而 TLS 连接需要在 transfer 完成后再
					// 操作, 否则有可能因为 async_shutdown 内部异步 io 导致未定义行业.
					if (from_ec)
					{
						log_conn_warning()
							<< ", shutdown to endpoint: " << to_endpoint;
						co_await async_shutdown(to, net_awaitable[to_ec]);
					}

					// 只要出错即可退出.
					co_return;
				}
			}

			co_return;
		}

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

		// 拼接文档目录和目标路径.
		static fs::path path_cat(
			const std::wstring& doc, const std::wstring& target) noexcept;

		// 从 HTTP 请求目标路径生成文件系统路径字符串.
		static std::wstring make_target_path(const std::string& target) noexcept;

		// 从文档根目录和请求目标生成实际文件系统路径.
		static fs::path make_real_target_path(const std::string& doc_directory,
			const std::string& target) noexcept;

		// 格式化目录下的文件列表, 返回每个文件/目录的名称.
		std::vector<std::wstring>
		format_path_list(const fs::path& path, boost::system::error_code& ec) const;

		// HTTP JSON 目录列表实现的模板函数, 支持迭代器遍历目录.
		template <typename DirIter>
		inline net::awaitable<void> on_http_json_impl(const http_context& hctx) noexcept
		{
			boost::system::error_code ec;

			auto& request = hctx.request_;
			auto target = hctx.target_path_;

			DirIter end;
			DirIter it(target, fs::directory_options::skip_permission_denied, ec);
			if (ec)
			{
				string_response res{ http::status::found, request.version() };
				res.set(http::field::server, version_string);
				res.set(http::field::date, server_date_string());
				res.set(http::field::location, "/");
				res.keep_alive(request.keep_alive());
				res.prepare_payload();

				http::serializer<false, string_body, http::fields> sr(res);
				co_await http::async_write(
					m_local_socket,
					sr,
					net_awaitable[ec]);
				if (ec)
				{
					log_conn_warning()
						<< ", http_dir write location err: "
						<< ec.message();
				}

				co_return;
			}

			bool hash = false;

			urls::params_view qp(hctx.regex_results_[1]);
			if (qp.find("hash") != qp.end())
				hash = true;

			boost::json::array path_list;

			for (; it != end && !m_abort; it++)
			{
				const auto& item = it->path();
				boost::json::object obj;

				auto [ftime, unc_path] = file_last_write_time(item);
				obj["last_write_time"] = ftime;

				if (fs::is_directory(unc_path.empty() ? item : unc_path, ec))
				{
					auto leaf = boost::nowide::narrow(fs::relative(item, target).wstring());
					obj["filename"] = leaf;
					obj["is_dir"] = true;
				}
				else
				{
					auto leaf = boost::nowide::narrow(fs::relative(item, target).wstring());
					obj["filename"] = leaf;
					obj["is_dir"] = false;
					if (unc_path.empty())
						unc_path = item;
					auto sz = fs::file_size(unc_path, ec);
					if (ec)
						sz = 0;
					obj["filesize"] = sz;
					if (hash)
					{
						auto ret = co_await
							fileop::async_hash_file(unc_path, net_awaitable[ec]);
						if (ec)
							ret = "";
						obj["hash"] = ret;
					}
				}

				path_list.push_back(obj);
			}

			auto body = boost::json::serialize(path_list);

			string_response res{ http::status::ok, request.version() };
			res.set(http::field::server, version_string);
			res.set(http::field::date, server_date_string());
			res.set(http::field::content_type, "application/json");
			res.keep_alive(request.keep_alive());
			res.body() = body;
			res.prepare_payload();

			http::serializer<false, string_body, http::fields> sr(res);
			co_await http::async_write(
				m_local_socket,
				sr,
				net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", http dir write body err: "
					<< ec.message();
			}

			co_return;
		}

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

		// 处理 HTTP DNS 查询请求 (支持 DNS-over-HTTPS 和 UDP).
		net::awaitable<void> on_http_dns_query(const http_context& hctx) noexcept;

		// udp_dns_query 通过传统 UDP 协议转发 DNS 查询到上游服务器.
		net::awaitable<void> udp_dns_query(
			const string_request& request, const std::string& dns_query) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// DNS 辅助工具

		// dns_encode_name 将域名编码为 DNS wire-format 标签序列.
		static std::string dns_encode_name(const std::string& name) noexcept;

		// dns_type_from_string 将类型名字符串转换为 DNS 类型数值.
		static uint16_t dns_type_from_string(const std::string& type_name) noexcept;

		// dns_type_to_string 将 DNS 类型数值转换为字符串.
		static std::string dns_type_to_string(uint16_t type) noexcept;

		// build_dns_wire_query 构建 DNS wire-format 查询包.
		static std::string build_dns_wire_query(
			const std::string& name, uint16_t type,
			bool cd = false, bool do_bit = false) noexcept;

		// dns_parse_name 从 wire-format 解析域名 (支持压缩指针).
		static std::pair<std::string, const char*>
		dns_parse_name(const char* p, const char* end, const char* msg_start) noexcept;

		// dns_rdata_to_string 将 RDATA 按类型解析为可读字符串.
		static std::string dns_rdata_to_string(
			uint16_t type, uint16_t rdlength,
			const char* rdata, const char* end,
			const char* msg_start) noexcept;

		// dns_svcparams_to_string 解析 HTTPS/SVCB 记录的 SvcParams (RFC 9460).
		static std::string dns_svcparams_to_string(
			const char* p, const char* end) noexcept;

		// dns_response_to_json 将 DNS wire-format 响应解析为 Google JSON API 格式.
		static std::string dns_response_to_json(
			const std::string& wire_response,
			const std::string& question_name,
			uint16_t question_type) noexcept;

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
		net::awaitable<void> doh_dns_query(
			const string_request& request, const std::string& dns_query) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// HTTP 响应辅助

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

		// 返回 HTTP 401 Unauthorized 响应.
		net::awaitable<void> unauthorized_http_route(const string_request& request) noexcept;

		// 发送 HTTP 字符串响应, 自动设置常用头部 (server, date, content-type, keep-alive).
		net::awaitable<void> send_http_string_response(
			const string_request& request,
			http::status status,
			std::string content_type,
			std::string body,
			bool keep_alive = true) noexcept;

		//////////////////////////////////////////////////////////////////////////
		// 流控制工具

		// 配置指定用户的速率限制.
		void user_rate_limit_config(const std::string& user) noexcept;

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

		//////////////////////////////////////////////////////////////////////////

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

		// 用于使用 ssl 加密通信与下游代理服务器通信时的 ssl context.
		net::ssl::context m_ssl_cli_context{ net::ssl::context::sslv23_client };

		// m_last_activity 用于双向超时检测, 在代理传输过程中, 只要有任何一个
		// 方向上有数据传输，都不应触发超时。
		std::atomic<int64_t> m_last_activity{ 0 };

		// 当前 session 是否被中止的状态.
		std::atomic<bool> m_abort{ false };
	};
}

#endif // INCLUDE__2023_10_18__PROXY_SESSION_HPP