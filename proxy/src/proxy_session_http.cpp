//
// proxy_session_http.cpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_session.hpp"
#include "proxy/http_error_body.hpp"

namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// HTTP 静态文件服务辅助

	static constexpr auto head_fmt =
		LR"(<html><head><meta charset="UTF-8"><title>Index of {}</title></head><body bgcolor="white"><h1>Index of {}</h1><hr><pre>)";
	static constexpr auto tail_fmt =
		L"</pre><hr></body></html>";
	static constexpr auto body_fmt =
		L"<a href=\"{}\">{}</a>{} {}       {}\r\n";

	//////////////////////////////////////////////////////////////////////////

	static const std::unordered_map<std::string_view, std::string_view> global_mimes =
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

	// http_ranges 用于保存 http range 请求头的解析结果.
	// 例如: bytes=0-100,200-300,400-500
	// 解析后的结果为: { {0, 100}, {200, 300}, {400, 500} }
	// 例如: bytes=0-100,200-300,400-500,600
	// 解析后的结果为: { {0, 100}, {200, 300}, {400, 500}, {600, -1} }
	// 如果解析失败, 则返回空数组.
	using http_ranges = std::vector<std::pair<int64_t, int64_t>>;

	// parser_http_ranges 用于解析 http range 请求头.
	// 使用 string_view 解析, 解析过程不产生内存分配.
	static http_ranges parser_http_ranges(std::string_view range) noexcept
	{
		http_ranges result;

		// 去掉首尾空白.
		while (!range.empty() && is_ascii_space(range.front()))
			range.remove_prefix(1);
		while (!range.empty() && is_ascii_space(range.back()))
			range.remove_suffix(1);

		// range 必须以 bytes= 开头, 否则返回空数组.
		if (range.size() < 6 || range.substr(0, 6) != "bytes=")
			return {};

		range.remove_prefix(6);
		if (range.empty())
			return {};

		size_t start = 0;
		while (start < range.size())
		{
			size_t comma_pos = range.find(',', start);

			std::string_view part = (comma_pos == std::string_view::npos)
							? range.substr(start)
							: range.substr(start, comma_pos - start);

			// 去除首尾空白.
			while (!part.empty() && is_ascii_space(part.front()))
				part.remove_prefix(1);
			while (!part.empty() && is_ascii_space(part.back()))
				part.remove_suffix(1);

			if (part.empty())
				return {};

			// 统计 '-' 数量（最多1个）
			size_t dash_count = 0;
			for (char c : part)
				if (c == '-')
					++dash_count;
			if (dash_count > 1)
				return {};  // 多个 '-' 非法（如 0-100-200）

			int64_t first = -1;
			int64_t second = -1;

			if (dash_count == 0)
			{
				// 纯数字：如 "600" → {600, -1}
				auto n = parse_num(part);
				if (!n)
					return {};
				first = *n;
				second = -1;
			}
			else if (part.front() == '-')
			{
				// 后缀长度：如 "-500"
				auto suffix = part.substr(1);
				while (!suffix.empty() && is_ascii_space(suffix.front()))
					suffix.remove_prefix(1);
				if (suffix.empty())
					return {};
				auto n = parse_num(suffix);
				if (!n || *n <= 0)
					return {};
				second = *n;
			}
			else {
				// 普通范围： "0-100" 或 "200-"
				size_t dash_pos = part.find('-');
				std::string_view left = part.substr(0, dash_pos);
				std::string_view right = part.substr(dash_pos + 1);

				// 去除空白
				while (!left.empty() && is_ascii_space(left.front()))
					left.remove_prefix(1);
				while (!left.empty() && is_ascii_space(left.back()))
					left.remove_suffix(1);
				while (!right.empty() && is_ascii_space(right.front()))
					right.remove_prefix(1);
				while (!right.empty() && is_ascii_space(right.back()))
					right.remove_suffix(1);

				if (!left.empty())
				{
					auto n = parse_num(left);
					if (!n)
						return {};
					first = *n;
				}

				if (!right.empty())
				{
					auto n = parse_num(right);
					if (!n)
						return {};
					second = *n;
				} else {
					second = -1;  // "200-" 形式
				}

				if (first == -1 && second == -1)
					return {};

				if (first != -1 && second != -1 && first > second)
					return {};
			}

			result.emplace_back(first, second);

			if (comma_pos == std::string_view::npos)
				break;
			start = comma_pos + 1;
		}

		return result;
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
			// 后缀长度超过文件大小时, 从文件头开始返回整个文件.
			if (offset < 0)
				offset = 0;
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

		// 请求范围超出文件大小时, 将结束位置截断到文件末尾, 避免
		// Content-Length 与实际发送字节数不一致.
		if (end > content_length - 1)
			end = content_length - 1;

		return { offset, end, http::status::partial_content };
	}

	//////////////////////////////////////////////////////////////////////////
	// HTTP 静态服务

	net::awaitable<void>
	proxy_session::static_web_server(http::request<http::string_body>& req, std::optional<request_parser>& parser) noexcept
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
				parser->body_limit(1024 * 1024 * 10);
				m_local_buffer.consume(m_local_buffer.size());

				// 慢速请求防护: keepalive 后续请求读取同样受 tcp_timeout 约束.
				stream_expires_after(m_local_socket, std::chrono::seconds(m_option.tcp_timeout_));

				// keepalive 下须连同 body 一起读取: /dns-query 走 POST,
				// 只读请求头会导致后续请求 body 为空而被拒.
				co_await http::async_read(
					m_local_socket,
					m_local_buffer,
					*parser,
					net_awaitable[ec]);
				if (ec)
				{
					log_conn_debug()
						<< (keep_alive ? ", keepalive" : "")
						<< ", web async_read: "
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

			// 静态文件服务只支持 GET/HEAD 方法, 其他方法返回 405.
			// /dns-query 为 DNS over HTTPS 接口, 单独支持 POST (由
			// on_http_dns_query 处理), 因此需要排除该路由.
			bool dns_proxy = m_option.dns_upstream_.has_value();
			if (req.method() != http::verb::get &&
				req.method() != http::verb::head &&
				!(dns_proxy &&
					boost::regex_match(
						std::string(req.target()),
						boost::regex{R"(^\/dns-query(.*)$)"})))
			{
				co_await method_not_allowed_http_route(req);
				co_return;
			}

			std::string target = req.target();
			boost::smatch what;
			auto target_path = make_real_target_path(
				m_option.doc_directory_, req.target());
			if (target_path.empty())
			{
				log_conn_warning()
					<< ", reject unsafe http target: "
					<< std::string(req.target());
				co_await default_http_route(
					req, fake_400_content, http::status::bad_request);
				co_return;
			}

			http_context http_ctx{
				{}, req, req.target(), target_path };

#define BEGIN_HTTP_ROUTE() if (false) {}
#define ON_HTTP_ROUTE(skip, exp, func) \
			else if (skip && boost::regex_match( \
				target, what, boost::regex{ exp })) { \
				for (auto i = 1; i < static_cast<int>(what.size()); i++) \
					http_ctx.regex_results_.emplace_back(what[i]); \
				co_await func(http_ctx); \
			}
#define END_HTTP_ROUTE() else { \
				co_await default_http_route( \
					req, \
					fake_400_content, \
					http::status::bad_request ); }

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
			try {
				fs::path normalized = p.lexically_normal();
				return boost::nowide::narrow(normalized.filename().wstring());
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
			// index.html 同样支持 Last-Modified / ETag 缓存协商.
			boost::system::error_code size_ec;
			auto index_length = fs::file_size(index_html_path, size_ec);
			auto [mtime, mtime_valid, unc_path] = file_last_write_time_raw(index_html_path);
			(void)unc_path;

			std::string last_modified;
			std::string etag;
			if (!size_ec && mtime_valid)
			{
				last_modified = http_date_string(mtime);
				etag = make_file_etag(mtime, index_length);
			}

			if (should_send_not_modified(request, etag, mtime, mtime_valid))
			{
				co_await send_not_modified_http_route(request, etag, last_modified);
				co_return;
			}

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
				if (!etag.empty())
					res.set(http::field::etag, etag);
				if (!last_modified.empty())
					res.set(http::field::last_modified, last_modified);
				res.keep_alive(request.keep_alive());
				res.body() = content;
				res.prepare_payload();

				http::serializer<false, string_body, http::fields> sr(res);
				// HEAD 请求只返回响应头, 不发送 body.
				if (request.method() == http::verb::head)
					co_await http::async_write_header(
						m_local_socket,
						sr,
						net_awaitable[ec]);
				else
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

		auto target_path = make_target_path(std::string(hctx.target_));
		// 预分配并连续追加, 避免多次拼接产生中间临时字符串.
		std::wstring body;
		body.reserve(4096);
		fmt::format_to(std::back_inserter(body), head_fmt, target_path, target_path);
		fmt::format_to(std::back_inserter(body), body_fmt, L"../", L"../", L"", L"", L"");

		for (auto& s : path_list)
			body += s;
		body += tail_fmt;

		string_response res{ http::status::ok, request.version() };
		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string());
		res.keep_alive(request.keep_alive());
		res.body() = boost::nowide::narrow(body);
		res.prepare_payload();

		http::serializer<false, string_body, http::fields> sr(res);
		// HEAD 请求只返回响应头, 不发送 body.
		if (request.method() == http::verb::head)
			co_await http::async_write_header(
				m_local_socket,
				sr,
				net_awaitable[ec]);
		else
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
				if (is_crypto_stream())
					url.set_scheme_id(urls::scheme::https);
				else
					url.set_scheme_id(urls::scheme::http);
				url.set_encoded_authority(request[http::field::host]);
				url.set_path(std::string(hctx.target_) + "/");

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

		// 获取文件元数据, 生成 Last-Modified / ETag 用于缓存协商.
		auto [mtime, mtime_valid, unc_path] = file_last_write_time_raw(path);
		(void)unc_path;

		std::string last_modified;
		std::string etag;
		if (mtime_valid)
		{
			last_modified = http_date_string(mtime);
			etag = make_file_etag(mtime, content_length);
		}

		// 条件请求命中(If-None-Match / If-Modified-Since)则返回 304.
		if (should_send_not_modified(request, etag, mtime, mtime_valid))
		{
			log_conn_debug()
				<< ", http "
				<< hctx.target_
				<< " not modified";

			co_await send_not_modified_http_route(request, etag, last_modified);
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
		// parser_http_ranges 内部使用 string_view 解析并去除空白, 无需复制.
		auto range = parser_http_ranges(request["Range"]);

		// If-Range 校验失败时忽略 Range, 返回完整内容而非部分内容.
		if (!range.empty() && request.count(http::field::if_range))
		{
			auto value = request[http::field::if_range];
			std::string_view if_range(value.data(), value.size());

			if (!if_range_matches(if_range, etag, mtime, mtime_valid))
				range.clear();
		}

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

		// 返回缓存校验头, 便于客户端进行条件请求.
		if (!etag.empty())
			res.set(http::field::etag, etag);
		if (!last_modified.empty())
			res.set(http::field::last_modified, last_modified);

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

		// HEAD 请求只返回响应头, 不发送文件内容.
		if (request.method() == http::verb::head)
			co_return;

		auto buf_size = 5 * 1024 * 1024;
		if (m_option.tcp_rate_limit_ > 0 && m_option.tcp_rate_limit_ < buf_size)
			buf_size = m_option.tcp_rate_limit_;

		std::streamsize total = 0;

		stream_rate_limit(m_local_socket, m_option.tcp_rate_limit_);

#if defined (BOOST_ASIO_HAS_FILE)
		// 双缓冲并发读写, 发送文件时读和写同时进行, 尽量填满 IO.
		std::unique_ptr<char, decltype(&std::free)> buf0((char*)std::malloc(buf_size), &std::free);
		std::unique_ptr<char, decltype(&std::free)> buf1((char*)std::malloc(buf_size), &std::free);
		auto primary_buf = buf0.get();
		auto secondary_buf = buf1.get();

		boost::system::error_code file_ec;

		// 首先读取第一块数据作为预备, 以用于后面的交替读写逻辑.
		std::streamsize remain = content_length;
		auto bytes = co_await file.async_read_some(
			net::buffer(primary_buf, std::min<std::streamsize>(remain, buf_size)),
			net_awaitable[file_ec]);
		remain -= bytes;
		total += bytes;
		if (file_ec || m_abort)
			co_return;

		for (; !m_abort && remain > 0;)
		{
			stream_expires_after(m_local_socket, std::chrono::seconds(m_option.tcp_timeout_));

			auto read_size = std::min<std::streamsize>(remain, buf_size);

			// 并发读写, 将上次读到的数据 primary_buf 发给 socket, 同时继续读取文件.
			auto [write_bytes, read_bytes] =
				co_await(
					net::async_write(m_local_socket,
						net::buffer(primary_buf, bytes),
						net_awaitable[ec])
					&&
					file.async_read_some(
						net::buffer(secondary_buf, read_size),
						net_awaitable[file_ec])
				);
			(void)write_bytes;

			// 交换主从缓冲区.
			std::swap(primary_buf, secondary_buf);

			bytes = read_bytes;
			remain -= bytes;
			total += bytes;

			// 读取到 EOF 或任一方出错即可退出.
			if (read_bytes == 0 || ec || file_ec)
				break;
		}

		if (!m_abort && bytes > 0)
		{
			stream_expires_after(m_local_socket, std::chrono::seconds(m_option.tcp_timeout_));
			co_await net::async_write(
				m_local_socket,
				net::buffer(primary_buf, bytes),
				net_awaitable[ec]);
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
#else
		std::unique_ptr<char, decltype(&std::free)> bufs((char*)std::malloc(buf_size), &std::free);
		char* buf = bufs.get();

		// 响应头已写出, 直接发送文件内容, 不再经由 serializer.
		do
		{
			auto bytes_transferred = fileop::read(
				file,
				std::span<char>(buf, std::min<std::streamsize>(buf_size, content_length - total)));
			if (bytes_transferred <= 0)
				break;

			stream_expires_after(m_local_socket, std::chrono::seconds(m_option.tcp_timeout_));

			co_await net::async_write(
				m_local_socket,
				net::buffer(buf, bytes_transferred),
				net::transfer_all(),
				net_awaitable[ec]);
			total += bytes_transferred;
			if (ec)
			{
				log_conn_warning()
					<< ", http async_write: "
					<< ec.message()
					<< ", already write: "
					<< total;
				co_return;
			}
		} while (!m_abort && total < content_length);
#endif

		XLOG_DBG << "connection id: "
			<< m_connection_id
			<< ", http request: "
			<< hctx.target_
			<< ", completed";

		co_return;
	}

	//////////////////////////////////////////////////////////////////////////
	// 构造函数与析构函数

	proxy_session::proxy_session(
		net::any_io_executor executor,
		net::io_context& backend_context,
		bool scheduler_locking,
		dns_cache& cache,
		variant_stream_type&& socket,
		size_t id,
		std::weak_ptr<proxy_server_base> server
	)
		: m_executor(executor)
		, m_backend_context(backend_context)
		, m_scheduler_locking(scheduler_locking)
		, m_dns_cache(cache)
		, m_local_socket(std::move(socket))
		, m_remote_socket(init_proxy_stream(executor))
		, m_connection_id(id)
		, m_local_buffer(10485760u) // 10MB max buffer size.
		, m_proxy_server(server)
	{
		// 记录连接建立时间（供 launcher 连接列表计算 elapsed）.
		m_started_at = static_cast<int64_t>(std::time(nullptr));
	}

	proxy_session::~proxy_session()
	{
		auto server = m_proxy_server.lock();
		if (!server)
			return;

		// 释放用户连接计数槽位（用户连接数限制）.
		if (!m_conn_user.empty())
			server->release_user_connection(m_conn_user);

		// 上报会话结束，并汇报累计流量.
		server->session_closed(m_connection_id,
			m_total_rx.load(), m_total_tx.load(), m_auth_user);

		// 打印当前 session 数量相关日志.
		auto num = server->num_session();

		log_conn_debug() << ", terminated, " << num << " active connections remaining.";
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

	net::awaitable<void> proxy_session::send_not_modified_http_route(
		const string_request& request,
		const std::string& etag,
		const std::string& last_modified) noexcept
	{
		boost::system::error_code ec;

		string_response res{ http::status::not_modified, request.version() };
		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string());
		if (!etag.empty())
			res.set(http::field::etag, etag);
		if (!last_modified.empty())
			res.set(http::field::last_modified, last_modified);
		res.keep_alive(request.keep_alive());
		res.body() = std::string{};
		res.prepare_payload();

		http::serializer<false, string_body, http::fields> sr(res);
		co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			log_conn_warning()
				<< ", send 304 response error: "
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

net::awaitable<void> proxy_session::method_not_allowed_http_route(const string_request& request) noexcept
{
	boost::system::error_code ec;

	// 405 响应必须携带 Allow 头, 声明服务端支持的方法.
	string_response res{ http::status::method_not_allowed, request.version() };
	res.set(http::field::server, version_string);
	res.set(http::field::date, server_date_string());
	res.set(http::field::content_type, "text/html");
	res.set(http::field::allow, "GET, HEAD");
	res.keep_alive(true);
	res.body() = fake_405_content;
	res.prepare_payload();

	http::serializer<false, string_body, http::fields> sr(res);
	co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
	if (ec)
	{
		log_conn_warning()
			<< ", method not allowed http route err: "
			<< ec.message();
	}

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

} // namespace proxy
