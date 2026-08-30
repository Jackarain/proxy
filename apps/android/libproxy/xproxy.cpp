#include "xproxy.hpp"

#include "proxy/proxy.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/json.hpp>
#include <boost/url.hpp>

#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef VERSION_GIT
# define VERSION_GIT ""
#endif

namespace xproxy {

namespace {

namespace net = boost::asio;
namespace json = boost::json;
namespace urls = boost::urls;

// 当前运行的 io_context 池与 proxy 服务实例.
class io_context_pool
{
public:
	io_context_pool()
		: m_work(net::make_work_guard(m_ioc))
	{
		m_thread = std::thread([this] { m_ioc.run(); });
	}

	~io_context_pool()
	{
		stop();
	}

	void stop()
	{
		m_work.reset();
		m_ioc.stop();
		if (m_thread.joinable())
			m_thread.join();
	}

	net::any_io_executor executor()
	{
		return m_ioc.get_executor();
	}

private:
	net::io_context m_ioc;
	net::executor_work_guard<net::io_context::executor_type> m_work;
	std::thread m_thread;
};

std::unique_ptr<io_context_pool> g_io_pool;
std::shared_ptr<proxy::proxy_server> g_server;
// 保护 g_server/g_io_pool: start/stop 可能来自不同线程
// (Android 上 stop 常由 UI 线程调用, 启停在工作线程).
std::mutex g_mutex;

// 停止并释放服务实例; 调用方须持有 g_mutex.
void stop_locked()
{
	if (g_server)
		g_server->close();

	// close() 内部（tunio 引擎/launcher 协程等）通过 post/dispatch 在
	// io_context 上异步清理; 停止 io_context 前必须先消费完这些任务,
	// 否则注入的 tun fd 不会被关闭: 会导致 Android VpnService 的 VPN
	// 无法撤销、服务无法销毁, 反复启停后 tun 设备残留直至崩溃.
	// 单线程 io_context 保证已提交任务先于哨兵执行.
	if (g_io_pool)
	{
		std::promise<void> done;
		net::post(g_io_pool->executor(),
			[&done]() { done.set_value(); });
		done.get_future().wait();
	}

	if (g_io_pool)
		g_io_pool->stop();

	g_server.reset();
	g_io_pool.reset();
}

// 从 json 对象取字符串数组, 不存在或类型不符时返回空.
std::vector<std::string> json_string_array(
	const json::object& obj, const char* key)
{
	std::vector<std::string> out;
	auto it = obj.if_contains(key);
	if (!it || !it->is_array())
		return out;
	for (const auto& v : it->as_array())
	{
		if (v.is_string())
			out.emplace_back(v.as_string().c_str());
	}
	return out;
}

// JSON 配置转 proxy_server_option, 失败返回 false.
bool config_to_option(const std::string& config, proxy::proxy_server_option& opt)
{
	auto value = json::parse(config);
	if (!value.is_object())
		return false;
	const auto& obj = value.as_object();

	// tun 模式必须指定 proxy_pass 上游代理.
	auto proxy_pass = obj.if_contains("proxy_pass");
	if (!proxy_pass || !proxy_pass->is_string() ||
		proxy_pass->as_string().empty())
		return false;
	auto url = urls::parse_uri(proxy_pass->as_string().c_str());
	if (!url.has_value())
		return false;
	opt.proxy_pass_ = url.value();

	// tun 相关选项.
	if (auto it = obj.if_contains("tun"); it && it->is_bool())
		opt.tun_ = it->as_bool();
	else
		opt.tun_ = true;
	if (auto it = obj.if_contains("tun_mtu"); it && it->is_int64())
		opt.tun_mtu_ = static_cast<int>(it->as_int64());
	if (auto it = obj.if_contains("tun_wait_fd"); it && it->is_bool())
		opt.tun_wait_fd_ = it->as_bool();
	if (auto it = obj.if_contains("udp_timeout"); it && it->is_int64())
		opt.udp_timeout_ = static_cast<int>(it->as_int64());

	// proxy_pass 预选连接池大小 (0 表示禁用).
	if (auto it = obj.if_contains("proxy_pass_pool_size"); it && it->is_int64())
		opt.proxy_pass_pool_size_ = static_cast<size_t>(it->as_int64());

	// 分流表.
	opt.proxy_domains_ = json_string_array(obj, "proxy_domains");
	opt.proxy_cidr_ = json_string_array(obj, "proxy_cidr");

	// DNS 分流（国内/国外 DNS 与国外 DoH）.
	opt.dns_domestic_ = json_string_array(obj, "dns_domestic");
	opt.dns_foreign_ = json_string_array(obj, "dns_foreign");
	if (auto it = obj.if_contains("dns_doh"); it && it->is_string())
		opt.dns_doh_ = it->as_string().c_str();

	// DNS 查询结果缓存（TUN 内重复查询命中直接回包）.
	if (auto it = obj.if_contains("dns_cache_size"); it && it->is_int64())
		opt.dns_cache_size_ = static_cast<int>(it->as_int64());
	if (auto it = obj.if_contains("dns_cache_ttl"); it && it->is_int64())
		opt.dns_cache_ttl_ = static_cast<int>(it->as_int64());
	// 禁用 IPv6 解析返回：AAAA 查询直接回空应答，不转发上游.
	if (auto it = obj.if_contains("dns_no_ipv6"); it && it->is_bool())
		opt.dns_no_ipv6_ = it->as_bool();

	// launcher 控制通道 (Android app 端 WebSocket 服务器地址).
	if (auto it = obj.if_contains("launcher_url"); it && it->is_string())
		opt.launcher_url_ = it->as_string().c_str();

	if (auto it = obj.if_contains("disable_check_cert"); it && it->is_bool())
		opt.disable_check_cert_ = it->as_bool();
	// 与 proxy_pass 建立 TLS 连接时使用的 SNI (空表示用 proxy_pass 主机名).
	if (auto it = obj.if_contains("ssl_sni"); it && it->is_string())
		opt.proxy_ssl_name_ = it->as_string().c_str();
	else if (auto it = obj.if_contains("proxy_ssl_name"); it && it->is_string())
		opt.proxy_ssl_name_ = it->as_string().c_str();

	return true;
}

} // namespace

std::string min_sdk_version()
{
	return "minSdkVersion: " + std::to_string(__ANDROID_MIN_SDK_VERSION__);
}

std::string build_version()
{
	// VERSION_GIT 形如 "abc1234 (2026-08-08 10:00:00)"，取 hash 前 6 位。
	std::string v = VERSION_GIT;
	auto pos = v.find(' ');
	if (pos != std::string::npos)
		v = v.substr(0, pos);
	if (v.size() > 6)
		v = v.substr(0, 6);
	return v;
}

int start(const std::string& config)
{
	std::lock_guard<std::mutex> lock(g_mutex);

	// 已有实例先停止, 保证同一时刻只有一个 proxy 服务.
	if (g_server)
		stop_locked();

	try {
		proxy::proxy_server_option opt;
		if (!config_to_option(config, opt))
		{
			stop_locked();
			return -1;
		}

		g_io_pool = std::make_unique<io_context_pool>();
		g_server = proxy::proxy_server::make(
			g_io_pool->executor(), std::move(opt));
		if (!g_server)
		{
			stop_locked();
			return -1;
		}

		g_server->start();
	} catch (...) {
		stop_locked();
		return -1;
	}

	return 0;
}

void stop()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	stop_locked();
}

} // namespace xproxy
