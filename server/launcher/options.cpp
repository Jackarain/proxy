//
// options.cpp
// ~~~~~~~~~~~
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "options.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace launcher {

namespace {

namespace json = boost::json;

// 与 golang options.All 完全一致（名称/类型/帮助/默认值/隐藏/重启/常用/提示/分组）。
const option k_options[] = {
	{ "help", option_kind::boolean, "显示帮助信息并退出。", false, false, 0, "", {}, true, false, false, "", "其他" },
	{ "config", option_kind::string, "从指定配置文件加载参数选项。", false, false, 0, "", {}, true, false, false, "", "其他" },
	{ "server_listen", option_kind::string_list, "指定服务监听地址与端口（可重复添加）。",
	  true, false, 0, "", { "[::0]:1080" }, false, false, true,
	  "代理服务监听地址（可添加多条）。格式 IP:端口，如 0.0.0.0:1080；加 -v6only 仅监听 IPv6；Unix socket 用 unix://路径。",
	  "监听" },
	{ "auth_users", option_kind::string_list, "认证用户列表（默认 jack:1111），格式 user:password[:addr[:proxy_url]]（可重复添加）。",
	  true, false, 0, "", { "jack:1111" }, false, false, true,
	  "认证用户列表（可添加多条）。格式 user:password；再追加 :出口IP 绑定该用户出口，或再追加 :代理URL 指定该用户专属上游。清空表示匿名访问。",
	  "认证" },
	{ "stdio", option_kind::string, "stdio 代理模式的目标 host:port。", false, false, 0, "", {}, true, true, false, "", "监听" },
	{ "reuse_port", option_kind::boolean, "启用 TCP SO_REUSEPORT 套接字选项（Linux 3.9 起可用）。", false, false, 0, "", {}, false, false, false, "", "监听" },
	{ "happyeyeballs", option_kind::boolean, "出站 TCP 连接启用 Happy Eyeballs 算法（默认启用）。", true, true, 0, "", {}, false, false, false, "", "网络" },
	{ "v6only", option_kind::boolean, "出站 TCP 连接仅使用 IPv6。", false, false, 0, "", {}, false, false, false, "", "网络" },
	{ "v4only", option_kind::boolean, "出站 TCP 连接仅使用 IPv4。", false, false, 0, "", {}, false, false, false, "", "网络" },
	{ "local_ip", option_kind::string, "指定出站 TCP 连接的本地网口 IP 地址。", false, false, 0, "", {}, false, false, false, "", "网络" },
	{ "transparent", option_kind::boolean, "启用透明代理模式（仅 Linux）。", false, false, 0, "", {}, false, true, true,
	  "启用透明代理（TPROXY，仅 Linux）。需 iptables TPROXY 规则配合；修改后需重启实例。", "监听" },
	{ "so_mark", option_kind::integer, "为出站连接设置 SO_MARK 套接字标记（仅 Linux）。", true, false, -1, "", {}, false, false, false, "", "网络" },
	{ "tcp_timeout", option_kind::integer, "TCP 连接空闲超时（秒），-1 表示不设超时。", true, false, -1, "", {}, false, false, false, "", "超时" },
	{ "udp_timeout", option_kind::integer, "UDP 会话超时时间（秒）。", true, false, 60, "", {}, false, false, false, "", "超时" },
	{ "rate_limit", option_kind::integer, "每连接 TCP 速率限制（字节/秒），-1 表示不限速。", true, false, -1, "", {}, false, false, false,
	  "全局每连接限速（字节/秒）。-1 不限速；1048576 ≈ 1MB/s。", "限速" },
	{ "pam_auth", option_kind::string, "使用指定的 PAM 服务名称启用 PAM 认证。", false, false, 0, "", {}, false, false, false, "", "认证" },
	{ "users_rate_limit", option_kind::string_list, "按用户限速（字节/秒），格式 user:rate（可重复添加）。", false, false, 0, "", {}, false, false, false,
	  "按用户独立限速（可添加多条）。格式 user:字节/秒，支持单位（如 jack:2.5M、alice:10.8G）。留空则所有用户使用全局限速。", "限速" },
	{ "users_quota", option_kind::string_list, "按用户下载流量配额（字节），格式 user:bytes（可重复添加；超限停止该用户）。", false, false, 0, "", {}, false, false, false,
	  "按用户下载流量配额（可添加多条）。格式 user:字节或带单位（如 jack:2.5G、alice:10.8M）。下载超过配额后该用户被停止连接，默认不限制。", "限速" },
	{ "allow_region", option_kind::string_list, "仅允许指定地区或 CIDR 的连接（可重复添加）。", false, false, 0, "", {}, false, false, false,
	  "仅允许这些地区/网段访问（可添加多条）。地区如 CN、US；网段如 1.2.3.0/24。需配置 IPIP 数据库。", "地区" },
	{ "deny_region", option_kind::string_list, "拒绝指定地区或 CIDR 的连接（可重复添加）。", false, false, 0, "", {}, false, false, false,
	  "拒绝这些地区/网段访问（可添加多条），优先级高于 allow_region。需配置 IPIP 数据库。", "地区" },
	{ "proxy_pass", option_kind::string, "指定上游代理地址 URL（如 socks5://user:passwd@ip:port）。", false, false, 0, "", {}, false, false, true,
	  "级联上游代理。格式 scheme://user:pass@host:port，支持 socks5 / socks4 / http / https。留空则直连目标。", "上游" },
	{ "proxy_pass_ssl", option_kind::boolean, "向上游代理连接启用 SSL/TLS 加密。", false, false, 0, "", {}, false, false, false,
	  "向上游代理发起连接时使用 TLS 加密，可配合 ssl_sni 指定 SNI。", "上游" },
	{ "ssl_certificate_dir", option_kind::string, "SSL/TLS 证书存放目录。", false, false, 0, "", {}, false, false, false, "", "SSL" },
	{ "ssl_cacert_dir", option_kind::string, "SSL/TLS 根 CA 证书目录，叠加于内置 Mozilla CA bundle 之上（用于客户端校验）。", false, false, 0, "", {}, false, false, false, "", "SSL" },
	{ "ssl_sni", option_kind::string, "指定 SNI 主机名，用于单一 IP 上多证书匹配。", false, false, 0, "", {}, false, false, true,
	  "指定 SNI 主机名，用于单一 IP 上多证书匹配。", "上游" },
	{ "ssl_ciphers", option_kind::string, "指定启用的 SSL/TLS 加密套件。", false, false, 0, "", {}, false, true, false, "", "SSL" },
	{ "ssl_prefer_server_ciphers", option_kind::boolean, "优先使用服务端加密套件顺序。", false, false, 0, "", {}, false, true, false, "", "SSL" },
	{ "ipip_db", option_kind::string, "指定 IP 地理数据库文件名（地区查询）。", true, false, 0, "17monipdb.datx", {}, false, false, true,
	  "IP 地理数据库文件（17monipdb.datx / .ipdb，或 MaxMind GeoIP .mmdb），用于地区过滤与客户端位置展示。保持默认 17monipdb.datx 时依次尝试 17monipdb.datx 与 Country.mmdb，都没有则不加载。", "地区" },
	{ "http_doc", option_kind::string, "指定 HTTP 静态文件根目录。", false, false, 0, "", {}, false, false, true,
	  "HTTP 静态文件根目录；配置后实例可作为文件服务器，配合 autoindex 浏览目录。", "HTTP" },
	{ "htpasswd", option_kind::boolean, "为 HTTP 服务器启用 Basic 认证（WWW-Authenticate）。", false, false, 0, "", {}, false, false, false, "", "认证" },
	{ "autoindex", option_kind::boolean, "为 HTTP 服务器启用目录浏览。", false, false, 0, "", {}, false, false, true,
	  "启用目录浏览（需配合 http_doc 设置文件根目录）。", "HTTP" },
	{ "dns_upstream", option_kind::string, "指定上游 DNS 服务器，支持 UDP DNS(ip:port) 与 DoH(https://host/path)，DoH 可用 ?sni= 覆盖 TLS SNI。", false, false, 0, "", {}, false, false, false, "", "DNS" },
	{ "dns_udp_port", option_kind::integer, "监听 UDP DNS 请求的端口，0 表示不启用。", false, false, 0, "", {}, false, false, false,
	  "监听 UDP DNS 请求的端口，配置后实例可作为普通 DNS 服务器。指定了 dns_upstream 时转发到上游，否则按系统默认解析。0 或留空表示不启用。", "DNS" },
	{ "dns_cache_size", option_kind::integer, "DNS 查询结果缓存条数上限，0 表示不启用。", true, false, 0, "", {}, false, false, false,
	  "DNS 查询结果缓存条数上限。缓存命中直接返回结果并重置过期时间；达到上限后按最近最少使用（LRU）淘汰。0 表示不启用。", "DNS" },
	{ "dns_cache_ttl", option_kind::integer, "DNS 查询结果缓存过期时间（秒），0 表示不启用。", true, false, 0, "", {}, false, false, false,
	  "DNS 查询结果缓存过期时间（秒）。过期后删除；过期前重复查询该域名会重置过期时间。0 表示不启用。", "DNS" },
	{ "dns_no_ipv6", option_kind::boolean, "禁用 DNS 的 IPv6 解析返回，开启后 AAAA 查询返回空应答，只返回 IPv4 地址。", false, false, 0, "", {}, false, false, false, "", "DNS" },
	{ "logs_path", option_kind::string, "指定日志文件输出目录。", false, false, 0, "", {}, false, false, false, "", "日志" },
	{ "disable_logs", option_kind::boolean, "禁止在终端输出日志。", false, false, 0, "", {}, false, false, false, "", "日志" },
	{ "http2", option_kind::boolean, "启用 HTTP/2 协议（TLS 协商 h2 与明文 h2c），默认关闭。", false, false, 0, "", {}, false, false, false, "", "协议" },
	{ "disable_http", option_kind::boolean, "禁用 HTTP 代理协议。", false, false, 0, "", {}, false, false, false, "", "协议" },
	{ "disable_socks", option_kind::boolean, "禁用 SOCKS 代理协议。", false, false, 0, "", {}, false, false, false, "", "协议" },
	{ "disable_udp", option_kind::boolean, "禁用 UDP 代理功能。", false, false, 0, "", {}, false, false, false, "", "协议" },
	{ "disable_insecure", option_kind::boolean, "禁止非加密（非 SSL）代理连接。", false, false, 0, "", {}, false, false, false, "", "协议" },
	{ "disable_check_cert", option_kind::boolean, "禁用 TLS 证书校验。", false, false, 0, "", {}, false, false, false, "", "SSL" },
	{ "scramble", option_kind::boolean, "启用噪声数据加扰以增强安全性。", false, false, 0, "", {}, false, false, false, "", "加密" },
	{ "noise_length", option_kind::integer, "噪声数据长度（字节），-1 表示禁用，0-4095。", true, false, -1, "", {}, false, false, false, "", "加密" },
	{ "asio_config", option_kind::string, "配置 Boost.Asio 的环境变量名（默认 ASIO）。", false, false, 0, "", {}, true, false, false, "", "其他" },
	// launcher 为内部选项：由 launcher 在拉起 proxy_server 时传入，不参与 WebUI 表单。
	{ "launcher", option_kind::string, "Launcher WebSocket 地址（内部使用，由 launcher 传入）。", false, false, 0, "", {}, true, false, false, "", "其他" },
	// pid_file 为 launcher 内部选项：指定进程 PID 写入的 pid 文件路径。
	{ "pid_file", option_kind::string, "把进程 PID 写入指定文件（内部使用，由 launcher 传入）。", false, false, 0, "", {}, true, false, false, "", "其他" },
};

const std::vector<option>& all_options_impl() {
	static const std::vector<option> all(std::begin(k_options), std::end(k_options));
	return all;
}

// 判断配置值是否与注册表默认值相同（相同则无需显式传入命令行）。
bool equals_default(const option& o, const json::value& val) {
	const bool has_def = o.has_default;
	const auto kind = o.kind;

	// 无默认值：仅当值等于该类型的零值时视为"未设置"。
	auto is_zero_value = [kind](const json::value& v) -> bool {
		switch (kind) {
		case option_kind::boolean:
			return v.is_bool() && !v.as_bool();
		case option_kind::integer:
			return v.is_int64() ? v.as_int64() == 0
				: (v.is_uint64() ? v.as_uint64() == 0 : (v.is_double() && v.as_double() == 0));
		case option_kind::string:
			return v.is_string() ? v.as_string().empty() : (v.is_null() || v.is_bool());
		case option_kind::string_list:
			return v.is_array() ? v.as_array().empty()
				: (v.is_string() ? v.as_string().empty() : v.is_null());
		}
		return false;
	};

	if (!has_def)
		return is_zero_value(val);

	switch (kind) {
	case option_kind::boolean: {
		bool b = val.is_bool() ? val.as_bool() : false;
		return b == o.def_bool;
	}
	case option_kind::integer: {
		std::int64_t v = to_int_value(val);
		return v == o.def_int;
	}
	case option_kind::string:
		return to_string_value(val) == o.def_str;
	case option_kind::string_list: {
		std::vector<std::string> list = to_string_list(val);
		if (list.size() != o.def_list.size())
			return false;
		for (std::size_t i = 0; i < list.size(); i++)
			if (list[i] != o.def_list[i])
				return false;
		return true;
	}
	}
	return false;
}

} // namespace

// 兼容 int / int64 / float64 的整数取值。
std::int64_t to_int_value(const json::value& v) {
	if (v.is_int64())
		return v.as_int64();
	if (v.is_uint64())
		return static_cast<std::int64_t>(v.as_uint64());
	if (v.is_double())
		return static_cast<std::int64_t>(v.as_double());
	if (v.is_string()) {
		const auto& s = v.as_string();
		double f = 0;
		auto res = std::from_chars(s.data(), s.data() + s.size(), f);
		if (res.ec == std::errc())
			return static_cast<std::int64_t>(f);
	}
	return 0;
}

std::string to_string_value(const json::value& v) {
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

// stringlist 值统一转为 []string。
std::vector<std::string> to_string_list(const json::value& v) {
	std::vector<std::string> out;
	if (v.is_array()) {
		for (const auto& e : v.as_array())
			out.push_back(to_string_value(e));
	} else if (v.is_string()) {
		if (!v.as_string().empty())
			out.push_back(std::string(v.as_string()));
	}
	return out;
}

const std::vector<option>& all_options() {
	return all_options_impl();
}

const option* find_option(const std::string& name) {
	for (const auto& o : all_options_impl())
		if (o.name == name)
			return &o;
	return nullptr;
}

const char* kind_type_name(option_kind kind) {
	switch (kind) {
	case option_kind::boolean: return "bool";
	case option_kind::integer: return "int";
	case option_kind::string_list: return "stringlist";
	default: return "string";
	}
}

json::object default_config() {
	json::object cfg;
	for (const auto& o : all_options_impl()) {
		if (o.hidden)
			continue;
		if (!o.has_default) {
			switch (o.kind) {
			case option_kind::string_list: cfg[o.name] = json::array(); break;
			case option_kind::integer: cfg[o.name] = std::int64_t{0}; break;
			case option_kind::boolean: cfg[o.name] = false; break;
			default: cfg[o.name] = "";
			}
			continue;
		}
		switch (o.kind) {
		case option_kind::string_list: {
			json::array arr;
			for (const auto& s : o.def_list)
				arr.emplace_back(s);
			cfg[o.name] = std::move(arr);
			break;
		}
		case option_kind::boolean:
			cfg[o.name] = o.def_bool;
			break;
		case option_kind::integer:
			cfg[o.name] = o.def_int;
			break;
		default:
			cfg[o.name] = o.def_str;
			break;
		}
	}
	return cfg;
}

std::vector<std::string> args_for(const json::object& cfg) {
	// 按名称排序（与 golang sort.Strings 一致）。
	std::vector<std::string> names;
	names.reserve(cfg.size());
	for (const auto& kv : cfg)
		names.push_back(std::string(kv.key()));
	std::sort(names.begin(), names.end());

	std::vector<std::string> args;
	for (const auto& name : names) {
		const option* o = find_option(name);
		if (o == nullptr || o->hidden)
			continue;
		const json::value& val = cfg.at(name);
		// 跳过与注册表默认值相同的选项。
		if (equals_default(*o, val))
			continue;
		switch (o->kind) {
		case option_kind::string_list: {
			auto items = to_string_list(val);
			if (items.empty()) {
				args.push_back("--" + name);
				args.push_back("");
				continue;
			}
			for (const auto& item : items) {
				args.push_back("--" + name);
				args.push_back(item);
			}
			break;
		}
		case option_kind::boolean: {
			bool b = false;
			if (val.is_bool())
				b = val.as_bool();
			else if (val.is_string()) {
				const auto& s = val.as_string();
				b = (s == "true" || s == "1" || s == "yes" || s == "on");
			} else if (val.is_int64())
				b = val.as_int64() != 0;
			else if (val.is_double())
				b = val.as_double() != 0;
			args.push_back("--" + name + "=" + (b ? "true" : "false"));
			break;
		}
		default:
			args.push_back("--" + name);
			args.push_back(to_string_value(val));
			break;
		}
	}
	return args;
}

std::string validate_config(json::object& cfg) {
	// 兼容旧配置：proxy_ssl_name 已并入 ssl_sni。
	if (auto it = cfg.find("proxy_ssl_name"); it != cfg.end()) {
		if (!cfg.contains("ssl_sni"))
			cfg["ssl_sni"] = it->value();
		cfg.erase(it);
	}
	for (const auto& kv : cfg) {
		const option* o = find_option(std::string(kv.key()));
		if (o == nullptr)
			return "unknown option: " + std::string(kv.key());
		if (o->hidden)
			return "internal option cannot be set: " + std::string(kv.key());
	}
	return {};
}

} // namespace launcher
