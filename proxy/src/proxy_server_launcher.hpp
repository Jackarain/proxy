//
// proxy_server_launcher.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2019 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__PROXY_PROXY_SERVER_LAUNCHER_HPP
#define INCLUDE__PROXY_PROXY_SERVER_LAUNCHER_HPP

#include "proxy/proxy_server.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

namespace proxy {

//////////////////////////////////////////////////////////////////////////

// launcher 控制通道状态结构体（定义于 .cpp, 避免在头文件中暴露声明;
// 所有使用它的成员函数均实现在本文件）.
struct launcher_state
{
	// 服务启动时间（Unix 秒）与版本标识.
	uint64_t started_at_{ 0 };
	std::string server_version_;

	// 全局累计连接数与收发字节数.
	std::atomic<uint64_t> conn_total_{ 0 };
	std::atomic<uint64_t> global_rx_{ 0 };
	std::atomic<uint64_t> global_tx_{ 0 };

	// 已关闭会话的累计用户流量（user -> (rx, tx, conns)）.
	// conns 为已关闭会话累积连接数，用于状态上报 conn_total（含历史累计）.
	struct user_total
	{
		uint64_t rx{ 0 };
		uint64_t tx{ 0 };
		uint64_t conns{ 0 };
	};
	std::mutex user_mutex_;
	std::map<std::string, user_total> user_totals_;

	// launcher 记录的该用户历史已用流量（字节）。proxy_server 重启后，launcher
	// 会把这份历史用量下发回来，配额判断从“历史用量 + 新增用量”继续累计，
	// 避免重启导致用量归零.
	std::mutex usage_mutex_;
	std::map<std::string, int64_t> user_usage_;

	// 当前已认证的连接数（user -> 连接数），用于用户连接数限制.
	// acquire 在锁内完成“比较 + 自增”，保证并发认证不会超出限制.
	std::mutex user_conn_mutex_;
	std::map<std::string, int> user_conn_count_;

	// 最近一次状态报告（get_status 返回用；仅 io_context 线程访问）.
	boost::json::value last_report_;

	// 控制通道停止标志与当前连接关闭标志.
	std::atomic_bool stopped_{ false };
	std::atomic_bool session_closed_{ false };

	// 当前会话的 protect 请求发送器（serve 期间有效, 会话断开后清空）.
	// 经它向 app 发起 protect 请求放行出站 socket; 为空表示无会话 (放行).
	std::function<net::awaitable<bool>(int)> call_protect_;

	// 在途请求处理协程计数（serve 结束时等待其归零, 确保会话对象存活期内
	// 所有引用会话的协程已完成, 从而无需 shared_ptr 管理会话生命周期）.
	std::atomic<int> active_requests_{ 0 };

	// TLS 客户端上下文（信任 launcher 自签证书）.
	net::ssl::context ssl_ctx_{ net::ssl::context::tls_client };
};

// launcher 控制通道请求处理错误: 由 launcher_dispatch 抛出,
// launcher_handle_request 转换为 JSON-RPC error 响应.
struct launcher_error
{
	int code{ -32000 };
	std::string message;
};

// launcher 控制通道使用的命名空间别名.
namespace json = boost::json;
using launcher_ws = beast::websocket::stream<tcp::socket>;
using launcher_wss = beast::websocket::stream<beast::ssl_stream<tcp::socket>>;

// 状态上报间隔.
inline constexpr std::chrono::milliseconds k_launcher_status_interval{ 2000 };
// protect RPC 超时: app 端无响应时 tinyrpc 调用无超时会永久挂起,
// 黑洞下所有出站连接都会被悬挂; 超时后按未放行处理并取消该调用.
inline constexpr std::chrono::milliseconds k_launcher_protect_timeout{ 3000 };
// 建立连接（含解析/连接/握手）超时.
inline constexpr std::chrono::milliseconds k_launcher_dial_timeout{ 10000 };
// 重连最大退避.
inline constexpr int k_launcher_max_backoff_ms = 30000;
// 未认证（匿名）连接在状态上报中的用户标识. 匿名用户不受配额限制.
inline constexpr const char* k_anon_user = "(匿名)";

namespace detail {

// 判断 websocket 底层流是否为 ssl::stream（wss）.
template <class T>
struct is_ssl_stream : std::false_type {};
template <class T>
struct is_ssl_stream<beast::ssl_stream<T>> : std::true_type {};

// 取 json 对象中的整数字段.
inline std::int64_t json_num(const json::object& obj, const char* key)
{
	auto it = obj.find(key);
	if (it == obj.end())
		return 0;
	const auto& v = it->value();
	if (v.is_int64())
		return v.as_int64();
	if (v.is_uint64())
		return static_cast<std::int64_t>(v.as_uint64());
	if (v.is_double())
		return static_cast<std::int64_t>(v.as_double());
	return 0;
}

// 取 json 对象中的字符串字段.
inline std::string json_str(const json::object& obj, const char* key)
{
	auto it = obj.find(key);
	if (it == obj.end() || !it->value().is_string())
		return {};
	return std::string(it->value().as_string());
}

// 任意值转字符串。
inline std::string to_str(const json::value& v)
{
	if (v.is_string())
		return std::string(v.as_string());
	if (v.is_bool())
		return v.as_bool() ? "true" : "false";
	if (v.is_int64())
		return std::to_string(v.as_int64());
	if (v.is_uint64())
		return std::to_string(v.as_uint64());
	if (v.is_double())
		return json::serialize(v);
	return {};
}

// 任意值转整数（兼容 int / float / string）。
inline int to_int(const json::value& v)
{
	if (v.is_int64())
		return static_cast<int>(v.as_int64());
	if (v.is_uint64())
		return static_cast<int>(v.as_uint64());
	if (v.is_double())
		return static_cast<int>(v.as_double());
	if (v.is_bool())
		return v.as_bool() ? 1 : 0;
	if (v.is_string())
		return std::atoi(std::string(v.as_string()).c_str());
	return 0;
}

// 任意值转布尔。
inline bool to_bool(const json::value& v)
{
	if (v.is_bool())
		return v.as_bool();
	if (v.is_int64())
		return v.as_int64() != 0;
	if (v.is_double())
		return v.as_double() != 0;
	if (v.is_string())
	{
		const auto& s = v.as_string();
		return s == "true" || s == "1" || s == "yes" || s == "on";
	}
	return false;
}

// 任意值转字符串列表。
inline std::vector<std::string> to_str_list(const json::value& v)
{
	std::vector<std::string> out;
	if (v.is_array())
	{
		for (const auto& e : v.as_array())
			out.push_back(to_str(e));
	}
	else if (v.is_string())
	{
		if (!v.as_string().empty())
			out.push_back(std::string(v.as_string()));
	}
	return out;
}

// 解析 server_listen 端点字符串为 (tcp::endpoint, v6only) 元组。
// 支持 ip:port、[ipv6]:port 格式，端口后可选 v6only/-v6only/ipv6only 后缀。
// 成功返回 true 并填充 endp/v6only。
inline bool parse_listen_endpoint(const std::string& str,
	tcp::endpoint& endp, bool& v6only)
{
	v6only = false;
	std::string host, port;
	size_t pos = 0;

	if (!str.empty() && str[0] == '[')
	{
		// [ipv6]:port 格式.
		auto close = str.find(']');
		if (close == std::string::npos)
			return false;
		host = str.substr(1, close - 1);
		pos = close + 1;
	}
	else
	{
		// ip:port 格式.
		auto colon = str.find(':');
		if (colon == std::string::npos)
			return false;
		host = str.substr(0, colon);
		pos = colon;
	}

	if (pos >= str.size() || str[pos] != ':')
		return false;
	pos++;

	// 解析端口.
	auto pstart = pos;
	while (pos < str.size() && str[pos] >= '0' && str[pos] <= '9')
		pos++;
	port = str.substr(pstart, pos - pstart);
	if (port.empty())
		return false;

	// 可选后缀 v6only.
	if (pos < str.size())
	{
		auto opt = str.substr(pos);
		if (opt == "ipv6only" || opt == "-ipv6only" ||
			opt == "v6only" || opt == "-v6only")
			v6only = true;
		else
			return false;
	}

	int p;
	try { p = std::stoi(port); }
	catch (const std::exception&) { return false; }
	if (p < 0 || p > 65535)
		return false;

	boost::system::error_code ec;
	auto addr = net::ip::make_address(host, ec);
	if (ec)
		return false;

	endp = tcp::endpoint(addr, static_cast<unsigned short>(p));
	return true;
}

} // namespace detail

} // namespace proxy

#endif // INCLUDE__PROXY_PROXY_SERVER_LAUNCHER_HPP
