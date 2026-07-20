//
// main.cpp
// ~~~~~~~~
//
// Copyright (c) 2023 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4005 4267 4244)
#endif // _MSC_VER

#ifdef USE_SNMALLOC
# ifdef NDEBUG
#  define SNMALLOC_STATIC_LIBRARY_PREFIX sn_
#  include "src/snmalloc/override/malloc.cc"
#  include "src/snmalloc/override/new.cc"
# endif
#endif // USE_SNMALLOC


#include "proxy/logging.hpp"
#include "proxy/use_awaitable.hpp"
#include "proxy/default_cert.hpp"

#include "main.hpp"
#include "httpc/httpc.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <boost/nowide/args.hpp>
#include <boost/nowide/convert.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include <fmt/format.h>

#include <limits>
#include <fstream>
#include <map>
#include <ctime>

#ifdef _MSC_VER
# pragma warning(pop)
#endif

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;

//////////////////////////////////////////////////////////////////////////

// 各平台 proxy_server 的下载地址
#if defined(__linux__)
#define DOWNLOAD_URL "https://nightly.link/Jackarain/proxy/workflows/Build/master/proxy_server-alpine_musl_x64.zip"
#elif defined(_WIN32)
#define DOWNLOAD_URL "https://nightly.link/Jackarain/proxy/workflows/Build/master/proxy_server-msvc_x64.zip"
#elif defined(__APPLE__)
#define DOWNLOAD_URL "https://nightly.link/Jackarain/proxy/workflows/Build/master/proxy_server-macos-universal.zip"
#endif

// 启动参数
std::string config;
std::string asio_config;
std::string listen_endpoint;
std::string httpd_root;


//////////////////////////////////////////////////////////////////////////

net::awaitable<void> download_proxy_server(net::io_context& ioc)
{
	httpc::http_client httpc(ioc.get_executor(), net::buffer(default_root_certificates()));
    httpc::http_request req;

	req.method(httpc::verb::get);
	httpc.set_download_file("./proxy_server.zip");
	httpc.max_redirects(10);
	httpc.user_agent("curl/8.21.0");

	auto result = co_await httpc.async_perform(
		DOWNLOAD_URL,
		req);
	if (result)
	    auto& resp = *result;    // http_response
	else
		XLOG_ERR << "async_perform failed: " << result.error().message();
	co_return;
}

//////////////////////////////////////////////////////////////////////////
// HTTP 静态文件服务器

namespace httpd_detail {

using string_body = http::string_body;
using string_response = http::response<string_body>;

using buffer_body = http::buffer_body;
using buffer_response = http::response<buffer_body>;
using response_serializer = http::response_serializer<buffer_body, http::fields>;

using dynamic_body = http::dynamic_body;
using dynamic_request = http::request<dynamic_body>;
using request_parser = http::request_parser<dynamic_request::body_type>;
using flat_buffer = beast::flat_buffer;

const auto global_buffer_size = 5 * 1024 * 1024;
const char* version_string = "launcher/1.0";

const static std::map<std::string, std::string> global_mimes =
{
	{ ".html", "text/html; charset=utf-8" },
	{ ".htm", "text/html; charset=utf-8" },
	{ ".js", "application/javascript; charset=utf-8" },
	{ ".json", "application/json; charset=utf-8" },
	{ ".css", "text/css; charset=utf-8" },
	{ ".txt", "text/plain; charset=utf-8" },
	{ ".md", "text/plain; charset=utf-8" },
	{ ".xml", "text/xml" },
	{ ".ico", "image/x-icon" },
	{ ".ttf", "application/x-font-ttf" },
	{ ".pdf", "application/pdf" },
	{ ".png", "image/png" },
	{ ".jpg", "image/jpg" },
	{ ".jpeg", "image/jpg" },
	{ ".gif", "image/gif" },
	{ ".webp", "image/webp" },
	{ ".svg", "image/svg+xml" },
	{ ".wav", "audio/x-wav" },
	{ ".ogg", "video/ogg" },
	{ ".mp3", "audio/mpeg" },
	{ ".mp4", "video/mp4" },
	{ ".zip", "application/zip" },
	{ ".xz", "application/x-xz" },
	{ ".webm", "video/webm" },
	{ ".m3u8", "application/vnd.apple.mpegurl" }
};


inline std::string server_date_string()
{
	auto time = std::time(nullptr);
	auto gmt = std::gmtime(&time);

	std::string str(64, '\0');
	auto ret = std::strftime(str.data(), 64, "%a, %d %b %Y %H:%M:%S GMT", gmt);
	str.resize(ret);
	return str;
}

template <typename Stream>
inline net::awaitable<void> error_session(Stream& stream,
	dynamic_request& req, http::status code, const std::string& message)
{
	boost::system::error_code ec;

	string_response res{code, req.version()};
	res.set(http::field::server, version_string);
	res.set(http::field::content_type, "text/html");
	res.keep_alive(req.keep_alive());
	res.body() = message;
	res.prepare_payload();

	http::response_serializer<string_body, http::fields> sr{res};
	co_await http::async_write(stream, sr, net_awaitable[ec]);
	if (ec)
		XLOG_ERR << "error_session async_write: " << ec.message();
	co_return;
}

inline std::string select_content_type(const fs::path& file)
{
	auto ext = file.extension().string();
	// 转小写
	for (auto& c : ext) c = std::tolower(static_cast<unsigned char>(c));
	auto it = global_mimes.find(ext);
	if (it != global_mimes.end())
		return it->second;
	return "application/octet-stream";
}

template <typename Stream>
inline net::awaitable<void> stream_file_body(Stream& stream,
	buffer_response& res, response_serializer& sr,
	std::fstream& file_stream, size_t content_length)
{
	boost::system::error_code ec;

	res.body().data = nullptr;
	res.body().more = false;

	co_await http::async_write_header(stream, sr, net_awaitable[ec]);
	if (ec)
	{
		XLOG_WARN << "async_write_header: " << ec.message();
		co_return;
	}

	std::vector<char> buffer(global_buffer_size);
	char* bufs = buffer.data();
	std::streamsize total = 0;

	do
	{
		file_stream.read(bufs, global_buffer_size);

		auto bytes_transferred = std::min<std::streamsize>(
			file_stream.gcount(),
			static_cast<std::streamsize>(content_length) - total);

		if (bytes_transferred == 0 ||
			total >= static_cast<std::streamsize>(content_length))
		{
			res.body().data = nullptr;
			res.body().more = false;
		}
		else
		{
			res.body().data = bufs;
			res.body().size = bytes_transferred;
			res.body().more = true;
		}

		co_await http::async_write(stream, sr, net_awaitable[ec]);
		total += bytes_transferred;

		if (ec == http::error::need_buffer)
		{
			ec = {};
			continue;
		}
		if (ec)
		{
			XLOG_WARN << "async_write: " << ec.message();
			co_return;
		}
	} while (!sr.is_done());

	co_return;
}

// 处理文件下载
template <typename Stream>
inline net::awaitable<void> file_session(Stream& stream,
	dynamic_request& req, fs::path file)
{
	if (req.method() != http::verb::get)
	{
		co_await error_session(stream, req,
			http::status::bad_request, "Bad request");
		co_return;
	}

	if (!fs::exists(file))
	{
		co_await error_session(stream, req,
			http::status::not_found, "Not Found");
		co_return;
	}

	boost::system::error_code ec;
	size_t content_length = fs::file_size(file, ec);

	std::fstream file_stream(
		file.string(),
		std::ios_base::binary | std::ios_base::in);

	buffer_response res{http::status::ok, req.version()};
	res.set(http::field::server, version_string);
	res.set(http::field::content_type, select_content_type(file));
	res.set(http::field::accept_ranges, "bytes");
	res.keep_alive(req.keep_alive());
	res.content_length(content_length);

	XLOG_INFO << "serving file: " << file.filename().string()
		<< ", size: " << content_length;

	response_serializer sr(res);

	co_await stream_file_body(
		stream, res, sr, file_stream, content_length);

	co_return;
}

// 解析请求路径并做路径遍历防护.
inline fs::path resolve_request_path(const fs::path& doc_root,
	const std::string& target, boost::system::error_code& ec)
{
	std::string path_part = target;

	// 去掉开头的 '/'
	if (!path_part.empty() && path_part[0] == '/')
		path_part.erase(0, 1);

	// 去掉查询字符串
	auto qpos = path_part.find('?');
	if (qpos != std::string::npos)
		path_part = path_part.substr(0, qpos);

	if (path_part.empty())
		return doc_root;

	auto current_path = fs::canonical(
		doc_root / path_part, ec).make_preferred();

	if (ec || !current_path.string().starts_with(doc_root.string()))
	{
		ec = boost::asio::error::not_found;
		return {};
	}

	return current_path;
}

// 生成目录列表 HTML（类似 nginx autoindex）
inline std::string generate_directory_listing(
	const fs::path& dir_path,
	const std::string& request_target)
{
	std::string html;
	html += "<html>\n<head><title>Index of ";
	html += request_target;
	html += "</title></head>\n<body>\n<h1>Index of ";
	html += request_target;
	html += "</h1><hr><pre>\n";

	// 添加父目录链接
	if (request_target != "/")
	{
		auto parent = fs::path(request_target).parent_path().string();
		if (parent.empty())
			parent = "/";
		html += "<a href=\"";
		html += parent;
		html += "/\">../</a>\n";
	}

	boost::system::error_code ec;
	std::vector<fs::directory_entry> entries;

	for (auto& entry : fs::directory_iterator(dir_path, ec))
	{
		if (ec)
			break;
		entries.push_back(entry);
	}

	// 排序：目录在前，文件在后，按名称字母顺序
	std::sort(entries.begin(), entries.end(),
		[](const fs::directory_entry& a, const fs::directory_entry& b)
		{
			bool a_dir = fs::is_directory(a.path());
			bool b_dir = fs::is_directory(b.path());
			if (a_dir != b_dir)
				return a_dir;  // 目录排在文件前面
			return a.path().filename().string() < b.path().filename().string();
		});

	for (auto& entry : entries)
	{
		auto filename = entry.path().filename().string();
		auto link_name = filename;

		// 对特殊字符进行 HTML 转义
		auto escape_html = [](const std::string& s) -> std::string
		{
			std::string result;
			result.reserve(s.size());
			for (char c : s)
			{
				switch (c)
				{
				case '&': result += "&amp;"; break;
				case '<': result += "&lt;"; break;
				case '>': result += "&gt;"; break;
				case '"': result += "&quot;"; break;
				default: result += c; break;
				}
			}
			return result;
		};

		auto escaped = escape_html(link_name);

		html += "<a href=\"";
		html += escaped;

		bool is_dir = fs::is_directory(entry.path(), ec);

		if (is_dir)
		{
			html += "/\">";
			html += escaped;
			html += "/</a>\n";
		}
		else
		{
			html += "\">";
			html += escaped;
			// 填充空格使时间列对齐
			std::string padding(60 - escaped.size() - 3, ' ');
			if (padding.size() > 60) padding.clear();
			html += "</a>";
			html += padding;

			// 最后修改时间
			auto ft = fs::last_write_time(entry.path(), ec);
			if (!ec)
			{
				std::time_t cftime = static_cast<std::time_t>(ft);
				std::tm tm;
#ifdef _WIN32
				gmtime_s(&tm, &cftime);
#else
				gmtime_r(&cftime, &tm);
#endif
				char timebuf[64];
				std::strftime(timebuf, sizeof(timebuf), "%d-%b-%Y %H:%M", &tm);
				html += timebuf;
				html += "    ";
			}

			// 文件大小
			auto size = fs::file_size(entry.path(), ec);
			if (!ec)
				html += fmt::format("{:>12}", size);
		}
		html += "\n";
	}

	html += "</pre><hr></body>\n</html>\n";
	return html;
}

// 处理目录列表请求
template <typename Stream>
inline net::awaitable<void> directory_listing(
	Stream& stream,
	dynamic_request& req,
	const fs::path& dir_path,
	const std::string& request_target)
{
	boost::system::error_code ec;

	std::string body = generate_directory_listing(dir_path, request_target);

	string_response res{http::status::ok, req.version()};
	res.set(http::field::server, version_string);
	res.set(http::field::content_type, "text/html; charset=utf-8");
	res.keep_alive(req.keep_alive());
	res.body() = body;
	res.prepare_payload();

	http::response_serializer<string_body, http::fields> sr{res};
	co_await http::async_write(stream, sr, net_awaitable[ec]);
	if (ec)
		XLOG_WARN << "directory_listing async_write: " << ec.message();

	co_return;
}

// 处理单个 HTTP 会话
template <typename Stream>
inline net::awaitable<void> http_session(Stream stream, fs::path doc_root)
{
	boost::system::error_code ec;
	flat_buffer buffer;
	buffer.reserve(global_buffer_size);

	for (;;)
	{
		request_parser parser;
		parser.body_limit(std::numeric_limits<uint64_t>::max());
		parser.header_limit(static_cast<uint32_t>(global_buffer_size >> 2));

		co_await http::async_read_header(stream, buffer, parser, net_awaitable[ec]);
		if (ec)
		{
			if (ec != http::error::end_of_stream)
				XLOG_WARN << "async_read_header: " << ec.message();
			co_return;
		}

		dynamic_request req = parser.release();

		if (req.method() != http::verb::get)
		{
			co_await error_session(stream, req,
				http::status::method_not_allowed, "Method not allowed");
			if (!req.keep_alive())
				co_return;
			continue;
		}

		auto current_path = resolve_request_path(
			doc_root, std::string(req.target()), ec);

		if (ec || current_path.empty())
		{
			co_await error_session(stream, req,
				http::status::not_found, "Not Found");
			if (!req.keep_alive())
				co_return;
			continue;
		}

		if (fs::is_directory(current_path))
		{
			// 尝试 index.html / index.htm
			boost::system::error_code iec;
			auto index_path = current_path / "index.html";
			if (!fs::exists(index_path, iec))
				index_path = current_path / "index.htm";
			if (!iec && fs::exists(index_path, iec))
			{
				co_await file_session(stream, req, index_path);
				if (!req.keep_alive())
					co_return;
				continue;
			}

			// 没有 index 文件，生成目录列表
			co_await directory_listing(stream, req,
				current_path, std::string(req.target()));
			if (!req.keep_alive())
				co_return;
			continue;
		}

		if (fs::is_regular_file(current_path))
		{
			co_await file_session(stream, req, current_path);
			if (!req.keep_alive())
				co_return;
			continue;
		}

		co_await error_session(stream, req,
			http::status::not_found, "Not Found");
		if (!req.keep_alive())
			co_return;
	}
}

} // namespace httpd_detail


net::awaitable<void> start_launcher_server(net::io_context& ioc,
	std::string httpd_listen, fs::path doc_root)
{
	using namespace httpd_detail;
	boost::system::error_code ec;

	// 规范化文档根目录路径.
	doc_root = fs::canonical(doc_root, ec);
	if (ec)
	{
		XLOG_ERR << "Invalid document root: "
			<< doc_root.string()
			<< ", err: "
			<< ec.message();
		co_return;
	}

	// 检查文档根目录是否存在且为目录.
	if (!fs::exists(doc_root) || !fs::is_directory(doc_root))
	{
		XLOG_ERR << "Document root does not exist: " << doc_root.string();
		co_return;
	}

	// 解析监听地址和端口.
	std::string host, port;
	bool ipv6only = false;

	// 解析监听地址和端口.
	if (!parse_endpoint_string(httpd_listen, host, port, ipv6only))
	{
		XLOG_ERR << "Invalid httpd listen address: " << httpd_listen;
		co_return;
	}

	auto listen_addr = boost::asio::ip::make_address(host, ec);
	if (ec)
	{
		XLOG_ERR << "Invalid listen address: " << host;
		co_return;
	}

	tcp::acceptor acceptor(ioc,
		tcp::endpoint(listen_addr, (unsigned short)atoi(port.c_str())));

	XLOG_INFO << "HTTP file server listening on "
		<< listen_addr.to_string() << ":" << port
		<< ", document root: " << doc_root.string();

	for (;;)
	{
		boost::system::error_code ec;

		// 接受新连接.
		tcp::socket client = co_await acceptor.async_accept(net_awaitable[ec]);
		if (ec)
		{
			XLOG_ERR << "Accept error: " << ec.message();
			break;
		}

		net::socket_base::keep_alive ka(true);
		client.set_option(ka, ec);
		tcp::no_delay nd(true);
		client.set_option(nd, ec);

		// 创建 tcp_stream 对象.
		beast::tcp_stream stream(std::move(client));

		// 启动 HTTP 会话协程.
		net::co_spawn(
			stream.get_executor(),
			http_session(std::move(stream), doc_root),
			net::detached);
	}

	co_return;
}


//////////////////////////////////////////////////////////////////////////

inline std::optional<std::string> try_as_string(const boost::any& var)
{
	try {
		return boost::any_cast<std::string>(var);
	}
	catch (const boost::bad_any_cast&) {
		return std::nullopt;
	}
}

inline std::optional<bool> try_as_bool(const boost::any& var)
{
	try {
		return boost::any_cast<bool>(var);
	}
	catch (const boost::bad_any_cast&) {
		return std::nullopt;
	}
}

inline std::optional<int> try_as_int(const boost::any& var)
{
	try {
		return boost::any_cast<int>(var);
	}
	catch (const boost::bad_any_cast&) {
		return std::nullopt;
	}
}


inline void print_args(int argc, char** argv, const po::variables_map& vm)
{
	XLOG_INFO << "Current directory: "
		<< fs::current_path().string();

	if (!vm.count("config"))
	{
		std::vector<std::string> args(argv, argv + argc);
		XLOG_INFO << "Run: " << boost::algorithm::join(args, " ");
		return;
	}

	for (const auto& [key, value] : vm)
	{
		if (value.empty() || key == "config")
			continue;

		if (auto s = try_as_string(value.value()))
		{
			XLOG_INFO << key << " = " << *s;
			continue;
		}

		if (auto b = try_as_bool(value.value()))
		{
			XLOG_INFO << key << " = " << *b;
			continue;
		}

		if (auto i = try_as_int(value.value()))
		{
			XLOG_INFO << key << " = " << *i;
			continue;
		}
	}
}

namespace std
{
	std::ostream& operator<<(std::ostream &os, const std::vector<std::string> &users)
	{
		for (auto it = users.begin(); it != users.end();)
		{
			os << *it;
			if (++it == users.end())
				break;
			os << " ";
		}

		return os;
	}
}

//////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
	[[maybe_unused]]boost::nowide::args a(argc, argv);
	platform_init();

	std::string config;

#ifdef NDEBUG
	const bool default_logs = true;
#else
	const bool default_logs = false;
#endif

	po::options_description desc("Options");
	desc.add_options()
		("help,h", "Show this help message and exit.")
		("config", po::value<std::string>(&config)->value_name("config.conf"), "Load configuration options from the specified file.")
		("asio_config", po::value<std::string>(&asio_config)->value_name("enable asio config env")->default_value("ASIO"), "Environment variable name for configuring Boost.Asio (default: ASIO).")
		("httpd_listen", po::value<std::string>(&listen_endpoint)->value_name("addr:port")->default_value("0.0.0.0:8080"), "HTTP server listen address and port (default: 0.0.0.0:8080).")
		("httpd_root", po::value<std::string>(&httpd_root)->value_name("path")->default_value(fs::current_path().string()), "HTTP server document root directory (default: current directory).")
	;

	// 解析命令行.
	po::variables_map vm;

	try {
		po::store(
			po::command_line_parser(argc, argv)
			.options(desc)
			.style(po::command_line_style::unix_style
				| po::command_line_style::allow_long_disguise)
			.run()
			, vm);
		po::notify(vm);
	}
	catch (const po::error& e)
	{
		std::cerr << "Error parsing command line: " << e.what() << "\n";
		return EXIT_FAILURE;
	}

	// 帮助输出.
	if (vm.count("help") || argc == 1)
	{
		version_info();
		std::cout << desc
				  << "\n"
				  << R"(Email bug reports, questions, discussions to <jack.wgm@gmail.com>
and/or open issues at https://github.com/Jackarain/proxy)"
				  << "\n";
		return EXIT_SUCCESS;
	}

	if (vm.count("config"))
	{
		boost::system::error_code ignore_ec;
		if (!fs::exists(config, ignore_ec))
		{
			std::cerr << "No such config file: " << config << std::endl;
			return EXIT_FAILURE;
		}

		try {
			auto cfg = po::parse_config_file(config.c_str(), desc, false);
			po::store(cfg, vm);
			po::notify(vm);
		}
		catch (const po::error& e)
		{
			std::cerr << "Error parsing config file: " << e.what() << "\n";
			return EXIT_FAILURE;
		}
	}

	print_args(argc, argv, vm);

	auto cfg = net::config_from_env(asio_config);
	net::io_context ioc(cfg);
	net::signal_set terminator_signal(ioc);

#ifdef __linux__
	signal(SIGPIPE, SIG_IGN);
#endif

	// 启动 HTTP 服务器.
	net::co_spawn(ioc,
		start_launcher_server(ioc, listen_endpoint, httpd_root),
		net::detached);

	ioc.run();

	return EXIT_SUCCESS;
}
