#include "xproxy.hpp"

#include "proxy/proxy.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/json.hpp>
#include <boost/url.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <pthread.h>
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
		m_thread = std::thread([this]
			{
				m_ioc.run();
				m_finished.store(true, std::memory_order_release);
			});
#if defined(__linux__) && !defined(_WIN32)
		pthread_setname_np(m_thread.native_handle(), "xproxy-ioc");
#endif
	}

	~io_context_pool()
	{
		stop();
	}

	// 停止 io_context 并回收线程. io_context 线程可能因协程忙循环卡死
	// 无法退出, 此时 join 会永久阻塞停止线程 (持锁后后续启停全部失败);
	// 有限等待后分离线程, 返回 true 表示线程已被分离, 池对象必须由
	// 调用方保活 (运行中的线程仍可能访问其 io_context).
	bool stop()
	{
		m_work.reset();
		m_ioc.stop();
		if (m_thread.joinable() &&
			m_thread.get_id() != std::this_thread::get_id())
		{
			const auto deadline = std::chrono::steady_clock::now() +
				std::chrono::milliseconds(4000);
			// 不能以 joinable() 轮询判断线程是否退出: joinable 在 join 前
			// 恒为 true, 会把已退出线程误判为卡死并空等至超时. 以线程
			// 入口设置的完成标志为准: 正常停止时 run() 随 stop() 立即
			// 返回并置位, 等待循环快速结束; 仅真正卡死(协程忙循环)时
			// 才等满超时并 detach.
			while (!m_finished.load(std::memory_order_acquire) &&
				std::chrono::steady_clock::now() < deadline)
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			if (m_finished.load(std::memory_order_acquire))
			{
				if (m_thread.joinable())
					m_thread.join();
			}
			else
			{
				m_thread.detach();
				m_detached = true;
			}
		}
		return m_detached;
	}

	net::any_io_executor executor()
	{
		return m_ioc.get_executor();
	}

private:
	net::io_context m_ioc;
	net::executor_work_guard<net::io_context::executor_type> m_work;
	std::thread m_thread;
	bool m_detached = false;
	std::atomic<bool> m_finished{ false };
};

std::unique_ptr<io_context_pool> g_io_pool;
std::shared_ptr<proxy::proxy_server> g_server;
// 保护 g_server/g_io_pool: start/stop 可能来自不同线程
// (Android 上 stop 常由 UI 线程调用, 启停在工作线程).
std::mutex g_mutex;

// io_context/backend 线程因卡死被分离后, 池与服务对象不能随引用释放
// (运行中的线程仍可能访问其成员). 移入僵尸列表保活; 仅发生在 io_context
// 忙循环无法回收的异常路径, 数量极少, 进程退出时随 OS 回收.
std::vector<std::unique_ptr<io_context_pool>> g_zombie_pools;
std::vector<std::shared_ptr<proxy::proxy_server>> g_zombie_servers;

// 等待 future 完成, 最多等 [timeout]; 超时返回 false (io_context 可能
// 已卡死, 无法再消费任务), 避免 stop 流程被永久阻塞.
bool wait_future(std::future<void>& fut,
	std::chrono::milliseconds timeout = std::chrono::milliseconds(2000))
{
	return fut.wait_for(timeout) == std::future_status::ready;
}

// 停止并释放服务实例; 调用方须持有 g_mutex.
void stop_locked()
{
	bool unclean = false;
	if (g_server)
	{
		g_server->close();
		unclean = g_server->backend_detached();
	}

	// close() 内部（tunio 引擎/launcher 协程等）通过 post/dispatch 在
	// io_context 上异步清理; 停止 io_context 前必须先消费完这些任务,
	// 否则注入的 tun fd 不会被关闭: 会导致 Android VpnService 的 VPN
	// 无法撤销、服务无法销毁, 反复启停后 tun 设备残留直至崩溃.
	// 单线程 io_context 保证已提交任务先于哨兵执行.
	// io_context 卡死时哨兵无法执行: 无界等待会持 g_mutex 永久卡住,
	// 后续所有启停失败 (界面永远等待控制通道).
	if (g_io_pool)
	{
		std::promise<void> done;
		auto fut = done.get_future();
		net::post(g_io_pool->executor(),
			[&done]() { done.set_value(); });
		if (!wait_future(fut))
			unclean = true;
	}

	bool pool_detached = false;
	if (g_io_pool)
		pool_detached = g_io_pool->stop();

	// 线程因卡死被分离后仍可能访问服务对象: 直接 reset 析构会导致分离
	// 线程访问已释放成员而崩溃. 移入僵尸列表保活, 下次 start 新建实例.
	if (unclean || pool_detached)
	{
		if (g_io_pool)
			g_zombie_pools.push_back(std::move(g_io_pool));
		if (g_server)
			g_zombie_servers.push_back(std::move(g_server));
	}

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
