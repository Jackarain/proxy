//
// options.hpp
// ~~~~~~~~~~~~
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// proxy_server 全部命令行选项的单一事实来源：供 WebUI 配置表单、新建实例默认
// 配置、启动参数生成共用，保证 WebUI 覆盖的功能与 proxy_server 命令行一致。
//

#ifndef LAUNCHER_OPTIONS_HPP
#define LAUNCHER_OPTIONS_HPP

#include <boost/json.hpp>
#include <string>
#include <vector>

namespace launcher {

// 选项值类型。
enum class option_kind {
	boolean,       // bool
	integer,       // int
	string,        // string
	string_list    // stringlist
};

// 一个可配置项。
struct option
{
	std::string name_;
	option_kind kind_ = option_kind::string;
	std::string help_;

	// 默认值。
	bool has_default_ = false;
	bool def_bool_ = false;
	std::int64_t def_int_ = 0;
	std::string def_str_;
	std::vector<std::string> def_list_;

	// hidden_ 为 true 时不在 WebUI 表单中展示
	// （help/config/asio_config/launcher/pid_file 等进程内部或已废弃选项）。
	bool hidden_ = false;
	// restart_only_ 为 true 时运行期修改需重启实例生效（如 stdio/transparent）。
	bool restart_only_ = false;
	// common_ 为 true 时在 WebUI 配置页的「常用配置」区置顶展示（不折叠）。
	bool common_ = false;
	// hint_ 为常用选项的中文用法说明，展示在表单输入框上方。
	std::string hint_;
	// category_ 为选项在 WebUI 表单中的分组。
	std::string category_;
};

// 全部可配置项。
const std::vector<option>& all_options();

// 按名称查找选项，未找到返回 nullptr。
const option* find_option(const std::string& name);

// 选项值类型名（与命令行解析类型对应）。
const char* kind_type_name(option_kind kind);

// 返回全部非隐藏选项的默认配置（供新建实例预填表单）。
// stringlist 以 json::array 表示，与 JSON 反序列化后的类型一致。
boost::json::object default_config();

// 由实例配置生成 proxy_server 启动参数（不含 --launcher）。
// 空 stringlist 以 --name "" 形式显式传空，避免 proxy_server 落到注册表默认值。
std::vector<std::string> args_for(const boost::json::object& cfg);

// 校验配置：选项必须存在且非隐藏。返回错误信息，空表示合法。
// 兼容旧配置：proxy_ssl_name 已并入 ssl_sni（就地迁移）。
std::string validate_config(boost::json::object& cfg);

// 兼容 int / float / string 的整数取值。
std::int64_t to_int_value(const boost::json::value& v);

// 任意值转字符串。
std::string to_string_value(const boost::json::value& v);

// stringlist 值统一转为 []string。
std::vector<std::string> to_string_list(const boost::json::value& v);

} // namespace launcher

#endif // LAUNCHER_OPTIONS_HPP
