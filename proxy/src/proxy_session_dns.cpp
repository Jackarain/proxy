//
// proxy_session_dns.cpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/proxy_session.hpp"
#include "proxy/async_connect.hpp"
#include "proxy/dns_server.hpp"
#include "proxy/http_error_body.hpp"
#include "proxy/proxy_util.hpp"

#include <vector>


namespace proxy {

	//////////////////////////////////////////////////////////////////////////
	// DNS 查询相关实现

	net::awaitable<void> proxy_session::on_http_dns_query(const http_context& hctx) noexcept
	{
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
						return std::string(query_str.substr(vstart));
					return std::string(query_str.substr(vstart, vend - vstart));
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
			// string_view 指向 request 内部数据, 需要拷贝后修改.
			std::string b64(target.substr(pos + 5));
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

		// 从服务端获取 DNS 缓存与 dns_no_ipv6 配置（UDP DNS 服务器与 HTTP DNS 共享）.
		auto server = m_proxy_server.lock();
		dns_response_cache* dns_cache =
			server ? server->dns_query_cache() : nullptr;
		bool no_ipv6 = server && server->dns_no_ipv6();

		// 解析查询用于 no_ipv6 判断与缓存键.
		std::string qname;
		uint16_t qtype = DNS_TYPE_A;
		bool cd = false;
		bool do_flag = false;
		dns_parse_query(dns_query, qname, qtype);
		dns_query_flags(dns_query, cd, do_flag);

		// 禁用 IPv6 解析返回：AAAA 查询直接返回空应答（NODATA），不转发上游.
		if (no_ipv6 && qtype == DNS_TYPE_AAAA)
		{
			auto resp = dns_build_response(dns_query, 0, {});
			if (resp.empty())
			{
				co_await default_http_route(
					request, fake_500_content_fmt, http::status::internal_server_error);
				co_return;
			}
			log_conn_debug()
				<< ", dns query: " << qname
				<< " type " << dns_type_to_string(qtype)
				<< ", ipv6 disabled, return empty";
			co_await send_http_string_response(
				request, http::status::ok, "application/dns-message", resp, true);
			log_dns_result(dns_query, resp);
			co_return;
		}

		// 缓存命中直接返回（改写事务 ID）.
		std::string cache_key;
		if (dns_cache && !qname.empty())
			cache_key = dns_cache_key(qname, qtype, cd, do_flag);

		if (dns_cache && !cache_key.empty())
		{
			if (auto hit = dns_cache->get(cache_key); hit)
			{
				uint16_t qid = static_cast<uint16_t>(
					(static_cast<uint8_t>(dns_query[0]) << 8) |
					static_cast<uint8_t>(dns_query[1]));
				auto resp = dns_set_id(*hit, qid);
				log_conn_debug()
					<< ", dns query: " << qname
					<< " type " << dns_type_to_string(qtype)
					<< ", cache hit";
				co_await send_http_string_response(
					request, http::status::ok, "application/dns-message", resp, true);
				log_dns_result(dns_query, resp);
				co_return;
			}
		}

		// 判断上游 DNS 服务器类型并转发查询.
		auto& upstream = *m_option.dns_upstream_;
		std::string response;

		if (boost::istarts_with(upstream, "https://"))
		{
			// DoH (DNS over HTTPS) 上游.
			co_await doh_dns_query(request, dns_query, &response);
		}
		else
		{
			// 传统 UDP DNS 上游.
			co_await udp_dns_query(request, dns_query, &response);
		}

		// 写入缓存（剥离事务 ID；SERVFAIL 是临时故障，不缓存）.
		if (dns_cache && !cache_key.empty() &&
			!response.empty() && dns_cacheable(response))
			dns_cache->put(cache_key, dns_strip_id(response));

		co_return;
	}

	// udp_dns_query 通过传统 UDP 协议转发 DNS 查询到上游服务器.
	net::awaitable<void> proxy_session::udp_dns_query(
		const string_request& request, const std::string& dns_query,
		std::string* output) noexcept
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

		auto body = std::string(recv_buf.data(), recv_len);
		if (output)
			*output = body;

		// 返回 DNS 响应.
		co_await send_http_string_response(
			request, http::status::ok, "application/dns-message",
			std::move(body), true);
		log_dns_result(dns_query, std::string(recv_buf.data(), recv_len));

		co_return;
	}

	// DNS 工具函数 (static)

	// 统一的 DNS 类型映射表与 wire 工具函数已移至 dns_server.cpp.

	// build_dns_wire_query / dns_parse_name / dns_svcparams_to_string 已移至 dns_server.cpp.

	// dns_parse_name 已移至 dns_server.cpp.

	// dns_rdata_to_string 已移至 dns_server.cpp.

	// dns_response_to_json 已移至 dns_server.cpp.

	// log_dns_result
	void proxy_session::log_dns_result(
		const std::string& query_msg,
		const std::string& response_msg,
		const std::string& json_body) const noexcept
	{
		// debug 日志被级别过滤时直接返回, 避免无谓的 JSON 解析与格式化.
		if (xlogger::logging_level() > xlogger::_logger_debug_id__)
			return;

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

	// dns_parse_query / dns_query_flags / dns_cache_key / dns_strip_id /
	// dns_set_id / dns_cacheable / dns_build_response 已移至 dns_server.cpp.

	net::awaitable<void> proxy_session::dns_json_query(
		const string_request& request,
		const std::string& question_name,
		uint16_t question_type,
		bool cd_flag,
		bool do_flag) noexcept
	{
		// 1. 构建 DNS wire-format 查询.
		auto dns_query = build_dns_wire_query(
			question_name, question_type, cd_flag, do_flag);

		// 2. 从服务端获取 DNS 缓存与 dns_no_ipv6 配置（UDP DNS 服务器与 HTTP DNS 共享）.
		auto server = m_proxy_server.lock();
		dns_response_cache* dns_cache =
			server ? server->dns_query_cache() : nullptr;
		bool no_ipv6 = server && server->dns_no_ipv6();

		std::string wire_response;

		// 禁用 IPv6 解析返回：AAAA 查询直接返回空应答（NODATA），不转发上游.
		if (no_ipv6 && question_type == DNS_TYPE_AAAA)
		{
			wire_response = dns_build_response(dns_query, 0, {});
		}
		else
		{
			// 缓存键（域名 + 类型 + CD/DO 标志）.
			std::string cache_key;
			if (dns_cache && !question_name.empty())
				cache_key = dns_cache_key(
					question_name, question_type, cd_flag, do_flag);

			// 缓存命中直接返回（改写事务 ID）.
			if (dns_cache && !cache_key.empty())
			{
				if (auto hit = dns_cache->get(cache_key); hit)
				{
					uint16_t qid = static_cast<uint16_t>(
						(static_cast<uint8_t>(dns_query[0]) << 8) |
						static_cast<uint8_t>(dns_query[1]));
					wire_response = dns_set_id(*hit, qid);
				}
			}

			// 未命中缓存：通过上游转发查询.
			if (wire_response.empty())
			{
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

				// 写入缓存（剥离事务 ID；SERVFAIL 是临时故障，不缓存）.
				if (dns_cache && !cache_key.empty() &&
					dns_cacheable(wire_response))
					dns_cache->put(cache_key, dns_strip_id(wire_response));
			}
		}

		if (wire_response.empty())
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

		// 获取进程内共享的 SSL context (按主机名缓存), 避免每个 DoH 请求都重新创建
		// context 并加载 CA 根证书, 该操作开销很大, 会成为 CPU 热点.
		auto doh_ssl_ctx = get_shared_ssl_client_ctx(
			m_option.disable_check_cert_, std::string(doh_host));
		if (doh_ssl_ctx.has_error())
		{
			log_conn_warning()
				<< ", configure ssl context for doh: "
				<< doh_host << " error: " << doh_ssl_ctx.error().message();
			co_return false;
		}

		ssl_tcp_stream ssl_stream(std::move(doh_socket), *doh_ssl_ctx.value());

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
		const string_request& request, const std::string& dns_query,
		std::string* output) noexcept
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

		// 获取进程内共享的 SSL context (按主机名缓存), 避免每个 DoH 请求都重新创建
		// context 并加载 CA 根证书, 该操作开销很大, 会成为 CPU 热点.
		auto doh_ssl_ctx = get_shared_ssl_client_ctx(
			m_option.disable_check_cert_, std::string(doh_host));
		if (doh_ssl_ctx.has_error())
		{
			log_conn_warning()
				<< ", configure ssl context for doh: "
				<< doh_host << " error: " << doh_ssl_ctx.error().message();

			co_await default_http_route(
				request, fake_500_content_fmt, http::status::internal_server_error);
			co_return;
		}

		ssl_tcp_stream ssl_stream(std::move(doh_socket), *doh_ssl_ctx.value());

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
		if (output)
			*output = doh_res.body();
		log_dns_result(dns_query, doh_res.body());

		co_return;
	}

} // namespace proxy
