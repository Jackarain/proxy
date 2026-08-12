//
// manager.cpp
// ~~~~~~~~~~~
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "manager.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <future>
#include <iomanip>
#include <sstream>

#include <openssl/rand.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/filesystem.hpp>

#include "options.hpp"

namespace launcher {

namespace fs = boost::filesystem;
namespace json = boost::json;

// 用户用量持久化的节流间隔。
inline constexpr std::chrono::seconds kUsageSaveInterval{ 5 };
// 进程启动后等待控制通道接入的告警阈值。
inline constexpr std::chrono::seconds kConnectWarnInterval{ 15 };
// 崩溃自动重启参数。
inline constexpr std::chrono::seconds kCrashWindow{ 60 };
inline constexpr int kMaxCrashes = 3;

namespace {

const char* kInstancesFile = "instances.json";

// 任意值转 int64。
std::int64_t as_int64(const json::value& v)
{
	if (v.is_int64())
		return v.as_int64();
	if (v.is_uint64())
		return static_cast<std::int64_t>(v.as_uint64());
	if (v.is_double())
		return static_cast<std::int64_t>(v.as_double());
	if (v.is_bool())
		return v.as_bool() ? 1 : 0;
	if (v.is_string()) {
		try {
			return std::stoll(std::string(v.as_string()));
		} catch (...) {}
	}
	return 0;
}

// 任意值转字符串。
std::string as_string(const json::value& v)
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

// 随机 hex 字符串（crypto 强度，跨平台：OpenSSL RAND_bytes）。
std::string random_hex(std::size_t bytes)
{
	std::string out;
	out.resize(bytes * 2);
	std::vector<unsigned char> buf(bytes);
	if (RAND_bytes(buf.data(), static_cast<int>(bytes)) != 1) {
		// 随机源失败时回退：实例 ID/令牌可预测，认证形同虚设，直接终止。
		std::fprintf(stderr, "crypto random failed\n");
		std::abort();
	}
	static const char* hex = "0123456789abcdef";
	for (std::size_t i = 0; i < bytes; i++) {
		out[i * 2] = hex[buf[i] >> 4];
		out[i * 2 + 1] = hex[buf[i] & 0xf];
	}
	return out;
}

// 取 "user:..." 条目的用户名。
std::string user_of_entry(const std::string& entry)
{
	auto pos = entry.find(':');
	if (pos == std::string::npos)
		return entry;
	return entry.substr(0, pos);
}

// 拼接 user:password[:addr[:proxy_url]]。
std::string build_user_entry(const std::string& user, const std::string& password,
	const std::string& addr, const std::string& proxy_url)
{
	std::string e = user + ":" + password;
	if (!addr.empty() || !proxy_url.empty()) {
		e += ":" + addr;
		if (!proxy_url.empty())
			e += ":" + proxy_url;
	}
	return e;
}

// 替换同名用户或追加新用户条目。
std::vector<std::string> upsert_user_entry(const std::vector<std::string>& list,
	const std::string& user, const std::string& entry)
{
	std::vector<std::string> out;
	bool found = false;
	for (const auto& e : list) {
		if (user_of_entry(e) == user) {
			if (!found) {
				out.push_back(entry);
				found = true;
			}
			continue;
		}
		out.push_back(e);
	}
	if (!found)
		out.push_back(entry);
	return out;
}

// 删除指定用户的条目。
std::vector<std::string> remove_user_entry(const std::vector<std::string>& list,
	const std::string& user)
{
	std::vector<std::string> out;
	for (const auto& e : list)
		if (user_of_entry(e) != user)
			out.push_back(e);
	return out;
}

// 替换指定用户的密码，保留 addr/proxy_url 段。
std::vector<std::string> set_user_password_entry(const std::vector<std::string>& list,
	const std::string& user, const std::string& password)
{
	std::vector<std::string> out;
	for (const auto& e : list) {
		if (user_of_entry(e) == user) {
			std::vector<std::string> parts;
			std::stringstream ss(e);
			std::string token;
			while (std::getline(ss, token, ':'))
				parts.push_back(token);
			std::string addr, proxy_url;
			if (parts.size() >= 3)
				addr = parts[2];
			if (parts.size() >= 4)
				proxy_url = parts[3];
			out.push_back(build_user_entry(user, password, addr, proxy_url));
			continue;
		}
		out.push_back(e);
	}
	return out;
}

// 替换或移除指定用户的独立限速条目（rate<=0 移除）。
std::vector<std::string> set_user_rate_limit_entry(const std::vector<std::string>& list,
	const std::string& user, int rate)
{
	std::vector<std::string> out;
	for (const auto& e : list)
		if (user_of_entry(e) != user)
			out.push_back(e);
	if (rate > 0)
		out.push_back(user + ":" + std::to_string(rate));
	return out;
}

// 替换或移除指定用户的配额条目（quota<=0 移除）。
std::vector<std::string> set_user_quota_entry(const std::vector<std::string>& list,
	const std::string& user, std::int64_t quota)
{
	std::vector<std::string> out;
	for (const auto& e : list)
		if (user_of_entry(e) != user)
			out.push_back(e);
	if (quota > 0)
		out.push_back(user + ":" + std::to_string(quota));
	return out;
}

// 提取配置中的 stringlist。
std::vector<std::string> config_list(const json::object& cfg, const char* key)
{
	if (auto it = cfg.find(key); it != cfg.end())
		return to_string_list(it->value());
	return {};
}

// 写回配置中的 stringlist。
void set_config_list(json::object& cfg, const char* key, const std::vector<std::string>& list)
{
	json::array arr;
	for (const auto& s : list)
		arr.emplace_back(s);
	cfg[key] = std::move(arr);
}

// 2 空格缩进的 JSON 序列化, 便于阅读与跨端调试一致.
std::string serialize_pretty(const json::value& v, int indent = 0)
{
	std::string out;
	switch (v.kind()) {
	case json::kind::object: {
		out += "{\n";
		bool first = true;
		for (const auto& [k, val] : v.as_object()) {
			if (!first)
				out += ",\n";
			first = false;
			out += std::string(indent + 2, ' ') + json::serialize(json::value(k)) + ": " +
				serialize_pretty(val, indent + 2);
		}
		out += "\n" + std::string(indent, ' ') + "}";
		break;
	}
	case json::kind::array: {
		out += "[\n";
		bool first = true;
		for (const auto& val : v.as_array()) {
			if (!first)
				out += ",\n";
			first = false;
			out += std::string(indent + 2, ' ') + serialize_pretty(val, indent + 2);
		}
		out += "\n" + std::string(indent, ' ') + "]";
		break;
	}
	default:
		out += json::serialize(v);
		break;
	}
	return out;
}

} // namespace

manager::manager(std::string data_dir, std::string proxy_path, std::string work_dir)
	: m_data_dir_(std::move(data_dir))
	, m_proxy_path_(std::move(proxy_path))
	, m_work_dir_(std::move(work_dir))
{}

void manager::set_ws_addr(const std::string& host, int port, bool https)
{
	std::lock_guard<std::mutex> lock(m_mu_);
	m_host_ = host;
	m_port_ = port;
	m_https_ = https;
}

std::string manager::persist_path() const
{
	return (fs::path(m_data_dir_) / kInstancesFile).string();
}

std::string manager::pid_file_path(const std::string& id) const
{
	return (fs::path(m_data_dir_) / "pid" / (id + ".pid")).string();
}

// 控制通道连接应使用的 host。
static std::string control_host(const std::string& host)
{
	std::string h = host;
	if (h.size() >= 2 && h.front() == '[' && h.back() == ']')
		h = h.substr(1, h.size() - 2);
	if (h.empty() || h == "0.0.0.0" || h == "::")
		return "127.0.0.1";
	if (h == "::1")
		return "[::1]";
	if (h.find(':') != std::string::npos)
		return "[" + h + "]";
	return h;
}

std::string manager::ws_url(const instance_ptr& in) const
{
	std::string scheme = m_https_ ? "wss" : "ws";
	return scheme + "://" + control_host(m_host_) + ":" + std::to_string(m_port_) +
		"/rpc?instance=" + in->id_ + "&token=" + in->token_;
}

bool manager::load()
{
	if (!load_from_disk())
		return false;
	for (const auto& id : ids()) {
		instance_ptr in = find_instance(id);
		if (in && in->autostart_) {
			std::string err;
			if (!start(id, err))
				std::fprintf(stderr, "[warn] autostart instance %s failed: %s\n", id.c_str(), err.c_str());
		}
	}
	return true;
}

bool manager::load_from_disk()
{
	std::string path = persist_path();
	std::ifstream ifs(path);
	if (!ifs) {
		// 文件不存在视为正常。
		if (!fs::exists(path))
			return true;
		return false;
	}
	std::stringstream ss;
	ss << ifs.rdbuf();
	boost::system::error_code ec;
	auto jv = json::parse(ss.str(), ec);
	if (ec || !jv.is_array())
		return false;

	std::lock_guard<std::mutex> lock(m_mu_);
	for (const auto& item : jv.as_array()) {
		if (!item.is_object())
			continue;
		const auto& obj = item.as_object();
		auto id_it = obj.find("id");
		if (id_it == obj.end() || !id_it->value().is_string())
			continue;
		auto in = std::make_shared<instance>();
		in->id_ = std::string(id_it->value().as_string());
		if (in->id_.empty())
			continue;
		in->name_ = obj.if_contains("name") && obj.at("name").is_string()
			? std::string(obj.at("name").as_string()) : "";
		in->autostart_ = obj.if_contains("autostart") && obj.at("autostart").is_bool()
			? obj.at("autostart").as_bool() : false;
		in->token_ = obj.if_contains("token") && obj.at("token").is_string()
			? std::string(obj.at("token").as_string()) : "";
		if (in->token_.empty())
			in->token_ = random_hex(16);
		if (auto c = obj.if_contains("config"); c && c->is_object())
			in->config_ = c->as_object();
		if (in->config_.empty())
			in->config_ = json::object();
		if (auto u = obj.if_contains("user_usage"); u && u->is_object())
			in->user_usage_ = u->as_object();
		if (auto t = obj.if_contains("created_at"); t && t->is_string()) {
			time_point tp;
			if (rfc3339_parse(std::string(t->as_string()), tp))
				in->created_at_ = tp;
		}
		// 兼容旧配置：proxy_ssl_name 已并入 ssl_sni。
		if (auto it = in->config_.find("proxy_ssl_name"); it != in->config_.end()) {
			if (!in->config_.contains("ssl_sni"))
				in->config_["ssl_sni"] = it->value();
			in->config_.erase(it);
		}
		if (in->created_at_ == zero_time())
			in->created_at_ = now_time();
		in->logs_ = std::make_shared<ringbuf>(2000);
		m_instances_[in->id_] = in;
	}
	return true;
}

std::vector<std::string> manager::ids()
{
	std::lock_guard<std::mutex> lock(m_mu_);
	std::vector<std::string> out;
	out.reserve(m_instances_.size());
	for (const auto& [id, _] : m_instances_)
		out.push_back(id);
	return out;
}

bool manager::save()
{
	std::vector<std::pair<instance_ptr, json::object>> list;
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		list.reserve(m_instances_.size());
		for (const auto& [_, in] : m_instances_) {
			// 深拷贝 map：锁外序列化期间其他线程可能在锁内就地修改 config_/user_usage_。
			list.emplace_back(in, json::object(in->config_));
		}
	}
	json::array arr;
	for (auto& [in, cfg] : list) {
		json::object item;
		item["id"] = in->id_;
		item["name"] = in->name_;
		item["config"] = std::move(cfg);
		item["autostart"] = in->autostart_;
		item["token"] = in->token_;
		item["created_at"] = rfc3339_format(in->created_at_);
		item["user_usage"] = json::object(in->user_usage_);
		arr.emplace_back(std::move(item));
	}

	std::string data = serialize_pretty(arr);
	{
		std::lock_guard<std::mutex> lock(m_save_mu_);
		boost::system::error_code ec;
		fs::create_directories(m_data_dir_, ec);
		if (ec)
			return false;
		std::string tmp = persist_path() + ".tmp";
		{
			std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
			if (!ofs)
				return false;
			ofs << data;
			ofs.flush();
			if (!ofs)
				return false;
		}
		fs::rename(tmp, persist_path(), ec);
		return !ec;
	}
}

instance_ptr manager::find_instance(const std::string& id)
{
	std::lock_guard<std::mutex> lock(m_mu_);
	auto it = m_instances_.find(id);
	return it == m_instances_.end() ? nullptr : it->second;
}

// 持锁状态下的实例查找（调用者须已持有 m_mu_）。
instance_ptr manager::find_instance_unlocked(const std::string& id) const
{
	auto it = m_instances_.find(id);
	return it == m_instances_.end() ? nullptr : it->second;
}

instance_ptr manager::get(const std::string& id)
{
	return find_instance(id);
}

instance_ptr manager::create(const std::string& name, json::object config, std::string& err)
{
	if (config.empty())
		config = default_config();
	err = validate_config(config);
	if (!err.empty())
		return nullptr;

	auto in = std::make_shared<instance>();
	in->id_ = random_hex(8);
	in->name_ = name.empty() ? in->id_ : name;
	in->config_ = std::move(config);
	in->created_at_ = now_time();
	in->token_ = random_hex(16);
	in->logs_ = std::make_shared<ringbuf>(2000);

	{
		std::lock_guard<std::mutex> lock(m_mu_);
		m_instances_[in->id_] = in;
	}
	if (!save())
		err = "save instances failed";
	return in;
}

net::awaitable<bool> manager::del(const std::string& id, std::string& err)
{
	if (!find_instance(id)) {
		err = "instance not found";
		co_return false;
	}
	co_await stop(id, err);
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		m_instances_.erase(id);
	}
	// 清理实例残留的 pid 文件。
	boost::system::error_code ec;
	fs::remove(pid_file_path(id), ec);
	save();
	co_return true;
}

bool manager::update(const std::string& id, const std::string& name,
	const std::optional<bool>& autostart, std::string& err)
{
	auto in = find_instance(id);
	if (!in) {
		err = "instance not found";
		return false;
	}
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		if (!name.empty() && name != in->name_)
			in->name_ = name;
		if (autostart)
			in->autostart_ = *autostart;
	}
	save();
	return true;
}

bool manager::start(const std::string& id, std::string& err)
{
	auto in = find_instance(id);
	if (!in) {
		err = "instance not found";
		return false;
	}
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		in->stopping_ = false;
		in->restart_count_ = 0;
		in->last_exit_ = time_point{};
	}
	return start_internal(id, err);
}

bool manager::start_internal(const std::string& id, std::string& err)
{
	std::vector<std::string> args;
	std::string proxy_path, work_dir, launcher_url, pid_path;
	{
		auto in = find_instance(id);
		if (!in) {
			err = "instance not found";
			return false;
		}
		std::lock_guard<std::mutex> lock(m_mu_);
		if (in->stopping_) {
			err = "instance " + id + " is stopping";
			return false;
		}
		if (in->proc_ && in->proc_->alive_) {
			err = "instance " + id + " already running";
			return false;
		}
		args = args_for(in->config_);
		launcher_url = ws_url(in);
		pid_path = pid_file_path(id);
		args.push_back("--launcher");
		args.push_back(launcher_url);
		args.push_back("--pid_file");
		args.push_back(pid_path);
		proxy_path = m_proxy_path_;
		work_dir = m_work_dir_;
	}

	// 按 pid 文件终止上次运行残留的进程（锁外执行，可能阻塞）。
	kill_by_pid_file(id);

	{
		auto in = find_instance(id);
		if (!in) {
			err = "instance not found";
			return false;
		}
		std::lock_guard<std::mutex> lock(m_mu_);
		// 自动重启与 Stop 并发时：kill 残留进程期间用户可能点了停止。
		if (in->stopping_) {
			err = "instance " + id + " is stopping";
			return false;
		}
		if (in->proc_ && in->proc_->alive_) {
			err = "instance " + id + " already running";
			return false;
		}

		auto logs = std::make_shared<ringbuf>(2000);
		// 捕获 manager 的 shared_ptr，保证监控线程回调期间对象存活。
		auto self = shared_from_this();
		auto p = spawn_proc(proxy_path, args, work_dir, logs,
			[self, id]() { self->wait_exit(id); });
		if (!p) {
			err = "spawn proxy_server failed";
			return false;
		}
		in->proc_ = p;
		in->logs_ = logs;
	}

	// 记录 pid（供日志）。
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		auto in = find_instance_unlocked(id);
		if (in && in->proc_)
			std::fprintf(stderr, "[info] instance %s pid=%d\n", id.c_str(), in->proc_->pid_);
	}
	// 诊断：进程启动后若长时间未连上控制通道，打印明确告警。
	std::thread([self = shared_from_this(), id]() {
			std::this_thread::sleep_for(kConnectWarnInterval);
		bool no_conn = false;
		{
			std::lock_guard<std::mutex> lock(self->m_mu_);
			auto in = self->find_instance_unlocked(id);
			if (in && !in->online() && in->proc_ && in->proc_->alive_)
				no_conn = true;
		}
		if (no_conn)
			std::fprintf(stderr,
				"[warn] instance %s: proxy_server did not connect to launcher within %llds; "
				"check the proxy_server binary supports --launcher and the control channel URL\n",
				id.c_str(), (long long)kConnectWarnInterval.count());
	}).detach();
	return true;
}

void manager::wait_exit(const std::string& id)
{
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		auto in = find_instance_unlocked(id);
		if (!in)
			return;
		if (in->proc_)
			in->proc_ = nullptr;
		if (!in->online())
			in->last_seen_ = now_time();
	}
	save();

	instance_ptr in = find_instance(id);
	if (in && should_auto_restart(in)) {
		std::string err;
		if (!start_internal(id, err))
			std::fprintf(stderr, "[warn] instance %s auto-restart failed: %s\n", id.c_str(), err.c_str());
	}
}

bool manager::should_auto_restart(const instance_ptr& in)
{
	std::lock_guard<std::mutex> lock(m_mu_);
	if (in->stopping_)
		return false;
	if (!m_instances_.count(in->id_))
		return false;
	if (in->proc_ && in->proc_->alive_)
		return false;
	auto now = now_time();
	// 距上次崩溃超过窗口：进程此前已稳定运行，重置连续崩溃计数。
	if (now - in->last_exit_ > kCrashWindow)
		in->restart_count_ = 0;
	in->last_exit_ = now;
	in->restart_count_++;
	if (in->restart_count_ > kMaxCrashes) {
		std::fprintf(stderr,
			"[warn] instance %s: crashed %d times within %llds, stop auto-restarting\n",
			in->id_.c_str(), in->restart_count_, (long long)kCrashWindow.count());
		in->restart_count_ = 0;
		in->last_exit_ = time_point{};
		return false;
	}
	return true;
}

void manager::kill_by_pid_file(const std::string& id)
{
	std::string path = pid_file_path(id);
	std::ifstream ifs(path);
	if (!ifs)
		return;
	std::string data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	process_id pid = 0;
	try {
		pid = static_cast<process_id>(std::stoll(data));
	} catch (...) {
		return;
	}	if (pid <= 0)
		return;
	if (!process_alive(pid)) {
		// 进程已退出：仅清理陈旧 pid 文件。
		boost::system::error_code ec;
		fs::remove(path, ec);
		return;
	}
	// 防御 pid 复用：确认该进程确实以本实例的 pid 文件参数启动。
	if (!process_matches_pid_file(pid, path)) {
		std::fprintf(stderr,
			"[warn] instance %s: pid %d in pid file %s is not this instance's process, skip killing\n",
			id.c_str(), pid, path.c_str());
		boost::system::error_code ec;
		fs::remove(path, ec);
		return;
	}
	std::fprintf(stderr, "[info] instance %s: killing leftover process %d from pid file %s\n",
		id.c_str(), pid, path.c_str());
	stop_pid(pid);
	boost::system::error_code ec;
	fs::remove(path, ec);
}

net::awaitable<bool> manager::stop(const std::string& id, std::string& err)
{
	std::shared_ptr<proc> proc;
	jsonrpc_session sess;
	process_id orphan_pid = 0;
	{
		auto in = find_instance(id);
		if (!in) {
			err = "instance not found";
			co_return false;
		}
		std::lock_guard<std::mutex> lock(m_mu_);
		in->stopping_ = true;
		proc = in->proc_;
		sess = in->channel_;
	}

	if (proc == nullptr) {
		// 孤儿实例（launcher 崩溃后残留）：按 register 上报的 PID 终止。
		{
			std::lock_guard<std::mutex> lock(m_mu_);
			auto in = find_instance_unlocked(id);
			if (in) {
				orphan_pid = in->reg_pid_;
				in->reg_pid_ = 0;
			}
		}
		if (orphan_pid > 0)
			stop_pid(orphan_pid);
		if (sess.valid())
			sess.stop();
		co_return true;
	}

	// 先 RPC shutdown 优雅退出（异步，不阻塞 io_context 线程）。
	if (sess.valid())
		(void)co_await sess.async_call("shutdown", json::value(), std::chrono::seconds(3));
	stop_proc(proc, 5);
	if (sess.valid())
		sess.stop();
	// 停止时持久化用户用量。
	save();
	co_return true;
}

net::awaitable<bool> manager::restart(const std::string& id, std::string& err)
{
	std::shared_ptr<proc> proc;
	jsonrpc_session sess;
	{
		auto in = find_instance(id);
		if (!in) {
			err = "instance not found";
			co_return false;
		}
		std::lock_guard<std::mutex> lock(m_mu_);
		in->stopping_ = true;
		proc = in->proc_;
		sess = in->channel_;
	}

	// 直接杀死子进程（不等优雅退出），实现快速重启。
	if (proc)
		stop_proc(proc, 0);
	if (sess.valid())
		sess.stop();

	// 等待监控线程完成退出处理（proc_ 清空、不再自动重启），
	// 避免紧接着 start 时与 wait_exit 竞争导致新进程句柄被清空。
	auto ex = co_await net::this_coro::executor;
	net::steady_timer timer(ex);
	for (int i = 0; i < 50; i++) {
		{
			auto in = find_instance(id);
			std::lock_guard<std::mutex> lock(m_mu_);
			if (!in || in->proc_ == nullptr)
				break;
		}
		timer.expires_after(std::chrono::milliseconds(50));
		boost::system::error_code tec;
		co_await timer.async_wait(net::redirect_error(net::use_awaitable, tec));
	}
	co_return start(id, err);
}

net::awaitable<bool> manager::apply_config(const std::string& id, json::object config,
	json::value& result, std::string& err)
{
	err = validate_config(config);
	if (!err.empty())
		co_return false;

	jsonrpc_session sess;
	instance_ptr in = find_instance(id);
	if (!in) {
		err = "instance not found";
		co_return false;
	}
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		sess = in->channel_;
	}

	if (sess.valid()) {
		json::object params;
		params["options"] = config;
		auto res = co_await sess.async_call("set_config", params, std::chrono::seconds(15));
		if (!res.ok()) {
			err = "set_config rpc: " + res.error_message_;
			co_return false;
		}
		result = res.result_;
	}
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		auto in = find_instance_unlocked(id);
		if (in)
			in->config_ = std::move(config);
	}
	save();
	co_return true;
}

boost::json::value manager::current_users_state(const instance_ptr& in)
{
	json::object st;
	st["auth_users"] = json::array();
	st["users_rate_limit"] = json::array();
	st["users_quota"] = json::array();
	set_config_list(st, "auth_users", config_list(in->config_, "auth_users"));
	set_config_list(st, "users_rate_limit", config_list(in->config_, "users_rate_limit"));
	set_config_list(st, "users_quota", config_list(in->config_, "users_quota"));
	return st;
}

void manager::sync_users_state(const instance_ptr& in, const json::value& st)
{
	if (!st.is_object())
		return;
	const auto& obj = st.as_object();
	for (const char* key : { "auth_users", "users_rate_limit", "users_quota" }) {
		if (auto it = obj.find(key); it != obj.end())
			in->config_[key] = it->value();
	}
}

net::awaitable<bool> manager::add_user(const std::string& id, const json::object& req,
	json::value& result, std::string& err)
{
	std::string user = req.if_contains("user") && req.at("user").is_string()
		? std::string(req.at("user").as_string()) : "";
	std::string password = req.if_contains("password") && req.at("password").is_string()
		? std::string(req.at("password").as_string()) : "";
	std::string addr = req.if_contains("addr") && req.at("addr").is_string()
		? std::string(req.at("addr").as_string()) : "";
	std::string proxy_url = req.if_contains("proxy_url") && req.at("proxy_url").is_string()
		? std::string(req.at("proxy_url").as_string()) : "";
	if (user.empty()) {
		err = "user is required";
		co_return false;
	}

	jsonrpc_session sess;
	instance_ptr in = find_instance(id);
	if (!in) {
		err = "instance not found";
		co_return false;
	}
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		sess = in->channel_;
	}

	if (sess.valid()) {
		json::object params;
		params["user"] = user;
		params["password"] = password;
		params["addr"] = addr;
		params["proxy_url"] = proxy_url;
		auto res = co_await sess.async_call("add_user", params, std::chrono::seconds(10));
		if (!res.ok()) {
			err = res.error_message_;
			co_return false;
		}
		result = res.result_;
		sync_users_state(in, res.result_);
		save();
		co_return true;
	}

	// 离线：直接修改配置中的 auth_users。
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		auto list = config_list(in->config_, "auth_users");
		list = upsert_user_entry(list, user, build_user_entry(user, password, addr, proxy_url));
		set_config_list(in->config_, "auth_users", list);
	}
	result = current_users_state(in);
	save();
	co_return true;
}

net::awaitable<bool> manager::del_user(const std::string& id, const std::string& user,
	json::value& result, std::string& err)
{
	if (user.empty()) {
		err = "user is required";
		co_return false;
	}
	jsonrpc_session sess;
	instance_ptr in = find_instance(id);
	if (!in) {
		err = "instance not found";
		co_return false;
	}
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		sess = in->channel_;
	}
	if (sess.valid()) {
		json::object params;
		params["user"] = user;
		auto res = co_await sess.async_call("del_user", params, std::chrono::seconds(10));
		if (!res.ok()) {
			err = res.error_message_;
			co_return false;
		}
		result = res.result_;
		sync_users_state(in, res.result_);
		save();
		co_return true;
	}
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		auto list = config_list(in->config_, "auth_users");
		list = remove_user_entry(list, user);
		set_config_list(in->config_, "auth_users", list);
	}
	result = current_users_state(in);
	save();
	co_return true;
}

net::awaitable<bool> manager::set_user_password(const std::string& id, const std::string& user,
	const std::string& password, json::value& result, std::string& err)
{
	if (user.empty()) {
		err = "user is required";
		co_return false;
	}
	jsonrpc_session sess;
	instance_ptr in = find_instance(id);
	if (!in) {
		err = "instance not found";
		co_return false;
	}
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		sess = in->channel_;
	}
	if (sess.valid()) {
		json::object params;
		params["user"] = user;
		params["password"] = password;
		auto res = co_await sess.async_call("set_user_password", params, std::chrono::seconds(10));
		if (!res.ok()) {
			err = res.error_message_;
			co_return false;
		}
		result = res.result_;
		sync_users_state(in, res.result_);
		save();
		co_return true;
	}
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		auto list = config_list(in->config_, "auth_users");
		list = set_user_password_entry(list, user, password);
		set_config_list(in->config_, "auth_users", list);
	}
	result = current_users_state(in);
	save();
	co_return true;
}

net::awaitable<bool> manager::set_user_rate_limit(const std::string& id, const std::string& user, int rate,
	json::value& result, std::string& err)
{
	if (user.empty()) {
		err = "user is required";
		co_return false;
	}
	jsonrpc_session sess;
	instance_ptr in = find_instance(id);
	if (!in) {
		err = "instance not found";
		co_return false;
	}
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		sess = in->channel_;
	}
	if (sess.valid()) {
		json::object params;
		params["user"] = user;
		params["rate"] = rate;
		auto res = co_await sess.async_call("set_user_rate_limit", params, std::chrono::seconds(10));
		if (!res.ok()) {
			err = res.error_message_;
			co_return false;
		}
		result = res.result_;
		sync_users_state(in, res.result_);
		save();
		co_return true;
	}
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		auto list = config_list(in->config_, "users_rate_limit");
		list = set_user_rate_limit_entry(list, user, rate);
		set_config_list(in->config_, "users_rate_limit", list);
	}
	result = current_users_state(in);
	save();
	co_return true;
}

net::awaitable<bool> manager::set_user_quota(const std::string& id, const std::string& user, std::int64_t quota,
	json::value& result, std::string& err)
{
	if (user.empty()) {
		err = "user is required";
		co_return false;
	}
	jsonrpc_session sess;
	instance_ptr in = find_instance(id);
	if (!in) {
		err = "instance not found";
		co_return false;
	}
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		sess = in->channel_;
	}
	if (sess.valid()) {
		json::object params;
		params["user"] = user;
		params["quota"] = quota;
		auto res = co_await sess.async_call("set_user_quota", params, std::chrono::seconds(10));
		if (!res.ok()) {
			err = res.error_message_;
			co_return false;
		}
		result = res.result_;
		sync_users_state(in, res.result_);
		save();
		co_return true;
	}
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		auto list = config_list(in->config_, "users_quota");
		list = set_user_quota_entry(list, user, quota);
		set_config_list(in->config_, "users_quota", list);
	}
	result = current_users_state(in);
	save();
	co_return true;
}

boost::json::value manager::summaries()
{
	std::lock_guard<std::mutex> lock(m_mu_);
	std::vector<summary> items;
	items.reserve(m_instances_.size());
	for (const auto& [_, in] : m_instances_) {
		summary s;
		s.id_ = in->id_;
		s.name_ = in->name_;
		s.state_ = in->state();
		s.online_ = in->online();
		s.pid_ = in->pid();
		s.autostart_ = in->autostart_;
		s.created_at_ = in->created_at_;
		s.listen_ = config_list(in->config_, "server_listen");
		if (in->last_report_.is_object()) {
			const auto& rep = in->last_report_.as_object();
			if (auto t = rep.if_contains("ts"); t && t->is_int64() && t->as_int64() != 0) {
				if (auto a = rep.if_contains("active_connections"); a)
					s.active_ = static_cast<int>(as_int64(*a));
				if (auto r = rep.if_contains("rates"); r && r->is_object()) {
					if (auto rx = r->as_object().if_contains("rx_rate_bps"); rx && rx->is_double())
						s.rx_rate_ = rx->as_double();
					if (auto tx = r->as_object().if_contains("tx_rate_bps"); tx && tx->is_double())
						s.tx_rate_ = tx->as_double();
				}
			}
		}
		items.push_back(std::move(s));
	}
	// 按创建顺序（升序）显示；同时刻创建时按 ID 稳定排序兜底。
	std::stable_sort(items.begin(), items.end(), [](const summary& a, const summary& b) {
		if (a.created_at_ == b.created_at_)
			return a.id_ < b.id_;
		return a.created_at_ < b.created_at_;
	});

	json::array out;
	for (const auto& s : items) {
		json::object o;
		o["id"] = s.id_;
		o["name"] = s.name_;
		o["state"] = s.state_;
		o["online"] = s.online_;
		o["pid"] = s.pid_;
		o["autostart"] = s.autostart_;
		if (!s.listen_.empty()) {
			json::array arr;
			for (const auto& l : s.listen_)
				arr.emplace_back(l);
			o["listen"] = std::move(arr);
		}
		o["active"] = s.active_;
		o["rx_rate_bps"] = s.rx_rate_;
		o["tx_rate_bps"] = s.tx_rate_;
		out.emplace_back(std::move(o));
	}
	return out;
}

bool manager::view(const std::string& id, launcher::view& out)
{
	std::lock_guard<std::mutex> lock(m_mu_);
	auto in = find_instance_unlocked(id);
	if (!in)
		return false;
	out.id_ = in->id_;
	out.name_ = in->name_;
	out.state_ = in->state();
	out.online_ = in->online();
	out.pid_ = in->pid();
	out.autostart_ = in->autostart_;
	out.config_ = json::object(in->config_);
	out.created_at_ = in->created_at_;
	out.last_report_ = in->last_report_;
	out.last_seen_ = in->last_seen_;
	return true;
}

bool manager::status_view(const std::string& id, json::value& out)
{
	std::lock_guard<std::mutex> lock(m_mu_);
	auto in = find_instance_unlocked(id);
	if (!in)
		return false;
	json::object o;
	o["online"] = in->online();
	o["state"] = in->state();
	o["pid"] = in->pid();
	o["last_seen"] = rfc3339_format(in->last_seen_);
	o["report"] = in->last_report_.is_object() ? in->last_report_ : json::value(json::object_kind);
	out = std::move(o);
	return true;
}

bool manager::logs(const std::string& id, std::int64_t since, json::value& out) {
	std::lock_guard<std::mutex> lock(m_mu_);
	auto in = find_instance_unlocked(id);
	if (!in)
		return false;
	if (!in->logs_)
		return false;

	std::vector<std::string> l;
	std::vector<std::int64_t> s;
	json::array lines;
	json::array seqs;
	if (since < 0)
		in->logs_->tail_seq(500, l, s);
	else
		in->logs_->since(since, l, s);
	for (std::size_t i = 0; i < l.size(); i++) {
		lines.emplace_back(l[i]);
		seqs.emplace_back(s[i]);
	}
	json::object o;
	o["lines"] = std::move(lines);
	o["seqs"] = std::move(seqs);
	o["next"] = in->logs_->next_seq();
	o["gen"] = in->logs_->generation();
	out = std::move(o);
	return true;
}

instance_ptr manager::ws_auth(const std::string& id, const std::string& token)
{
	std::lock_guard<std::mutex> lock(m_mu_);
	auto in = find_instance_unlocked(id);
	if (!in)
		return nullptr;
	if (in->token_.empty() || in->token_ != token)
		return nullptr;
	return in;
}

void manager::ws_attached(const instance_ptr& in, jsonrpc_session sess)
{
	// 存入实例并关闭旧连接（若存在）。
	jsonrpc_session old;
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		old = in->channel_;
		in->channel_ = std::move(sess);
		in->online_ = true;
	}
	if (old.valid())
		old.stop();

	// 续接持久化的用户已用量，使配额在重启/重连后延续（proxy 侧只增不减）。
	json::object usage;
	{
		std::lock_guard<std::mutex> lock(m_mu_);
		usage = json::object(in->user_usage_);
	}
	if (!usage.empty()) {
		jsonrpc_session cur;
		{
			std::lock_guard<std::mutex> lock(m_mu_);
			cur = in->channel_;
		}
		if (cur.valid()) {
			json::object params;
			params["usage"] = std::move(usage);
			cur.notify("set_user_usage", params);
		}
	}
}

// 处理控制通道通知（register/status/log）。由 http_server 直接用
// jsonrpc::jsonrpc_session 绑定通知回调后转发到本方法。
void manager::handle_notify(const instance_ptr& in, const std::string& method,
	const json::value& params)
{
	if (method == "register") {
		std::lock_guard<std::mutex> lock(m_mu_);
		if (params.is_object()) {
			if (auto p = params.as_object().if_contains("pid"); p && p->is_int64())
				in->reg_pid_ = static_cast<process_id>(p->as_int64());
			else if (p && p->is_uint64())
				in->reg_pid_ = static_cast<process_id>(p->as_uint64());
		}
		in->last_seen_ = now_time();
		return;
	}

	if (method == "status") {
		bool need_save = false;
		{
			std::lock_guard<std::mutex> lock(m_mu_);
			in->last_report_ = params;
			in->last_seen_ = now_time();
			// 记录用户已用量（usage_total 为含续接基线的累计总流量），重启后续接。
			// 兼容旧版 proxy_server：未上报 usage_total 时回退到会话级 tx+rx。
			bool usage_changed = false;
			if (params.is_object()) {
				if (auto u = params.as_object().if_contains("users"); u && u->is_array()) {
					for (const auto& uu : u->as_array()) {
						if (!uu.is_object())
							continue;
						const auto& uo = uu.as_object();
						auto user_it = uo.find("user");
						if (user_it == uo.end())
							continue;
						std::string user = as_string(user_it->value());
						// 匿名用户无配额，不持久化。
						if (user == "(匿名)")
							continue;
						std::int64_t total = 0;
						if (auto ut = uo.find("usage_total"); ut != uo.end())
							total = as_int64(ut->value());
						if (total <= 0) {
							// 兼容旧版 proxy_server：未上报 usage_total 时回退到会话级 tx+rx。
							auto tx_it = uo.find("tx_bytes");
							auto rx_it = uo.find("rx_bytes");
							total = (tx_it != uo.end() ? as_int64(tx_it->value()) : 0)
								+ (rx_it != uo.end() ? as_int64(rx_it->value()) : 0);
						}
						std::int64_t cur = 0;
						if (auto it = in->user_usage_.find(user); it != in->user_usage_.end())
							cur = as_int64(it->value());
						if (total > cur) {
							in->user_usage_[user] = total;
							usage_changed = true;
						}
					}
				}
			}
			// 仅当用量变化时持久化（节流）。首次变化立即保存。
			if (usage_changed && now_time() - in->last_usage_save_ >= kUsageSaveInterval) {
				in->last_usage_save_ = now_time();
				need_save = true;
			}
		}
		if (need_save)
			save();
		return;
	}

	if (method == "log") {
		std::lock_guard<std::mutex> lock(m_mu_);
		if (in->logs_ && params.is_object()) {
			if (auto l = params.as_object().if_contains("lines"); l && l->is_array()) {
				for (const auto& line : l->as_array())
					in->logs_->add(as_string(line));
			}
		}
		return;
	}
}

void manager::ws_detached(const instance_ptr& in, std::uint64_t gen)
{
	std::lock_guard<std::mutex> lock(m_mu_);
	// 已有新连接接入时不清空实例会话。
	if (gen != in->chan_gen_.load())
		return;
	in->channel_ = jsonrpc_session{};
	in->online_ = false;
	in->last_seen_ = now_time();
}

} // namespace launcher
