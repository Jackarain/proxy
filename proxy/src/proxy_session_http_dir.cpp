//
// proxy_session_http_dir.cpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_session.hpp"
#include "proxy/http_error_body.hpp"
#include "proxy_session_http_detail.hpp"


namespace proxy {
	static constexpr auto head_fmt =
		LR"(<html><head><meta charset="UTF-8"><title>Index of {}</title></head><body bgcolor="white"><h1>Index of {}</h1><hr><pre>)";
	static constexpr auto tail_fmt =
		L"</pre><hr></body></html>";
	static constexpr auto body_fmt =
		L"<a href=\"{}\">{}</a>{} {}       {}\r\n";

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
				res.set(http::field::content_type, detail::http_content_type(ext));
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

} // namespace proxy
