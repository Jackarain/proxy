
#include "proxy/proxy_server.hpp"

#include <boost/json/src.hpp>

namespace proxy
{
	inline constexpr auto head_fmt =
		R"(<html><head><meta charset="UTF-8"><title>Index of {}</title></head><body bgcolor="white"><h1>Index of {}</h1><hr><div><table><tbody>)";
	inline constexpr auto tail_fmt =
		"</tbody></table></div><hr></body></html>";
	inline constexpr auto body_fmt =
		// "<a href=\"{}\">{}</a>{} {}       {}\r\n";
		"<tr><td class=\"link\"><a href=\"{}\">{}</a></td><td class=\"size\">{}</td><td class=\"date\">{}</td></tr>\r\n";


		inline std::string file_hash(const fs::path& p, boost::system::error_code& ec)
		{
			ec = {};

			boost::nowide::ifstream file(p.string(), std::ios::binary);
			if (!file)
			{
				ec = boost::system::error_code(errno,
					boost::system::generic_category());
				return {};
			}

			boost::uuids::detail::sha1 sha1;
			const auto buf_size = 1024 * 1024 * 4;
			std::unique_ptr<char, decltype(&std::free)> bufs((char*)std::malloc(buf_size), &std::free);

			while (file.read(bufs.get(), buf_size) || file.gcount())
				sha1.process_bytes(bufs.get(), file.gcount());

			boost::uuids::detail::sha1::digest_type hash;
			sha1.get_digest(hash);

			std::stringstream ss;
			for (auto const& c : hash)
				ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(c);

			return ss.str();
		}

		template <typename CompletionToken>
		inline auto async_hash_file(const fs::path& path, CompletionToken&& token)
		{
			return net::async_initiate<CompletionToken,
				void (boost::system::error_code, std::string)>(
					[path](auto&& handler) mutable
					{
						std::thread(
							[path, handler = std::move(handler)]() mutable
							{
								boost::system::error_code ec;

								auto hash = file_hash(path, ec);

								auto executor = net::get_associated_executor(handler);
								net::post(executor, [
									ec = std::move(ec),
									hash = std::move(hash),
									handler = std::move(handler)]() mutable
									{
										handler(ec, hash);
									});
							}
						).detach();
					}, token);
		}

	net::awaitable<bool> proxy_session::connect_bridge_proxy(tcp::socket& remote_socket, std::string target_host,
															 uint16_t target_port, boost::system::error_code& ec)
	{
		auto executor = co_await net::this_coro::executor;

		tcp::resolver resolver{executor};

		auto proxy_host = std::string(m_bridge_proxy->host());
		std::string proxy_port;
		if (m_bridge_proxy->port_number() == 0)
		{
			proxy_port = std::to_string(urls::default_port(m_bridge_proxy->scheme_id()));
		}
		else
		{
			proxy_port = std::to_string(m_bridge_proxy->port_number());
		}
		if (proxy_port.empty())
		{
			proxy_port = m_bridge_proxy->scheme();
		}

		XLOG_DBG << "connection id: " << m_connection_id << ", connect to next proxy: " << proxy_host << ":"
				 << proxy_port;

		tcp::resolver::results_type targets;

		if (!detect_hostname(proxy_host))
		{
			net::ip::tcp::endpoint endp(net::ip::address::from_string(proxy_host),
										m_bridge_proxy->port_number()
											? m_bridge_proxy->port_number()
											: urls::default_port(m_bridge_proxy->scheme_id()));

			targets = tcp::resolver::results_type::create(endp, proxy_host, m_bridge_proxy->scheme());
		}
		else
		{
			targets = co_await resolver.async_resolve(proxy_host, proxy_port, net_awaitable[ec]);

			if (ec)
			{
				XLOG_FWARN("connection id: {},"
						   " resolver to next proxy {}:{} error: {}",
						   m_connection_id, std::string(m_bridge_proxy->host()), std::string(m_bridge_proxy->port()),
						   ec.message());

				co_return false;
			}
		}

		if (m_option.happyeyeballs_)
		{
			co_await asio_util::async_connect(remote_socket, targets, [this](const auto& ec, auto& stream, auto& endp)
			{ return check_condition(ec, stream, endp); }, net_awaitable[ec]);
		}
		else
		{
			for (auto endpoint : targets)
			{
				ec = boost::asio::error::host_not_found;

				if (m_option.connect_v4_only_)
				{
					if (endpoint.endpoint().address().is_v6())
					{
						continue;
					}
				}
				else if (m_option.connect_v6_only_)
				{
					if (endpoint.endpoint().address().is_v4())
					{
						continue;
					}
				}

				boost::system::error_code ignore_ec;
				remote_socket.close(ignore_ec);

				if (m_bind_interface)
				{
					tcp::endpoint bind_endpoint(*m_bind_interface, 0);

					remote_socket.open(bind_endpoint.protocol(), ec);
					if (ec)
					{
						break;
					}

					remote_socket.bind(bind_endpoint, ec);
					if (ec)
					{
						break;
					}
				}

				co_await remote_socket.async_connect(endpoint, net_awaitable[ec]);
				if (!ec)
				{
					break;
				}
			}
		}

		if (ec)
		{
			XLOG_FWARN("connection id: {},"
					   " connect to next proxy {}:{} error: {}",
					   m_connection_id, std::string(m_bridge_proxy->host()), std::string(m_bridge_proxy->port()),
					   ec.message());

			co_return false;
		}

		XLOG_DBG << "connection id: " << m_connection_id << ", connect to next proxy: " << proxy_host << ":"
				 << proxy_port << " success";

		// 如果启用了 noise, 则在向上游代理服务器发起 tcp 连接成功后, 发送 noise
		// 数据以及接收 noise 数据.
		if (m_option.scramble_)
		{
			if (!co_await noise_handshake(remote_socket, m_outin_key, m_outout_key))
			{
				co_return false;
			}

			XLOG_DBG << "connection id: " << m_connection_id << ", with upstream noise completed";
		}

		// 使用ssl加密与下一级代理通信.
		if (m_option.proxy_pass_use_ssl_)
		{
			// 设置 ssl cert 证书目录.
			if (fs::exists(m_option.ssl_cacert_path_))
			{
				m_ssl_cli_context.add_verify_path(m_option.ssl_cacert_path_, ec);
				if (ec)
				{
					XLOG_FWARN("connection id: {}, "
							   "load cert path: {}, "
							   "error: {}",
							   m_connection_id, m_option.ssl_cacert_path_, ec.message());

					co_return false;
				}
			}
		}

		auto scheme = m_bridge_proxy->scheme();

		auto instantiate_stream = [this, &scheme, &proxy_host, &remote_socket,
								   &ec]() mutable -> net::awaitable<variant_stream_type>
		{
			ec = {};

			XLOG_DBG << "connection id: " << m_connection_id << ", connect to next proxy: " << proxy_host
					 << " instantiate stream";

			if (m_option.proxy_pass_use_ssl_ || scheme == "https")
			{
				m_ssl_cli_context.set_verify_mode(net::ssl::verify_peer);
				auto cert = default_root_certificates();
				m_ssl_cli_context.add_certificate_authority(net::buffer(cert.data(), cert.size()), ec);
				if (ec)
				{
					XLOG_FWARN("connection id: {},"
							   " add_certificate_authority error: {}",
							   m_connection_id, ec.message());
				}

				m_ssl_cli_context.use_tmp_dh(net::buffer(default_dh_param()), ec);

				m_ssl_cli_context.set_verify_callback(net::ssl::rfc2818_verification(proxy_host), ec);
				if (ec)
				{
					XLOG_FWARN("connection id: {},"
							   " set_verify_callback error: {}",
							   m_connection_id, ec.message());
				}

				// 生成 ssl socket 对象.
				auto sock_stream = init_proxy_stream(std::move(remote_socket), m_ssl_cli_context);

				// get origin ssl stream type.
				ssl_stream& ssl_socket = boost::variant2::get<ssl_stream>(sock_stream);

				if (m_option.scramble_)
				{
					auto& next_layer = ssl_socket.next_layer();

					using NextLayerType = std::decay_t<decltype(next_layer)>;

					if constexpr (!std::same_as<tcp::socket, NextLayerType>)
					{
						next_layer.set_scramble_key(m_outout_key);

						next_layer.set_unscramble_key(m_outin_key);
					}
				}

				std::string sni = m_option.proxy_ssl_name_.empty() ? proxy_host : m_option.proxy_ssl_name_;

				// Set SNI Hostname.
				if (!SSL_set_tlsext_host_name(ssl_socket.native_handle(), sni.c_str()))
				{
					XLOG_FWARN("connection id: {},"
							   " SSL_set_tlsext_host_name error: {}",
							   m_connection_id, ::ERR_get_error());
				}

				XLOG_DBG << "connection id: " << m_connection_id << ", do async ssl handshake...";

				// do async handshake.
				co_await ssl_socket.async_handshake(net::ssl::stream_base::client, net_awaitable[ec]);
				if (ec)
				{
					XLOG_FWARN("connection id: {},"
							   " ssl client protocol handshake error: {}",
							   m_connection_id, ec.message());
				}

				XLOG_FDBG("connection id: {}, ssl handshake: {}", m_connection_id, proxy_host);

				co_return sock_stream;
			}

			auto sock_stream = init_proxy_stream(std::move(remote_socket));

			auto& sock = boost::variant2::get<proxy_tcp_socket>(sock_stream);

			if (m_option.scramble_)
			{
				using NextLayerType = std::decay_t<decltype(sock)>;

				if constexpr (!std::same_as<tcp::socket, NextLayerType>)
				{
					sock.set_scramble_key(m_outout_key);

					sock.set_unscramble_key(m_outin_key);
				}
			}

			co_return sock_stream;
		};

		m_remote_socket = std::move(co_await instantiate_stream());

		XLOG_DBG << "connection id: " << m_connection_id << ", connect to next proxy: " << proxy_host << ":"
				 << proxy_port << " start upstream handshake with " << std::string(scheme);

		if (scheme.starts_with("socks"))
		{
			socks_client_option opt;

			opt.target_host = target_host;
			opt.target_port = target_port;
			opt.proxy_hostname = true;
			opt.username = std::string(m_bridge_proxy->user());
			opt.password = std::string(m_bridge_proxy->password());

			if (scheme == "socks4")
			{
				opt.version = socks4_version;
			}
			else if (scheme == "socks4a")
			{
				opt.version = socks4a_version;
			}

			co_await async_socks_handshake(m_remote_socket, opt, net_awaitable[ec]);
		}
		else if (scheme.starts_with("http"))
		{
			http_proxy_client_option opt;

			opt.target_host = target_host;
			opt.target_port = target_port;
			opt.username = std::string(m_bridge_proxy->user());
			opt.password = std::string(m_bridge_proxy->password());

			co_await async_http_proxy_handshake(m_remote_socket, opt, net_awaitable[ec]);
		}

		if (ec)
		{
			XLOG_FWARN("connection id: {}"
					   ", {} connect to next host {}:{} error: {}",
					   m_connection_id, std::string(scheme), target_host, target_port, ec.message());

			co_return false;
		}

		co_return true;
	}

	net::awaitable<void> proxy_session::on_http_json(const http_context& hctx)
	{
		boost::system::error_code ec;
		auto& request = hctx.request_;

		auto target = make_real_target_path(hctx.command_[0]);

		std::array<std::byte, 4096> pre_alloc_buf;
		std::pmr::monotonic_buffer_resource mbr(pre_alloc_buf.data(), pre_alloc_buf.size());
		std::pmr::polymorphic_allocator<char> alloc(&mbr);

		fs::directory_iterator end;
		fs::directory_iterator it(target, ec);
		if (ec)
		{
			string_response res{
				std::piecewise_construct,
				std::make_tuple(alloc),
				std::make_tuple(http::status::found, request.version(), alloc)
			};
			res.set(http::field::server, version_string);
			res.set(http::field::date, server_date_string(alloc));
			res.set(http::field::location, "/");
			res.keep_alive(request.keep_alive());
			res.prepare_payload();

			string_response_serializer sr(res);
			co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
			if (ec)
			{
				XLOG_WARN << "connection id: " << m_connection_id << ", http_dir write location err: " << ec.message();
			}

			co_return;
		}

		bool hash = false;

		urls::params_view qp(hctx.command_[1]);
		if (qp.find("hash") != qp.end())
		{
			hash = true;
		}

		boost::json::array path_list;

		for (; it != end && !m_abort; it++)
		{
			const auto& item = it->path();
			boost::json::object obj;

			auto [ftime, unc_path] = file_last_wirte_time(item);
			obj["last_write_time"] = ftime;

			if (fs::is_directory(unc_path.empty() ? item : unc_path, ec))
			{
				obj["filename"] = item.filename().string();
				obj["is_dir"] = true;
			}
			else
			{
				obj["filename"] = item.filename().string();
				obj["is_dir"] = false;
				if (unc_path.empty())
				{
					unc_path = item;
				}
				auto sz = fs::file_size(unc_path, ec);
				if (ec)
				{
					sz = 0;
				}
				obj["filesize"] = sz;
				if (hash)
				{
					auto ret = co_await async_hash_file(unc_path, net_awaitable[ec]);
					if (ec)
					{
						ret = "";
					}
					obj["hash"] = ret;
				}
			}

			path_list.push_back(obj);
		}

		auto body = boost::json::serialize(path_list);

		string_response res{
			std::piecewise_construct,
			std::make_tuple(alloc),
			std::make_tuple(http::status::ok, request.version(), alloc)
		};
		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string(alloc));
		res.set(http::field::content_type, "application/json");
		res.keep_alive(request.keep_alive());
		res.body() = body;
		res.prepare_payload();

		string_response_serializer sr(res);
		co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", http dir write body err: " << ec.message();
		}

		co_return;
	}

	net::awaitable<void> proxy_session::on_http_dir(const http_context& hctx)
	{
		using namespace std::literals;

		boost::system::error_code ec;
		auto& request = hctx.request_;

		std::array<std::byte, 16384> pre_alloc_buf;
		std::pmr::monotonic_buffer_resource mbr(pre_alloc_buf.data(), pre_alloc_buf.size());
		std::pmr::polymorphic_allocator<char> alloc(&mbr);

		// 查找目录下是否存在 index.html 或 index.htm 文件, 如果存在则返回该文件.
		// 否则返回目录下的文件列表.
		auto index_html = fs::path(hctx.target_path_) / "index.html";
		fs::exists(index_html, ec) ? index_html = index_html : index_html = fs::path(hctx.target_path_) / "index.htm";

		if (fs::exists(index_html, ec))
		{
			boost::nowide::ifstream file(index_html.string(), std::ios::binary);
			if (file)
			{
				std::pmr::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>(),
										 alloc);

				string_response res{
					std::piecewise_construct,
					std::make_tuple(content, alloc),
					std::make_tuple(http::status::ok, request.version(), alloc)
				};
				res.set(http::field::server, version_string);
				res.set(http::field::date, server_date_string(alloc));
				auto ext = strutil::to_lower(index_html.extension().string());
				if (global_mimes.count(ext))
				{
					res.set(http::field::content_type, global_mimes.at(ext));
				}
				else
				{
					res.set(http::field::content_type, "text/plain");
				}
				res.keep_alive(request.keep_alive());
				res.prepare_payload();

				string_response_serializer sr(res);
				co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
				if (ec)
				{
					XLOG_WARN << "connection id: " << m_connection_id << ", http dir write index err: " << ec.message();
				}

				co_return;
			}
		}

		auto path_list = format_path_list(hctx.target_path_, ec, alloc);

		assert(path_list.get_allocator() == alloc);

		if (ec)
		{
			string_response res{
				std::piecewise_construct,
				std::make_tuple(alloc),
				std::make_tuple(http::status::found, request.version(), alloc)
			};

			res.set(http::field::server, version_string);
			res.set(http::field::date, server_date_string(alloc));
			res.set(http::field::location, "/");
			res.keep_alive(request.keep_alive());
			res.prepare_payload();

			string_response_serializer sr(res);
			co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
			if (ec)
			{
				XLOG_WARN << "connection id: " << m_connection_id << ", http_dir write location err: " << ec.message();
			}

			co_return;
		}

		auto target_path = make_target_path(hctx.target_);
		std::pmr::string autoindex_page{alloc};
		autoindex_page.reserve(4096);
		fmt::format_to(std::back_inserter(autoindex_page), head_fmt, target_path, target_path);
		fmt::format_to(std::back_inserter(autoindex_page), body_fmt, "../", "../", "", "");

		for (const auto& s : path_list)
		{
			autoindex_page += s;
		}

		autoindex_page += tail_fmt;

		string_response res{
			std::piecewise_construct,
			std::make_tuple(std::move(autoindex_page), alloc),
			std::make_tuple(http::status::ok, request.version(), alloc)
		};

		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string(alloc));
		res.keep_alive(request.keep_alive());
		res.prepare_payload();

		string_response_serializer sr(res);
		co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", http dir write body err: " << ec.message();
		}

		co_return;
	}

	net::awaitable<void> proxy_session::on_http_get(const http_context& hctx)
	{
		boost::system::error_code ec;

		const auto& request = hctx.request_;
		const fs::path& path = hctx.target_path_;

		if (!fs::exists(path, ec))
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", http " << hctx.target_ << " file not exists";

			std::pmr::string fake_page{hctx.alloc};

			fmt::vformat_to(std::back_inserter(fake_page), fake_404_content_fmt, fmt::make_format_args(server_date_string(hctx.alloc)));

			co_await net::async_write(m_local_socket, net::buffer(fake_page), net::transfer_all(), net_awaitable[ec]);

			co_return;
		}

		if (fs::is_directory(path, ec))
		{
			XLOG_DBG << "connection id: " << m_connection_id << ", http " << hctx.target_ << " is directory";

			std::pmr::string url = {"http://", hctx.alloc};
			if (is_crytpo_stream())
			{
				url = "https://";
			}
			url += request[http::field::host];
			urls::url u(url);
			std::pmr::string target{hctx.target_ , hctx.alloc};
			target += "/";
			u.set_path(target);

			co_await location_http_route(request, u.buffer());

			co_return;
		}

		size_t content_length = fs::file_size(path, ec);
		if (ec)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", http " << hctx.target_
					  << " file size error: " << ec.message();

			co_await default_http_route(request, fake_400_content, http::status::bad_request);

			co_return;
		}

		boost::nowide::fstream file(path.string(), std::ios_base::binary | std::ios_base::in);

		std::string user_agent;
		if (request.count(http::field::user_agent))
		{
			user_agent = std::string(request[http::field::user_agent]);
		}

		std::string referer;
		if (request.count(http::field::referer))
		{
			referer = std::string(request[http::field::referer]);
		}

		XLOG_DBG << "connection id: " << m_connection_id << ", http file: " << hctx.target_
				 << ", size: " << content_length
				 << (request.count("Range") ? ", range: " + std::string(request["Range"]) : std::string())
				 << (!user_agent.empty() ? ", user_agent: " + user_agent : std::string())
				 << (!referer.empty() ? ", referer: " + referer : std::string());

		http::status st = http::status::ok;
		auto range = parser_http_ranges(request["Range"]);

		// 只支持一个 range 的请求, 不支持多个 range 的请求.
		if (range.size() == 1)
		{
			st = http::status::partial_content;
			auto& r = range.front();

			// 起始位置为 -1, 表示从文件末尾开始读取, 例如 Range: -500
			// 则表示读取文件末尾的 500 字节.
			if (r.first == -1)
			{
				// 如果第二个参数也为 -1, 则表示请求有问题, 返回 416.
				if (r.second < 0)
				{
					co_await default_http_route(request, fake_416_content, http::status::range_not_satisfiable);
					co_return;
				}
				else if (r.second >= 0)
				{
					// 计算起始位置和结束位置, 例如 Range: -5
					// 则表示读取文件末尾的 5 字节.
					// content_length - r.second 表示起始位置.
					// content_length - 1 表示结束位置.
					// 例如文件长度为 10 字节, 则起始位置为 5,
					// 结束位置为 9(数据总长度为[0-9]), 一共 5 字节.
					r.first = content_length - r.second;
					r.second = content_length - 1;
				}
			}
			else if (r.second == -1)
			{
				// 起始位置为正数, 表示从文件头开始读取, 例如 Range: 500
				// 则表示读取文件头的 500 字节.
				if (r.first < 0)
				{
					co_await default_http_route(request, fake_416_content, http::status::range_not_satisfiable);
					co_return;
				}
				else
				{
					r.second = content_length - 1;
				}
			}

			file.seekg(r.first, std::ios_base::beg);
		}

		buffer_response res{
			std::piecewise_construct,
			std::make_tuple(),
			std::make_tuple(st, request.version(), hctx.alloc)
		};

		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string(hctx.alloc));

		auto ext = strutil::to_lower(fs::path(path).extension().string());

		if (global_mimes.count(ext))
		{
			res.set(http::field::content_type, global_mimes.at(ext));
		}
		else
		{
			res.set(http::field::content_type, "text/plain");
		}

		if (st == http::status::ok)
		{
			res.set(http::field::accept_ranges, "bytes");
		}

		if (st == http::status::partial_content)
		{
			const auto& r = range.front();

			if (r.second < r.first && r.second >= 0)
			{
				co_await default_http_route(request, fake_416_content, http::status::range_not_satisfiable);
				co_return;
			}

			std::pmr::string content_range{hctx.alloc};
			fmt::format_to(std::back_inserter(content_range), "bytes {}-{}/{}", r.first, r.second, content_length);

			content_length = r.second - r.first + 1;
			res.set(http::field::content_range, content_range);
		}

		res.keep_alive(hctx.request_.keep_alive());
		res.content_length(content_length);

		response_serializer sr(res);

		res.body().data = nullptr;
		res.body().more = false;

		co_await http::async_write_header(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", http async_write_header: " << ec.message();

			co_return;
		}

		auto buf_size = 5 * 1024 * 1024;
		if (m_option.tcp_rate_limit_ > 0 && m_option.tcp_rate_limit_ < buf_size)
		{
			buf_size = m_option.tcp_rate_limit_;
		}

		std::unique_ptr<char, decltype(&std::free)> bufs((char*)std::malloc(buf_size), &std::free);
		char* buf = bufs.get();
		std::streamsize total = 0;

		stream_rate_limit(m_local_socket, m_option.tcp_rate_limit_);

		do
		{
			auto bytes_transferred = fileop::read(file, std::span<char>(buf, buf_size));
			bytes_transferred = std::min<std::streamsize>(bytes_transferred, content_length - total);
			if (bytes_transferred == 0 || total >= (std::streamsize)content_length)
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

			co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
			total += bytes_transferred;
			if (ec == http::error::need_buffer)
			{
				ec = {};
				continue;
			}
			if (ec)
			{
				XLOG_WARN << "connection id: " << m_connection_id << ", http async_write: " << ec.message()
						  << ", already write: " << total;
				co_return;
			}
		}
		while (!sr.is_done());

		XLOG_DBG << "connection id: " << m_connection_id << ", http request: " << hctx.target_ << ", completed";

		co_return;
	}

	net::awaitable<void> proxy_session::normal_web_server(string_request& req, pmr_alloc_t alloc)
	{
		boost::system::error_code ec;

		bool keep_alive = false;
		bool has_read_header = true;

		for (; !m_abort;)
		{
			std::optional<request_parser> parser;
			if (!has_read_header)
			{
				// normal_web_server 调用是从 http_proxy_get
				// 跳转过来的, 该函数已经读取了请求头, 所以第1次不需
				// 要再次读取请求头, 即 has_read_header 为 true.
				// 当 keepalive 时，需要读取请求头, 此时 has_read_header
				// 为 false, 则在此读取和解析后续的 http 请求头.
				parser.emplace(std::piecewise_construct, std::make_tuple(alloc), std::make_tuple(alloc));
				parser->body_limit(1024 * 512); // 512k
				m_local_buffer.consume(m_local_buffer.size());

				co_await http::async_read_header(m_local_socket, m_local_buffer, *parser, net_awaitable[ec]);
				if (ec)
				{
					XLOG_DBG << "connection id: " << m_connection_id << (keep_alive ? ", keepalive" : "")
							 << ", web async_read_header: " << ec.message();
					co_return;
				}

				req = parser->release();
			}

			if (req[http::field::expect] == "100-continue")
			{
				http::response<http::empty_body> res;
				res.version(11);
				res.result(http::status::method_not_allowed);

				co_await http::async_write(m_local_socket, res, net_awaitable[ec]);
				if (ec)
				{
					XLOG_DBG << "connection id: " << m_connection_id << ", web expect async_write: " << ec.message();
				}
				co_return;
			}

			has_read_header = false;
			keep_alive = req.keep_alive();

			if (beast::websocket::is_upgrade(req))
			{
				std::pmr::string fake_page{alloc};

				fmt::vformat_to(std::back_inserter(fake_page), fake_404_content_fmt, fmt::make_format_args(server_date_string(alloc)));

				co_await net::async_write(m_local_socket, net::buffer(fake_page), net::transfer_all(),
										  net_awaitable[ec]);

				co_return;
			}

			std::pmr::string target{req.target(), alloc};
			boost::match_results<
				std::pmr::string::const_iterator,
				std::pmr::polymorphic_allocator<boost::sub_match<std::pmr::string::const_iterator>>
			> what{alloc};

			http_context http_ctx{
				alloc,
				std::pmr::vector<std::pmr::string>{alloc},
				req,
				req.target(),
				make_real_target_path(req.target())
            };

#define BEGIN_HTTP_ROUTE() if (false) {}

#define ON_HTTP_ROUTE(exp, func)                                                                                       \
	else if (boost::regex_match(target, what, boost::regex{exp}))                                                      \
	{                                                                                                                  \
		for (auto i = 1; i < static_cast<int>(what.size()); i++)                                                       \
			http_ctx.command_.push_back(what[i]);                                                                      \
		co_await func(http_ctx);                                                                                       \
	}
#define END_HTTP_ROUTE()                                                                                               \
	else                                                                                                               \
	{                                                                                                                  \
		co_await default_http_route(req, fake_400_content, http::status::bad_request);                                 \
	}

			BEGIN_HTTP_ROUTE()
                ON_HTTP_ROUTE(R"(^(.*)?\/$)", on_http_dir)
                ON_HTTP_ROUTE(R"(^(.*)?(\/\?q=json.*)$)", on_http_json)
                ON_HTTP_ROUTE(R"(^(?!.*\/$).*$)", on_http_get)
			END_HTTP_ROUTE()

			if (!keep_alive)
			{
				break;
			}
			continue;
		}

		co_await m_local_socket.lowest_layer().async_wait(net::socket_base::wait_read, net_awaitable[ec]);

		co_return;
	}

	int proxy_session::http_authorization(std::string_view pa)
	{
		if (m_option.auth_users_.empty())
		{
			return PROXY_AUTH_SUCCESS;
		}

		if (pa.empty())
		{
			return PROXY_AUTH_NONE;
		}

		auto pos = pa.find(' ');
		if (pos == std::string::npos)
		{
			return PROXY_AUTH_ILLEGAL;
		}

		auto type = pa.substr(0, pos);
		auto auth = pa.substr(pos + 1);

		if (type != "Basic")
		{
			return PROXY_AUTH_ILLEGAL;
		}

		char buff[1024];
		std::pmr::monotonic_buffer_resource mbr(buff, sizeof buff);
		pmr_alloc_t alloc(&mbr);

		std::pmr::string userinfo(beast::detail::base64::decoded_size(auth.size()), 0, alloc);
		auto [len, _] = beast::detail::base64::decode((char*)userinfo.data(), auth.data(), auth.size());
		userinfo.resize(len);

		pos = userinfo.find(':');

		std::pmr::string uname{userinfo.substr(0, pos), alloc};
		std::pmr::string passwd{userinfo.substr(pos + 1), alloc};

		bool verify_passed = m_option.auth_users_.empty();

		for (auto [user, pwd] : m_option.auth_users_)
		{
			if (uname == user && passwd == pwd)
			{
				verify_passed = true;
				user_rate_limit_config(user);
				break;
			}
		}

		auto endp = m_local_socket.remote_endpoint();
		auto client = endp.address().to_string();
		client += ":" + std::to_string(endp.port());

		if (!verify_passed)
		{
			return PROXY_AUTH_FAILED;
		}

		return PROXY_AUTH_SUCCESS;
	}

	net::awaitable<bool> proxy_session::http_proxy_get()
	{
		boost::system::error_code ec;
		bool keep_alive = false;
		bool first = true;

		while (!m_abort)
		{
			std::array<std::byte, 4096> pre_alloc_buf;
			std::pmr::monotonic_buffer_resource mbr(pre_alloc_buf.data(), pre_alloc_buf.size());
			pmr_alloc_t alloc(&mbr);
			std::optional<request_parser> parser;
			parser.emplace(std::piecewise_construct, std::make_tuple(alloc), std::make_tuple(alloc));

			parser->body_limit(1024 * 1024 * 10);
			if (!first)
			{
				m_local_buffer.consume(m_local_buffer.size());
			}

			// 读取 http 请求头.
			co_await http::async_read(m_local_socket, m_local_buffer, *parser, net_awaitable[ec]);
			if (ec)
			{
				XLOG_WARN << "connection id: " << m_connection_id << (keep_alive ? ", keepalive" : "")
						  << ", http_proxy_get request async_read: " << ec.message();

				co_return !first;
			}

			auto req = parser->release();
			auto mth = std::pmr::string(req.method_string(), alloc);
			auto target_view = std::pmr::string(req.target(), alloc);
			auto pa = std::pmr::string(req[http::field::proxy_authorization], alloc);

			keep_alive = req.keep_alive();

			XLOG_DBG << "connection id: " << m_connection_id << ", method: " << mth << ", target: " << target_view
					 << (pa.empty() ? std::pmr::string(alloc) : ", proxy_authorization: " + pa);

			// 判定是否为 GET url 代理模式.
			bool get_url_proxy = false;
			if (boost::istarts_with(target_view, "https://") || boost::istarts_with(target_view, "http://"))
			{
				get_url_proxy = true;
			}

			// http 代理认证, 如果请求的 rarget 不是 http url 或认证
			// 失败, 则按正常 web 请求处理.
			auto auth = http_authorization(pa);
			if (auth != PROXY_AUTH_SUCCESS || !get_url_proxy)
			{
				auto expect_url = urls::parse_absolute_uri(target_view);

				if (!expect_url.has_error())
				{
					XLOG_WARN << "connection id: " << m_connection_id << ", proxy err: " << pauth_error_message(auth);

					co_return !first;
				}

				// 如果 doc 目录为空, 则不允许访问目录
				// 这里直接返回错误页面.
				if (m_option.doc_directory_.empty())
				{
					co_return !first;
				}

				// htpasswd 表示需要用户认证.
				if (m_option.htpasswd_)
				{
					// 处理 http 认证, 如果客户没有传递认证信息, 则返回 401.
					// 如果用户认证信息没有设置, 则直接返回 401.
					auto auth = req[http::field::authorization];
					if (auth.empty() || m_option.auth_users_.empty())
					{
						XLOG_WARN << "connection id: " << m_connection_id
								  << ", auth error: " << (auth.empty() ? "no auth" : "no user");

						co_await unauthorized_http_route(req);
						co_return true;
					}

					auto auth_result = http_authorization(auth);
					if (auth_result != PROXY_AUTH_SUCCESS)
					{
						XLOG_WARN << "connection id: " << m_connection_id
								  << ", auth error: " << pauth_error_message(auth_result);

						co_await unauthorized_http_route(req);
						co_return true;
					}
				}

				// 如果不允许目录索引, 检查请求的是否为文件, 如果是具体文件则按文
				// 件请求处理, 否则返回 403.
				if (!m_option.autoindex_)
				{
					auto path = make_real_target_path(req.target());

					if (!fs::is_directory(path, ec))
					{
						co_await normal_web_server(req, alloc);
						co_return true;
					}

					// 如果不允许目录索引, 则直接返回 403 forbidden.
					co_await forbidden_http_route(req);

					co_return true;
				}

				// 按正常 http 目录请求来处理.
				co_await normal_web_server(req, alloc);
				co_return true;
			}

			const auto authority_pos = target_view.find_first_of("//") + 2;

			std::string host;

			const auto scheme_id = urls::string_to_scheme(target_view.substr(0, authority_pos - 3));
			uint16_t port = urls::default_port(scheme_id);

			auto host_pos = authority_pos;
			auto host_end = std::string::npos;

			auto port_start = std::string::npos;

			for (auto pos = authority_pos; pos < target_view.size(); pos++)
			{
				const auto& c = target_view[pos];
				if (c == '@')
				{
					host_pos = pos + 1;

					host_end = std::string::npos;
					port_start = std::string::npos;
				}
				else if (c == ':')
				{
					host_end = pos;
					port_start = pos + 1;
				}
				else if (c == '/' || (pos + 1 == target_view.size()))
				{
					if (host_end == std::string::npos)
					{
						host_end = pos;
					}
					host = target_view.substr(host_pos, host_end - host_pos);

					if (port_start != std::string::npos)
					{
						port = (uint16_t)std::atoi(target_view.substr(port_start, pos - port_start).c_str());
					}

					break;
				}
			}

			if (!m_remote_socket.is_open())
			{
				// 连接到目标主机.
				co_await start_connect_host(host, port ? port : 80, ec, true);
				if (ec)
				{
					XLOG_FWARN("connection id: {},"
							   " connect to target {}:{} error: {}",
							   m_connection_id, host, port, ec.message());

					co_return !first;
				}
			}

			// 处理代理请求头.
			const auto path_pos = target_view.find_first_of("/", authority_pos);
			if (path_pos == std::string_view::npos)
			{
				req.target("/");
			}
			else
			{
				req.target(std::string(target_view.substr(path_pos)));
			}

			req.set(http::field::host, host);

			if (req.find(http::field::connection) == req.end() && req.find(http::field::proxy_connection) != req.end())
			{
				req.set(http::field::connection, req[http::field::proxy_connection]);
			}

			req.erase(http::field::proxy_authorization);
			req.erase(http::field::proxy_connection);

			co_await http::async_write(m_remote_socket, req, net_awaitable[ec]);
			if (ec)
			{
				XLOG_WARN << "connection id: " << m_connection_id
						  << ", http_proxy_get request async_write: " << ec.message();
				co_return !first;
			}

			m_local_buffer.consume(m_local_buffer.size());
			beast::flat_buffer buf;

			response_parser _parser{std::piecewise_construct, std::make_tuple(alloc), std::make_tuple(alloc)};
			_parser.body_limit(1024 * 1024 * 10);

			auto bytes = co_await http::async_read(m_remote_socket, buf, _parser, net_awaitable[ec]);
			if (ec)
			{
				XLOG_WARN << "connection id: " << m_connection_id
						  << ", http_proxy_get response async_read: " << ec.message();
				co_return !first;
			}

			co_await http::async_write(m_local_socket, _parser.release(), net_awaitable[ec]);
			if (ec)
			{
				XLOG_WARN << "connection id: " << m_connection_id
						  << ", http_proxy_get response async_write: " << ec.message();
				co_return !first;
			}

			XLOG_DBG << "connection id: " << m_connection_id << ", transfer completed"
					 << ", remote to local: " << bytes;

			first = false;
			if (!keep_alive)
			{
				break;
			}
		}

		co_return true;
	}
	net::awaitable<bool> proxy_session::http_proxy_connect()
	{
		http::request<http::string_body> req;
		boost::system::error_code ec;

		// 读取 http 请求头.
		co_await http::async_read(m_local_socket, m_local_buffer, req, net_awaitable[ec]);
		if (ec)
		{
			XLOG_ERR << "connection id: " << m_connection_id << ", http_proxy_connect async_read: " << ec.message();

			co_return false;
		}

		auto mth = std::string(req.method_string());
		auto target_view = std::string(req.target());
		auto pa = std::string(req[http::field::proxy_authorization]);

		XLOG_DBG << "connection id: " << m_connection_id << ", method: " << mth << ", target: " << target_view
				 << (pa.empty() ? std::string() : ", proxy_authorization: " + pa);

		// http 代理认证.
		auto auth = http_authorization(pa);
		if (auth != PROXY_AUTH_SUCCESS)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", proxy err: " << pauth_error_message(auth);

			auto fake_page = fmt::vformat(fake_407_content_fmt, fmt::make_format_args(server_date_string()));

			co_await net::async_write(m_local_socket, net::buffer(fake_page), net::transfer_all(), net_awaitable[ec]);

			co_return true;
		}

		auto pos = target_view.find(':');
		if (pos == std::string::npos)
		{
			XLOG_ERR << "connection id: " << m_connection_id << ", illegal target: " << target_view;
			co_return false;
		}

		std::string host(target_view.substr(0, pos));
		std::string port(target_view.substr(pos + 1));

		co_await start_connect_host(host, static_cast<uint16_t>(std::atol(port.c_str())), ec, true);
		if (ec)
		{
			XLOG_FWARN("connection id: {},"
					   " connect to target {}:{} error: {}",
					   m_connection_id, host, port, ec.message());
			co_return false;
		}

		http::response<http::empty_body> res{http::status::ok, req.version()};
		res.reason("Connection established");

		co_await http::async_write(m_local_socket, res, net_awaitable[ec]);
		if (ec)
		{
			XLOG_FWARN("connection id: {},"
					   " async write response {}:{} error: {}",
					   m_connection_id, host, port, ec.message());
			co_return false;
		}

		size_t l2r_transferred = 0;
		size_t r2l_transferred = 0;

		co_await (transfer(m_local_socket, m_remote_socket, l2r_transferred) &&
				  transfer(m_remote_socket, m_local_socket, r2l_transferred));

		XLOG_DBG << "connection id: " << m_connection_id << ", transfer completed"
				 << ", local to remote: " << l2r_transferred << ", remote to local: " << r2l_transferred;

		co_return true;
	}

	net::awaitable<bool> proxy_session::socks_auth()
	{
		//  +----+------+----------+------+----------+
		//  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
		//  +----+------+----------+------+----------+
		//  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
		//  +----+------+----------+------+----------+
		//  [           ]

		boost::system::error_code ec;
		m_local_buffer.consume(m_local_buffer.size());
		auto bytes =
			co_await net::async_read(m_local_socket, m_local_buffer, net::transfer_exactly(2), net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "connection id: " << m_connection_id
					  << ", read client username/passwd error: " << ec.message();
			co_return false;
		}

		auto p = net::buffer_cast<const char*>(m_local_buffer.data());
		int auth_version = read<int8_t>(p);
		if (auth_version != 1)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", socks negotiation, unsupported socks5 protocol";
			co_return false;
		}
		int name_length = read<uint8_t>(p);
		if (name_length <= 0 || name_length > 255)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", socks negotiation, invalid name length";
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
		bytes = co_await net::async_read(m_local_socket, m_local_buffer, net::transfer_exactly(name_length),
										 net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", read client username error: " << ec.message();
			co_return false;
		}

		std::string uname;

		p = net::buffer_cast<const char*>(m_local_buffer.data());
		for (size_t i = 0; i < bytes - 1; i++)
		{
			uname.push_back(read<int8_t>(p));
		}

		int passwd_len = read<uint8_t>(p);
		if (passwd_len <= 0 || passwd_len > 255)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", socks negotiation, invalid passwd length";
			co_return false;
		}

		//  +----+------+----------+------+----------+
		//  |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
		//  +----+------+----------+------+----------+
		//  | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
		//  +----+------+----------+------+----------+
		//                                [          ]
		m_local_buffer.consume(m_local_buffer.size());
		bytes = co_await net::async_read(m_local_socket, m_local_buffer, net::transfer_exactly(passwd_len),
										 net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", read client passwd error: " << ec.message();
			co_return false;
		}

		std::string passwd;

		p = net::buffer_cast<const char*>(m_local_buffer.data());
		for (size_t i = 0; i < bytes; i++)
		{
			passwd.push_back(read<int8_t>(p));
		}

		// SOCKS5验证用户和密码.
		auto endp = m_local_socket.remote_endpoint();
		auto client = endp.address().to_string();
		client += ":" + std::to_string(endp.port());

		// 用户认证逻辑.
		bool verify_passed = m_option.auth_users_.empty();

		for (auto [user, pwd] : m_option.auth_users_)
		{
			if (uname == user && passwd == pwd)
			{
				verify_passed = true;
				user_rate_limit_config(user);
				break;
			}
		}

		XLOG_DBG << "connection id: " << m_connection_id << ", auth: " << uname << ", passwd: " << passwd
				 << ", client: " << client;

		net::streambuf wbuf;
		auto wp = net::buffer_cast<char*>(wbuf.prepare(16));
		write<uint8_t>(0x01, wp); // version 只能是1.
		if (verify_passed)
		{
			write<uint8_t>(0x00, wp); // 认证通过返回0x00, 其它值为失败.
		}
		else
		{
			write<uint8_t>(0x01, wp); // 认证返回0x01为失败.
		}

		// 返回认证状态.
		//  +----+--------+
		//  |VER | STATUS |
		//  +----+--------+
		//  | 1  |   1    |
		//  +----+--------+
		wbuf.commit(2);
		co_await net::async_write(m_local_socket, wbuf, net::transfer_exactly(2), net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", server write status error: " << ec.message();
			co_return false;
		}

		co_return verify_passed;
	}

	std::pmr::vector<std::pmr::string> proxy_session::format_path_list(
		const std::string& path, boost::system::error_code& ec, pmr_alloc_t alloc)
	{
		fs::directory_iterator end;
		fs::directory_iterator it(path, ec);
		if (ec)
		{
			XLOG_DBG << "connection id: " << m_connection_id << ", format_path_list read dir: " << path
					 << ", error: " << ec.message();
			return {};
		}

		std::pmr::vector<std::pmr::string> path_list{alloc};
		std::pmr::vector<std::pmr::string> file_list{alloc};

		for (; it != end && !m_abort; it++)
		{
			const auto& item = it->path();

			auto [time_string, unc_path] = file_last_wirte_time(item);
			// std::wstring time_string = boost::nowide::widen(ftime);

			std::pmr::string rpath{alloc};

			if (fs::is_directory(unc_path.empty() ? item : unc_path, ec))
			{
				rpath = item.filename().string();
				rpath += "/";

				auto show_path = rpath;
				if (show_path.size() > 50)
				{
					show_path = show_path.substr(0, 47);
					show_path += "..&gt;";
				}
				std::pmr::string str(alloc);
				fmt::format_to(std::back_inserter(str), body_fmt, rpath, show_path, time_string, "-");

				path_list.push_back(std::move(str));
			}
			else
			{
				rpath = item.filename().string();
				std::string filesize;
				if (unc_path.empty())
				{
					unc_path = item;
				}
				auto sz = static_cast<float>(fs::file_size(unc_path, ec));
				if (ec)
				{
					sz = 0;
				}
				filesize = strutil::add_suffix(sz);
				auto show_path = rpath;
				if (show_path.size() > 50)
				{
					show_path = show_path.substr(0, 47);
					show_path += "..&gt;";
				}
				std::pmr::string str(alloc);
				fmt::format_to(std::back_inserter(str), body_fmt, rpath, show_path, time_string, filesize);

				file_list.push_back(std::move(str));
			}
		}

		ec = {};

		path_list.insert(path_list.end(), file_list.begin(), file_list.end());

		return path_list;
	}

	std::pmr::string proxy_session::server_date_string(pmr_alloc_t alloc)
	{
		auto time = std::time(nullptr);
		auto gmt = gmtime((const time_t*)&time);

		std::pmr::string str(64, '\0', alloc);
		auto ret = strftime((char*)str.data(), 64, "%a, %d %b %Y %H:%M:%S GMT", gmt);
		str.resize(ret);

		return str;
	}

	fs::path proxy_session::path_cat(std::string_view doc, std::string_view target)
	{
		size_t start_pos = 0;
		for (auto& c : target)
		{
			if (!(c == '/' || c == '\\'))
			{
				break;
			}

			start_pos++;
		}

		std::array<std::byte, 4096> pre_alloc_buf;
		std::pmr::monotonic_buffer_resource mbr(pre_alloc_buf.data(), pre_alloc_buf.size());
		std::pmr::polymorphic_allocator<char> alloc(&mbr);

		std::string_view sv;
		std::pmr::string slash{"/", alloc};

		if (start_pos < target.size())
		{
			sv = target.substr(start_pos);
		}
#ifdef WIN32
		slash = "\\";
		if (doc.back() == '/' || doc.back() == '\\')
		{
			slash = "";
		}
		auto filename = std::pmr::string(doc, alloc) + slash + std::pmr::string(sv, alloc);
		return fs::path(std::string_view(filename));
#else
		if (doc.back() == '/')
		{
			slash = "";
		}
		return fs::path(std::pmr::string(doc, alloc) + slash + std::pmr::string(sv, alloc));
#endif // WIN32
	}

	net::awaitable<void> proxy_session::default_http_route(const string_request& request, std::string response,
														   http::status status)
	{
		boost::system::error_code ec;

		std::array<std::byte, 4096> pre_alloc_buf;
		std::pmr::monotonic_buffer_resource mbr(pre_alloc_buf.data(), pre_alloc_buf.size());
		pmr_alloc_t alloc(&mbr);

		string_response res{std::piecewise_construct, std::make_tuple(alloc),
							std::make_tuple(status, request.version(), alloc)};

		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string(alloc));
		res.set(http::field::content_type, "text/html");

		res.keep_alive(true);
		res.body() = response;
		res.prepare_payload();

		string_response_serializer sr(res);
		co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", default http route err: " << ec.message();
		}

		co_return;
	}

	net::awaitable<void> proxy_session::location_http_route(const string_request& request, const std::string& path)
	{
		boost::system::error_code ec;

		std::array<std::byte, 4096> pre_alloc_buf;
		std::pmr::monotonic_buffer_resource mbr(pre_alloc_buf.data(), pre_alloc_buf.size());
		pmr_alloc_t alloc(&mbr);

		string_response res{std::piecewise_construct, std::make_tuple(alloc),
							std::make_tuple(http::status::moved_permanently, request.version(), alloc)};

		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string(alloc));
		res.set(http::field::content_type, "text/html");
		res.set(http::field::location, path);

		res.keep_alive(true);
		res.body() = fake_302_content;
		res.prepare_payload();

		string_response_serializer sr(res);
		co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", location http route err: " << ec.message();
		}

		co_return;
	}

	net::awaitable<void> proxy_session::forbidden_http_route(const string_request& request)
	{
		boost::system::error_code ec;

		std::array<std::byte, 4096> pre_alloc_buf;
		std::pmr::monotonic_buffer_resource mbr(pre_alloc_buf.data(), pre_alloc_buf.size());
		pmr_alloc_t alloc(&mbr);

		string_response res{std::piecewise_construct, std::make_tuple(alloc),
							std::make_tuple(http::status::forbidden, request.version(), alloc)};

		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string(alloc));
		res.set(http::field::content_type, "text/html");

		res.keep_alive(true);
		res.body() = fake_403_content;
		res.prepare_payload();

		http::serializer<false, string_body, http::basic_fields<pmr_alloc_t>> sr(res);
		co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", forbidden http route err: " << ec.message();
		}

		co_return;
	}

	net::awaitable<void> proxy_session::unauthorized_http_route(const string_request& request)
	{
		boost::system::error_code ec;

		std::array<std::byte, 4096> pre_alloc_buf;
		std::pmr::monotonic_buffer_resource mbr(pre_alloc_buf.data(), pre_alloc_buf.size());
		pmr_alloc_t alloc(&mbr);

		string_response res{std::piecewise_construct, std::make_tuple(alloc),
							std::make_tuple(http::status::unauthorized, request.version(), alloc)};

		res.set(http::field::server, version_string);
		res.set(http::field::date, server_date_string(alloc));
		res.set(http::field::content_type, "text/html; charset=UTF-8");
		res.set(http::field::www_authenticate, "Basic realm=\"proxy\"");

		res.keep_alive(true);
		res.body() = fake_401_content;
		res.prepare_payload();

		http::serializer<false, string_body, http::basic_fields<pmr_alloc_t>> sr(res);
		co_await http::async_write(m_local_socket, sr, net_awaitable[ec]);
		if (ec)
		{
			XLOG_WARN << "connection id: " << m_connection_id << ", unauthorized http route err: " << ec.message();
		}

		co_return;
	}
} // namespace proxy
