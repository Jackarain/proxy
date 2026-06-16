//
// proxy_session.cpp
// ~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_session.hpp"
#include "proxy/async_connect.hpp"
#include "proxy/proxy_util.hpp"
#include "proxy/fileop.hpp"

#include <charconv>

#ifdef USE_PAM_AUTH
# include <security/pam_appl.h>
# include <security/pam_misc.h>
#endif

namespace proxy {

	using io_util::read;
	using io_util::write;

	//////////////////////////////////////////////////////////////////////////

	static const char* fake_500_content_fmt =
R"x*x*x(<html>
<head><title>500 Internal Server Error</title></head>
<body>
<center><h1>500 Internal Server Error</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>)x*x*x";

	static const char* fake_400_content_fmt =
R"x*x*x(HTTP/1.1 400 Bad Request
Server: nginx/1.20.2
Date: {}
Content-Type: text/html
Content-Length: 165
Connection: close

<html>
<head><title>400 Bad Request</title></head>
<body bgcolor="white">
<center><h1>400 Bad Request</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>)x*x*x";

	static const char* fake_400_content =
R"x*x*x(<html>
<head><title>400 Bad Request</title></head>
<body bgcolor="white">
<center><h1>400 Bad Request</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>)x*x*x";

	static const char* fake_401_content =
R"x*x*x(<html>
<head><title>401 Authorization Required</title></head>
<body>
<center><h1>401 Authorization Required</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>)x*x*x";

	static const char* fake_403_content =
R"x*x*x(<html>
<head><title>403 Forbidden</title></head>
<body>
<center><h1>403 Forbidden</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>
)x*x*x";

	static const char* fake_404_content_fmt =
R"x*x*x(HTTP/1.1 404 Not Found
Server: nginx/1.20.2
Date: {}
Content-Type: text/html
Content-Length: 145
Connection: close

<html><head><title>404 Not Found</title></head>
<body>
<center><h1>404 Not Found</h1></center>
<hr>
<center>nginx/1.20.2</center>
</body>
</html>)x*x*x";

	static const char* fake_407_content_fmt =
R"x*x*x(HTTP/1.1 407 Proxy Authentication Required
Server: nginx/1.20.2
Date: {}
Connection: close
Proxy-Authenticate: Basic realm="proxy"
Proxy-Connection: close
Content-Length: 0

)x*x*x";

	static const char* fake_416_content =
R"x*x*x(<html>
<head><title>416 Requested Range Not Satisfiable</title></head>
<body>
<center><h1>416 Requested Range Not Satisfiable</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>
)x*x*x";

	static const char* fake_302_content =
R"x*x*x(<html>
<head><title>301 Moved Permanently</title></head>
<body>
<center><h1>301 Moved Permanently</h1></center>
<hr><center>nginx/1.20.2</center>
</body>
</html>
)x*x*x";


	static constexpr auto head_fmt =
		LR"(<html><head><meta charset="UTF-8"><title>Index of {}</title></head><body bgcolor="white"><h1>Index of {}</h1><hr><pre>)";
	static constexpr auto tail_fmt =
		L"</pre><hr></body></html>";
	static constexpr auto body_fmt =
		L"<a href=\"{}\">{}</a>{} {}       {}\r\n";

	//////////////////////////////////////////////////////////////////////////

	static const std::unordered_map<std::string, std::string> global_mimes =
	{
		{ ".html", "text/html; charset=utf-8" },
		{ ".htm", "text/html; charset=utf-8" },
		{ ".js", "application/javascript" },
		{ ".h", "text/javascript" },
		{ ".hpp", "text/javascript" },
		{ ".cpp", "text/javascript" },
		{ ".cxx", "text/javascript" },
		{ ".cc", "text/javascript" },
		{ ".c", "text/javascript" },
		{ ".json", "application/json" },
		{ ".css", "text/css" },
		{ ".txt", "text/plain; charset=utf-8" },
		{ ".md", "text/plain; charset=utf-8" },
		{ ".log", "text/plain; charset=utf-8" },
		{ ".xml", "text/xml" },
		{ ".ico", "image/x-icon" },
		{ ".ttf", "application/x-font-ttf" },
		{ ".eot", "application/vnd.ms-fontobject" },
		{ ".woff", "application/x-font-woff" },
		{ ".pdf", "application/pdf" },
		{ ".png", "image/png" },
		{ ".jpg", "image/jpg" },
		{ ".jpeg", "image/jpg" },
		{ ".gif", "image/gif" },
		{ ".webp", "image/webp" },
		{ ".svg", "image/svg+xml" },
		{ ".wav", "audio/x-wav" },
		{ ".ogg", "video/ogg" },
		{ ".m4a", "audio/mp4" },
		{ ".mp3", "audio/mpeg" },
		{ ".mp4", "video/mp4" },
		{ ".flv", "video/x-flv" },
		{ ".f4v", "video/x-f4v" },
		{ ".ts", "video/MP2T" },
		{ ".mov", "video/quicktime" },
		{ ".avi", "video/x-msvideo" },
		{ ".wmv", "video/x-ms-wmv" },
		{ ".3gp", "video/3gpp" },
		{ ".mkv", "video/x-matroska" },
		{ ".7z", "application/x-7z-compressed" },
		{ ".ppt", "application/vnd.ms-powerpoint" },
		{ ".zip", "application/zip" },
		{ ".xz", "application/x-xz" },
		{ ".xml", "application/xml" },
		{ ".webm", "video/webm" },
		{ ".weba", "audio/webm" },
		{ ".m3u8", "application/vnd.apple.mpegurl" },
	};

	//////////////////////////////////////////////////////////////////////////

	// ====================================================================
	// DNS wire-format 编码/解码辅助函数 (用于 application/dns-json 支持)
	// ====================================================================

	// DNS 记录类型常量.
	static constexpr uint16_t DNS_TYPE_A = 1;
	static constexpr uint16_t DNS_TYPE_NS = 2;
	static constexpr uint16_t DNS_TYPE_CNAME = 5;
	static constexpr uint16_t DNS_TYPE_SOA = 6;
	static constexpr uint16_t DNS_TYPE_PTR = 12;
	static constexpr uint16_t DNS_TYPE_MX = 15;
	static constexpr uint16_t DNS_TYPE_TXT = 16;
	static constexpr uint16_t DNS_TYPE_AAAA = 28;
	static constexpr uint16_t DNS_TYPE_SRV = 33;
	static constexpr uint16_t DNS_TYPE_HTTPS = 65;
	static constexpr uint16_t DNS_TYPE_ANY = 255;
	static constexpr uint16_t DNS_TYPE_CAA = 257;

	static constexpr uint16_t DNS_CLASS_IN = 1;

	//////////////////////////////////////////////////////////////////////////

	// http_ranges 用于保存 http range 请求头的解析结果.
	// 例如: bytes=0-100,200-300,400-500
	// 解析后的结果为: { {0, 100}, {200, 300}, {400, 500} }
	// 例如: bytes=0-100,200-300,400-500,600
	// 解析后的结果为: { {0, 100}, {200, 300}, {400, 500}, {600, -1} }
	// 如果解析失败, 则返回空数组.
	using http_ranges = std::vector<std::pair<int64_t, int64_t>>;

	// parser_http_ranges 用于解析 http range 请求头.
	static http_ranges parser_http_ranges(std::string range) noexcept
	{
		// 去掉前后空白.
		range = strutil::remove_spaces(range);

		// range 必须以 bytes= 开头, 否则返回空数组.
		if (!range.starts_with("bytes="))
			return {};

		// 去掉开头的 bytes= 字符串 (不区分大小写).
		static constexpr std::string_view bytes_prefix = "bytes=";
		range.erase(0, bytes_prefix.size());

		http_ranges results;

		// 获取其中所有 range 字符串.
		auto ranges = strutil::split(range, ",");
		for (const auto& str : ranges)
		{
			auto r = strutil::split(std::string(str), "-");

			// range 只有一个数值.
			if (r.size() == 1)
			{
				if (str.empty())
				{
					results.emplace_back(0, -1);
				}
				else if (str.front() == '-')
				{
					auto pos = std::atoll(r.front().data());
					results.emplace_back(-1, pos);
				}
				else
				{
					auto pos = std::atoll(r.front().data());
					results.emplace_back(pos, -1);
				}

				continue;
			}

			if (r.size() == 2)
			{
				// range 有 start 和 end 的情况, 解析成整数到容器.
				auto& start_str = r[0];
				auto& end_str = r[1];

				if (start_str.empty() && !end_str.empty())
				{
					auto end = std::atoll(end_str.data());
					results.emplace_back(-1, end);
				}
				else
				{
					auto start = std::atoll(start_str.data());
					auto end = std::atoll(end_str.data());
					if (end_str.empty())
						end = -1;

					results.emplace_back(start, end);
				}

				continue;
			}

			// 在一个 range 项中不应该存在3个或以上的'-', 这属于无效的范围请求.
			return {};
		}

		return results;
	}

	// 根据 range 计算文件偏移位置.
	static std::tuple<int64_t, int64_t, http::status>
	offset_from_range(const http_ranges& range, int64_t content_length)
	{
		if (range.size() != 1)
			return { -1, -1, http::status::ok };

		auto& r = range.front();
		int64_t offset = r.first;
		int64_t end = r.second;

		// 起始位置为 -1, 表示从文件末尾开始读取, 例如 Range: -500
		// 则表示读取文件末尾的 500 字节.
		if (offset == -1)
		{
			// 如果第二个参数也为 -1, 则表示请求有问题, 返回 416.
			if (r.second < 0)
				return { offset, end, http::status::range_not_satisfiable };

			// 计算起始位置和结束位置, 例如 Range: -5
			// 则表示读取文件末尾的 5 字节.
			// content_length - r.second 表示起始位置.
			// content_length - 1 表示结束位置.
			// 例如文件长度为 10 字节, 则起始位置为 5,
			// 结束位置为 9(数据总长度为[0-9]), 一共 5 字节.
			offset = content_length - r.second;
			end = content_length - 1;
		}
		else if (end == -1)
		{
			// 起始位置为正数, 表示从文件头开始读取, 例如 Range: 500
			// 则表示读取文件头的 500 字节.
			if (r.first < 0)
				return { offset, end,  http::status::range_not_satisfiable };

			offset = r.first;
			end = content_length - 1;
		}

		if (offset == -1)
			return { offset, end, http::status::ok };

		return { offset, end, http::status::partial_content };
	}

	//////////////////////////////////////////////////////////////////////////
	// proxy_session 辅助函数

	enum {
		PROXY_AUTH_SUCCESS = 0,
		PROXY_AUTH_FAILED,
		PROXY_AUTH_NONE,
		PROXY_AUTH_ILLEGAL,
	};

	std::string proxy_session::pauth_error_message(int code) noexcept
	{
		switch (code)
		{
		case PROXY_AUTH_SUCCESS:
			return "auth success";
		case PROXY_AUTH_FAILED:
			return "auth failed";
		case PROXY_AUTH_NONE:
			return "auth none";
		case PROXY_AUTH_ILLEGAL:
			return "auth illegal";
		default:
			return "auth unknown";
		}
	}

	void proxy_session::update_bind_interface(const std::string& addr) noexcept
	{
		if (addr.empty())
			return;

		boost::system::error_code ec;
		auto bind_if = net::ip::make_address(addr, ec);
		if (ec)
		{
			// bind 地址有问题, 忽略 bind 参数, 并输出日志.
			log_conn_warning()
				<< ", bind address: " << addr
				<< ", invalid: " << ec.message();
		}
		else
		{
			m_bind_interface = bind_if;
		}
	}

	size_t proxy_session::connection_id() const noexcept
	{
		return m_connection_id;
	}

	bool proxy_session::is_crytpo_stream() const noexcept
	{
		return boost::variant2::holds_alternative<ssl_tcp_stream>(m_remote_socket);
	}

	//////////////////////////////////////////////////////////////////////////
	// http 相关实现

	net::awaitable<void> proxy_session::on_http_all_json(const http_context& hctx) noexcept
	{
		co_await on_http_json_impl<fs::recursive_directory_iterator>(hctx);
	}

	net::awaitable<void> proxy_session::on_http_json(const http_context& hctx) noexcept
	{
		co_await on_http_json_impl<fs::directory_iterator>(hctx);
	}

	std::string proxy_session::server_date_string() noexcept
	{
		// 缓存 date string 以避免频繁调用 time/gmtime/strftime.
		// 每秒刷新一次即可满足 HTTP Date 头精度要求.
		// 使用 mutex 保护静态变量, 防止多协程并发访问导致数据竞争.
		static std::string cached;
		static std::chrono::steady_clock::time_point last_update;
		static std::mutex mtx;

		auto now = std::chrono::steady_clock::now();
		{
			std::lock_guard<std::mutex> lock(mtx);
			if (now - last_update >= std::chrono::seconds(1))
			{
				auto time = std::time(nullptr);
				auto gmt = gmtime((const time_t*)&time);

				cached.resize(64, '\0');
				auto ret = strftime(cached.data(), 64, "%a, %d %b %Y %H:%M:%S GMT", gmt);
				cached.resize(ret);
				last_update = now;
			}
			return cached;
		}
	}

	void proxy_session::user_rate_limit_config(const std::string& user) noexcept
	{
		// 在这里使用用户指定的速率设置替换全局速率配置.
		auto found = m_option.users_rate_limit_.find(user);
		if (found != m_option.users_rate_limit_.end())
		{
			auto& rate = *found;
			m_option.tcp_rate_limit_ = rate.second;
		}
	}

	void proxy_session::stream_expires_never(variant_stream_type& stream) noexcept
	{
		boost::variant2::visit([](auto& s) mutable
		{
			using ValueType = std::decay_t<decltype(s)>;
			using NextLayerType = util::proxy_tcp_socket::next_layer_type;

			if constexpr (std::same_as<NextLayerType, util::tcp_socket>)
			{
				if constexpr (std::same_as<util::proxy_tcp_socket, ValueType>)
				{
					auto& next_layer = s.next_layer();
					next_layer.expires_never();
				}
				else if constexpr (std::same_as<util::ssl_tcp_stream, ValueType>)
				{
					auto& next_layer = s.next_layer().next_layer();
					next_layer.expires_never();
				}
			}
		}, stream);
	}

	void proxy_session::stream_expires_after(
		variant_stream_type& stream, net::steady_timer::duration expiry_time) noexcept
	{
		if (expiry_time.count() < 0)
			return;

		boost::variant2::visit([expiry_time](auto& s) mutable
		{
			using ValueType = std::decay_t<decltype(s)>;
			using NextLayerType = util::proxy_tcp_socket::next_layer_type;

			if constexpr (std::same_as<NextLayerType, util::tcp_socket>)
			{
				if constexpr (std::same_as<util::proxy_tcp_socket, ValueType>)
				{
					auto& next_layer = s.next_layer();
					next_layer.expires_after(expiry_time);
				}
				else if constexpr (std::same_as<util::ssl_tcp_stream, ValueType>)
				{
					auto& next_layer = s.next_layer().next_layer();
					next_layer.expires_after(expiry_time);
				}
			}
		}, stream);
	}

	void proxy_session::stream_rate_limit(variant_stream_type& stream, int rate) noexcept
	{
		boost::variant2::visit([rate](auto& s) mutable
			{
				using ValueType = std::decay_t<decltype(s)>;
				using NextLayerType = proxy_tcp_socket::next_layer_type;

				if constexpr (std::same_as<NextLayerType, tcp_socket>)
				{
					if constexpr (std::same_as<proxy_tcp_socket, ValueType>)
					{
						auto& next_layer = s.next_layer();
						next_layer.rate_limit(rate);
					}
					else if constexpr (std::same_as<ssl_tcp_stream, ValueType>)
					{
						auto& next_layer = s.next_layer().next_layer();
						next_layer.rate_limit(rate);
					}
				}
			}, stream);
	}

	//////////////////////////////////////////////////////////////////////////
	// PAM 认证

#ifdef USE_PAM_AUTH
	int proxy_session::pam_conv_func(int num_msg, const struct pam_message **msg,
		struct pam_response **resp, void *appdata_ptr)
	{
		if (num_msg <= 0 || num_msg > PAM_MAX_NUM_MSG)
			return PAM_CONV_ERR;

		*resp = (struct pam_response *)std::calloc(num_msg, sizeof(struct pam_response));
		if (*resp == nullptr)
			return PAM_BUF_ERR;

		const char *password = (const char *)appdata_ptr;

		for (int i = 0; i < num_msg; i++)
		{
			if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF)
			{
				(*resp)[i].resp = strdup(password);
				(*resp)[i].resp_retcode = 0;
			}
			else
			{
				(*resp)[i].resp = nullptr;
			}
		}

		return PAM_SUCCESS;
	}

	bool proxy_session::pam_authenticate_user(const char *service, const char *username, const char *password) noexcept
	{
		pam_handle_t *pamh = nullptr;
		struct pam_conv conv = {
			.conv = pam_conv_func,
			.appdata_ptr = (void *)password  // 传入密码
		};

		int retval = pam_start(service, username, &conv, &pamh);
		if (retval != PAM_SUCCESS)
		{
			log_conn_warning() << ", pam_start failed: " << pam_strerror(pamh, retval);
			return false;
		}

		retval = pam_authenticate(pamh, 0);  // 核心认证
		if (retval != PAM_SUCCESS) {
			log_conn_warning() << ", authentication failed: " << pam_strerror(pamh, retval);
			pam_end(pamh, retval);
			return false;
		}

		retval = pam_acct_mgmt(pamh, 0);	// 检查账户（如锁定、过期）
		if (retval != PAM_SUCCESS) {
			log_conn_warning() << ", account management failed: " << pam_strerror(pamh, retval);
			pam_end(pamh, retval);
			return false;
		}

		pam_end(pamh, PAM_SUCCESS);

		log_conn_debug() << ", pam_authenticate_user success";

		return true;
	}
#endif // USE_PAM_AUTH

	//////////////////////////////////////////////////////////////////////////
	// 认证与授权

	bool proxy_session::auth_required() const noexcept
	{
		if (!m_option.auth_users_.empty())
			return true;

		if (!m_option.pam_auth_.empty())
			return true;

		return false;
	}

	net::awaitable<bool> proxy_session::check_userpasswd(
		const std::string& username,
		const std::string& passwd, bool skip_passwd) noexcept
	{
		// 若不需要认证, 直接返回 true.
		if (!auth_required())
			co_return true;

		// 检查用户名和密码是否匹配.
		for (const auto& [user, pwd, addr, proxy_pass] : m_option.auth_users_)
		{
			if (username == user)
			{
				if (!skip_passwd && passwd != pwd)
					continue;

				user_rate_limit_config(user);
				update_bind_interface(addr);

				if (proxy_pass)
					m_proxy_pass = proxy_pass;

				co_return true;
			}
		}

		// 如果启用了 PAM 认证, 则尝试使用 PAM 认证.
		if (!skip_passwd && !m_option.pam_auth_.empty())
		{
#ifdef USE_PAM_AUTH
			boost::system::error_code ec;
			auto result = co_await async_pam_auth(username, passwd,
				m_option.pam_auth_, net_awaitable[ec]);
			if (result)
			{
				user_rate_limit_config(username);
				co_return true;
			}
#endif
		}

		co_return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// 公共接口

	void proxy_session::start() noexcept
	{
		auto server = m_proxy_server.lock();
		if (!server)
			return;

		// 保存 server 的参数选项.
		m_option = server->option();

		// 将 local_ip 转换为 ip::address 对象, 用于后面向外发起连接时
		// 绑定到指定的本地地址.
		boost::system::error_code ec;
		m_bind_interface = net::ip::make_address(m_option.local_ip_, ec);
		if (ec)
		{
			// bind 地址有问题, 忽略bind参数.
			m_bind_interface.reset();
		}

		// 如果指定了 proxy_pass_ 参数, 则记录这个参数, 在后面若用户又特指了更具体的
		// proxy_pass, 则在 check_userpasswd 中更新 m_proxy_pass 为用户特定的
		// proxy_pass.
		m_proxy_pass = m_option.proxy_pass_;

		// 保持 self 对象指针, 以防止在协程完成后 this 被销毁.
		auto self = this->shared_from_this();

		// 如果是透明代理, 则启动透明代理协程.
		if (m_tproxy_remote)
		{
			net::co_spawn(m_executor,
				[this, self]() -> net::awaitable<void>
				{
					co_await transparent_proxy();
					co_return;
				}, net::detached);

			return;
		}

		// 如果是 stdio proxy, 则启动 stdio proxy 协程.
		if (!m_option.stdio_target_.empty())
		{
			net::co_spawn(m_executor,
				[this, self]() -> net::awaitable<void>
				{
					co_await stdio_proxy();
					co_return;
				}, net::detached);

			return;
		}

		// 启动协议侦测协程.
		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				if (boost::variant2::holds_alternative<proxy_tcp_socket>(m_local_socket))
				{
					co_await proto_detect<proxy_tcp_socket>();
				}
				else if (boost::variant2::holds_alternative<proxy_uds_socket>(m_local_socket))
				{
					co_await proto_detect<proxy_uds_socket>();
				}

				co_return;
			}, net::detached);
	}

	void proxy_session::close() noexcept
	{
		if (m_abort)
			return;

		m_abort = true;

		boost::system::error_code ignore_ec;

		// 关闭所有 socket.
		m_local_socket.close(ignore_ec);
		m_remote_socket.close(ignore_ec);
	}

	void proxy_session::setup_tproxy(const net::ip::tcp::endpoint& tproxy_remote) noexcept
	{
		log_conn_debug()
			<< ", tproxy setup: " << tproxy_remote;

		m_tproxy_remote = tproxy_remote;
	}

	//////////////////////////////////////////////////////////////////////////
	// 专用代理模式

	net::awaitable<void> proxy_session::stdio_proxy() noexcept
	{
		auto executor = co_await net::this_coro::executor;
		boost::system::error_code ec;

		if (m_option.stdio_target_.empty())
		{
			log_conn_error() << ", stdio proxy requires a stdio_target";
			co_return;
		}

		try
		{
			boost::system::result<url_info> expect_url;
			std::string url = m_option.stdio_target_;

			if (url.find("://") == std::string::npos)
				url = "http://" + url;

			expect_url = parse_urlinfo(url);
			if (expect_url.has_error())
			{
				log_conn_error() << ", stdio proxy param stdio target is bad: " << url;
				co_return;
			}

			auto [scheme, user, passwd, host, port, resource] = *expect_url;

			// 查询 stdio_target_ 目标服务器的域名信息.
			auto targets = co_await resolve_proxy_pass_targets();

			// 创建 tcp socket 用于连接到目标服务器.
			tcp::socket& remote_socket = net_tcp_socket(m_remote_socket);

			// 发起连接到 stdio_target_ 目标服务器.
			ec = co_await connect_proxy_pass(remote_socket, targets);
			if (ec)
				co_return;

			// 与网关中继服务器握手.
			ec = co_await proxy_pass_handshake(remote_socket,
				std::string(host),
				port,
				*m_proxy_pass);
			if (ec)
				co_return;

			size_t l2r_transferred = 0;
			size_t r2l_transferred = 0;

#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
			net::posix::stream_descriptor stream_stdout(m_executor, ::dup(STDOUT_FILENO));
			stdio_stream stream(std::move(stream_stdout));

			auto out = init_proxy_stream(std::move(stream));
			auto& in = m_local_socket;

			// 并发读写, 在 local 和 remote 之间互传数据.
			co_await(
				transfer(in, m_remote_socket, l2r_transferred)
				&&
				transfer(m_remote_socket, out, r2l_transferred)
				);

#else
			auto in_executor = boost::variant2::get<stdio_stream>(m_local_socket).get_in_executor();
			auto out_executor = boost::variant2::get<stdio_stream>(m_local_socket).get_out_executor();

			stdio_stream stream(in_executor, out_executor);

			auto out = init_proxy_stream(std::move(stream));
			auto& in = m_local_socket;

			auto self = shared_from_this();

			net::co_spawn(in_executor,
				[this, self, &in, &l2r_transferred]() mutable -> net::awaitable<void>
				{
					boost::system::error_code ec;
					constexpr size_t buf_size = 4096;
					char buf[buf_size]{ 0 };

					while (!m_abort)
					{
						DWORD read_bytes = 0;
						read_bytes = (DWORD)co_await in.async_read_some(
							net::buffer(buf, buf_size), net_awaitable[ec]);
						if (ec)
							break;

						co_await net::async_write(
							m_remote_socket,
							net::buffer(buf, read_bytes),
							net_awaitable[ec]);
						if (ec)
							break;

						l2r_transferred += read_bytes;
					}

					co_return;
				}, net::detached);

			co_await net::co_spawn(out_executor,
				[this, self, &out, &r2l_transferred]() mutable -> net::awaitable<void>
				{
					boost::system::error_code ec;
					constexpr size_t buf_size = 4096;
					char buf[buf_size]{ 0 };

					while (!m_abort)
					{
						DWORD read_bytes = (DWORD)co_await m_remote_socket.async_read_some(
							net::buffer(buf, buf_size), net_awaitable[ec]);
						if (ec)
							break;

						co_await net::async_write(
							out,
							net::buffer(buf, read_bytes),
							net_awaitable[ec]);
						if (ec)
							break;

						r2l_transferred += read_bytes;
					}

					co_return;
				}, net::deferred);
#endif

			log_conn_debug()
				<< ", transfer completed"
				<< ", local to remote: "
				<< l2r_transferred
				<< ", remote to local: "
				<< r2l_transferred;
		}
		catch (const std::exception& e)
		{
			log_conn_error() << ", stdio_proxy exception: " << e.what();
		}
		co_return;
	}

	net::awaitable<void> proxy_session::transparent_proxy() noexcept
	{
		auto executor = co_await net::this_coro::executor;
		boost::system::error_code ec;

		if (!m_proxy_pass)
		{
			XLOG_ERR << "transparent proxy requires a proxy_pass";
			co_return;
		}

		try
		{
			tcp::socket& remote_socket = net_tcp_socket(m_remote_socket);

			// 查询网关代理中继服务器域名信息.
			auto targets = co_await resolve_proxy_pass_targets();

			// 发起连接到网关代理中继服务器.
			ec = co_await connect_proxy_pass(remote_socket, targets);
			if (ec)
				co_return;

			// 与网关中继服务器握手.
			ec = co_await proxy_pass_handshake(remote_socket,
				m_tproxy_remote->address().to_string(),
				m_tproxy_remote->port(),
				*m_proxy_pass);
			if (ec)
				co_return;

			co_await concurrent_transfer();
		}
		catch (const std::exception& e)
		{
			log_conn_error()
				<< ", transparent_proxy exception: " << e.what();
		}

		co_return;
	}

	//////////////////////////////////////////////////////////////////////////
	// 混淆与协议侦测

	template
	net::awaitable<bool>
	proxy_session::noise_handshake<proxy_tcp_socket>(
		proxy_tcp_socket& socket,
		std::array<uint8_t, 16>& inkey,
		std::array<uint8_t, 16>& outkey) noexcept;

	template
	net::awaitable<bool>
	proxy_session::noise_handshake<net::local::stream_protocol::socket>(
		net::local::stream_protocol::socket& socket,
		std::array<uint8_t, 16>& inkey,
		std::array<uint8_t, 16>& outkey) noexcept;

	//////////////////////////////////////////////////////////////////////////
	// 代理协议处理

	net::awaitable<void> proxy_session::start_proxy() noexcept
	{
		// read
		//  +----+----------+----------+
		//  |VER | NMETHODS | METHODS  |
		//  +----+----------+----------+
		//  | 1  |    1     | 1 to 255 |
		//  +----+----------+----------+
		//  [               ]
		// or
		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//  | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//    1    1      2        4                  variable       1
		//  [         ]
		// 读取[]里的部分.

		boost::system::error_code ec;

		[[maybe_unused]] auto bytes =
			co_await net::async_read(
				m_local_socket,
				m_local_buffer,
				net::transfer_exactly(2),
				net_awaitable[ec]);
		if (ec)
		{
			log_conn_error()
				<< ", read socks version: "
				<< ec.message();
			co_return;
		}
		BOOST_ASSERT(bytes == 2);

		auto p = (const char*)m_local_buffer.data().data();
		int socks_version = read<uint8_t>(p);

		if (socks_version == SOCKS_VERSION_5)
		{
			if (m_option.disable_socks_)
			{
				log_conn_debug()
					<< ", socks5 protocol disabled";
				co_return;
			}

			log_conn_debug()
				<< ", socks version: "
				<< socks_version;

			co_await socks_connect_v5();
			co_return;
		}
		if (socks_version == SOCKS_VERSION_4)
		{
			if (m_option.disable_socks_)
			{
				log_conn_debug()
					<< ", socks4 protocol disabled";
				co_return;
			}

			log_conn_debug()
				<< ", socks version: "
				<< socks_version;

			co_await socks_connect_v4();
			co_return;
		}

		bool ret = false;
		if (socks_version == 'G' || socks_version == 'P')
		{
			if (m_option.disable_http_)
			{
				log_conn_debug()
					<< ", http protocol disabled";
				co_return;
			}

			ret = co_await http_proxy_get();
		}
		else if (socks_version == 'C')
		{
			if (m_option.disable_http_)
			{
				log_conn_debug()
					<< ", http protocol disabled";
				co_return;
			}

			ret = co_await http_proxy_connect();
		}

		if (!ret)
		{
			auto date_string = server_date_string();
			auto fake_page =
				fmt::vformat(fake_400_content_fmt,
					fmt::make_format_args(date_string));

			co_await net::async_write(
				m_local_socket,
				net::buffer(fake_page),
				net::transfer_all(),
				net_awaitable[ec]);
		}

		co_return;
	}

	net::awaitable<void> proxy_session::socks_connect_v5() noexcept
	{
		auto p = (const char*)m_local_buffer.data().data();

		auto socks_version = read<int8_t>(p);
		BOOST_ASSERT(socks_version == SOCKS_VERSION_5);
		int nmethods = read<int8_t>(p);
		if (nmethods <= 0 || nmethods > 255)
		{
			log_conn_error()
				<< ", unsupported method : "
				<< nmethods;
			co_return;
		}

		//  +----+----------+----------+
		//  |VER | NMETHODS | METHODS  |
		//  +----+----------+----------+
		//  | 1  |    1     | 1 to 255 |
		//  +----+----------+----------+
		//                  [          ]
		m_local_buffer.consume(m_local_buffer.size());
		boost::system::error_code ec;
		auto bytes = co_await net::async_read(m_local_socket,
			m_local_buffer,
			net::transfer_exactly(nmethods),
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_error()
				<< ", read socks methods: "
				<< ec.message();
			co_return;
		}

		// 服务端是否需要认证.
		auto is_auth_required = auth_required();

		// 循环读取客户端支持的代理方式.
		p = (const char*)m_local_buffer.data().data();

		int method = SOCKS5_AUTH_UNACCEPTABLE;
		while (bytes != 0)
		{
			int m = read<int8_t>(p);

			if (!is_auth_required)
			{
				if (m == SOCKS5_AUTH_NONE)
				{
					method = m;
					break;
				}

				method = m;
			}
			else if (m == SOCKS5_AUTH)
			{
				method = m;
				break;
			}

			bytes--;
		}

		net::streambuf wbuf{};

		// 回复客户端, server所选择的代理方式.
		auto wp = (char*)wbuf.prepare(1024).data();
		write<uint8_t>(socks_version, wp);
		write<uint8_t>((uint8_t)method, wp);

		wbuf.commit(2);

		//  +----+--------+
		//  |VER | METHOD |
		//  +----+--------+
		//  | 1  |   1    |
		//  +----+--------+
		//  [             ]
		bytes = co_await net::async_write(m_local_socket,
			wbuf,
			net::transfer_exactly(2),
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", write server method error : "
				<< ec.message();
			co_return;
		}

		if (method == SOCKS5_AUTH_UNACCEPTABLE)
		{
			log_conn_warning()
				<< ", no acceptable methods for server";
			co_return;
		}

		// 认证模式, 则进入认证子协程.
		if (method == SOCKS5_AUTH)
		{
			auto ret = co_await socks_auth();
			if (!ret)
				co_return;
		}

		//  +----+-----+-------+------+----------+----------+
		//  |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
		//  +----+-----+-------+------+----------+----------+
		//  | 1  |  1  | X'00' |  1   | Variable |    2     |
		//  +----+-----+-------+------+----------+----------+
		//  [                          ]
		m_local_buffer.consume(m_local_buffer.size());
		bytes = co_await net::async_read(m_local_socket,
			m_local_buffer,
			net::transfer_exactly(5),
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", read client request error: "
				<< ec.message();
			co_return;
		}

		p = (const char*)m_local_buffer.data().data();
		auto ver = read<int8_t>(p);
		if (ver != SOCKS_VERSION_5)
		{
			log_conn_warning()
				<< ", socks requests, invalid protocol: "
				<< ver;
			co_return;
		}

		int command = read<int8_t>(p);		// CONNECT/BIND/UDP
		read<int8_t>(p);					// reserved.
		int atyp = read<int8_t>(p);			// atyp.

		//  +----+-----+-------+------+----------+----------+
		//  |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
		//  +----+-----+-------+------+----------+----------+
		//  | 1  |  1  | X'00' |  1   | Variable |    2     |
		//  +----+-----+-------+------+----------+----------+
		//                              [                   ]
		int length = 0;

		// 消费掉前4个字节, 保存第1个字节.
		m_local_buffer.consume(4);

		if (atyp == SOCKS5_ATYP_IPV4)
			length = 5; // 6 - 1
		else if (atyp == SOCKS5_ATYP_DOMAINNAME)
		{
			length = read<uint8_t>(p) + 2;
			m_local_buffer.consume(1);
		}
		else if (atyp == SOCKS5_ATYP_IPV6)
			length = 17; // 18 - 1
		else
		{
			log_conn_warning()
				<< ", socks5 request, unsupported address type: "
				<< atyp;
			co_return;
		}

		bytes = co_await net::async_read(m_local_socket,
			m_local_buffer,
			net::transfer_exactly(length),
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", read client request dst.addr error: "
				<< ec.message();
			co_return;
		}

		tcp::endpoint dst_endpoint;
		std::string domain;
		uint16_t port = 0;
		bool resolved = true;

		auto executor = co_await net::this_coro::executor;

		p = (const char*)m_local_buffer.data().data();
		if (atyp == SOCKS5_ATYP_IPV4)
		{
			dst_endpoint.address(net::ip::address_v4(read<uint32_t>(p)));
			dst_endpoint.port(read<uint16_t>(p));

			domain = dst_endpoint.address().to_string();
			port = dst_endpoint.port();

			log_conn_debug()
				<< ", "
				<< remote_endpoint_string(m_local_socket)
				<< " to ipv4: "
				<< dst_endpoint
				<< ", command: "
				<< command;
		}
		else if (atyp == SOCKS5_ATYP_DOMAINNAME)
		{
			for (size_t i = 0; i < bytes - 2; i++)
				domain.push_back(read<int8_t>(p));
			port = read<uint16_t>(p);

			dst_endpoint.port(port);
			dst_endpoint.address(
				net::ip::make_address(domain, ec));
			if (ec ||
				dst_endpoint.address() == net::ip::make_address_v4("0.0.0.0") ||
				dst_endpoint.address() == net::ip::make_address_v6("::0"))
			{
				if (ec)
					resolved = false;

				auto address = tcp_remote_endpoint(m_local_socket).address();
				dst_endpoint.address(address);
			}

			log_conn_debug()
				<< ", "
				<< remote_endpoint_string(m_local_socket)
				<< " to domain: "
				<< domain
				<< ":"
				<< port
				<< ", command: "
				<< command;
		}
		else if (atyp == SOCKS5_ATYP_IPV6)
		{
			net::ip::address_v6::bytes_type addr;
			for (auto i = addr.begin();
				i != addr.end(); ++i)
			{
				*i = read<int8_t>(p);
			}

			dst_endpoint.address(net::ip::address_v6(addr));
			dst_endpoint.port(read<uint16_t>(p));

			domain = dst_endpoint.address().to_string();
			port = dst_endpoint.port();

			log_conn_debug()
				<< ", "
				<< remote_endpoint_string(m_local_socket)
				<< " to ipv6: "
				<< dst_endpoint
				<< ", command: "
				<< command;
		}

		if (command == SOCKS_CMD_CONNECT)
		{
			// 连接目标主机.
			ec = co_await start_connect_host(domain, port, resolved);
			if (ec)
			{
				log_conn_warning()
					<< ", connect to target "
					<< domain
					<< ":"
					<< port
					<< " error: "
					<< ec.message();
			}
		}
		else if (command == SOCKS5_CMD_UDP)
		do {
			if (m_option.disable_udp_)
			{
				log_conn_debug()
					<< ", udp protocol disabled";
				ec = net::error::connection_refused;
				break;
			}

			auto remote_endp = tcp_remote_endpoint(m_local_socket);
			auto protocol = remote_endp.address().is_v4() ? udp::v4() : udp::v6();

			// 创建UDP端口.
			udp::socket local_udp_socket(m_executor);
			local_udp_socket.open(protocol, ec);
			if (ec)
			{
				log_conn_warning()
					<< ", open udp socket error: "
					<< ec.message();
				break;
			}

			// 如果有指定绑定本地地址, 则创建绑定到指定地址的 udp socket 用于数据发送.
			std::optional<udp::socket> remote_bind_socket;

			// 绑定输出网络.
			auto bind_if = udp::endpoint(protocol, 0);
			if (m_bind_interface)
			{
				bind_if.address(*m_bind_interface);

				remote_bind_socket.emplace(m_executor);
				remote_bind_socket->open(protocol, ec);
				if (ec)
				{
					log_conn_warning()
						<< ", open remote bind udp socket error: "
						<< ec.message();
					break;
				}

				// 绑定到指定的地址.
				remote_bind_socket->bind(bind_if, ec);
				if (ec)
				{
					log_conn_warning()
						<< ", bind remote udp socket to "
						<< bind_if
						<< " error: "
						<< ec.message();
					break;
				}

				if (m_option.so_mark_)
				{
					auto ret = set_socket_mark(remote_bind_socket->native_handle(), m_option.so_mark_.value());
					if (ret.has_error())
					{
						log_conn_warning()
							<< ", set socket mark error: "
							<< ret.error().message();
					}
				}
			}

			// 绑定到和 tcp socket 相同的地址.
			udp::endpoint local_udp_endp(tcp_local_endpoint(m_local_socket).address(), 0);
			local_udp_socket.bind(local_udp_endp, ec);
			if (ec)
			{
				log_conn_warning()
					<< ", bind local udp socket to "
					<< local_udp_endp
					<< " error: "
					<< ec.message();
				break;
			}

			// 对 local_udp_socket 也设置 SO_MARK，确保非 bridge 模式下
			// 由 local_udp_socket 发出的 UDP 数据包也带有正确的标记。
			if (m_option.so_mark_)
			{
				auto ret = set_socket_mark(local_udp_socket.native_handle(), m_option.so_mark_.value());
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", set socket mark error: "
						<< ret.error().message();
				}
			}

			if (m_proxy_pass)
			{
				// 如果是代理桥接模式, 则连接桥接服务器, 然后所有接收到 client 端的数据
				// 统统转发给桥接服务器, 桥接服务器上接收到的数据再转发给客户端.
				// 中间不需要解包和封包 udp 数据报.

				uint16_t port = 0;
				if (remote_bind_socket)
					port = remote_bind_socket->local_endpoint(ec).port();
				else
					port = local_udp_socket.local_endpoint(ec).port();

				ec = co_await start_connect_host("0.0.0.0", port, true, SOCKS5_CMD_UDP);
				if (ec)
				{
					log_conn_warning()
						<< ", start connect SOCKS5_CMD_UDP error: "
						<< ec.message();
					break;
				}

				log_conn_debug()
					<< ", start connect SOCKS5_CMD_UDP success";
			}

			// 所有发向 udp socket 的数据, 都将转发到 m_local_udp_endpoint
			// 除非地址是 m_local_udp_endpoint 本身除外.
			m_local_udp_endpoint.address(remote_endp.address());
			m_local_udp_endpoint.port(dst_endpoint.port());

			// 开始回复客户端 udp 关联成功消息.
			wbuf.consume(wbuf.size());
			auto wp = (char*)wbuf.prepare(64 + domain.size()).data();

			write<uint8_t>(SOCKS_VERSION_5, wp);	// VER
			write<uint8_t>(0, wp);					// REP
			write<uint8_t>(0x00, wp);				// RSV

			auto local_endp = local_udp_socket.local_endpoint(ec);
			if (ec)
			{
				log_conn_warning()
					<< ", get local udp socket endpoint error: "
					<< ec.message();
				break;
			}

			log_conn_debug()
				<< ", udp client address: "
				<< m_local_udp_endpoint
				<< ", udp local socket: "
				<< local_endp
				<< ", tcp local socket: "
				<< local_endpoint_string(m_local_socket)
				<< ", udp bind interface: "
				<< bind_if.address().to_string();

			if (local_endp.address().is_v4())
			{
				auto uaddr = local_endp.address().to_v4().to_uint();
				if (uaddr == 0)
					uaddr = tcp_local_endpoint(m_local_socket).address().to_v4().to_uint();

				write<uint8_t>(SOCKS5_ATYP_IPV4, wp);
				write<uint32_t>(uaddr, wp);
				write<uint16_t>(local_endp.port(), wp);
			}
			else if (local_endp.address().is_v6())
			{
				write<uint8_t>(SOCKS5_ATYP_IPV6, wp);
				auto data = local_endp.address().to_v6().to_bytes();
				for (auto c : data)
					write<uint8_t>(c, wp);
				write<uint16_t>(local_endp.port(), wp);
			}

			auto len = wp - (const char*)wbuf.data().data();
			wbuf.commit(len);
			bytes = co_await net::async_write(m_local_socket,
				wbuf,
				net::transfer_exactly(len),
				net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", write server response error: "
					<< ec.message();
				co_return;
			}

			// 在此等待 tcp 连接断开.
			if (!remote_bind_socket)
			{
				co_await(m_local_socket.async_read_some(m_local_buffer.prepare(1), net_awaitable[ec])
					||
					forward_udp(local_udp_socket, local_udp_socket));
			}
			else
			{
				co_await(m_local_socket.async_read_some(m_local_buffer.prepare(1), net_awaitable[ec])
					||
					forward_udp(local_udp_socket, *remote_bind_socket)
					||
					forward_udp(*remote_bind_socket, local_udp_socket)
					);
			}

			// 关闭 udp socket.
			local_udp_socket.close(ec);
			if (remote_bind_socket)
				remote_bind_socket->close(ec);

			co_return;
		} while (0);
		else
		{
			log_conn_warning()
				<< ", unsupported socks5 command: "
				<< command;
			ec = make_error_code(boost::system::errc::not_supported);
		}

		// 连接成功或失败.
		{
			int8_t error_code = SOCKS5_SUCCEEDED;

			if (ec == net::error::connection_refused)
				error_code = SOCKS5_CONNECTION_REFUSED;
			else if (ec == net::error::network_unreachable)
				error_code = SOCKS5_NETWORK_UNREACHABLE;
			else if (ec == net::error::host_unreachable)
				error_code = SOCKS5_HOST_UNREACHABLE;
			else if (ec == boost::system::errc::not_supported)
				error_code = SOCKS5_COMMAND_NOT_SUPPORTED;
			else if (ec)
				error_code = SOCKS5_GENERAL_SOCKS_SERVER_FAILURE;

			if (ec)
			{
				log_conn_warning()
					<< ", connect to target error: "
					<< ec.message();
			}

			//  +----+-----+-------+------+----------+----------+
			//  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
			//  +----+-----+-------+------+----------+----------+
			//  | 1  |  1  | X'00' |  1   | Variable |    2     |
			//  +----+-----+-------+------+----------+----------+
			//  [                                               ]

			wbuf.consume(wbuf.size());
			auto wp = (char*)wbuf.prepare(64 + domain.size()).data();

			write<uint8_t>(SOCKS_VERSION_5, wp); // VER
			write<uint8_t>(error_code, wp);		// REP
			write<uint8_t>(0x00, wp);			// RSV

			if (dst_endpoint.address().is_v4())
			{
				auto uaddr = dst_endpoint.address().to_v4().to_uint();

				write<uint8_t>(SOCKS5_ATYP_IPV4, wp);
				write<uint32_t>(uaddr, wp);
				write<uint16_t>(dst_endpoint.port(), wp);
			}
			else if (dst_endpoint.address().is_v6())
			{
				write<uint8_t>(SOCKS5_ATYP_IPV6, wp);
				auto data = dst_endpoint.address().to_v6().to_bytes();
				for (auto c : data)
					write<uint8_t>(c, wp);
				write<uint16_t>(dst_endpoint.port(), wp);
			}
			else if (!domain.empty())
			{
				write<uint8_t>(SOCKS5_ATYP_DOMAINNAME, wp);
				write<uint8_t>(static_cast<int8_t>(domain.size()), wp);
				std::copy(domain.begin(), domain.end(), wp);
				wp += domain.size();
				write<uint16_t>(port, wp);
			}
			else
			{
				write<uint8_t>(0x1, wp);
				write<uint32_t>(0, wp);
				write<uint16_t>(0, wp);
			}

			auto len = wp - (const char*)wbuf.data().data();
			wbuf.commit(len);
			bytes = co_await net::async_write(m_local_socket,
				wbuf,
				net::transfer_exactly(len),
				net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", write server response error: "
					<< ec.message();
				co_return;
			}

			if (error_code != SOCKS5_SUCCEEDED)
				co_return;
		}

		log_conn_debug()
			<< ", connected start transfer";

		// 发起数据传输协程.
		if (command == SOCKS_CMD_CONNECT)
		{
			co_await concurrent_transfer();
		}
		else
		{
			log_conn_warning()
				<< ", SOCKS_CMD_BIND and SOCKS5_CMD_UDP is unsupported";
		}

		co_return;
	}

	//////////////////////////////////////////////////////////////////////////
	// UDP 转发

	net::awaitable<void> proxy_session::forward_udp(udp::socket& client, udp::socket& server) noexcept
	{
		[[maybe_unused]] auto self = shared_from_this();

		boost::system::error_code ec;

		udp::endpoint remote_endp;
		udp::endpoint final_remote_endp;
		std::string latest_domain;

		// SOCKS5 UDP 最大头部长度: RSV(2) + FRAG(1) + ATYP(1) + 地址(最大255) + PORT(2)
		static constexpr size_t udp_max_header_size = 261;

		char read_buffer[4096];
		size_t send_total = 0;
		size_t recv_total = 0;

		const char* rbuf = &read_buffer[udp_max_header_size];

		while (!m_abort)
		{
			auto bytes = co_await client.async_receive_from(
				net::buffer((char*)rbuf, sizeof(read_buffer) - udp_max_header_size),
				remote_endp,
				net_awaitable[ec]);
			if (ec)
				break;

			// 桥接代理模式下, 直接转发数据包到桥接服务器.
			if (m_proxy_pass)
			{
				if (remote_endp.address() == m_local_udp_endpoint.address())
				{
					m_local_udp_endpoint = remote_endp;

					send_total++;
					co_await server.async_send_to(
						net::buffer(rbuf, bytes),
						m_remote_udp_endpoint,
						net_awaitable[ec]);
				}
				else if (remote_endp.address() == m_remote_udp_endpoint.address())
				{
					recv_total++;
					co_await server.async_send_to(
						net::buffer(rbuf, bytes),
						m_local_udp_endpoint,
						net_awaitable[ec]);
				}

				continue;
			}

			auto rp = rbuf;

			// 如果数据包来自 socks 客户端, 则解析数据包并将数据转发给目标主机.
			if (remote_endp.address() == m_local_udp_endpoint.address())
			{
				m_local_udp_endpoint = remote_endp;

				//  +----+------+------+----------+-----------+----------+
				//  |RSV | FRAG | ATYP | DST.ADDR | DST.PORT  |   DATA   |
				//  +----+------+------+----------+-----------+----------+
				//  | 2  |  1   |  1   | Variable |    2      | Variable |
				//  +----+------+------+----------+-----------+----------+

				// 去掉包头转发至远程主机.
				read<uint16_t>(rp); // rsv
				auto frag = read<uint8_t>(rp);  // frag

				// 不支持udp分片.
				if (frag != 0)
					continue;

				auto atyp = read<uint8_t>(rp);

				if (atyp == SOCKS5_ATYP_IPV4)
				{
					final_remote_endp.address(
						net::ip::address_v4(read<uint32_t>(rp)));
					final_remote_endp.port(read<uint16_t>(rp));
				}
				else if (atyp == SOCKS5_ATYP_DOMAINNAME)
				{
					auto length = read<uint8_t>(rp);
					std::string domain;

					for (size_t i = 0; i < length; i++)
						domain.push_back(read<int8_t>(rp));
					auto port = read<uint16_t>(rp);

					if (latest_domain != domain)
					{
						auto executor = co_await switch_to_backend_executor();

						udp::resolver resolver{ executor };

						auto targets =
							co_await resolver.async_resolve(
								domain,
								std::to_string(port),
								net_awaitable[ec]);

						co_await switch_from_backend_executor();

						if (ec)
							break;

						if (targets.empty())
							continue;

						final_remote_endp = targets.begin()->endpoint();
						latest_domain = domain;
					}
				}
				else if (atyp == SOCKS5_ATYP_IPV6)
				{
					net::ip::address_v6::bytes_type addr;
					for (auto i = addr.begin();
						i != addr.end(); ++i)
					{
						*i = read<int8_t>(rp);
					}

					final_remote_endp.address(net::ip::address_v6(addr));
					final_remote_endp.port(read<uint16_t>(rp));
				}
				else
				{
					continue;
				}

				auto head_size = rp - rbuf;
				auto udp_size = bytes - head_size;

				send_total++;

				co_await server.async_send_to(
					net::buffer(rp, udp_size),
					final_remote_endp,
					net_awaitable[ec]);
			}
			else // 如果数据包来自远程主机, 则解析数据包并将数据转发给 socks 客户端.
			{
				// 6 + 4 表示 socks5 udp 头部长度, 6 是 (RSV + FRAG + ATYP + DST.PORT)
				// 这部分的固定长度, 4 是 DST.ADDR 的长度.
				auto head_size = 6 + (remote_endp.address().is_v6() ? 16 : 4);
				auto udp_size = bytes + head_size;

				// 在数据包前面添加 socks5 udp 头部, 然后转发给 socks 客户端.
				char* wp = (char*)(rbuf - head_size);

				write<uint16_t>(0x0, wp); // rsv
				write<uint8_t>(0x0, wp); // frag

				if (remote_endp.address().is_v4())
				{
					auto uaddr = remote_endp.address().to_v4().to_uint();
					write<uint8_t>(SOCKS5_ATYP_IPV4, wp); // atyp

					write<uint32_t>(uaddr, wp);
					write<uint16_t>(remote_endp.port(), wp);
				}
				if (remote_endp.address().is_v6())
				{
					write<uint8_t>(SOCKS5_ATYP_IPV6, wp); // atyp

					auto data = remote_endp.address().to_v6().to_bytes();
					for (auto c : data)
						write<uint8_t>(c, wp);
					write<uint16_t>(remote_endp.port(), wp);
				}

				recv_total++;

				// 指向 udp header 位置开始发送数据.
				auto wbuf = rbuf - head_size;

				co_await server.async_send_to(
					net::buffer(wbuf, udp_size),
					m_local_udp_endpoint,
					net_awaitable[ec]);
			}
		}

		log_conn_debug()
			<< ", recv total: "
			<< recv_total
			<< ", send total: "
			<< send_total
			<< ", forward_udp quit";

		co_return;
	}

	net::awaitable<void> proxy_session::socks_connect_v4() noexcept
	{
		auto self = shared_from_this();
		auto p = (const char*)m_local_buffer.data().data();

		[[maybe_unused]] auto socks_version = read<int8_t>(p);
		BOOST_ASSERT(socks_version == SOCKS_VERSION_4);
		auto command = read<int8_t>(p);

		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//  | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//  | 1  | 1  |    2    |         4         | variable     | 1  |
		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//            [                             ]
		m_local_buffer.consume(m_local_buffer.size());
		boost::system::error_code ec;
		auto bytes = co_await net::async_read(m_local_socket,
			m_local_buffer,
			net::transfer_exactly(6),
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", read socks4 dst: "
				<< ec.message();
			co_return;
		}

		tcp::endpoint dst_endpoint;
		p = (const char*)m_local_buffer.data().data();

		auto port = read<uint16_t>(p);
		dst_endpoint.port(port);
		dst_endpoint.address(net::ip::address_v4(read<uint32_t>(p)));

		bool socks4a = false;
		auto ip_bytes = dst_endpoint.address().to_v4().to_bytes();
		if (ip_bytes[0] == 0 && ip_bytes[1] == 0 && ip_bytes[2] == 0 && ip_bytes[3] != 0)
			socks4a = true;

		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//  | VN | CD | DSTPORT |      DSTIP        | USERID       |NULL|
		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//  | 1  | 1  |    2    |         4         | variable     | 1  |
		//  +----+----+----+----+----+----+----+----+----+----+....+----+
		//                                          [                   ]
		m_local_buffer.consume(m_local_buffer.size());
		bytes = co_await net::async_read_until(m_local_socket,
			m_local_buffer, '\0', net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", read socks4 userid: "
				<< ec.message();
			co_return;
		}

		std::string userid;
		if (bytes > 1)
		{
			userid.resize(bytes - 1);
			m_local_buffer.sgetn(&userid[0], bytes - 1);
		}
		m_local_buffer.consume(1); // consume `null`

		std::string hostname;
		if (socks4a)
		{
			bytes = co_await net::async_read_until(m_local_socket,
				m_local_buffer, '\0', net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", read socks4a hostname: "
					<< ec.message();
				co_return;
			}

			if (bytes > 1)
			{
				hostname.resize(bytes - 1);
				m_local_buffer.sgetn(&hostname[0], bytes - 1);
			}
		}

		log_conn_debug()
			<< ", use "
			<< (socks4a ? "domain: " : "ip: ")
			<< (socks4a ? hostname : dst_endpoint.address().to_string());

		// 用户认证逻辑, 如果用户认证列表为空, 则表示不需要用户认证.
		bool verify_passed = co_await check_userpasswd(userid, "", true);

		if (auth_required())
		{
			if (verify_passed)
				log_conn_debug() << ", socks4 userid: " << userid << ", user authenticated successfully";
			else
				log_conn_warning() << ", socks4 userid: " << userid << ", user authentication failed";
		}

		if (!verify_passed)
		{
			//  +----+----+----+----+----+----+----+----+
			//  | VN | CD | DSTPORT |      DSTIP        |
			//  +----+----+----+----+----+----+----+----+
			//  | 1  | 1  |    2    |         4         |
			//  +----+----+----+----+----+----+----+----+
			//  [                                       ]

			net::streambuf wbuf;
			auto wp = (char*)wbuf.prepare(16).data();

			write<uint8_t>(0, wp);
			write<uint8_t>(SOCKS4_REQUEST_REJECTED_USER_NO_ALLOW, wp);

			write<uint16_t>(dst_endpoint.port(), wp);
			write<uint32_t>(dst_endpoint.address().to_v4().to_uint(), wp);

			wbuf.commit(8);
			bytes = co_await net::async_write(m_local_socket,
				wbuf,
				net::transfer_exactly(8),
				net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", write socks4 no allow: "
					<< ec.message();
				co_return;
			}

			log_conn_warning()
				<< ", socks4 "
				<< userid
				<< " auth fail";
			co_return;
		}

		int error_code = SOCKS4_REQUEST_GRANTED;
		if (command == SOCKS_CMD_CONNECT)
		{
			if (socks4a)
				ec = co_await start_connect_host(
					hostname,
					port,
					false);
			else
				ec = co_await start_connect_host(
					dst_endpoint.address().to_string(),
					port,
					true);
			if (ec)
			{
				log_conn_warning()
					<< ", connect to target "
					<< dst_endpoint.address().to_string()
					<< ":"
					<< port
					<< " error: "
					<< ec.message();
				error_code = SOCKS4_CANNOT_CONNECT_TARGET_SERVER;
			}
		}
		else
		{
			error_code = SOCKS4_REQUEST_REJECTED_OR_FAILED;
			XLOG_FWARN("connection id: {},"
				" unsupported command for socks4",
				m_connection_id);
		}

		//  +----+----+----+----+----+----+----+----+
		//  | VN | CD | DSTPORT |      DSTIP        |
		//  +----+----+----+----+----+----+----+----+
		//  | 1  | 1  |    2    |         4         |
		//  +----+----+----+----+----+----+----+----+
		//  [                                       ]

		net::streambuf wbuf;
		auto wp = (char*)wbuf.prepare(16).data();

		write<uint8_t>(0, wp);
		write<uint8_t>((uint8_t)error_code, wp);

		// 返回IP:PORT.
		write<uint16_t>(dst_endpoint.port(), wp);
		write<uint32_t>(dst_endpoint.address().to_v4().to_uint(), wp);

		wbuf.commit(8);
		bytes = co_await net::async_write(m_local_socket,
			wbuf,
			net::transfer_exactly(8),
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", write socks4 response: "
				<< ec.message();
			co_return;
		}

		if (error_code != SOCKS4_REQUEST_GRANTED)
			co_return;

		co_await concurrent_transfer();

		co_return;
	}

	net::awaitable<int> proxy_session::http_authorization(std::string_view pa) noexcept
	{
		if (!auth_required())
			co_return PROXY_AUTH_SUCCESS;

		if (pa.empty())
			co_return PROXY_AUTH_NONE;

		auto pos = pa.find(' ');
		if (pos == std::string::npos)
			co_return PROXY_AUTH_ILLEGAL;

		auto type = pa.substr(0, pos);
		auto auth = pa.substr(pos + 1);

		if (type != "Basic")
			co_return PROXY_AUTH_ILLEGAL;

		std::string userinfo(
			beast::detail::base64::decoded_size(auth.size()), 0);
		auto [len, _] = beast::detail::base64::decode(
			(char*)userinfo.data(),
			auth.data(),
			auth.size());
		userinfo.resize(len);

		pos = userinfo.find(':');

		std::string uname = userinfo.substr(0, pos);
		std::string passwd = userinfo.substr(pos + 1);

		bool verify_passed = co_await check_userpasswd(uname, passwd);
		if (!verify_passed)
			co_return PROXY_AUTH_FAILED;

		co_return PROXY_AUTH_SUCCESS;
	}

	net::awaitable<bool> proxy_session::http_proxy_get() noexcept
	{
		boost::system::error_code ec;
		bool keep_alive = false;
		std::optional<request_parser> parser;
		bool first = true;

		while (!m_abort)
		{
			parser.emplace();
			parser->body_limit(1024 * 1024 * 10);
			if (!first)
				m_local_buffer.consume(m_local_buffer.size());

			// 读取 http 请求头.
			co_await http::async_read(
				m_local_socket,
				m_local_buffer,
				*parser,
				net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< (keep_alive ? ", keepalive" : "")
					<< ", http_proxy_get request async_read: "
					<< ec.message();

				co_return !first;
			}

			auto req = parser->release();
			auto mth = std::string(req.method_string());
			auto target_view = std::string(req.target());
			auto pa = std::string(req[http::field::proxy_authorization]);

			keep_alive = req.keep_alive();

			log_conn_debug()
				<< ", method: " << mth
				<< ", target: " << target_view
				<< (pa.empty() ? std::string()
					: ", proxy_authorization: " + pa);

			// 判定是否为 GET url 代理模式.
			bool get_url_proxy = false;
			boost::system::result<url_info> expect_url;

			if (boost::istarts_with(target_view, "https://") ||
				boost::istarts_with(target_view, "http://"))
			{
				get_url_proxy = true;
				expect_url = parse_urlinfo(target_view);
			}

			// http 代理认证, 如果请求的 rarget 不是 http url 或认证
			// 失败, 则按正常 web 请求处理.
			auto auth_result = co_await http_authorization(pa);
			if (auth_result != PROXY_AUTH_SUCCESS || !get_url_proxy)
			{
				// 如果 doc 目录为空, 则不允许访问目录
				// 这里直接返回错误页面.
				if (m_option.doc_directory_.empty())
					co_return !first;

				// htpasswd 表示需要用户认证.
				if (m_option.htpasswd_)
				{
					// 处理 http 认证, 如果客户没有传递认证信息, 则返回 401.
					// 如果用户认证信息没有设置, 则直接返回 401.
					if (pa.empty() || auth_required())
					{
						log_conn_warning()
							<< ", auth error: "
							<< (pa.empty() ? "no auth" : "no user");

						co_await unauthorized_http_route(req);
						co_return true;
					}

					if (auth_result != PROXY_AUTH_SUCCESS)
					{
						log_conn_warning()
							<< ", auth error: "
							<< pauth_error_message(auth_result);

						co_await unauthorized_http_route(req);
						co_return true;
					}
				}

				// 如果不允许目录索引, 检查请求的是否为文件, 如果是具体文件则按文
				// 件请求处理, 否则返回 403.
				if (!m_option.autoindex_)
				{
					auto path = make_real_target_path(m_option.doc_directory_, req.target());

					if (!fs::is_directory(path, ec))
					{
						co_await normal_web_server(req, parser);
						co_return true;
					}

					// 如果不允许目录索引, 则直接返回 403 forbidden.
					co_await forbidden_http_route(req);

					co_return true;
				}

				// 按正常 http 目录请求来处理.
				co_await normal_web_server(req, parser);
				co_return true;
			}

			// 检查 url 是否解析成功.
			if (expect_url.has_error())
			{
				log_conn_warning()
					<< ", parse url: " << target_view
					<< ", error: " << expect_url.error().message();

				co_return false;
			}

			// 解构 url 中的信息.
			auto [scheme, user, passwd, host, port, resource] = *expect_url;

			bool http_connect_udp = false;

			// RFC 9298 CONNECT-UDP 检测: 检查是否为 connect-udp upgrade 请求.
			{
				auto upgrade = req.find("Upgrade");
				auto capsule_proto = req.find("Capsule-Protocol");
				auto connection = req.find("Connection");

				if (mth == "GET" &&
					upgrade != req.end() &&
					capsule_proto != req.end() &&
					connection != req.end())
				{
					std::string upgrade_val = std::string(upgrade->value());
					std::string conn_val = std::string(connection->value());
					std::string cap_val = std::string(capsule_proto->value());

					if (boost::iequals(upgrade_val, "connect-udp") &&
						boost::icontains(conn_val, "upgrade") &&
						cap_val == "?1")
					{
						http_connect_udp = true;
					}
				}
			}

			if (http_connect_udp)
			{
				// 从 URI path 中解析 target_host 和 target_port.
				std::string target_host;
				uint16_t target_port = 0;

				// 格式: /.well-known/masque/udp/{target_host}/{target_port}/
				if (!parse_connect_udp_target(resource, target_host, target_port))
				{
					log_conn_warning()
						<< ", connect-udp: invalid target from path: "
						<< resource;

					co_return false;
				}

				log_conn_debug()
					<< ", connect-udp request for target: "
					<< target_host << ":" << target_port;

				// 处理 connect-udp 请求.
				co_await http_proxy_connect_udp(req, target_host, target_port);

				// 完成 connect-udp 请求后返回.
				co_return !first;
			}

			if (!m_remote_socket.is_open())
			{
				// 连接到目标主机.
				ec = co_await start_connect_host(std::string(host), port, false);
				if (ec)
				{
					log_conn_warning()
						<< ", connect to target "
						<< host
						<< ":"
						<< port
						<< " error: "
						<< ec.message();

					co_return true;
				}
			}

			// 处理代理请求头.
			if (resource.empty())
				req.target("/");
			else
				req.target(std::string(resource));

			req.set(http::field::host, host);

			if (req.find(http::field::connection) == req.end() &&
				req.find(http::field::proxy_connection) != req.end())
				req.set(http::field::connection, req[http::field::proxy_connection]);

			req.erase(http::field::proxy_authorization);
			req.erase(http::field::proxy_connection);

			co_await http::async_write(
				m_remote_socket, req, net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", http_proxy_get request async_write: "
					<< ec.message();
				co_return !first;
			}

			m_local_buffer.consume(m_local_buffer.size());
			beast::flat_buffer buf;

			http::response_parser<http::string_body> parser;
			parser.body_limit(1024 * 1024 * 10);

			auto bytes = co_await http::async_read(
				m_remote_socket, buf, parser, net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", http_proxy_get response async_read: "
					<< ec.message();
				co_return !first;
			}

			co_await http::async_write(
				m_local_socket, parser.release(), net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", http_proxy_get response async_write: "
					<< ec.message();
				co_return !first;
			}

			log_conn_debug()
				<< ", transfer completed"
				<< ", remote to local: "
				<< bytes;

			first = false;

			if (!keep_alive)
			{
				if (m_local_socket.is_open())
					co_await async_shutdown(m_local_socket, net_awaitable[ec]);
				if (m_remote_socket.is_open())
					co_await async_shutdown(m_remote_socket, net_awaitable[ec]);

				break;
			}
		}

		co_return true;
	}

	net::awaitable<bool> proxy_session::http_proxy_connect() noexcept
	{
		boost::system::error_code ec;
		http::request_parser<http::string_body> parser;

		parser.header_limit(128 * 1024);

		// 读取 http 请求头.
		co_await http::async_read_header(m_local_socket,
			m_local_buffer, parser, net_awaitable[ec]);
		if (ec)
		{
			log_conn_error()
				<< ", http_proxy_connect async_read: "
				<< ec.message();

			co_return false;
		}

		http::request<http::string_body> req = parser.release();

		auto mth = std::string(req.method_string());
		auto target_view = std::string(req.target());
		auto pa = std::string(req[http::field::proxy_authorization]);

		std::string padding_value;
		std::size_t padding_length = 0;

		// 如果有 padding 头, 则回复一个随机长度的 padding 头, 以迷惑扫描者.
		if (req.find("Proxy-Padding") != req.end())
		{
			auto noise_length = req["Proxy-Padding"].size();
			padding_length = noise_length;

			std::mt19937 gen(global_random_device()());
			std::uniform_int_distribution<std::size_t> dis(16, std::max<std::size_t>(16, noise_length));
			noise_length = dis(gen);

			auto noise = generate_noise(global_random_device(), static_cast<uint16_t>(noise_length));
			auto len = boost::beast::detail::base64::encoded_size(noise_length);
			padding_value.resize(len);
			len = beast::detail::base64::encode(padding_value.data(), noise.data(), noise.size());
			padding_value.resize(len);
		}

		log_conn_debug()
			<< ", method: " << mth
			<< ", target: " << target_view
			<< (pa.empty() ? std::string()
				: ", proxy_authorization: " + pa)
			<< (padding_length > 0 ?
				", in padding: " + std::to_string(padding_length)
				: std::string())
			<< (padding_value.empty() ? std::string() :
				", out padding: " + std::to_string(padding_value.size()));

		// http 代理认证.
		auto auth = co_await http_authorization(pa);
		if (auth != PROXY_AUTH_SUCCESS)
		{
			log_conn_warning()
				<< ", proxy auth error: "
				<< pauth_error_message(auth);

			auto date_string = server_date_string();
			auto fake_page = fmt::vformat(fake_407_content_fmt,
				fmt::make_format_args(date_string));

			co_await net::async_write(
				m_local_socket,
				net::buffer(fake_page),
				net::transfer_all(),
				net_awaitable[ec]);

			co_return true;
		}

		auto pos = target_view.find(':');
		if (pos == std::string::npos)
		{
			log_conn_error()
				<< ", illegal target: "
				<< target_view;
			co_return false;
		}

		std::string host(target_view.substr(0, pos));
		std::string port(target_view.substr(pos + 1));

		ec = co_await start_connect_host(host, static_cast<uint16_t>(std::atol(port.c_str())), false);
		if (ec)
		{
			log_conn_warning()
				<< ", connect to target "
				<< host
				<< ":"
				<< port
				<< " error: "
				<< ec.message();

			co_return true;
		}

		http::response<http::empty_body> res{
			http::status::ok, req.version() };
		res.reason("Connection established");

		if (!padding_value.empty())
			res.insert("Proxy-Padding", padding_value);

		co_await http::async_write(
			m_local_socket,
			res,
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", write http connect response "
				<< host
				<< ":"
				<< port
				<< " error: "
				<< ec.message();

			co_return false;
		}

		co_await concurrent_transfer();

		co_return true;
	}

	net::awaitable<void> proxy_session::http_proxy_connect_udp(
		const http::request<http::string_body>& req,
		const std::string& target_host,
		uint16_t target_port) noexcept
	{
		boost::system::error_code ec;

		// 如果指定了 proxy_pass（有下级代理服务器）.
		if (m_proxy_pass)
		{
			co_await http_proxy_udp_proxy_pass(req, target_host, target_port);
			co_return;
		}

		// 以下为终端代理场景: 无 proxy_pass, 直接与目标 UDP 服务器通信.
		log_conn_debug()
			<< ", connect-udp: final proxy, creating UDP socket to "
			<< target_host << ":" << target_port;

		// 解析目标主机 (如果是域名需要 DNS 解析).
		auto protocol = udp::v4();
		net::ip::address target_addr;

		// 尝试将目标主机解析为 IP 地址, 若发生错误, 则说明是域名需要通过 DNS 解析.
		auto addr = net::ip::make_address(target_host, ec);
		if (ec)
		{
			co_await switch_to_backend_executor();

			udp::resolver resolver(m_backend_context);
			auto results = co_await resolver.async_resolve(
				target_host, std::to_string(target_port), net_awaitable[ec]);

			co_await switch_from_backend_executor();

			if (ec || results.empty())
			{
				log_conn_warning()
					<< ", connect-udp: DNS resolve failed for "
					<< target_host << ": " << ec.message();

				http::response<http::string_body> res{
					http::status::bad_gateway,
					req.version() };

				res.set(http::field::content_type, "text/html");
				res.set("Proxy-Status", "dns_error");
				res.body() = "<html><body><h1>502 Bad Gateway</h1>"
					"<p>DNS resolution failed.</p></body></html>";
				res.prepare_payload();

				co_await http::async_write(
					m_local_socket, res, net_awaitable[ec]);

				co_return;
			}

			// 解析成功, 从结果中提取地址, 只取第一个地址即可.
			target_addr = results.begin()->endpoint().address();
			protocol = target_addr.is_v4() ? udp::v4() : udp::v6();
		}
		else
		{
			target_addr = addr;
			protocol = target_addr.is_v4() ? udp::v4() : udp::v6();
		}

		// 创建并连接 UDP socket, 专为目标地址的 udp socket 服务.
		udp::socket udp_socket(m_executor);
		udp_socket.open(protocol, ec);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: open UDP socket: " << ec.message();
			co_return;
		}

		// 如果有绑定接口, 则绑定到指定接口.
		if (m_bind_interface)
		{
			udp::endpoint bind_endpoint(*m_bind_interface, 0);
			udp_socket.bind(bind_endpoint, ec);
			if (ec)
			{
				log_conn_warning()
					<< ", connect-udp: bind UDP socket: " << ec.message();
				co_return;
			}
		}

		// 设置 SO_MARK.
		if (m_option.so_mark_)
		{
			auto ret = set_socket_mark(
				udp_socket.native_handle(), m_option.so_mark_.value());
			if (ret.has_error())
			{
				log_conn_warning()
					<< ", connect-udp: set socket mark: "
					<< ret.error().message();
			}
		}

		// "连接" UDP socket 到目标地址.
		udp::endpoint target_endpoint(target_addr, target_port);
		udp_socket.connect(target_endpoint, ec);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: connect UDP socket to "
				<< target_host << ":" << target_port
				<< " error: " << ec.message();
			co_return;
		}

		log_conn_debug()
			<< ", connect-udp: UDP socket connected to "
			<< target_endpoint;

		// 发送 101 Switching Protocols 响应.
		{
			http::response<http::empty_body> res;
			res.version(11);
			res.result(http::status::switching_protocols);
			res.set(http::field::connection, "Upgrade");
			res.set(http::field::upgrade, "connect-udp");
			res.set("Capsule-Protocol", "?1");

			co_await http::async_write(
				m_local_socket, res, net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", connect-udp: send 101 response: "
					<< ec.message();
				co_return;
			}
		}

		log_conn_debug()
			<< ", connect-udp: 101 sent, starting capsule/UDP forwarding";

		// 开始 capsule ↔ UDP 双向转发.
		auto self = shared_from_this();

		// HTTP → UDP 转发协程: 从 HTTP 流读取 capsule, 解析出 UDP 数据并发送到 UDP socket.
		auto http_to_udp = [this, self, &udp_socket]
			() mutable -> net::awaitable<void>
		{
			boost::system::error_code ec;

			while (!m_abort)
			{
				// 读取 capsule type.
				auto capsule_type = co_await read_varint_from_stream(m_local_socket, ec);
				if (ec)
				{
					log_conn_warning()
						<< ", connect-udp: read capsule type error: "
						<< ec.message();
					break;
				}

				// 读取 capsule length.
				auto capsule_length = co_await read_varint_from_stream(m_local_socket, ec);
				if (ec)
				{
					log_conn_warning()
						<< ", connect-udp: read capsule length error: "
						<< ec.message();
					break;
				}

				// 限制最大 capsule 大小.
				if (capsule_length > 65535)
				{
					log_conn_warning()
						<< ", connect-udp: capsule too large: "
						<< capsule_length;
					break;
				}

				// 读取 capsule value.
				std::string capsule_value(static_cast<size_t>(capsule_length), '\0');
				if (capsule_length > 0)
				{
					co_await net::async_read(
						m_local_socket,
						net::buffer(capsule_value),
						net_awaitable[ec]);
					if (ec)
					{
						log_conn_warning()
							<< ", connect-udp: read capsule value error: "
							<< ec.message();
						break;
					}
				}

				// 仅处理 DATAGRAM capsule 类型 (RFC 9297).
				if (capsule_type != udp_proxy_capsule_type)
				{
					log_conn_debug()
						<< ", connect-udp: unknown capsule type: "
						<< capsule_type
						<< ", skipping "
						<< capsule_length << " bytes";
					continue;
				}

				// 解析 context ID (应该为 0).
				auto data = reinterpret_cast<const uint8_t*>(capsule_value.data());
				size_t data_len = capsule_value.size();

				if (data_len == 0)
					continue;

				auto [ctx_id_len, ctx_id] = varint_int_decode(data);
				if (ctx_id != 0)
				{
					log_conn_debug()
						<< ", connect-udp: unknown context ID: "
						<< ctx_id
						<< ", skipping";
					continue;
				}

				// 剩余字节为 UDP payload.
				size_t udp_payload_len = data_len - ctx_id_len;
				if (udp_payload_len > 0)
				{
					co_await udp_socket.async_send(
						net::buffer(data + ctx_id_len, udp_payload_len),
						net_awaitable[ec]);
					if (ec)
					{
						log_conn_warning()
							<< ", connect-udp: UDP send error: "
							<< ec.message();
						break;
					}
				}
			}

			// 关闭连接.
			m_abort = true;

			udp_socket.close(ec);
			m_local_socket.close(ec);

			co_return;
		};

		// UDP → HTTP 转发协程: 从 UDP socket 读取数据, 封装成 capsule 发送到 HTTP 流.
		auto udp_to_http = [this, self, &udp_socket]
			() mutable -> net::awaitable<void>
		{
			boost::system::error_code ec;
			// 最大 UDP payload + capsule header (type + length + ctx_id).
			uint8_t buf[65536];

			while (!m_abort)
			{
				// 从 UDP socket 接收数据.
				auto bytes = co_await udp_socket.async_receive(
					net::buffer(buf + 32, 65527),
					net_awaitable[ec]);
				if (ec) break;

				// 构建 DATAGRAM capsule (RFC 9297).
				// Capsule type: DATAGRAM (0x00)
				// Capsule length: 1 + bytes (1 byte for context ID + UDP payload)
				// HTTP Datagram Payload (RFC 9298):
				//   Context ID: 0 (1 byte as varint)
				//   UDP payload

				// 计算 capsule header 部分和长度.
				auto capsule_length =
					varint_encoded_length(udp_proxy_capsule_type) +
					varint_encoded_length(1 + bytes) +
					varint_encoded_length(0);

				// 计算 capsule 起始位置.
				auto capsule_start = buf + 32 - capsule_length;
				size_t pos = 0;

				// 开始直接编码到 buf 中.
				pos += varint_int_encode(udp_proxy_capsule_type, capsule_start + pos);
				pos += varint_int_encode(1 + bytes, capsule_start + pos);
				pos += varint_int_encode(0, capsule_start + pos);

				co_await net::async_write(
					m_local_socket,
					net::buffer(capsule_start, capsule_length + bytes),
					net_awaitable[ec]);
				if (ec)
				{
					log_conn_warning()
						<< ", connect-udp: write capsule error: "
						<< ec.message();
					break;
				}
			}

			m_abort = true;

			udp_socket.close(ec);
			m_local_socket.close(ec);

			co_return;
		};

		// 并发运行 HTTP↔UDP 双向转发.
		co_await(
			http_to_udp()
			&&
			udp_to_http()
		);

		log_conn_debug()
			<< ", connect-udp: forwarding completed for "
			<< target_host << ":" << target_port;

		co_return;
	}

	net::awaitable<void> proxy_session::http_proxy_udp_proxy_pass(
		const http::request<http::string_body>& req,
		const std::string& target_host,
		uint16_t target_port
	)
	{
		boost::system::error_code ec;

		auto scheme = boost::to_lower_copy(std::string(m_proxy_pass->scheme()));

		// TODO: SOCKS proxy_pass 暂不支持, HTTP UDP 数据包转为 SOCKS UDP 转发太复杂.
		if (scheme.starts_with("socks"))
		{
			log_conn_warning()
				<< ", connect-udp: socks proxy_pass not supported";

			http::response<http::string_body> res{
				http::status::bad_gateway,
				req.version() };
			res.set(http::field::content_type, "text/html");
			res.body() = "<html><body><h1>502 Bad Gateway</h1>"
				"<p>connect-udp over socks proxy_pass not supported.</p>"
				"</body></html>";
			res.prepare_payload();

			co_await http::async_write(
				m_local_socket, res, net_awaitable[ec]);

			co_return;
		}

		// HTTP proxy_pass: 连接上游并转发原始请求.
		tcp::socket remote_socket(m_executor);
		auto targets = co_await resolve_proxy_pass_targets();

		ec = co_await connect_proxy_pass(remote_socket, targets);
		if (ec)
		{
			http::response<http::string_body> res{
				http::status::bad_gateway,
				req.version() };
			res.set(http::field::content_type, "text/html");
			res.body() = "<html><body><h1>502 Bad Gateway</h1>"
				"<p>Cannot connect to upstream proxy.</p>"
				"</body></html>";
			res.prepare_payload();

			co_await http::async_write(
				m_local_socket, res, net_awaitable[ec]);
			co_return;
		}

		// 将 m_remote_socket 初始化为 TCP 流类型.
		m_remote_socket = std::move(
			co_await instantiate_proxy_pass(
				std::move(remote_socket),
				m_option.proxy_pass_use_ssl_));

		// 发送原始请求头到上游.
		co_await http::async_write(
			m_remote_socket, req, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: forward request to upstream: "
				<< ec.message();
			co_return;
		}

		// 读取上游的响应.
		m_local_buffer.consume(m_local_buffer.size());
		beast::flat_buffer buf;

		http::response_parser<http::string_body> parser;
		parser.body_limit(1024 * 1024);

		co_await http::async_read(
			m_remote_socket, buf, parser, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: read upstream response: "
				<< ec.message();
			co_return;
		}

		auto upstream_res = parser.release();

		// 把上游响应转发给客户端.
		co_await http::async_write(
			m_local_socket, upstream_res, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", connect-udp: forward response to client: "
				<< ec.message();
			co_return;
		}

		// 如果上游返回 101, 则开始原始 TCP 双向转发 (上游负责 capsule ↔ UDP).
		if (upstream_res.result() == http::status::switching_protocols)
		{
			log_conn_debug()
				<< ", connect-udp: tunnel established via upstream, "
				<< "starting raw tcp forwarding";

			co_await concurrent_transfer();
		}

		co_return;
	}

	bool proxy_session::parse_connect_udp_target(std::string_view path,
		std::string& target_host, uint16_t& target_port)
	{
		if (path.empty() || path[0] != '/') return false;

		std::string_view r_host, r_port;

		// /.well-known/masque/udp/{target_host}/{target_port}/
		if (size_t pos = path.find("/.well-known/masque/udp/");
			pos != std::string_view::npos)
		{
			std::string_view sub = path.substr(pos + 24);
			size_t slash = sub.find('/');

			if (slash == std::string_view::npos || slash == 0)
				return false;

			r_host = sub.substr(0, slash);
			r_port = sub.substr(slash + 1);

			if (!r_port.empty() && r_port.back() == '/')
				r_port.remove_suffix(1);
		}
		// ?h=...&p=... 或者 ?target_host=...&target_port=...
		else if (size_t q_pos = path.find('?'); q_pos != std::string_view::npos)
		{
			std::string_view query = path.substr(q_pos + 1);
			size_t pos = 0;

			while (pos < query.size())
			{
				size_t next = query.find_first_of("&,", pos);
				std::string_view pair = query.substr(
					pos, next == std::string_view::npos ? next : next - pos);
				pos = (next == std::string_view::npos) ? next : next + 1;

				if (size_t eq = pair.find('='); eq != std::string_view::npos)
				{
					std::string_view k = pair.substr(0, eq);
					std::string_view v = pair.substr(eq + 1);

					if (k == "h" || k == "target_host")
						r_host = v;
					else if (k == "p" || k == "target_port")
						r_port = v;
				}
			}
		}

		if (r_host.empty() || r_port.empty())
			return false;

		// 解析端口
		auto [ptr, ec] = std::from_chars(
			r_port.data(), r_port.data() + r_port.size(), target_port);

		if (ec != std::errc{} || ptr != r_port.data() + r_port.size())
			return false;

		// 还原 Host, 处理 IPv6 中的 %3A 等情况.
		target_host.clear();
		target_host.reserve(r_host.size());

		for (size_t i = 0; i < r_host.size(); ++i)
		{
			if (r_host[i] == '%' && i + 2 < r_host.size())
			{
				char hex[3] = {r_host[i + 1], r_host[i + 2], 0};
				target_host += static_cast<char>(std::strtol(hex, nullptr, 16));
				i += 2;
			}
			else
			{
				target_host += r_host[i];
			}
		}

		return true;
	}

	net::awaitable<bool> proxy_session::socks_auth() noexcept
	{
		//  +----+------+----------+------+----------+
		//  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
		//  +----+------+----------+------+----------+
		//  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
		//  +----+------+----------+------+----------+
		//  [           ]

		boost::system::error_code ec;
		m_local_buffer.consume(m_local_buffer.size());
		auto bytes = co_await net::async_read(m_local_socket,
			m_local_buffer,
			net::transfer_exactly(2),
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", read client username/passwd error: "
				<< ec.message();
			co_return false;
		}

		auto p = (const char*)m_local_buffer.data().data();
		int auth_version = read<int8_t>(p);
		if (auth_version != 1)
		{
			log_conn_warning()
				<< ", socks negotiation, unsupported socks5 protocol";
			co_return false;
		}
		int name_length = read<uint8_t>(p);
		if (name_length < 0 || name_length > 255)
		{
			log_conn_warning()
				<< ", socks negotiation, invalid name length";
			co_return false;
		}
		name_length += 1;

		//  +----+------+----------+------+----------+
		//  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
		//  +----+------+----------+------+----------+
		//  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
		//  +----+------+----------+------+----------+
		//              [                 ]
		m_local_buffer.consume(m_local_buffer.size());
		bytes = co_await net::async_read(m_local_socket,
			m_local_buffer,
			net::transfer_exactly(name_length),
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", read client username error: "
				<< ec.message();
			co_return false;
		}

		std::string uname;

		p = (const char*)m_local_buffer.data().data();
		for (size_t i = 0; i < bytes - 1; i++)
			uname.push_back(read<int8_t>(p));

		int passwd_len = read<uint8_t>(p);
		if (passwd_len < 0 || passwd_len > 255)
		{
			log_conn_warning()
				<< ", socks negotiation, invalid passwd length";
			co_return false;
		}

		//  +----+------+----------+------+----------+
		//  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
		//  +----+------+----------+------+----------+
		//  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
		//  +----+------+----------+------+----------+
		//                                [          ]
		m_local_buffer.consume(m_local_buffer.size());
		bytes = co_await net::async_read(m_local_socket,
			m_local_buffer,
			net::transfer_exactly(passwd_len),
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", read client passwd error: "
				<< ec.message();
			co_return false;
		}

		std::string passwd;

		p = (const char*)m_local_buffer.data().data();
		for (size_t i = 0; i < bytes; i++)
			passwd.push_back(read<int8_t>(p));

		// SOCKS5验证用户和密码, 用户认证逻辑.
		bool verify_passed = co_await check_userpasswd(uname, passwd);

		if (auth_required())
		{
			if (verify_passed)
				log_conn_debug() << ", socks5 userid: " << uname << ", user authenticated successfully";
			else
				log_conn_warning() << ", socks5 userid: " << uname << ", user authentication failed";
		}

		net::streambuf wbuf{};

		auto wp = (char*)wbuf.prepare(16).data();
		write<uint8_t>(0x01, wp);			// version 只能是1.
		if (verify_passed)
		{
			write<uint8_t>(0x00, wp);		// 认证通过返回0x00, 其它值为失败.
		}
		else
		{
			write<uint8_t>(0x01, wp);		// 认证返回0x01为失败.
		}

		// 返回认证状态.
		//  +----+--------+
		//  |VER | STATUS |
		//  +----+--------+
		//  | 1  |   1    |
		//  +----+--------+
		wbuf.commit(2);
		co_await net::async_write(m_local_socket,
			wbuf,
			net::transfer_exactly(2),
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", server write status error: "
				<< ec.message();
			co_return false;
		}

		co_return verify_passed;
	}

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
			if (!co_await noise_handshake(remote_socket, m_outin_key, m_outout_key))
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
		if (scheme.ends_with("s"))
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
			opt.proxy_hostname = true;
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
					m_remote_udp_endpoint.address(remote_socket.remote_endpoint().address());
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
					net_awaitable[ec]);
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

	net::awaitable<boost::system::error_code> proxy_session::start_connect_host(
		std::string target_host,
		uint16_t target_port,
		bool resolved,
		int command) noexcept
	{
		tcp::socket& remote_socket = net_tcp_socket(m_remote_socket);
		tcp::resolver::results_type targets;
		boost::system::error_code ec;

		if (m_proxy_pass)
		{
			// 查询网关代理中继服务器域名信息.
			targets = co_await resolve_proxy_pass_targets();

			// 发起连接到网关代理中继服务器.
			ec = co_await connect_proxy_pass(remote_socket, targets);
			if (!ec)
			{
				// 与网关代理中继服务器握手.
				ec = co_await proxy_pass_handshake(remote_socket,
					target_host, target_port,
					*m_proxy_pass, command);
			}

			co_return ec;
		}
		else
		{
			switch (static_cast<int>(resolved))
			{
			case 1: // true
			{
				tcp::endpoint dst_endpoint;

				auto addr = net::ip::make_address(target_host, ec);
				if (!ec)
				{
					dst_endpoint.address(addr);
					dst_endpoint.port(target_port);

					targets = net::ip::basic_resolver_results<tcp>::create(dst_endpoint, "", "");
					break;
				}
			}
			[[fallthrough]];
			case 0:  // false
			default:
				targets = co_await resolve_targets(target_host, target_port);
				break;
			}
		}

		// 直接连接到目标机器.
		ec = co_await async_connect_targets(remote_socket, targets);
		if (ec)
			co_return ec;

		// 重新初始化 m_remote_socket 为 variant_stream_type 类型.
		m_remote_socket = init_proxy_stream(std::move(remote_socket));

		co_return ec;
	}

	//////////////////////////////////////////////////////////////////////////
	// HTTP 静态服务

	net::awaitable<void>
	proxy_session::normal_web_server(http::request<http::string_body>& req, std::optional<request_parser>& parser) noexcept
	{
		boost::system::error_code ec;

		bool keep_alive = false;
		bool has_read_header = true;

		for (; !m_abort;)
		{
			if (!has_read_header)
			{
				// normal_web_server 调用是从 http_proxy_get
				// 跳转过来的, 该函数已经读取了请求头, 所以第1次不需
				// 要再次读取请求头, 即 has_read_header 为 true.
				// 当 keepalive 时，需要读取请求头, 此时 has_read_header
				// 为 false, 则在此读取和解析后续的 http 请求头.
				parser.emplace();
				parser->body_limit(1024 * 512); // 512k
				m_local_buffer.consume(m_local_buffer.size());

				co_await http::async_read_header(
					m_local_socket,
					m_local_buffer,
					*parser,
					net_awaitable[ec]);
				if (ec)
				{
					log_conn_debug()
						<< (keep_alive ? ", keepalive" : "")
						<< ", web async_read_header: "
						<< ec.message();
					co_return;
				}

				req = parser->release();
			}

			if (req[http::field::expect] == "100-continue")
			{
				http::response<http::empty_body> res;
				res.version(11);
				res.result(http::status::method_not_allowed);

				co_await http::async_write(
					m_local_socket,
					res,
					net_awaitable[ec]);
				if (ec)
				{
					log_conn_debug()
						<< ", web expect async_write: "
						<< ec.message();
				}
				co_return;
			}

			has_read_header = false;
			keep_alive = req.keep_alive();

			if (beast::websocket::is_upgrade(req))
			{
				auto date_string = server_date_string();
				auto fake_page = fmt::vformat(fake_404_content_fmt,
					fmt::make_format_args(date_string));

				co_await net::async_write(
					m_local_socket,
					net::buffer(fake_page),
					net::transfer_all(),
					net_awaitable[ec]);

				co_return;
			}

			std::string target = req.target();
			boost::smatch what;
			http_context http_ctx{
				{},
				req,
				target,
				make_real_target_path(m_option.doc_directory_,req.target()) };

#define BEGIN_HTTP_ROUTE() if (false) {}
#define ON_HTTP_ROUTE(skip, exp, func) \
			else if (skip && boost::regex_match( \
				target, what, boost::regex{ exp })) { \
				for (auto i = 1; i < static_cast<int>(what.size()); i++) \
					http_ctx.command_.emplace_back(what[i]); \
				co_await func(http_ctx); \
			}
#define END_HTTP_ROUTE() else { \
				co_await default_http_route( \
					req, \
					fake_400_content, \
					http::status::bad_request ); }

			bool dns_proxy = m_option.dns_upstream_.has_value();

			BEGIN_HTTP_ROUTE()
				ON_HTTP_ROUTE(true, R"(^(.*)?\/$)", on_http_dir)
				ON_HTTP_ROUTE(true, R"(^(.*)?(\/\?r=json.*)$)", on_http_all_json)
				ON_HTTP_ROUTE(true, R"(^(.*)?(\/\?q=json.*)$)", on_http_json)
				ON_HTTP_ROUTE(dns_proxy, R"(^\/dns-query(.*)$)", on_http_dns_query)
				ON_HTTP_ROUTE(true, R"(^(?!.*\/$).*$)", on_http_get)
			END_HTTP_ROUTE()

			if (!keep_alive || !m_local_socket.is_open())
			{
				if (m_local_socket.is_open())
					co_await async_shutdown(m_local_socket, net_awaitable[ec]);
				break;
			}
		}

		co_await async_wait(m_local_socket, net_awaitable[ec]);

		co_return;
	}

	//////////////////////////////////////////////////////////////////////////
	// HTTP 路径工具 (static)

	fs::path proxy_session::path_cat(
		const std::wstring& doc, const std::wstring& target) noexcept
	{
		size_t start_pos = 0;
		for (auto& c : target)
		{
			if (!(c == L'/' || c == '\\'))
				break;

			start_pos++;
		}

		std::wstring_view sv;
		std::wstring slash = L"/";

		if (start_pos < target.size())
			sv = std::wstring_view(target.c_str() + start_pos);
#ifdef WIN32
		slash = L"\\";
		if (doc.back() == L'/' ||
			doc.back() == L'\\')
			slash = L"";
		return fs::path(doc + slash + std::wstring(sv));
#else
		if (doc.back() == L'/')
			slash = L"";
		return fs::path(
			boost::nowide::narrow(doc + slash + std::wstring(sv)));
#endif // WIN32
	}

	std::wstring proxy_session::make_target_path(const std::string& target) noexcept
	{
		try
		{
			auto result = urls::url(
				target.starts_with("/") ? target : "/" + target);

			return boost::nowide::widen(result.path());
		}
		catch (const std::exception&)
		{}

		return boost::nowide::widen(target);
	}

	fs::path proxy_session::make_real_target_path(const std::string& doc_directory,
		const std::string& target) noexcept
	{
		auto target_path = make_target_path(target);
		auto doc_path = boost::nowide::widen(doc_directory);

#ifdef WIN32
		auto ret = make_unc_path(path_cat(doc_path, target_path));
#else
		auto ret = path_cat(doc_path, target_path).string();
#endif

		return ret;
	}

	std::vector<std::wstring>
	proxy_session::format_path_list(const fs::path& path, boost::system::error_code& ec) const
	{
		fs::directory_iterator end;
		fs::directory_iterator it(path, fs::directory_options::skip_permission_denied, ec);
		if (ec)
		{
			log_conn_debug()
				<< ", format_path_list read dir: "
				<< path
				<< ", error: "
				<< ec.message();
			return {};
		}

		std::vector<std::wstring> path_list;
		std::vector<std::wstring> file_list;

		auto leaf_name = [this](const fs::path& p) -> std::string
		{
			static const std::codecvt_utf8<wchar_t> utf8_cvt;

			try {
				fs::path normalized = p.lexically_normal();
				return boost::nowide::narrow(normalized.filename().wstring(utf8_cvt));
			}
			catch (const std::exception& e)
			{
				log_conn_warning()
					<< ", exception in path normalization: "
					<< e.what();
			}

			return "";
		};

		for (; it != end && !m_abort; it++)
		{
			const auto& item = it->path();

			auto [ftime, unc_path] = file_last_write_time(item);
			std::wstring time_string = boost::nowide::widen(ftime);

			std::wstring rpath;

			if (fs::is_directory(unc_path.empty() ? item : unc_path, ec))
			{
				auto leaf = leaf_name(item);
				leaf = leaf + "/";
				rpath = boost::nowide::widen(leaf);
				int width = 50 - static_cast<int>(rpath.size());
				width = width < 0 ? 0 : width;
				std::wstring space(width, L' ');
				auto show_path = rpath;
				if (show_path.size() > 50) {
					show_path = show_path.substr(0, 47);
					show_path += L"..&gt;";
				}
				auto str = fmt::format(body_fmt,
					rpath,
					show_path,
					space,
					time_string,
					L"-");

				path_list.push_back(str);
			}
			else
			{
				auto leaf = leaf_name(item);
				rpath = boost::nowide::widen(leaf);
				int width = 50 - (int)rpath.size();
				width = width < 0 ? 0 : width;
				std::wstring space(width, L' ');
				std::wstring filesize;
				if (unc_path.empty())
					unc_path = item;
				auto sz = static_cast<float>(fs::file_size(
					unc_path, ec));
				if (ec)
					sz = 0;
				filesize = boost::nowide::widen(
					strutil::add_suffix(sz));
				auto show_path = rpath;
				if (show_path.size() > 50) {
					show_path = show_path.substr(0, 47);
					show_path += L"..&gt;";
				}
				auto str = fmt::format(body_fmt,
					rpath,
					show_path,
					space,
					time_string,
					filesize);

				file_list.push_back(str);
			}
		}

		ec = {};

		path_list.insert(path_list.end(),
			file_list.begin(), file_list.end());

		return path_list;
	}

	net::awaitable<void> proxy_session::on_http_dir(const http_context& hctx) noexcept
	{
		using namespace std::literals;

		boost::system::error_code ec;
		auto& request = hctx.request_;

		// 查找目录下是否存在 index.html 或 index.htm 文件, 如果存在则返回该文件.
		// 否则返回目录下的文件列表.
		auto index_html_path = hctx.target_path_ / "index.html";
		if (!fs::exists(index_html_path, ec))
			index_html_path = hctx.target_path_ / "index.htm";

		if (fs::exists(index_html_path, ec))
		{
			boost::nowide::fstream file(index_html_path, std::ios::in | std::ios::binary);
			if (file)
			{
				std::string content(
					(std::istreambuf_iterator<char>(file)),
					std::istreambuf_iterator<char>());

				string_response res{ http::status::ok, request.version() };
				res.set(http::field::server, version_string);
				res.set(http::field::date, server_date_string());
				auto ext = strutil::to_lower(index_html_path.extension().string());
				if (global_mimes.count(ext))
					res.set(http::field::content_type, global_mimes.at(ext));
				else
					res.set(http::field::content_type, "text/plain");
				res.keep_alive(request.keep_alive());
				res.body() = content;
				res.prepare_payload();

				http::serializer<false, string_body, http::fields> sr(res);
				co_await http::async_write(
					m_local_socket,
					sr,
					net_awaitable[ec]);
				if (ec)
				{
					log_conn_warning()
						<< ", http dir write index err: "
						<< ec.message();
				}

				co_return;
			}
		}

		auto path_list = format_path_list(hctx.target_path_, ec);
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

		auto target_path = make_target_path(hctx.target_);
		log_conn_debug() << ", httpd_dir access: " << target_path;

		std::wstring head = fmt::format(head_fmt,
			target_path,
			target_path);

		std::wstring body = fmt::format(body_fmt,
			L"../",
			L"../",
			L"",
			L"",
			L"");

		for (auto& s : path_list)
			body += s;
		body = head + body + tail_fmt;

		string_response res{ http::status::ok, request.version() };
		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string());
		res.keep_alive(request.keep_alive());
		res.body() = boost::nowide::narrow(body);
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

	net::awaitable<void> proxy_session::on_http_get(const http_context& hctx) noexcept
	{
		boost::system::error_code ec;

		const auto& request = hctx.request_;
		const fs::path& path = hctx.target_path_;

		if (!fs::exists(path, ec))
		{
			log_conn_warning()
				<< ", http "
				<< hctx.target_
				<< " file not exists";

			auto date_string = server_date_string();
			auto fake_page = fmt::vformat(fake_404_content_fmt,
				fmt::make_format_args(date_string));

			co_await net::async_write(
				m_local_socket,
				net::buffer(fake_page),
				net::transfer_all(),
				net_awaitable[ec]);

			co_return;
		}

		if (fs::is_directory(path, ec))
		{
			log_conn_debug()
				<< ", http "
				<< hctx.target_
				<< " is directory";

			try
			{
				urls::url url;
				if (is_crytpo_stream())
					url.set_scheme_id(urls::scheme::https);
				else
					url.set_scheme_id(urls::scheme::http);
				url.set_encoded_authority(request[http::field::host]);
				url.set_path(hctx.target_ + "/");

				co_await location_http_route(request, url.buffer());
			}
			catch (const std::exception& e)
			{
				log_conn_warning()
					<< ", http "
					<< hctx.target_
					<< " location error: "
					<< e.what();
			}

			co_return;
		}

		size_t content_length = fs::file_size(path, ec);
		if (ec)
		{
			log_conn_warning()
				<< ", http "
				<< hctx.target_
				<< " file size error: "
				<< ec.message();

			co_await default_http_route(
				request, fake_400_content, http::status::bad_request);

			co_return;
		}

#if defined (BOOST_ASIO_HAS_FILE)
# if defined(_WIN32)
		net::stream_file file(co_await net::this_coro::executor);

		file.assign(::CreateFileW(path.wstring().c_str(),
			GENERIC_READ, FILE_SHARE_READ, 0,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, 0), ec);
		if (ec)
		{
			log_conn_warning()
				<< ", http "
				<< hctx.target_
				<< " open file error: "
				<< ec.message();

			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);

			co_return;
		}
# else
		net::stream_file file(co_await net::this_coro::executor, path.string(), net::stream_file::read_only);
# endif
#else
		boost::nowide::fstream file(path, std::ios_base::binary | std::ios_base::in);
#endif

		std::string user_agent;
		if (request.count(http::field::user_agent))
			user_agent = std::string(request[http::field::user_agent]);

		std::string referer;
		if (request.count(http::field::referer))
			referer = std::string(request[http::field::referer]);

		// 解析 http 协议中的 range 部分.
		auto range = parser_http_ranges(request["Range"]);

		// 计算 range 得到偏移位置.
		auto [http_range_start, http_range_end, st] = offset_from_range(range, content_length);

		log_conn_debug()
			<< ", http file: "
			<< hctx.target_
			<< ", size: "
			<< content_length
			<< (request.count("Range") ?
				", range: " + std::string(request["Range"])
				+ ", range size: " + std::to_string(http_range_end - http_range_start + 1)
				: std::string())
			<< (!user_agent.empty() ?
				", user_agent: " + user_agent
				: std::string())
			<< (!referer.empty() ?
				", referer: " + referer
				: std::string());

		// 超出范围之外，返回 416 错误.
		if (st == http::status::range_not_satisfiable ||
			(http_range_end < http_range_start && http_range_end >= 0))
		{
			co_await default_http_route(request,
				fake_416_content,
				http::status::range_not_satisfiable);
			co_return;
		}

		if (st == http::status::partial_content)
		{
#if defined (BOOST_ASIO_HAS_FILE)
			file.seek(http_range_start, net::stream_file::seek_set);
#else
			file.seekg(http_range_start, std::ios_base::beg);
#endif
		}

		buffer_response res{ st, request.version() };

		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string());

		auto ext = strutil::to_lower(path.extension().string());

		if (global_mimes.count(ext))
			res.set(http::field::content_type, global_mimes.at(ext));
		else
			res.set(http::field::content_type, "text/plain");

		if (st == http::status::ok)
			res.set(http::field::accept_ranges, "bytes");

		if (st == http::status::partial_content)
		{
			std::string content_range = fmt::format(
				"bytes {}-{}/{}",
				http_range_start,
				http_range_end,
				content_length);

			content_length = http_range_end - http_range_start + 1;
			res.set(http::field::content_range, content_range);
		}

		res.keep_alive(hctx.request_.keep_alive());
		res.content_length(content_length);

		response_serializer sr(res);

		res.body().data = nullptr;
		res.body().more = false;

		co_await http::async_write_header(
			m_local_socket,
			sr,
			net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", http async_write_header: "
				<< ec.message();

			co_return;
		}

		auto buf_size = 5 * 1024 * 1024;
		if (m_option.tcp_rate_limit_ > 0 && m_option.tcp_rate_limit_ < buf_size)
			buf_size = m_option.tcp_rate_limit_;

		std::unique_ptr<char, decltype(&std::free)> bufs((char*)std::malloc(buf_size), &std::free);
		char* buf = bufs.get();

		std::streamsize total = 0;

		stream_rate_limit(m_local_socket, m_option.tcp_rate_limit_);

#if defined (BOOST_ASIO_HAS_FILE)
		for (; !m_abort;)
		{
			auto bytes_transferred = co_await file.async_read_some(net::buffer(buf, buf_size), net_awaitable[ec]);
			if (ec)
				co_return;

			stream_expires_after(m_local_socket, std::chrono::seconds(m_option.tcp_timeout_));

			total += bytes_transferred;
			co_await net::async_write(m_local_socket, net::buffer(buf, bytes_transferred), net_awaitable[ec]);
			if (ec)
			{
				log_conn_warning()
					<< ", http async_write: "
					<< ec.message()
					<< ", already write: "
					<< total;
				co_return;
			}

			if (total >= (std::streamsize)content_length)
				break;
		}
#else
		do
		{
			auto bytes_transferred = fileop::read(file, std::span<char>(buf, buf_size));
			bytes_transferred = std::min<std::streamsize>(
				bytes_transferred,
				content_length - total
			);
			if (bytes_transferred == 0 ||
				total >= (std::streamsize)content_length)
			{
				res.body().data = nullptr;
				res.body().more = false;
			}
			else
			{
				res.body().data = buf;
				res.body().size = bytes_transferred;
				res.body().more = true;
			}

			stream_expires_after(m_local_socket, std::chrono::seconds(m_option.tcp_timeout_));

			co_await http::async_write(
				m_local_socket,
				sr,
				net_awaitable[ec]);
			total += bytes_transferred;
			if (ec == http::error::need_buffer)
			{
				ec = {};
				continue;
			}
			if (ec)
			{
				log_conn_warning()
					<< ", http async_write: "
					<< ec.message()
					<< ", already write: "
					<< total;
				co_return;
			}
		} while (!sr.is_done() && !m_abort);
#endif

		XLOG_DBG << "connection id: "
			<< m_connection_id
			<< ", http request: "
			<< hctx.target_
			<< ", completed";

		co_return;
	}

	//////////////////////////////////////////////////////////////////////////
	// DNS 查询相关实现

	net::awaitable<void> proxy_session::on_http_dns_query(const http_context& hctx) noexcept
	{
		boost::system::error_code ec;
		const auto& request = hctx.request_;

		// 检查请求方法.
		if (request.method() != http::verb::post &&
			request.method() != http::verb::get)
		{
			co_await default_http_route(
				request, fake_400_content, http::status::bad_request);
			co_return;
		}

		// 提取 DNS 查询数据.
		std::string dns_query;

		if (request.method() == http::verb::post)
		{
			auto content_type = request[http::field::content_type];

			// 支持 JSON 格式的 DNS 查询 (application/dns-json).
			if (content_type == "application/dns-json")
			{
				bool json_error = false;
				std::string json_qname;
				uint16_t json_qtype = DNS_TYPE_A;
				bool json_cd = false;
				bool json_do = false;

				try
				{
					auto jv = boost::json::parse(request.body());
					auto& obj = jv.as_object();

					if (obj.contains("name"))
						json_qname = obj["name"].as_string().c_str();
					else
						json_error = true;

					if (obj.contains("type"))
					{
						auto& t = obj["type"];
						if (t.is_string())
							json_qtype = dns_type_from_string(t.as_string().c_str());
						else if (t.is_int64())
							json_qtype = static_cast<uint16_t>(t.as_int64());
					}

					if (obj.contains("cd"))
						json_cd = obj["cd"].as_bool();

					if (obj.contains("do"))
						json_do = obj["do"].as_bool();
				}
				catch (const std::exception& e)
				{
					log_conn_warning()
						<< ", dns json parse error: " << e.what();
					json_error = true;
				}

				if (json_error)
				{
					co_await default_http_route(
						request, fake_400_content, http::status::bad_request);
				}
				else
				{
					co_await dns_json_query(request, json_qname, json_qtype, json_cd, json_do);
				}
				co_return;
			}

			if (content_type != "application/dns-message")
			{
				co_await default_http_route(
					request, fake_400_content, http::status::bad_request);
				co_return;
			}
			dns_query = request.body();
		}
		else // GET
		{
			auto target = hctx.target_;

			// 检查是否为 Google JSON API 格式 (?name=...&type=...).
			auto name_pos = target.find("?name=");
			if (name_pos != std::string::npos)
			{
				auto query_str = target.substr(name_pos + 1); // 去掉 ?.

				auto get_param = [&](const std::string& key) -> std::string
				{
					auto kpos = query_str.find(key + "=");
					if (kpos == std::string::npos)
						return {};
					auto vstart = kpos + key.size() + 1;
					auto vend = query_str.find('&', vstart);
					if (vend == std::string::npos)
						return query_str.substr(vstart);
					return query_str.substr(vstart, vend - vstart);
				};

				auto qname = get_param("name");
				if (qname.empty())
				{
					co_await default_http_route(
						request, fake_400_content, http::status::bad_request);
					co_return;
				}

				// URL 解码.
				auto url_decode = [](std::string src) -> std::string
				{
					std::string ret;
					for (size_t i = 0; i < src.size(); i++)
					{
						if (src[i] == '%' && i + 2 < src.size())
						{
							char hex[3] = { src[i+1], src[i+2], 0 };
							ret += static_cast<char>(std::strtoul(hex, nullptr, 16));
							i += 2;
						}
						else
						{
							ret += src[i];
						}
					}
					return ret;
				};
				qname = url_decode(qname);

				auto type_str = get_param("type");
				uint16_t qtype = DNS_TYPE_A;
				if (!type_str.empty())
					qtype = dns_type_from_string(type_str);

				bool cd = false;
				auto cd_str = get_param("cd");
				if (cd_str == "true" || cd_str == "1")
					cd = true;

				bool do_flag = false;
				auto do_str = get_param("do");
				if (do_str == "true" || do_str == "1")
					do_flag = true;

				co_await dns_json_query(request, qname, qtype, cd, do_flag);
				co_return;
			}

			// 解析 ?dns=<base64url> 参数 (标准 DoH wire-format).
			auto pos = target.find("?dns=");
			if (pos == std::string::npos)
			{
				co_await default_http_route(
					request, fake_400_content, http::status::bad_request);
				co_return;
			}
			auto b64 = target.substr(pos + 5);
			// base64url -> standard base64.
			for (auto& c : b64)
			{
				if (c == '-') c = '+';
				else if (c == '_') c = '/';
			}
			// 补充 padding.
			switch (b64.size() % 4)
			{
				case 2: b64 += "=="; break;
				case 3: b64 += "="; break;
			}
			dns_query.resize(beast::detail::base64::decoded_size(b64.size()));
			auto [len, consumed] = beast::detail::base64::decode(
				dns_query.data(), b64.data(), b64.size());
			dns_query.resize(len);
		}

		if (dns_query.empty())
		{
			co_await default_http_route(
				request, fake_400_content, http::status::bad_request);
			co_return;
		}

		// 判断上游 DNS 服务器类型并转发查询.
		auto& upstream = *m_option.dns_upstream_;

		if (boost::istarts_with(upstream, "https://"))
		{
			// DoH (DNS over HTTPS) 上游.
			co_await doh_dns_query(request, dns_query);
		}
		else
		{
			// 传统 UDP DNS 上游.
			co_await udp_dns_query(request, dns_query);
		}

		co_return;
	}

	// udp_dns_query 通过传统 UDP 协议转发 DNS 查询到上游服务器.
	net::awaitable<void> proxy_session::udp_dns_query(
		const string_request& request, const std::string& dns_query) noexcept
	{
		boost::system::error_code ec;

		auto& upstream = *m_option.dns_upstream_;
		auto colon_pos = upstream.find(':');
		if (colon_pos == std::string::npos)
		{
			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}
		auto dns_host = upstream.substr(0, colon_pos);
		auto dns_port = (uint16_t)std::stoi(upstream.substr(colon_pos + 1));

		// 通过 UDP 转发 DNS 查询到上游服务器.
		udp::socket dns_socket(co_await net::this_coro::executor);
		udp::endpoint dns_endpoint(
			net::ip::make_address(dns_host, ec), dns_port);
		if (ec)
		{
			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}

		dns_socket.open(dns_endpoint.protocol(), ec);
		if (ec)
		{
			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}

		co_await dns_socket.async_send_to(
			net::buffer(dns_query), dns_endpoint, net_awaitable[ec]);
		if (ec)
		{
			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}

		// 接收 DNS 响应.
		std::array<char, 4096> recv_buf{};
		udp::endpoint recv_endp;
		auto recv_len = co_await dns_socket.async_receive_from(
			net::buffer(recv_buf), recv_endp, net_awaitable[ec]);
		if (ec)
		{
			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}

		dns_socket.close(ec);

		// 返回 DNS 响应.
		co_await send_http_string_response(
			request, http::status::ok, "application/dns-message",
			std::string(recv_buf.data(), recv_len), true);
		log_dns_result(dns_query, std::string(recv_buf.data(), recv_len));

		co_return;
	}

	// DNS 工具函数 (static)

	std::string proxy_session::dns_encode_name(const std::string& name) noexcept
	{
		std::string encoded;
		if (name.empty() || name == ".")
		{
			encoded.push_back('\0');
			return encoded;
		}
		std::string s = name;
		// 去掉末尾的 .
		if (!s.empty() && s.back() == '.')
			s.pop_back();

		size_t pos = 0;
		while (pos < s.size())
		{
			auto dot = s.find('.', pos);
			if (dot == std::string::npos)
				dot = s.size();
			auto label_len = dot - pos;
			if (label_len > 63)
				label_len = 63;
			encoded.push_back(static_cast<char>(static_cast<uint8_t>(label_len)));
			encoded.append(s.data() + pos, label_len);
			pos = dot + 1;
			if (dot == s.size())
				break;
		}
		encoded.push_back('\0');
		return encoded;
	}

	// 统一的 DNS 类型名称 <-> 数值映射表, 避免两份重复数据.
	static const std::unordered_map<uint16_t, std::string>& dns_type_map()
	{
		static const std::unordered_map<uint16_t, std::string> type_map = {
			{1, "A"}, {2, "NS"}, {3, "MD"}, {4, "MF"}, {5, "CNAME"},
			{6, "SOA"}, {7, "MB"}, {8, "MG"}, {9, "MR"}, {10, "NULL"},
			{11, "WKS"}, {12, "PTR"}, {13, "HINFO"}, {14, "MINFO"}, {15, "MX"},
			{16, "TXT"}, {17, "RP"}, {18, "AFSDB"}, {19, "X25"}, {20, "ISDN"},
			{21, "RT"}, {22, "NSAP"}, {23, "NSAP-PTR"}, {24, "SIG"}, {25, "KEY"},
			{26, "PX"}, {27, "GPOS"}, {28, "AAAA"}, {29, "LOC"}, {30, "NXT"},
			{33, "SRV"}, {35, "NAPTR"}, {36, "KX"}, {37, "CERT"}, {39, "DNAME"},
			{41, "OPT"}, {42, "APL"}, {43, "DS"}, {44, "SSHFP"}, {45, "IPSECKEY"},
			{46, "RRSIG"}, {47, "NSEC"}, {48, "DNSKEY"}, {49, "DHCID"},
			{50, "NSEC3"}, {51, "NSEC3PARAM"}, {52, "TLSA"}, {53, "SMIMEA"},
			{55, "HIP"}, {59, "CDS"}, {60, "CDNSKEY"}, {61, "OPENPGPKEY"},
			{62, "CSYNC"}, {63, "ZONEMD"}, {64, "SVCB"}, {65, "HTTPS"},
			{99, "SPF"}, {249, "TKEY"}, {250, "TSIG"}, {251, "IXFR"},
			{252, "AXFR"}, {255, "ANY"}, {256, "URI"}, {257, "CAA"},
			{32768, "TA"}, {32769, "DLV"},
		};
		return type_map;
	}

	uint16_t proxy_session::dns_type_from_string(const std::string& type_name) noexcept
	{
		const auto& type_map = dns_type_map();
		for (const auto& [num, name] : type_map)
		{
			if (name == type_name)
				return num;
		}
		// 尝试数字.
		try { return static_cast<uint16_t>(std::stoul(type_name)); }
		catch (...) { return DNS_TYPE_A; }
	}

	std::string proxy_session::dns_type_to_string(uint16_t type) noexcept
	{
		const auto& type_map = dns_type_map();
		auto it = type_map.find(type);
		if (it != type_map.end())
			return it->second;
		return std::to_string(type);
	}

	std::string proxy_session::build_dns_wire_query(
		const std::string& name, uint16_t type,
		bool cd, bool do_bit) noexcept
	{
		std::string query;
		query.reserve(512);

		// Header (12 bytes).
		uint16_t id = static_cast<uint16_t>(
			global_random_device()() & 0xFFFF);
		char hdr[12];
		char* hp = hdr;
		write<uint16_t>(id, hp);               // ID
		write<uint16_t>(0x0100, hp);            // Flags: RD=1
		write<uint16_t>(1, hp);                 // QDCOUNT = 1
		write<uint16_t>(0, hp);                 // ANCOUNT = 0
		write<uint16_t>(0, hp);                 // NSCOUNT = 0
		if (do_bit)
			write<uint16_t>(1, hp);             // ARCOUNT = 1 (OPT)
		else
			write<uint16_t>(0, hp);             // ARCOUNT = 0
		query.append(hdr, 12);

		// Question: QNAME.
		auto qname = dns_encode_name(name);
		query.append(qname);

		// Question: QTYPE + QCLASS.
		char qs[4];
		char* qp = qs;
		write<uint16_t>(type, qp);
		write<uint16_t>(DNS_CLASS_IN, qp);
		query.append(qs, 4);

		// OPT pseudo-record for DNSSEC DO bit.
		if (do_bit)
		{
			char opt[11];
			char* op = opt;
			write<uint8_t>(0, op);              // NAME: root (1 byte)
			write<uint16_t>(41, op);             // TYPE: OPT
			write<uint16_t>(1280, op);           // CLASS: UDP payload size
			write<uint16_t>(0x8000, op);         // TTL high: DO=1
			write<uint16_t>(0, op);              // TTL low
			write<uint16_t>(0, op);              // RDLEN = 0
			query.append(opt, 11);
		}

		// 若设置了 CD 标志, 修改 flags.
		if (cd)
		{
			// Flags at offset 2-3.
			uint16_t flags = 0x0100 | 0x0010; // RD=1, CD=1
			char* fp = &query[2];
			write<uint16_t>(flags, fp);
		}

		return query;
	}

	std::pair<std::string, const char*>
	proxy_session::dns_parse_name(const char* p, const char* end, const char* msg_start) noexcept
	{
		std::string name;
		bool jumped = false;
		const char* next = nullptr;

		while (p < end)
		{
			uint8_t len = static_cast<uint8_t>(*p);
			if (len == 0)
			{
				if (!jumped) next = p + 1;
				break;
			}
			if ((len & 0xC0) == 0xC0)
			{
				if (p + 1 >= end) break;
				uint16_t offset =
					((static_cast<uint16_t>(len & 0x3F)) << 8) |
					static_cast<uint8_t>(*(p + 1));
				if (!jumped) next = p + 2;
				p = msg_start + offset;
				jumped = true;
				continue;
			}
			if (p + 1 + len > end) break;
			if (!name.empty()) name += '.';
			name.append(p + 1, p + 1 + len);
			p += 1 + len;
		}

		if (!name.empty())
		{
			if (!jumped && next == nullptr)
				next = p + 1; // 正常结束于 0 长度标签.
			name += '.';
		}
		else if (p < end && *p == 0)
		{
			name = ".";
			if (!jumped) next = p + 1;
		}

		return {name, next ? next : p};
	}

	std::string proxy_session::dns_rdata_to_string(
		uint16_t type, uint16_t rdlength,
		const char* rdata, const char* end,
		const char* msg_start) noexcept
	{
		if (rdata + rdlength > end)
			return {rdata, (size_t)(end - rdata)};

		switch (type)
		{
		case DNS_TYPE_A:
		{
			if (rdlength < 4) return {};
			auto ip = net::ip::make_address_v4(
				read<uint32_t>(rdata));
			return ip.to_string();
		}
		case DNS_TYPE_AAAA:
		{
			if (rdlength < 16) return {};
			net::ip::address_v6::bytes_type bytes;
			for (auto& b : bytes)
				b = read<uint8_t>(rdata);
			return net::ip::make_address_v6(bytes).to_string();
		}
		case DNS_TYPE_CNAME:
		case DNS_TYPE_NS:
		case DNS_TYPE_PTR:
		{
			auto [name, _] = dns_parse_name(rdata, rdata + rdlength, msg_start);
			(void)_;
			return name;
		}
		case DNS_TYPE_MX:
		{
			if (rdlength < 2) return {};
			auto pref = read<uint16_t>(rdata);
			auto [name, _] = dns_parse_name(rdata, rdata + rdlength, msg_start);
			(void)_;
			return std::to_string(pref) + " " + name;
		}
		case DNS_TYPE_TXT:
		{
			std::string result;
			const char* txt_end = rdata + rdlength;
			while (rdata < txt_end)
			{
				uint8_t len = read<uint8_t>(rdata);
				if (rdata + len > txt_end) break;
				if (!result.empty()) result += "\n";
				result.append(rdata, len);
				rdata += len;
			}
			return result;
		}
		case DNS_TYPE_SOA:
		{
			auto [mname, p1] = dns_parse_name(rdata, rdata + rdlength, msg_start);
			if (!p1) return {};
			rdata = p1;
			auto [rname, p2] = dns_parse_name(rdata, rdata + rdlength, msg_start);
			if (!p2) return mname + " " + rname;
			rdata = p2;
			if (rdata + 20 > rdata + rdlength) return {};
			auto serial = read<uint32_t>(rdata);
			auto refresh = read<uint32_t>(rdata);
			auto retry = read<uint32_t>(rdata);
			auto expire = read<uint32_t>(rdata);
			auto minimum = read<uint32_t>(rdata);
			return mname + " " + rname + " " +
				std::to_string(serial) + " " +
				std::to_string(refresh) + " " +
				std::to_string(retry) + " " +
				std::to_string(expire) + " " +
				std::to_string(minimum);
		}
		case DNS_TYPE_SRV:
		{
			if (rdlength < 6) return {};
			auto priority = read<uint16_t>(rdata);
			auto weight = read<uint16_t>(rdata);
			auto srv_port = read<uint16_t>(rdata);
			auto [target, _] = dns_parse_name(rdata, rdata + rdlength, msg_start);
			(void)_;
			return std::to_string(priority) + " " +
				std::to_string(weight) + " " +
				std::to_string(srv_port) + " " + target;
		}
		case DNS_TYPE_HTTPS:
		{
			if (rdlength < 2) return {};
			auto svc_priority = read<uint16_t>(rdata);
			auto [svc_target, _] = dns_parse_name(rdata, rdata + rdlength, msg_start);
			(void)_;
			return std::to_string(svc_priority) + " " + svc_target;
		}
		case DNS_TYPE_CAA:
		{
			if (rdlength < 2) return {};
			auto flags = read<uint8_t>(rdata);
			uint8_t tag_len = read<uint8_t>(rdata);
			if (rdata + tag_len > rdata + rdlength) return {};
			std::string tag(rdata, tag_len);
			rdata += tag_len;
			std::string value(rdata, (rdata + rdlength) - rdata);
			return std::to_string(flags) + " " + tag + " \"" + value + "\"";
		}
		default:
			{
				std::string hex;
				for (uint16_t i = 0; i < rdlength; i++)
				{
					char buf[3];
					std::snprintf(buf, sizeof(buf), "%02x",
						static_cast<uint8_t>(rdata[i]));
					hex += buf;
				}
				return hex;
			}
		}
	}

	std::string proxy_session::dns_response_to_json(
		const std::string& wire_response,
		const std::string& question_name,
		uint16_t question_type) noexcept
	{
		boost::json::object root;

		const char* p = wire_response.data();
		const char* end = p + wire_response.size();
		const char* msg_start = p;

		if (wire_response.size() < 12)
		{
			root["Status"] = -1;
			root["Comment"] = "Response too short";
			return boost::json::serialize(root);
		}

		// Parse header.
		[[maybe_unused]] auto id = read<uint16_t>(p);
		auto flags = read<uint16_t>(p);
		auto qdcount = read<uint16_t>(p);
		auto ancount = read<uint16_t>(p);
		auto nscount = read<uint16_t>(p);
		auto arcount = read<uint16_t>(p);

		auto rcode = flags & 0x0F;
		bool tc = (flags & 0x0200) != 0;
		bool rd = (flags & 0x0100) != 0;
		bool ra = (flags & 0x0080) != 0;
		bool ad = (flags & 0x0020) != 0;
		bool cd = (flags & 0x0010) != 0;

		root["Status"] = rcode;
		root["TC"] = tc;
		root["RD"] = rd;
		root["RA"] = ra;
		root["AD"] = ad;
		root["CD"] = cd;

		// Questions.
		boost::json::array questions;
		for (uint16_t i = 0; i < qdcount && p < end; i++)
		{
			auto [qname, np] = dns_parse_name(p, end, msg_start);
			if (!np) break;
			p = np;
			if (p + 4 > end) break;
			auto qtype = read<uint16_t>(p);
			[[maybe_unused]] auto qclass = read<uint16_t>(p);

			boost::json::object q;
			q["name"] = qname;
			q["type"] = qtype;
			questions.push_back(std::move(q));
		}
		root["Question"] = std::move(questions);

		// Answers.
		boost::json::array answers;
		for (uint16_t i = 0; i < ancount && p < end; i++)
		{
			auto [aname, np] = dns_parse_name(p, end, msg_start);
			if (!np) break;
			p = np;
			if (p + 10 > end) break;
			auto atype = read<uint16_t>(p);
			[[maybe_unused]] auto aclass = read<uint16_t>(p);
			auto attl = read<uint32_t>(p);
			auto rdlength = read<uint16_t>(p);
			if (p + rdlength > end) break;

			boost::json::object a;
			a["name"] = aname;
			a["type"] = atype;
			a["TTL"] = attl;
			a["data"] = dns_rdata_to_string(atype, rdlength, p, end, msg_start);
			answers.push_back(std::move(a));

			p += rdlength;
		}
		root["Answer"] = std::move(answers);

		// Authority.
		boost::json::array authorities;
		for (uint16_t i = 0; i < nscount && p < end; i++)
		{
			auto [nsname, np] = dns_parse_name(p, end, msg_start);
			if (!np) break;
			p = np;
			if (p + 10 > end) break;
			auto nstype = read<uint16_t>(p);
			[[maybe_unused]] auto nsclass = read<uint16_t>(p);
			auto nsttl = read<uint32_t>(p);
			auto nsrdlength = read<uint16_t>(p);
			if (p + nsrdlength > end) break;

			boost::json::object ns;
			ns["name"] = nsname;
			ns["type"] = nstype;
			ns["TTL"] = nsttl;
			ns["data"] = dns_rdata_to_string(nstype, nsrdlength, p, end, msg_start);
			authorities.push_back(std::move(ns));

			p += nsrdlength;
		}
		root["Authority"] = std::move(authorities);

		// Additional.
		boost::json::array additional;
		for (uint16_t i = 0; i < arcount && p < end; i++)
		{
			auto [adname, np] = dns_parse_name(p, end, msg_start);
			if (!np) break;
			p = np;
			if (p + 10 > end) break;
			auto adtype = read<uint16_t>(p);
			[[maybe_unused]] auto adclass = read<uint16_t>(p);
			auto adttl = read<uint32_t>(p);
			auto adrdlength = read<uint16_t>(p);
			if (p + adrdlength > end) break;

			if (adtype != 41)
			{
				boost::json::object ad;
				ad["name"] = adname;
				ad["type"] = adtype;
				ad["TTL"] = adttl;
				ad["data"] = dns_rdata_to_string(adtype, adrdlength, p, end, msg_start);
				additional.push_back(std::move(ad));
			}

			p += adrdlength;
		}
		if (!additional.empty())
			root["Additional"] = std::move(additional);

		root["Comment"] = "Response from proxy DNS";

		return boost::json::serialize(root);
	}

	// log_dns_result
	void proxy_session::log_dns_result(
		const std::string& query_msg,
		const std::string& response_msg,
		const std::string& json_body) const noexcept
	{
		std::string qname;
		uint16_t qtype = DNS_TYPE_A;

		if (!json_body.empty())
		{
			// 从 JSON 响应体中提取域名和答案.
			try
			{
				auto jv = boost::json::parse(json_body);
				auto& obj = jv.as_object();

				if (obj.contains("Question") && !obj["Question"].as_array().empty())
				{
					auto& q = obj["Question"].as_array()[0].as_object();
					if (q.contains("name"))
						qname = q["name"].as_string().c_str();
					if (q.contains("type"))
						qtype = static_cast<uint16_t>(q["type"].as_int64());
				}

				size_t answer_count = 0;
				if (obj.contains("Answer"))
					answer_count = obj["Answer"].as_array().size();

				std::string answer_summary;
				auto& answers = obj["Answer"].as_array();
				for (size_t i = 0; i < answer_count && i < 5; i++)
				{
					auto& a = answers[i].as_object();
					auto aname = a["name"].as_string().c_str();
					auto atype = dns_type_to_string(
						static_cast<uint16_t>(a["type"].as_int64()));
					auto data = a["data"].as_string().c_str();
					if (!answer_summary.empty())
						answer_summary += "; ";
					answer_summary += aname;
					answer_summary += " ";
					answer_summary += atype;
					answer_summary += " ";
					answer_summary += data;
				}
				if (answer_count > 5)
					answer_summary += fmt::format(" ... ({} total)", answer_count);

				log_conn_debug()
					<< ", dns query completed"
					<< ", domain: " << qname
					<< ", type: " << dns_type_to_string(qtype)
					<< ", answers: " << answer_count
					<< (!answer_summary.empty() ?
						std::string(", result: ") + answer_summary
						: std::string(", no answer"));
			}
			catch (const std::exception& e)
			{
				log_conn_debug()
					<< ", dns query completed"
					<< ", (json parse error: " << e.what() << ")";
			}
			return;
		}

		// 从 wire-format 查询中提取问题域名.
		const char* qp = query_msg.data();
		const char* qend = qp + query_msg.size();
		if (query_msg.size() >= 12)
		{
			qp += 12; // 跳过 DNS 头.
			auto [parsed_qname, np] = dns_parse_name(qp, qend, query_msg.data());
			if (np && np + 4 <= qend)
			{
				qp = np;
				qname = parsed_qname;
				qtype = read<uint16_t>(qp);
			}
		}

		// 从 wire-format 响应中提取答案.
		const char* p = response_msg.data();
		const char* end = p + response_msg.size();
		const char* msg_start = p;

		size_t ancount = 0;
		std::string answer_summary;

		if (response_msg.size() >= 12)
		{
			read<uint16_t>(p);  // ID
			read<uint16_t>(p);  // Flags
			auto qdcount = read<uint16_t>(p);
			ancount = read<uint16_t>(p);
			read<uint16_t>(p);  // NSCOUNT
			read<uint16_t>(p);  // ARCOUNT

			// 跳过问题区域.
			for (uint16_t i = 0; i < qdcount && p < end; i++)
			{
				auto [_, np] = dns_parse_name(p, end, msg_start);
				if (!np) break;
				p = np;
				if (p + 4 > end) break;
				p += 4; // QTYPE + QCLASS
			}

			// 读取答案.
			for (uint16_t i = 0; i < ancount && p < end; i++)
			{
				auto [aname, np] = dns_parse_name(p, end, msg_start);
				if (!np) break;
				p = np;
				if (p + 10 > end) break;
				auto atype = read<uint16_t>(p);
				[[maybe_unused]] auto aclass = read<uint16_t>(p);
				[[maybe_unused]] auto attl = read<uint32_t>(p);
				auto rdlength = read<uint16_t>(p);
				if (p + rdlength > end) break;

				if (i < 5)
				{
					if (!answer_summary.empty())
						answer_summary += "; ";
					answer_summary += aname;
					answer_summary += " ";
					answer_summary += dns_type_to_string(atype);
					answer_summary += " ";
					answer_summary += dns_rdata_to_string(atype, rdlength, p, end, msg_start);
				}

				p += rdlength;
			}
		}

		if (ancount > 5)
			answer_summary += fmt::format(" ... ({} total)", ancount);

		log_conn_debug()
			<< ", dns query completed"
			<< ", domain: " << (qname.empty() ? "(unknown)" : qname)
			<< ", type: " << dns_type_to_string(qtype)
			<< ", answers: " << ancount
			<< (!answer_summary.empty() ?
				std::string(", result: ") + answer_summary
				: std::string(", no answer"));
	}

	net::awaitable<void> proxy_session::dns_json_query(
		const string_request& request,
		const std::string& question_name,
		uint16_t question_type,
		bool cd_flag,
		bool do_flag) noexcept
	{
		boost::system::error_code ec;

		// 1. 构建 DNS wire-format 查询.
		auto dns_query = build_dns_wire_query(
			question_name, question_type, cd_flag, do_flag);

		// 2. 通过上游转发查询并获取 wire-format 响应.
		std::string wire_response;
		auto& upstream = *m_option.dns_upstream_;
		bool ok = false;

		if (boost::istarts_with(upstream, "https://"))
		{
			ok = co_await doh_query_raw(dns_query, wire_response);
		}
		else
		{
			ok = co_await udp_query_raw(dns_query, wire_response);
		}

		if (!ok || wire_response.empty())
		{
			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}

		// 3. 解析 wire-format 响应为 JSON.
		auto json_body = dns_response_to_json(
			wire_response, question_name, question_type);

		// 4. 返回 JSON 响应给客户端.
		co_await send_http_string_response(
			request, http::status::ok, "application/dns-json",
			json_body, true);
		log_dns_result(dns_query, wire_response, json_body);

		co_return;
	}

	net::awaitable<bool> proxy_session::udp_query_raw(
		const std::string& dns_query, std::string& output) noexcept
	{
		boost::system::error_code ec;

		auto& upstream = *m_option.dns_upstream_;
		auto colon_pos = upstream.find(':');
		if (colon_pos == std::string::npos)
			co_return false;

		auto dns_host = upstream.substr(0, colon_pos);
		auto dns_port = (uint16_t)std::stoi(upstream.substr(colon_pos + 1));

		udp::socket dns_socket(co_await net::this_coro::executor);
		udp::endpoint dns_endpoint(
			net::ip::make_address(dns_host, ec), dns_port);
		if (ec)
			co_return false;

		dns_socket.open(dns_endpoint.protocol(), ec);
		if (ec)
			co_return false;

		co_await dns_socket.async_send_to(
			net::buffer(dns_query), dns_endpoint, net_awaitable[ec]);
		if (ec)
			co_return false;

		std::array<char, 4096> recv_buf{};
		udp::endpoint recv_endp;
		auto recv_len = co_await dns_socket.async_receive_from(
			net::buffer(recv_buf), recv_endp, net_awaitable[ec]);
		if (ec)
			co_return false;

		dns_socket.close(ec);

		output.assign(recv_buf.data(), recv_len);
		co_return true;
	}

	net::awaitable<bool> proxy_session::doh_query_raw(
		const std::string& dns_query, std::string& output) noexcept
	{
		boost::system::error_code ec;

		auto parsed = parse_urlinfo(*m_option.dns_upstream_);
		if (parsed.has_error())
			co_return false;

		auto [scheme, user, passwd, doh_host, doh_port, doh_path] = *parsed;
		if (doh_path.empty() || doh_path == "/")
			doh_path = "/dns-query";

		// 解析 DoH 服务器地址.
		tcp::resolver::results_type targets;
		if (!is_hostname(doh_host))
		{
			tcp::endpoint endp(
				net::ip::make_address(doh_host), doh_port);
			targets = tcp::resolver::results_type::create(
				endp, std::string(doh_host), "");
		}
		else
		{
			targets = co_await resolve_targets(
				std::string(doh_host), doh_port);
		}

		if (targets.empty())
			co_return false;

		// 连接到 DoH 服务器.
		tcp::socket doh_socket(m_executor);
		ec = co_await async_connect_targets(doh_socket, targets);
		if (ec)
			co_return false;

		// 创建 per-request SSL context, 确保每个 DoH 服务器使用正确的主机名校验.
		net::ssl::context doh_ssl_ctx(net::ssl::context::sslv23_client);
		ec = configure_ssl_client_ctx(doh_ssl_ctx,
			m_option.disable_check_cert_,
			std::string(doh_host));
		if (ec)
		{
			log_conn_warning()
				<< ", configure ssl context for doh: "
				<< doh_host << " error: " << ec.message();
			co_return false;
		}

		ssl_tcp_stream ssl_stream(std::move(doh_socket), doh_ssl_ctx);

		if (!SSL_set_tlsext_host_name(
			ssl_stream.native_handle(), std::string(doh_host).c_str()))
		{
			log_conn_debug()
				<< ", doh set sni name: "
				<< doh_host << " failed";
		}

		co_await ssl_stream.async_handshake(
			net::ssl::stream_base::client, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", doh ssl handshake with "
				<< doh_host
				<< " failed";
			co_return false;
		}

		// 构造 HTTP POST 请求.
		http::request<http::string_body> doh_req{
			http::verb::post, doh_path, 11 };
		doh_req.set(http::field::host, doh_host);
		doh_req.set(http::field::content_type, "application/dns-message");
		doh_req.set(http::field::accept, "application/dns-message");
		doh_req.body() = dns_query;
		doh_req.prepare_payload();

		co_await http::async_write(ssl_stream, doh_req, net_awaitable[ec]);
		if (ec)
			co_return false;

		beast::flat_buffer buf;
		http::response<http::string_body> doh_res;
		co_await http::async_read(ssl_stream, buf, doh_res, net_awaitable[ec]);
		if (ec)
			co_return false;

		if (doh_res.result() != http::status::ok)
		{
			log_conn_warning()
				<< ", doh query raw response status: "
				<< doh_res.result_int();
			co_return false;
		}

		log_conn_debug()
			<< ", doh query raw response received"
			<< ", status: " << doh_res.result_int()
			<< ", content-length: " << doh_res.payload_size().value_or(0);

		output = std::move(doh_res.body());
		co_return true;
	}

	net::awaitable<void> proxy_session::doh_dns_query(
		const string_request& request, const std::string& dns_query) noexcept
	{
		boost::system::error_code ec;

		auto parsed = parse_urlinfo(*m_option.dns_upstream_);
		if (parsed.has_error())
		{
			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}

		auto [scheme, user, passwd, doh_host, doh_port, doh_path] = *parsed;
		if (doh_path.empty() || doh_path == "/")
			doh_path = "/dns-query";

		// 解析 DoH 服务器地址.
		tcp::resolver::results_type targets;
		if (!is_hostname(doh_host))
		{
			tcp::endpoint endp(
				net::ip::make_address(doh_host), doh_port);
			targets = tcp::resolver::results_type::create(
				endp, std::string(doh_host), "");
		}
		else
		{
			targets = co_await resolve_targets(
				std::string(doh_host), doh_port);
		}

		if (targets.empty())
		{
			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}

		// 连接到 DoH 服务器.
		tcp::socket doh_socket(m_executor);
		ec = co_await async_connect_targets(doh_socket, targets);
		if (ec)
		{
			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}

		// 创建 per-request SSL context, 确保每个 DoH 服务器使用正确的主机名校验.
		net::ssl::context doh_ssl_ctx(net::ssl::context::sslv23_client);

		ec = configure_ssl_client_ctx(doh_ssl_ctx,
			m_option.disable_check_cert_,
			std::string(doh_host));
		if (ec)
		{
			log_conn_warning()
				<< ", configure ssl context for doh: "
				<< doh_host << " error: " << ec.message();

			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}

		ssl_tcp_stream ssl_stream(std::move(doh_socket), doh_ssl_ctx);

		if (!SSL_set_tlsext_host_name(
			ssl_stream.native_handle(), std::string(doh_host).c_str()))
		{
			log_conn_debug()
				<< ", doh set sni name: "
				<< doh_host
				<< " failed";
		}

		co_await ssl_stream.async_handshake(
			net::ssl::stream_base::client, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", doh tls handshake with "
				<< doh_host
				<< " failed: "
				<< ec.message();
			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}

		http::request<http::string_body> doh_req{
			http::verb::post, doh_path, 11 };
		doh_req.set(http::field::host, doh_host);
		doh_req.set(http::field::content_type, "application/dns-message");
		doh_req.set(http::field::accept, "application/dns-message");
		doh_req.body() = dns_query;
		doh_req.prepare_payload();

		co_await http::async_write(ssl_stream, doh_req, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", doh http write error: "
				<< ec.message();
			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}

		beast::flat_buffer buf;
		http::response<http::string_body> doh_res;
		co_await http::async_read(ssl_stream, buf, doh_res, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", doh http read error: "
				<< ec.message();
			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}

		if (doh_res.result() != http::status::ok)
		{
			log_conn_warning()
				<< ", doh response status: "
				<< doh_res.result_int();
			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}

		co_await send_http_string_response(
			request, http::status::ok, "application/dns-message",
			doh_res.body(), true);
		log_dns_result(dns_query, doh_res.body());

		co_return;
	}

	//////////////////////////////////////////////////////////////////////////
	// HTTP 响应辅助

	net::awaitable<void> proxy_session::send_http_string_response(
		const string_request& request,
		http::status status,
		std::string content_type,
		std::string body,
		bool keep_alive) noexcept
	{
		boost::system::error_code ec;

		string_response res{ status, request.version() };
		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string());
		res.set(http::field::content_type, std::move(content_type));
		res.keep_alive(keep_alive);
		res.body() = std::move(body);
		res.prepare_payload();

		http::serializer<false, string_body, http::fields> sr(res);
		co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", send http response error: "
				<< ec.message();
		}

		co_return;
	}

	net::awaitable<void> proxy_session::default_http_route(
		const string_request& request, std::string response, http::status status) noexcept
	{
		co_await send_http_string_response(
			request, status, "text/html", std::move(response), true);
		co_return;
	}

	net::awaitable<void> proxy_session::location_http_route(
		const string_request& request, const std::string& path) noexcept
	{
		boost::system::error_code ec;

		string_response res{ http::status::moved_permanently, request.version() };
		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string());
		res.set(http::field::content_type, "text/html");
		res.set(http::field::location, path);

		res.keep_alive(true);
		res.body() = fake_302_content;
		res.prepare_payload();

		http::serializer<false, string_body, http::fields> sr(res);
		co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", location http route err: "
				<< ec.message();
		}

		co_return;
	}

	net::awaitable<void> proxy_session::forbidden_http_route(const string_request& request) noexcept
	{
	co_await send_http_string_response(
		request, http::status::forbidden, "text/html",
		fake_403_content, true);
	co_return;
}

net::awaitable<void> proxy_session::unauthorized_http_route(const string_request& request) noexcept
{
	boost::system::error_code ec;

	// WWW-Authenticate 头无法通过 send_http_string_response 设置, 需单独构建.
	string_response res{ http::status::unauthorized, request.version() };
	res.set(http::field::server, version_string);
	res.set(http::field::date, server_date_string());
	res.set(http::field::content_type, "text/html; charset=UTF-8");
	res.set(http::field::www_authenticate, "Basic realm=\"proxy\"");
	res.keep_alive(true);
	res.body() = fake_401_content;
	res.prepare_payload();

	http::serializer<false, string_body, http::fields> sr(res);
	co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
	if (ec)
	{
		log_conn_warning()
			<< ", unauthorized http route err: "
			<< ec.message();
	}

	co_return;
}

	//////////////////////////////////////////////////////////////////////////
	// DNS 解析与缓存

	tcp::resolver::results_type
	proxy_session::get_resolver_from_cache(const std::string& hostname, uint16_t port) noexcept
	{
		auto dns = m_dns_cache.get(hostname);
		if (dns)
		{
			[[maybe_unused]] auto& [result, tp] = *dns;

			std::vector<tcp::endpoint> new_endpoints;
			new_endpoints.reserve(result.size());

			std::transform(result.begin(), result.end(),
				std::back_inserter(new_endpoints),
				[port](const tcp::endpoint& ep)
				{
					return tcp::endpoint(ep.address(), port);
				});

			return tcp::resolver::results_type::create(
				new_endpoints.begin(), new_endpoints.end(),
				hostname, std::to_string(port));
		}

		return {};
	}

	net::awaitable<net::any_io_executor> proxy_session::switch_to_backend_executor()
	{
		if (!m_scheduler_locking)
		{
			co_await net::post(
				net::bind_executor(m_backend_context.get_executor(), net::use_awaitable));
			co_return m_backend_context.get_executor();
		}
		co_return m_executor;
	}

	net::awaitable<void> proxy_session::switch_from_backend_executor()
	{
		if (!m_scheduler_locking)
			co_await net::post(
				net::bind_executor(m_executor, net::use_awaitable));
	}

	net::awaitable<tcp::resolver::results_type>
	proxy_session::resolve_targets(const std::string& hostname, uint16_t port_number) noexcept
	{
		net::ip::basic_resolver_results<tcp> targets;

		targets = get_resolver_from_cache(hostname, port_number);
		if (!targets.empty())
			co_return targets;

		boost::system::error_code ec;

		auto executor = co_await switch_to_backend_executor();

		tcp::resolver resolver{ executor };

		targets = co_await resolver.async_resolve(
			hostname,
			std::to_string(port_number),
			net_awaitable[ec]);

		co_await switch_from_backend_executor();

		if (ec)
		{
			log_conn_warning()
				<< ", resolve: "
				<< hostname
				<< ", error: "
				<< ec.message();

			co_return targets;
		}

		// 更新 dns 缓存.
		dns_result result = std::make_tuple(targets, std::chrono::steady_clock::now());
		m_dns_cache.put(hostname, std::move(result));

		co_return targets;
	}

	net::awaitable<tcp::resolver::results_type>
	proxy_session::resolve_proxy_pass_targets() noexcept
	{
		tcp::resolver::results_type targets;

		if (!m_proxy_pass)
			co_return targets;

		auto proxy_host = std::string(m_proxy_pass->host());
		uint16_t proxy_port_number = 0;

		if (m_proxy_pass->port_number() == 0)
			proxy_port_number = urls::default_port(m_proxy_pass->scheme_id());
		else
			proxy_port_number = m_proxy_pass->port_number();

		// 如果是 IP 地址, 则直接构造而不需要 DNS 查询.
		if (!is_hostname(proxy_host))
		{
			net::ip::tcp::endpoint endp(
				net::ip::make_address(proxy_host), proxy_port_number);

			targets = tcp::resolver::results_type::create(
				endp, proxy_host, m_proxy_pass->scheme());
		}
		else
		{
			// 进行 DNS 查询.
			targets = co_await resolve_targets(proxy_host, proxy_port_number);
		}

		co_return targets;
	}

	//////////////////////////////////////////////////////////////////////////

	net::awaitable<boost::system::error_code>
	proxy_session::async_connect_targets(tcp::socket& socket, tcp::resolver::results_type& targets) noexcept
	{
		boost::system::error_code ec;

		if (targets.empty())
		{
			ec = net::error::host_not_found;
			co_return ec;
		}

		if (m_option.happyeyeballs_)
		{
			co_await asio_util::async_connect(
				socket,
				targets,
				[this](const auto& ec, auto& stream, auto& endp) {
					return check_condition(ec, stream, endp);
				},
				net_awaitable[ec]);

			{
				auto ret = set_tcp_keepalive(socket.native_handle());
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", tcp keepalive error: "
						<< ret.error().message();
				}
			}

			if (m_option.so_mark_)
			{
				auto ret = set_socket_mark(socket.native_handle(), m_option.so_mark_.value());
				if (ret.has_error())
				{
					log_conn_warning()
						<< ", set socket mark error: "
						<< ret.error().message();
				}
			}

			co_return ec;
		}

		for (const auto& endpoint : targets)
		{
			ec = boost::asio::error::host_not_found;

			if (m_option.connect_v4_only_)
			{
				if (endpoint.endpoint().address().is_v6())
					continue;
			}
			else if (m_option.connect_v6_only_)
			{
				if (endpoint.endpoint().address().is_v4())
					continue;
			}

			boost::system::error_code ignore_ec;
			socket.close(ignore_ec);

			if (m_bind_interface)
			{
				tcp::endpoint bind_endpoint(
					*m_bind_interface,
					0);

				socket.open(
					bind_endpoint.protocol(),
					ec);
				if (ec)
					break;

				socket.bind(
					bind_endpoint,
					ec);
				if (ec)
					break;
			}

			co_await socket.async_connect(
				endpoint,
				net_awaitable[ec]);
			if (!ec)
			{
				{
					auto ret = set_tcp_keepalive(socket.native_handle());
					if (ret.has_error())
					{
						log_conn_warning()
							<< ", tcp keepalive error: "
							<< ret.error().message();
					}
				}

				if (m_option.so_mark_)
				{
					auto ret = set_socket_mark(socket.native_handle(), m_option.so_mark_.value());
					if (ret.has_error())
					{
						log_conn_warning()
							<< ", set socket mark error: "
							<< ret.error().message();
					}
				}

				break;
			}
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
			sock.set_scramble_key(m_outout_key);
			sock.set_unscramble_key(m_outin_key);

			co_return sock_stream;
		}

		// 获取 proxy_pass 的主机名称.
		auto proxy_host = std::string(m_proxy_pass->host());

		// 若用户配置了 SNI, 则使用用户配置的 SNI 否则使用默认 hostname.
		std::string sni = m_option.proxy_ssl_name_.empty() ? proxy_host : m_option.proxy_ssl_name_;

		// 使用通用函数配置 SSL context (验证模式、CA 证书、主机名验证).
		ec = configure_ssl_client_ctx(m_ssl_cli_context,
			m_option.disable_check_cert_,
			sni,
			m_option.ssl_cacert_path_);
		if (ec)
		{
			log_conn_warning()
				<< ", configure ssl context error: " << ec.message();
		}

		// 额外的 SSL 上下文配置: DH 参数和 ALPN 协议.
		m_ssl_cli_context.use_tmp_dh(net::buffer(default_dh_param()), ec);
		SSL_CTX_set_alpn_protos(m_ssl_cli_context.native_handle(),
			(const unsigned char *)"\x08http/1.1", 9);

		// 生成 ssl_tcp_stream 的 variant_stream_type 对象.
		auto sock_stream = init_proxy_stream(std::move(remote_socket), m_ssl_cli_context);

		// get origin ssl stream type.
		ssl_tcp_stream& ssl_socket = boost::variant2::get<ssl_tcp_stream>(sock_stream);

		if (m_option.scramble_)
		{
			// 若是 scramble 混淆, 则配置相应的 key.
			auto& next_layer = ssl_socket.next_layer();

			next_layer.set_scramble_key(m_outout_key);
			next_layer.set_unscramble_key(m_outin_key);
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

	net::awaitable<void> proxy_session::concurrent_transfer()
	{
		size_t l2r_transferred = 0;
		size_t r2l_transferred = 0;

		// 并发读写, 在 local 和 remote 之间互传数据.
		co_await(
			transfer(m_local_socket, m_remote_socket, l2r_transferred)
			&&
			transfer(m_remote_socket, m_local_socket, r2l_transferred)
		);

		log_conn_debug()
			<< ", transfer completed"
			<< ", local to remote: "
			<< l2r_transferred
			<< ", remote to local: "
			<< r2l_transferred;

		co_return;
	}

} // namespace proxy
