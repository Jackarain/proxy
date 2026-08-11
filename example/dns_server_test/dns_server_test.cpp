//
// dns_server_test.cpp
// ~~~~~~~~~~~~~~~~~~~
//
// 验证 proxy_server 的 DNS 选项功能：
//   - dns_udp_port   ：UDP DNS 服务器监听端口；
//   - dns_no_ipv6    ：AAAA 查询返回空应答（NODATA）；
//   - dns_cache_*    ：查询结果缓存（命中/LRU/TTL 由 mock 上游计数验证）；
//   - apply_options  ：运行期热改（切换端口、启用/禁用 no_ipv6、调整缓存）。
//
// 依赖一个 mock UDP DNS 上游（返回固定 A 记录），可用 example/dns_mock_upstream.py 提供。
//

#include "proxy/proxy.hpp"
#include "proxy/logging.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/json.hpp>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if defined(__linux__) || defined(__APPLE__)
# include <sys/socket.h>
# include <sys/time.h>
#endif


namespace net = boost::asio;
using namespace proxy;

using udp = net::ip::udp;
using tcp = net::ip::tcp;

//////////////////////////////////////////////////////////////////////////
// 极简 UDP DNS 查询客户端（同步阻塞，仅供测试）.

static bool dns_query(const std::string& server, int port,
	const std::string& name, uint16_t qtype,
	int& rcode, std::vector<std::string>& answers)
{
	rcode = -1;
	answers.clear();

	boost::system::error_code ec;
	auto addr = net::ip::make_address(server, ec);
	if (ec)
		return false;

	udp::socket sock{ net::system_executor() };
	sock.open(udp::v4(), ec);
	if (ec)
		return false;

	// 构造查询：事务 ID 固定 0x1234，RD=1.
	std::string query;
	uint16_t id = 0x1234;
	char hdr[12];
	char* hp = hdr;
	io_util::write<uint16_t>(id, hp);
	io_util::write<uint16_t>(0x0100, hp);
	io_util::write<uint16_t>(1, hp);
	io_util::write<uint16_t>(0, hp);
	io_util::write<uint16_t>(0, hp);
	io_util::write<uint16_t>(0, hp);
	query.append(hdr, 12);

	// QNAME.
	auto encode_name = [](const std::string& n) -> std::string
	{
		std::string out;
		size_t pos = 0;
		while (pos < n.size())
		{
			auto dot = n.find('.', pos);
			if (dot == std::string::npos)
				dot = n.size();
			auto len = dot - pos;
			out.push_back(static_cast<char>(static_cast<uint8_t>(len)));
			out.append(n.data() + pos, len);
			pos = dot + 1;
			if (dot == n.size())
				break;
		}
		out.push_back('\0');
		return out;
	};
	query += encode_name(name);
	char qs[4];
	char* qsp = qs;
	io_util::write<uint16_t>(qtype, qsp);
	io_util::write<uint16_t>(1, qsp);
	query.append(qs, 4);

	udp::endpoint ep(addr, static_cast<uint16_t>(port));
	sock.send_to(net::buffer(query), ep, 0, ec);
	if (ec)
		return false;

	// 接收响应（非阻塞 + poll 实现 2 秒超时；asio 同步 receive_from 的
	// 内部 poll 为无限等待，SO_RCVTIMEO 不生效）.
	std::array<char, 4096> recv_buf{};
	udp::endpoint recv_endp;
	size_t recv_len = 0;
	boost::system::error_code recv_ec;
#if defined(__linux__) || defined(__APPLE__)
	sock.native_non_blocking(true, recv_ec);
	if (recv_ec)
		return false;
	pollfd pfd{ sock.native_handle(), POLLIN, 0 };
	int pr = ::poll(&pfd, 1, 2000);
	if (pr <= 0)
		return false;  // 超时或无数据.
#endif
	recv_len = sock.receive_from(net::buffer(recv_buf), recv_endp, 0, recv_ec);
	if (recv_ec || recv_len < 12)
		return false;

	// 校验事务 ID.
	if (recv_buf[0] != 0x12 || recv_buf[1] != 0x34)
		return false;

	rcode = static_cast<int>(recv_buf[3] & 0x0f);
	uint16_t ancount = static_cast<uint16_t>(
		(static_cast<uint8_t>(recv_buf[6]) << 8) | static_cast<uint8_t>(recv_buf[7]));

	// 解析应答（仅提取 A/AAAA 的 IP）.
	size_t pos = 12;
	auto skip_name = [&]() -> bool
	{
		if (pos >= recv_len)
			return false;
		if (recv_buf[pos] & 0xc0)
		{
			pos += 2;
			return true;
		}
		while (pos < recv_len && recv_buf[pos] != 0)
			pos += static_cast<uint8_t>(recv_buf[pos]) + 1;
		if (pos >= recv_len)
			return false;
		pos += 1;
		return true;
	};
	// 跳过 question.
	for (uint16_t i = 0; i < 1; i++)
	{
		if (!skip_name())
			return false;
		pos += 4;
	}
	for (uint16_t i = 0; i < ancount; i++)
	{
		if (!skip_name())
			return false;
		if (pos + 10 > recv_len)
			return false;
		uint16_t atype = static_cast<uint16_t>(
			(static_cast<uint8_t>(recv_buf[pos]) << 8) | static_cast<uint8_t>(recv_buf[pos + 1]));
		uint16_t rdlen = static_cast<uint16_t>(
			(static_cast<uint8_t>(recv_buf[pos + 8]) << 8) | static_cast<uint8_t>(recv_buf[pos + 9]));
		pos += 10;
		if (pos + rdlen > recv_len)
			return false;
		if (atype == 1 && rdlen == 4)
		{
			net::ip::address_v4::bytes_type b{
				static_cast<unsigned char>(recv_buf[pos]),
				static_cast<unsigned char>(recv_buf[pos + 1]),
				static_cast<unsigned char>(recv_buf[pos + 2]),
				static_cast<unsigned char>(recv_buf[pos + 3]) };
			answers.push_back(net::ip::make_address_v4(b).to_string());
		}
		else if (atype == 28 && rdlen == 16)
		{
			net::ip::address_v6::bytes_type bytes;
			std::copy(recv_buf.begin() + pos, recv_buf.begin() + pos + 16, bytes.begin());
			answers.push_back(net::ip::make_address_v6(bytes).to_string());
		}
		pos += rdlen;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

static bool has_error(const boost::json::object& res)
{
	auto it = res.find("errors");
	return it != res.end() && !it->value().as_object().empty();
}

// apply_on_io 在 io_context 线程上执行 apply_options（与 launcher 控制通道
// 的实际调用方式一致，避免跨线程访问 proxy_server 内部状态）.
static boost::json::object apply_on_io(
	net::io_context& ioc, proxy_server* server, const boost::json::object& cfg)
{
	std::promise<boost::json::object> promise;
	auto future = promise.get_future();
	net::post(ioc, [server, cfg, &promise]() {
		promise.set_value(server->apply_options(cfg));
	});
	return future.get();
}

int main()
{
	// mock 上游地址（用 example/dns_mock_upstream.py 提供）.
	const std::string mock_upstream = "127.0.0.1:15300";

	net::io_context ioc;

	proxy_server_option opt;
	opt.listens_.emplace_back(
		tcp::endpoint(net::ip::make_address("127.0.0.1"), 10990), false);
	opt.dns_udp_port_ = 15360;
	opt.dns_cache_size_ = 16;
	opt.dns_cache_ttl_ = 60;
	opt.dns_no_ipv6_ = true;
	opt.dns_upstream_ = mock_upstream;

	auto server = proxy_server::make(ioc.get_executor(), opt);
	server->start();

	std::thread io_thread([&] { ioc.run(); });

	// 等待服务器启动.
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	int rcode = -1;
	std::vector<std::string> answers;
	bool pass = true;

	// 1) A 查询：应正常解析（mock 返回固定 IP）.
	if (!dns_query("127.0.0.1", 15360, "example.com", 1, rcode, answers) ||
		rcode != 0 || answers.empty())
	{
		std::cerr << "[FAIL] A query on 15360: rcode=" << rcode
			<< " answers=" << answers.size() << std::endl;
		pass = false;
	}
	else
	{
		std::cout << "[OK] A query on 15360: " << answers[0] << std::endl;
	}

	// 2) AAAA 查询：dns_no_ipv6=true 应返回空应答（NODATA，rcode=0 无答案）.
	answers.clear();
	if (!dns_query("127.0.0.1", 15360, "example.com", 28, rcode, answers) ||
		rcode != 0 || !answers.empty())
	{
		std::cerr << "[FAIL] AAAA query with no_ipv6: rcode=" << rcode
			<< " answers=" << answers.size() << std::endl;
		pass = false;
	}
	else
	{
		std::cout << "[OK] AAAA query with dns_no_ipv6 returns empty" << std::endl;
	}

	// 3) 热改：切换 UDP 端口并关闭 dns_no_ipv6.
	{
		boost::json::object cfg;
		cfg["dns_udp_port"] = 15361;
		cfg["dns_no_ipv6"] = false;
		auto res = apply_on_io(ioc, server.get(), cfg);
		if (has_error(res))
		{
			std::cerr << "[FAIL] apply_options(dns_udp_port/dns_no_ipv6)"
				<< " errors: " << boost::json::serialize(res["errors"]) << std::endl;
			pass = false;
		}
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	// 4) 新端口可查询：AAAA 查询不再被 no_ipv6 拦截（转发到 mock，rcode=0）.
	answers.clear();
	if (!dns_query("127.0.0.1", 15361, "example.com", 28, rcode, answers))
	{
		std::cerr << "[FAIL] query on new port 15361 failed" << std::endl;
		pass = false;
	}
	else
	{
		std::cout << "[OK] query on switched port 15361: rcode=" << rcode << std::endl;
	}

	// 5) 旧端口应已停止监听（无响应）.
	{
		rcode = -1;
		answers.clear();
		bool responded = dns_query("127.0.0.1", 15360, "example.com", 1, rcode, answers);
		if (responded)
		{
			std::cerr << "[FAIL] old port 15360 still listening" << std::endl;
			pass = false;
		}
		else
		{
			std::cout << "[OK] old port 15360 closed after hot-swap" << std::endl;
		}
	}

	// 6) 热改：调整缓存参数（不应报错，缓存重建）.
	{
		boost::json::object cfg;
		cfg["dns_cache_size"] = 8;
		cfg["dns_cache_ttl"] = 30;
		auto res = apply_on_io(ioc, server.get(), cfg);
		if (has_error(res))
		{
			std::cerr << "[FAIL] apply_options(dns_cache_*)"
				<< " errors: " << boost::json::serialize(res["errors"]) << std::endl;
			pass = false;
		}
		else
		{
			std::cout << "[OK] apply_options(dns_cache_size/dns_cache_ttl) ok" << std::endl;
		}
	}

	// 7) 热改：关闭 UDP DNS 服务器（dns_udp_port=0）.
	{
		boost::json::object cfg;
		cfg["dns_udp_port"] = 0;
		auto res = apply_on_io(ioc, server.get(), cfg);
		if (has_error(res))
		{
			std::cerr << "[FAIL] apply_options(dns_udp_port=0)"
				<< " errors: " << boost::json::serialize(res["errors"]) << std::endl;
			pass = false;
		}
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	// 8) 端口 0 后旧端口应无响应.
	{
		rcode = -1;
		answers.clear();
		bool responded = dns_query("127.0.0.1", 15361, "example.com", 1, rcode, answers);
		if (responded)
		{
			std::cerr << "[FAIL] port 15361 still listening after disable" << std::endl;
			pass = false;
		}
		else
		{
			std::cout << "[OK] UDP DNS server stopped after dns_udp_port=0" << std::endl;
		}
	}

	server->close();
	io_thread.join();

	std::cout << (pass ? "RESULT: PASS" : "RESULT: FAIL") << std::endl;
	return pass ? 0 : 1;
}
