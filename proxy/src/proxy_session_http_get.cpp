//
// proxy_session_http_get.cpp
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

		res.set(http::field::content_type, detail::http_content_type(ext));

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

} // namespace proxy
