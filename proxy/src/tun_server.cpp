//
// tun_server.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/tun_server.hpp"
#include "proxy/proxy_websocket_client.hpp"
#include "proxy/dns_server.hpp"
#include "proxy/http_proxy_client.hpp"
#include "proxy/logging.hpp"
#include "proxy/proxy_util.hpp"
#include "proxy/socks_client.hpp"
#include "proxy/ssl_connect.hpp"
#include "proxy/network_order.hpp"
#include "proxy/wintun_install.hpp"
#include "tun_server_detail.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/functional/hash.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <future>
#include <string>
#include <vector>

namespace proxy {

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

	//////////////////////////////////////////////////////////////////////////
	// tun_server

	tun_server::tun_server(net::any_io_executor executor, proxy_server_option opt)
		: m_ioc(static_cast<net::io_context&>(executor.context()))
		, m_executor(std::move(executor))
		, m_option(std::move(opt))
		, m_tunio(std::make_unique<tunio::tunio>(m_ioc))
	{
		build_match_index();
	}

	// build_match_index 预解析分流规则: proxy_cidr_ 解析为可比较的地址/网段,
	// proxy_domains_ 剔除空项. 规则在 tun_server 生命周期内不再变化, 因此只需
	// 在构造时做一次, 命中判断路径不再重复解析字符串.
	void tun_server::build_match_index()
	{
		m_cidr_rules.reserve(m_option.proxy_cidr_.size());
		for (const auto& cidr : m_option.proxy_cidr_)
		{
			if (!cidr.empty())
				m_cidr_rules.emplace_back(parse_cidr_rule(cidr));
		}

		m_domain_suffixes.reserve(m_option.proxy_domains_.size());
		for (const auto& domain : m_option.proxy_domains_)
		{
			if (!domain.empty())
				m_domain_suffixes.emplace_back(domain);
		}
	}

	std::shared_ptr<tun_server>
	tun_server::make(net::any_io_executor executor, proxy_server_option opt)
	{
		return std::shared_ptr<tun_server>(
			new tun_server(std::move(executor), std::move(opt)));
	}

	tun_server::~tun_server()
	{
		close();
	}

	net::awaitable<boost::system::result<bool>>
	tun_server::make_ssl_socket(tcp::socket& remote_socket,
		std::string_view sni, std::optional<variant_stream_type>& ssl_sock)
	{
		// 复用进程内共享的 SSL 客户端 context 完成 TLS 握手.
		auto ec = co_await ssl_upgrade_client_stream(
			remote_socket, ssl_sock, sni,
			m_option.disable_check_cert_, m_option.ssl_cacert_path_);
		if (ec)
			co_return ec;

		co_return true;
	}

	bool tun_server::open_engine() noexcept
	{
		tunio::tun_config cfg;
		cfg.dev_name = m_option.tun_name_;
		cfg.mtu = m_option.tun_mtu_ > 0 ? m_option.tun_mtu_ : 1500;
		cfg.udp_idle_timeout = std::chrono::seconds(tun_detail::udp_idle_timeout(m_option));

		// 地址/路由由外部脚本或 VpnService 配置，引擎只创建设备；
		// 但环路防护需要本地地址：Linux 下从设备查询实际 IP 供 tunio
		// 丢弃以本地地址为源的回环入包（其余平台由外部配置，留空则不启用）.
#if defined(__linux__)
		tun_detail::query_tun_device_ip(m_option.tun_name_, cfg.ipv4_addr);
#else
		cfg.ipv4_addr.clear();
#endif
		cfg.netmask.clear();

		// Android VpnService 场景: 注入外部已打开的 fd.
		if (m_option.tun_fd_ >= 0)
		{
#if defined(_WIN32)
			// Windows 下 native_handle_type 为 HANDLE(void*), 外部注入
			// 的句柄经 int 传递后按整型宽度转换.
			cfg.external_handle = reinterpret_cast<tunio::native_handle_type>(
				static_cast<intptr_t>(m_option.tun_fd_));
#else
			cfg.external_handle = m_option.tun_fd_;
#endif
			cfg.external_mtu = cfg.mtu;
		}

		boost::system::error_code ec;

#if defined(_WIN32)
		// Windows 下 tunio 使用 wintun 驱动: 未安装时先从 exe 资源解压安装.
		if (!ensure_wintun_driver())
		{
			XLOG_ERR << "tun open engine failed: wintun driver unavailable";
			return false;
		}
#endif
		if (!m_tunio->open(cfg, ec))
		{
			XLOG_ERR << "tun open engine failed: " << ec.message();
			return false;
		}

		m_tcp_acceptor = std::make_unique<tunio::tun_tcp_acceptor>(*m_tunio);
		m_udp_acceptor = std::make_unique<tunio::tun_udp_acceptor>(*m_tunio);

		return true;
	}

	void tun_server::start_accept_loops() noexcept
	{
		m_running = true;

		auto self = shared_from_this();
		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				return run_accept_tcp();
			}, net::detached);
		net::co_spawn(m_executor,
			[this, self]() -> net::awaitable<void>
			{
				return run_accept_udp();
			}, net::detached);
	}

	net::awaitable<void> tun_server::run_accept_tcp()
	{
		auto self = shared_from_this();

		for (; !m_abort;)
		{
			tunio::tun_tcp_socket peer(m_executor);
			boost::system::error_code ec;
			co_await m_tcp_acceptor->async_accept(peer, net_awaitable[ec]);
			if (ec)
				break;
			if (m_abort)
				break;

			const auto remote = peer.remote_endpoint();
			const auto target = peer.original_destination();

#if defined(__linux__)
			// 环路防护：客户端地址等于 tun 设备自身地址说明该连接是
			// 未绕过 tun 的出站连接回环（Android protect 失败 / Linux
			// 未配置 SO_MARK 策略路由），若继续转发将形成连接风暴.
			if (tun_detail::is_tun_loopback_source(remote.address(),
				m_option.tun_name_))
			{
				XLOG_WARN << "tun loopback connection rejected: "
					<< remote.address().to_string() << ":"
					<< remote.port() << " -> "
					<< target.address().to_string() << ":"
					<< target.port();
				peer.reset();
				continue;
			}
#endif

			tcp_flow_key key{
				remote.address(), remote.port(),
				target.address(), target.port()
			};

			{
				std::lock_guard<std::mutex> lk(m_flows_mutex);
				// 并发 flow 超限: 拒绝新连接, 防止流量风暴耗尽 fd.
				if (m_tcp_flows.size() >= k_max_tcp_flows)
					continue;

				auto flow = std::make_shared<tun_tcp_flow>(
					m_executor, self, m_option, key, std::move(peer));
				auto [it, inserted] = m_tcp_flows.emplace(key, flow);
				if (inserted)
				{
					flow->start();
					++m_conn_total;
				}
				else
				{
					// 键冲突（理论不可达）：关闭新连接.
					flow->close();
				}
			}
		}

		co_return;
	}

	net::awaitable<void> tun_server::run_accept_udp()
	{
		auto self = shared_from_this();

		for (; !m_abort;)
		{
			tunio::tun_udp_socket peer(m_executor);
			boost::system::error_code ec;
			co_await m_udp_acceptor->async_accept(peer, net_awaitable[ec]);
			if (ec)
				break;
			if (m_abort)
				break;

#if defined(__linux__)
			// 环路防护（与 TCP 一致）：客户端地址等于 tun 设备自身地址的
			// UDP 会话是未绕过 tun 的出站流量回环，拒绝避免形成风暴.
			if (tun_detail::is_tun_loopback_source(
				peer.client_endpoint().address(), m_option.tun_name_))
			{
				XLOG_WARN << "tun loopback udp session rejected: "
					<< peer.client_endpoint().address().to_string()
					<< ":" << peer.client_endpoint().port();
				continue;
			}
#endif

			// 并发 flow 超限: 关闭会话, 防止流量风暴耗尽 fd.
			{
				std::lock_guard<std::mutex> lk(m_flows_mutex);
				if (m_udp_flows.size() >= k_max_udp_flows)
					continue;
			}

			auto flow = std::make_shared<tun_udp_flow>(
				m_executor, self, m_option, std::move(peer));
			flow->start();
		}

		co_return;
	}

	void tun_server::start() noexcept
	{
		if (m_abort)
			return;

		// 等待外部注入 TUN fd 模式（Android VpnService）：不创建设备,
		// 由 launcher 控制通道 set_tun_fd 注入后启动 accept 循环.
		if (m_option.tun_wait_fd_)
			return;

		// 预建 proxy_pass 连接池（TCP flow 走代理时复用已建立的连接）.
		if (m_option.proxy_pass_ && m_option.proxy_pass_pool_size_ > 0)
			proxy_conn_pool();

		if (!open_engine())
			return;

		XLOG_INFO << "tun device opened, mtu: " << m_tunio->mtu();

		start_accept_loops();
	}

	void tun_server::set_tun_fd(int fd) noexcept
	{
		if (m_abort)
			return;

		// 关闭现有 flow：其流对象绑定旧引擎，重开后全部失效.
		close_flows();

		// 关闭旧引擎（取消挂起 accept，旧 accept 循环随之退出）.
		m_tunio->close();

		// 记录注入 fd，open_engine 以外部句柄模式打开新设备.
		m_option.tun_fd_ = fd;

		if (!open_engine())
			return;

		XLOG_INFO << "tun device injected: fd=" << fd
			<< ", mtu: " << m_tunio->mtu();

		// 重启 accept 循环.
		start_accept_loops();

		// 预建 proxy_pass 连接池（TCP flow 走代理时复用已建立的连接）.
		if (m_option.proxy_pass_ && m_option.proxy_pass_pool_size_ > 0)
			proxy_conn_pool();
	}

	void tun_server::set_protect_handler(
		std::function<net::awaitable<bool>(int)> handler)
	{
		m_protect_handler = std::move(handler);
	}

	void tun_server::set_dns_cache(dns_response_cache* cache) noexcept
	{
		m_dns_cache = cache;
		XLOG_INFO << "tun dns cache "
			<< (cache ? "enabled" : "disabled");
	}

	dns_response_cache* tun_server::dns_cache() const noexcept
	{
		return m_dns_cache;
	}

	net::awaitable<bool> tun_server::protect_socket(int fd)
	{
		if (m_protect_handler)
			co_return co_await m_protect_handler(fd);
		co_return true;
	}

	void tun_server::close_flows() noexcept
	{
		// 锁内搬出 flow，锁外逐个关闭，避免 flow 析构时
		// 反向调用 remove_*_flow 再次加锁造成死锁.
		std::vector<std::shared_ptr<tun_tcp_flow>> tcp_flows;
		std::vector<std::shared_ptr<tun_udp_flow>> udp_flows;
		{
			std::lock_guard<std::mutex> lk(m_flows_mutex);
			tcp_flows.reserve(m_tcp_flows.size());
			for (auto& [key, flow] : m_tcp_flows)
				tcp_flows.push_back(flow);
			udp_flows.reserve(m_udp_flows.size());
			for (auto& [key, flow] : m_udp_flows)
				udp_flows.push_back(flow);
			m_tcp_flows.clear();
			m_udp_flows.clear();
		}

		for (auto& flow : tcp_flows)
			flow->close();
		for (auto& flow : udp_flows)
			flow->close();
	}

	void tun_server::close() noexcept
	{
		auto impl = [this]()
		{
			m_abort = true;
			m_running = false;

			close_flows();

			// 关闭 DoH 连接池（唤醒所有挂起的 DNS 查询）.
			if (m_doh_client)
				m_doh_client->close();

			// 关闭 proxy_pass 预选连接池（释放所有空闲连接）.
			if (m_proxy_pool)
				m_proxy_pool->close();

			if (m_tunio)
				m_tunio->close();
		};

		// close() 可能由非 io_context 线程调用（Android 上 stop 常由
		// 服务工作线程触发）, 而 flows/DoH 连接池/proxy 连接池/tunio 引擎
		// 都在 io_context 线程上被数据通路协程访问: 直接在本线程关闭会与
		// 数据通路产生数据竞争（实测 doh_client 连接池并发释放导致
		// scudo double free）. 投递到 io_context 串行执行后再返回.
		if (m_ioc.stopped())
		{
			// io_context 已停止（析构路径）: 无并发协程, 直接清理.
			impl();
			return;
		}
		// 即使 impl() 抛异常也要解除调用方阻塞, 避免 stop 线程
		// 永久等待（io_context 捕获异常后不会再执行本 handler 剩余部分）.
		std::promise<void> done;
		struct done_guard
		{
			std::promise<void>* p;
			~done_guard()
			{
				if (p)
					p->set_value();
			}
		};
		net::dispatch(m_executor,
			[impl, &done]() mutable
			{
				done_guard g{&done};
				impl();
			});
		// io_context 线程被数据面协程卡死时本 dispatch 永不执行: 无界
		// 等待会永久阻塞调用线程（Android 上为服务工作线程, 卡死后
		// 反复启停全部失败）. 有限等待, 超时交由上层按线程回收情况处理.
		done.get_future().wait_for(std::chrono::milliseconds(2000));
	}

	tun_server::stats tun_server::get_stats() noexcept
	{
		stats st;
		st.rx_bytes = m_rx_bytes.load();
		st.tx_bytes = m_tx_bytes.load();
		st.conn_total = m_conn_total.load();
		st.pool_connections = m_proxy_pool ? m_proxy_pool->idle_count() : 0;
		st.pool_target = m_proxy_pool ? m_proxy_pool->target() : 0;
		{
			std::lock_guard<std::mutex> lk(m_flows_mutex);
			size_t active = m_tcp_flows.size();
			// DNS 查询流（目标 53 端口）为内部基础设施流量, 不纳入活跃连接.
			for (auto& [key, flow] : m_udp_flows)
			{
				if (flow->m_target.port() == 53)
					continue;
				++active;
			}
			st.active_connections = active;
		}
		return st;
	}

	std::vector<tun_server::conn_info> tun_server::connections() noexcept
	{
		std::vector<conn_info> out;

		std::vector<std::shared_ptr<tun_tcp_flow>> tcp_flows;
		std::vector<std::shared_ptr<tun_udp_flow>> udp_flows;
		{
			std::lock_guard<std::mutex> lk(m_flows_mutex);
			tcp_flows.reserve(m_tcp_flows.size());
			for (auto& [key, flow] : m_tcp_flows)
				tcp_flows.push_back(flow);
			udp_flows.reserve(m_udp_flows.size());
			for (auto& [key, flow] : m_udp_flows)
				udp_flows.push_back(flow);
		}

		const auto now = std::chrono::steady_clock::now();
		out.reserve(tcp_flows.size() + udp_flows.size());

		auto append = [&](uint64_t id, const std::string& client,
			const std::string& target, uint8_t proto,
			const std::chrono::steady_clock::time_point& started,
			uint64_t rx, uint64_t tx)
		{
			conn_info c;
			c.id = id;
			c.client_ip = client;
			c.target = target;
			switch (static_cast<tun_conn_proto>(proto))
			{
			case tun_conn_proto::http:
				c.proto = "http";
				break;
			case tun_conn_proto::https:
				c.proto = "https";
				break;
			case tun_conn_proto::socks5:
				c.proto = "socks5";
				break;
			case tun_conn_proto::ws:
				c.proto = "ws";
				break;
			case tun_conn_proto::wss:
				c.proto = "wss";
				break;
			case tun_conn_proto::ws_udp:
				c.proto = "ws-udp";
				break;
			case tun_conn_proto::wss_udp:
				c.proto = "wss-udp";
				break;
			case tun_conn_proto::connect_udp:
				c.proto = "connect-udp";
				break;
			case tun_conn_proto::doh_dns:
				c.proto = "doh-dns";
				break;
			case tun_conn_proto::udp:
				c.proto = "udp";
				break;
			default:
				c.proto = "tcp";
				break;
			}
			auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
				now - started).count();
			c.elapsed = elapsed < 0 ? 0 : static_cast<int64_t>(elapsed);
			c.rx_bytes = rx;
			c.tx_bytes = tx;
			out.emplace_back(std::move(c));
		};

		for (auto& flow : tcp_flows)
		{
			std::string client = flow->m_client.address().to_string() +
				":" + std::to_string(flow->m_client.port());
			std::string target = flow->m_target.address().to_string() +
				":" + std::to_string(flow->m_target.port());
			append(flow->m_conn_id, client, target,
				flow->m_proto.load(), flow->m_started,
				flow->m_rx_bytes.load(), flow->m_tx_bytes.load());
		}

		for (auto& flow : udp_flows)
		{
			// DNS 查询流（目标 53 端口）不列入会话明细, 避免大量
			// 查询刷屏会话列表.
			if (flow->m_target.port() == 53)
				continue;
			std::string client = flow->m_client.address().to_string() +
				":" + std::to_string(flow->m_client.port());
			std::string target = flow->m_target.address().to_string() +
				":" + std::to_string(flow->m_target.port());
			append(flow->m_conn_id, client, target,
				flow->m_proto.load(), flow->m_started,
				flow->m_rx_bytes.load(), flow->m_tx_bytes.load());
		}

		return out;
	}

	uint64_t tun_server::next_conn_id() noexcept
	{
		return m_conn_seq.fetch_add(1, std::memory_order_relaxed) + 1;
	}

	void tun_server::register_udp_flow(const tcp_flow_key& key,
		const std::shared_ptr<tun_udp_flow>& flow)
	{
		std::lock_guard<std::mutex> lk(m_flows_mutex);
		m_udp_flows.emplace(key, flow);
	}

	void tun_server::remove_tcp_flow(const tcp_flow_key& key,
		const tun_tcp_flow* flow)
	{
		std::lock_guard<std::mutex> lk(m_flows_mutex);
		auto it = m_tcp_flows.find(key);
		if (it != m_tcp_flows.end() && it->second.get() == flow)
			m_tcp_flows.erase(it);
	}

	void tun_server::remove_udp_flow(const tcp_flow_key& key,
		const tun_udp_flow* flow)
	{
		std::lock_guard<std::mutex> lk(m_flows_mutex);
		auto it = m_udp_flows.find(key);
		if (it != m_udp_flows.end() && it->second.get() == flow)
			m_udp_flows.erase(it);
	}

	bool tun_server::cidr_match(const net::ip::address& addr) const noexcept
	{
		// 规则已在 build_match_index 预解析, 此处只做地址比较. 显式判断地址族
		// 等价于原实现中 to_v4()/to_v6() 抛异常后跳过该条规则.
		return std::ranges::any_of(m_cidr_rules,
			[&addr](const cidr_rule& rule) -> bool
			{
				if (rule.single)
					return *rule.single == addr;

				if (rule.v4)
				{
					if (!addr.is_v4())
						return false;

					return (addr.to_v4().to_uint() & rule.v4->netmask().to_uint())
						== rule.v4->network().to_uint();
				}

				if (rule.v6)
				{
					if (!addr.is_v6())
						return false;

					auto net6 = net::ip::make_network_v6(
						addr.to_v6(), rule.v6->prefix_length());

					return net6.canonical() == rule.v6->canonical();
				}

				return false;
			});
	}

	bool tun_server::domain_match(const std::string& domain) const noexcept
	{
		// 空项已在 build_match_index 剔除; 后缀匹配要求域名以 '.' + 后缀结尾.
		return std::ranges::any_of(m_domain_suffixes,
			[&domain](const std::string& suffix) -> bool
			{
				if (domain.size() < suffix.size())
					return false;

				if (domain.size() == suffix.size())
					return domain == suffix;

				return domain.ends_with(suffix)
					&& domain[domain.size() - suffix.size() - 1] == '.';
			});
	}

	void tun_server::record_dns_answer(const char* data, size_t len) noexcept
	{
		std::string qname;
		std::vector<std::pair<net::ip::address, uint32_t>> answers;
		if (!tun_detail::parse_dns_response(data, len, qname, answers))
			return;

		// 仅记录命中代理域名列表的解析结果.
		if (!domain_match(qname))
			return;

		auto now = std::chrono::steady_clock::now();
		std::vector<dns_ip_entry> entries;
		entries.reserve(answers.size());
		for (const auto& [ip, ttl] : answers)
		{
			auto expire = now + std::chrono::seconds(ttl > 0 ? ttl : 60);
			entries.push_back({ ip, expire });
		}

		{
			std::lock_guard<std::mutex> lk(m_domain_ips_mutex);
			m_domain_ips[qname] = std::move(entries);
		}
	}

	bool tun_server::ip_match_proxy(const net::ip::address& addr) const noexcept
	{
		// 配置了上游代理但未指定任何分流规则时，默认全部走代理.
		if (m_option.proxy_pass_ &&
			m_option.proxy_cidr_.empty() &&
			m_option.proxy_domains_.empty())
			return true;

		if (cidr_match(addr))
			return true;

		// 域名解析缓存：目标 IP 命中且对应域名在代理表时走代理.
		std::lock_guard<std::mutex> lk(m_domain_ips_mutex);

		auto now = std::chrono::steady_clock::now();
		for (auto it = m_domain_ips.begin(); it != m_domain_ips.end();)
		{
			auto& entries = it->second;

			// 清理过期条目.
			entries.erase(std::remove_if(entries.begin(), entries.end(),
				[now](const dns_ip_entry& e) { return e.expire <= now; }),
				entries.end());

			if (entries.empty())
			{
				it = m_domain_ips.erase(it);
				continue;
			}

			for (const auto& e : entries)
			{
				if (e.ip == addr)
					return true;
			}

			++it;
		}

		return false;
	}

#else // 不支持的平台

	std::shared_ptr<tun_server>
	tun_server::make(net::any_io_executor executor, proxy_server_option opt)
	{
		(void)executor;
		(void)opt;
		return nullptr;
	}

#endif // defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

} // namespace proxy
