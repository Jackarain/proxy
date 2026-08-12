//
// manager.hpp
// ~~~~~~~~~~~
//
// 实例管理器：负责实例的增删改查、进程启停、运行期配置下发、
// WS 控制通道接纳与状态/日志采集（持久化格式、自动重启、用量续接等）。
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef LAUNCHER_MANAGER_HPP
#define LAUNCHER_MANAGER_HPP

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <boost/json.hpp>

#include "datetime.hpp"
#include "json_rpc.hpp"
#include "proc.hpp"
#include "ringbuf.hpp"

namespace launcher {

using instance_ptr = std::shared_ptr<struct instance>;

// 一个被管理的 proxy_server 实例。
struct instance : public std::enable_shared_from_this<instance> {
	// 持久化字段。
	std::string id;
	std::string name;
	boost::json::object config;
	bool autostart = false;
	time_point created_at;
	// 各用户累计下载用量（字节），持久化；重启实例后续接配额计数。
	boost::json::object user_usage;

	// 控制通道认证令牌（持久化；launcher 崩溃后孤儿进程可凭旧 token 重连）。
	std::string token;

	// 运行时字段。
	std::shared_ptr<proc> proc_;
	std::shared_ptr<rpc::endpoint_base> ep_;
	boost::json::value last_report; // status.Report
	time_point last_seen;
	std::shared_ptr<ringbuf> logs_;
	// 经 register 通知上报的进程 PID（用于停止 launcher 未拉起的孤儿实例）。
	process_id reg_pid = 0;
	// 上次持久化用量的时间（节流磁盘写入）。
	time_point last_usage_save;

	// 崩溃自动重启状态。
	bool stopping = false; // 用户主动停止标记。
	int restart_count = 0; // 连续意外退出次数。
	time_point last_exit;  // 上次意外退出的时间。

	// 控制通道是否已建立。
	bool online() const { return ep_ != nullptr; }

	// 根据控制通道与进程状态推导实例状态。
	std::string state() const {
		if (ep_ != nullptr)
			return "running";
		if (proc_ != nullptr && proc_->alive)
			return "starting";
		if (proc_ != nullptr)
			return "error";
		return "stopped";
	}

	// 进程 PID：优先取本 launcher 拉起的进程，否则用 register 上报的孤儿 PID。
	process_id pid() const {
		if (proc_ != nullptr && proc_->alive)
			return proc_->pid;
		return reg_pid;
	}
};

// 实例列表摘要（供列表页使用）。
struct summary {
	std::string id;
	std::string name;
	std::string state;
	bool online = false;
	process_id pid = 0;
	bool autostart = false;
	std::vector<std::string> listen;
	int active = 0;
	double rx_rate = 0;
	double tx_rate = 0;
	time_point created_at;
};

// 实例只读快照（供 API 使用）。
struct view {
	std::string id;
	std::string name;
	std::string state;
	bool online = false;
	process_id pid = 0;
	bool autostart = false;
	boost::json::object config;
	time_point created_at;
	boost::json::value last_report;
	time_point last_seen;
};

class manager : public std::enable_shared_from_this<manager> {
public:
	manager(std::string data_dir, std::string proxy_path, std::string work_dir);

	// 控制通道基础信息（由 main 设置）。
	void set_ws_addr(const std::string& host, int port, bool https);

	// 加载持久化配置，并自动拉起标记 autostart 的实例。返回 false 表示失败。
	bool load();

	std::vector<std::string> ids();

	// ---- CRUD ----

	// 创建新实例（默认配置 = 注册表默认值）。err 非空表示失败。
	instance_ptr create(const std::string& name, boost::json::object config, std::string& err);

	// 删除实例（先停止）。
	bool del(const std::string& id, std::string& err);

	// 更新实例名称与开机自启设置。
	bool update(const std::string& id, const std::string& name,
		const std::optional<bool>& autostart, std::string& err);

	instance_ptr get(const std::string& id);

	// ---- 生命周期 ----

	bool start(const std::string& id, std::string& err);
	bool stop(const std::string& id, std::string& err);
	bool restart(const std::string& id, std::string& err);

	// ---- 运行期配置与用户管理 ----

	bool apply_config(const std::string& id, boost::json::object config,
		boost::json::value& result, std::string& err);

	bool add_user(const std::string& id, const boost::json::object& req,
		boost::json::value& result, std::string& err);

	bool del_user(const std::string& id, const std::string& user,
		boost::json::value& result, std::string& err);

	bool set_user_password(const std::string& id, const std::string& user,
		const std::string& password, boost::json::value& result, std::string& err);

	bool set_user_rate_limit(const std::string& id, const std::string& user, int rate,
		boost::json::value& result, std::string& err);

	bool set_user_quota(const std::string& id, const std::string& user, std::int64_t quota,
		boost::json::value& result, std::string& err);

	// ---- 视图 ----

	boost::json::value summaries();
	bool view(const std::string& id, view& out);
	bool status_view(const std::string& id, boost::json::value& out);
	// 日志增量。since<0 时返回最近快照（含每行序号）。
	bool logs(const std::string& id, std::int64_t since, boost::json::value& out);

	// ---- WS 控制通道（http_server 调用） ----

	// 校验 instance + token。失败返回 nullptr。
	instance_ptr ws_auth(const std::string& id, const std::string& token);

	// 连接接纳后：注册通知处理器、保存 ep、续接用户用量。
	void ws_attached(const instance_ptr& in, const std::shared_ptr<rpc::endpoint_base>& ep);

	// 连接关闭后清理。
	void ws_detached(const instance_ptr& in, const std::shared_ptr<rpc::endpoint_base>& ep);

	// pid 文件路径（按实例 ID 区分）。
	std::string pid_file_path(const std::string& id) const;

	// 生成控制通道地址。
	std::string ws_url(const instance_ptr& in) const;

	std::string data_dir() const { return data_dir_; }

private:
	// 内部启动实现（自动重启也走此路径；与手动 Start 不同，不重置崩溃计数）。
	bool start_internal(const std::string& id, std::string& err);

	// 进程退出回调（监控线程调用）。
	void wait_exit(const std::string& id);

	// 判断进程意外退出后是否自动重启，并维护崩溃循环检测计数。
	bool should_auto_restart(const instance_ptr& in);

	// 读取 pid 文件，若进程仍存活则终止（清理 launcher 重启后的残留进程）。
	void kill_by_pid_file(const std::string& id);

	// 持久化。
	bool save();
	bool load_from_disk();

	// 用户状态辅助。
	boost::json::value current_users_state(const instance_ptr& in);
	void sync_users_state(const instance_ptr& in, const boost::json::value& st);

	std::string persist_path() const;

	std::mutex mu_;
	std::map<std::string, instance_ptr> instances_;

	std::string data_dir_;
	std::string proxy_path_;
	std::string work_dir_;

	// saveMu 串行化持久化写盘。
	std::mutex save_mu_;

	// 控制通道基础信息。
	std::string host_;
	int port_ = 0;
	bool https_ = false;
};

} // namespace launcher

#endif // LAUNCHER_MANAGER_HPP
