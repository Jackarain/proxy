//
// udp_tproxy_flow.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_10_13__UDP_TPROXY_FLOW_HPP
#define INCLUDE__2026_10_13__UDP_TPROXY_FLOW_HPP


#include "proxy/proxy_stream.hpp"

#include <boost/asio/ip/udp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container_hash/hash.hpp>

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>


namespace proxy {

	namespace net = boost::asio;

	using udp = net::ip::udp;               // from <boost/asio/ip/udp.hpp>

#ifdef __linux__
	using util::variant_stream_type;

	// udp_tproxy_flow_key 由客户端地址和原始目标地址组成. flow 表以完整端点判等,
	// 哈希值只用于定位桶, 避免哈希碰撞导致不同 flow 被误认为同一个.
	struct udp_tproxy_flow_key
	{
		udp::endpoint client_endp;
		udp::endpoint original_endp;

		bool operator==(const udp_tproxy_flow_key&) const = default;
	};

	struct udp_tproxy_flow_key_hash
	{
		size_t operator()(const udp_tproxy_flow_key& key) const noexcept
		{
			size_t h = hash_endpoint(key.client_endp);
			boost::hash_combine(h, hash_endpoint(key.original_endp));
			return h;
		}

	private:
		static size_t hash_endpoint(const udp::endpoint& endp) noexcept
		{
			size_t h = std::hash<net::ip::address>{}(endp.address());
			boost::hash_combine(h, endp.port());
			if (endp.address().is_v6())
				boost::hash_combine(h, endp.address().to_v6().scope_id());
			return h;
		}
	};

	using udp_tproxy_flow_map = std::unordered_map<
		udp_tproxy_flow_key, std::shared_ptr<struct udp_tproxy_flow>,
		udp_tproxy_flow_key_hash>;

	// udp_tproxy_flow 结构体保存每个 UDP TPROXY flow 的状态信息, 包括客户端地址、原始目标地址和 relay socket 等等.
	struct udp_tproxy_flow
	{
		udp_tproxy_flow(const udp::endpoint& client_endp,
			const udp::endpoint& original_endp)
			: client_endp_(client_endp)
			, original_endp_(original_endp)
			, flow_key_(udp_tproxy_flow_key_hash{}(
				{ client_endp, original_endp }))
		{}

		// client_endp_ 保存客户端的地址.
		udp::endpoint client_endp_;

		// original_endp_ 保存客户端请求的原始目标地址.
		udp::endpoint original_endp_;

		// 每个 flow 创建一个用于转发数据包的 backend socket.
		// backend_sock_ 与上游代理服务器收发数据.
		std::optional<udp::socket> backend_sock_;

		// 每个 flow 创建一个用于转发数据包的 relay socket, relay_sock_ 用于伪装成目标服务器
		// 与客户端通信, 从而让客户端认为自己直接与目标服务器通信一样.
		std::optional<udp::socket> relay_sock_;

		// flow_key_ 是客户端端点与原始目标端点的哈希值, 仅用于日志;
		// flow 表的查找和判等使用完整的 udp_tproxy_flow_key.
		size_t flow_key_{ 0 };

		// expire 用于检查 flow 是否已过期.
		int expire_{ 0 };

		// 当使用 HTTP proxy_pass (RFC 9298 connect-udp) 时, 保存与上游的 TCP 连接.
		std::optional<variant_stream_type> udp_http_sock_;
		bool using_connect_udp_{ false };

		// 发送队列, 用于序列化 connect-udp 数据包的 TCP 发送, 避免多个并发协程同时写入
		// udp_http_sock_ 导致 capsule 数据在 TCP 流上交错损坏.
		std::deque<std::vector<char>> send_queue_;

		// 待发送数据包上限, 防止上游握手中或发送阻塞时队列无界增长.
		static constexpr size_t k_max_udp_pending = 1024;

		// closing_ 置位表示该 flow 正在关闭. 读取协程退出时置位并唤醒发送协程,
		// 使最后一个释放 flow 的协程在没有任何在途 I/O 时才销毁 flow 内的流对象.
		bool closing_{ false };

		// 用于通知发送协程有新数据到达的定时器.
		// 当有新的 UDP 数据包推入 send_queue_ 时, 取消此定时器以唤醒发送协程.
		std::optional<net::steady_timer> notify_timer_;
	};
	using udp_tproxy_flow_ptr = std::shared_ptr<udp_tproxy_flow>;
#endif

}


#endif // INCLUDE__2026_10_13__UDP_TPROXY_FLOW_HPP
