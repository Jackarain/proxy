//
// proxy_config.hpp
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_10_13__PROXY_CONFIG_HPP
#define INCLUDE__2026_10_13__PROXY_CONFIG_HPP


#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/url.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef USE_BOOST_FILESYSTEM
# include <boost/filesystem.hpp>
#else
# include <filesystem>
#endif


namespace proxy {

	namespace net = boost::asio;

	using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>

	namespace urls = boost::urls;			// form <boost/url.hpp>

#ifdef USE_BOOST_FILESYSTEM
	namespace fs = boost::filesystem;
#else
	namespace fs = std::filesystem;
#endif

	//////////////////////////////////////////////////////////////////////////

	// tcp_session_expired_time 用于指定 tcp session 的默认过期时间, 单位为秒.
	inline const int tcp_session_expired_time = 60;

	// udp_session_expire_time 用于指定 udp session 的过期时间, 单位为秒.
	inline const int udp_session_expired_time = 300;

	//////////////////////////////////////////////////////////////////////////

	// proxy server 参数选项, 用于指定 proxy server 的各种参数.
	struct proxy_server_option
	{
		// 代理服务端 TCP 监听端口列表。
		//
		// - 支持同时监听多个 endpoint（多地址/多端口）。
		// - tuple 的第 1 个元素为 tcp::endpoint；第 2 个元素仅在 endpoint 为 IPv6 地址时生效：
		//   * true  : 表示设置 IPV6_V6ONLY，仅接受 IPv6 连接；
		//   * false : 表示允许 IPv4-mapped/双栈行为（具体由系统与 socket 选项决定）。
		std::vector<std::tuple<tcp::endpoint, bool>> listens_;

		// 代理服务端 Unix Domain Socket（UDS）监听端点列表。
		//
		// - 支持同时监听多个 UDS 路径。
		// - 适用于同机进程间通信、在容/本机环境中提供更安全/更低开销的入口。
		std::vector<net::local::stream_protocol::endpoint> uds_listens_;

		// 通过 proxy_pass 转发/桥接当前进程的 stdio 到指定目标服务器。
		//
		// - 典型用途：将 stdin/stdout 作为一条逻辑“连接”转发到远端（例如作为守护进程的 stdio 入口）。
		// - 为空表示不启用该功能。
		std::string stdio_target_;

		// launcher 控制通道 URL。
		//
		// - 为空表示不启用 launcher 控制通道。
		// - 否则 proxy_server 作为 WebSocket 客户端主动连接 launcher，上报实时状态
		//   （register/status）并处理 launcher 下发的运行期配置（set_config）与用户管理请求。
		// - 收到 launcher 的 shutdown 请求时，proxy_server 内部关闭所有 session/连接/
		//   accept/定时器，使进程自然退出，无需外部回调。
		std::string launcher_url_;

		// PAM 认证服务配置名称。
		//
		// - 为空表示不启用 PAM 认证。
		// - 否则，将使用指定的 PAM 服务配置进行认证。
		std::string pam_auth_;

		// 授权（认证）用户列表。
		//
		// auth_users 的含义：
		// - 第 1 个元素：用户名
		// - 第 2 个元素：密码
		// - 第 3 个元素：该用户绑定的网络出口 IP / 本地接口（用于对外发起连接时的绑定，或作为策略匹配字段）
		// - 第 4 个元素：该用户专属的 proxy_pass（可选），用于让不同用户走不同上游/链路
		//
		// 约定：
		// - auth_users_ 为空：表示不需要认证（匿名访问）。
		// - 支持多个用户，例如：
		//   { {"user1", "passwd1", "", std::nullopt}, {"user2", "passwd2", "1.2.3.4", url} }。
		using auth_users = std::tuple<std::string, std::string, std::string, std::optional<urls::url>>;
		std::vector<auth_users> auth_users_;

		// 用户限速配置（按用户名粒度）。
		//
		// - key   ：用户名
		// - value ：速率（单位建议与实现保持一致；若实现为 bytes/s，则此处即 bytes/second）
		// - 未配置某用户时：表示不对该用户单独限速（由全局 tcp_rate_limit_ 或默认策略决定）。
		std::unordered_map<std::string, int> users_rate_limit_;

		// 用户流量配额（按用户名粒度，单位字节，按上行+下行总和计算）。
		//
		// - key   ：用户名
		// - value ：流量配额（字节），0 表示不限制
		// - 用于 launcher 状态展示与配额续接（超限停止该用户连接）。
		std::unordered_map<std::string, int64_t> users_quota_;

		// 用户连接数限制（按用户名粒度，单位：同时存在的连接数）。
		//
		// - key   ：用户名
		// - value ：允许的最大同时连接数，<=0 表示不限制
		// - 仅在认证成功时拒绝超出限制的新连接，不影响已建立的连接。
		std::unordered_map<std::string, int> users_conn_limit_;

		// 允许访问的地区集合（白名单）。
		//
		// - 例如：{ "中国", "香港", "台湾" }
		// - 为空：表示不启用地区限制（允许所有地区）。
		// - 生效前提：必须正确配置 ipip_db_，并且地区解析能够命中。
		std::unordered_set<std::string> allow_regions_;

		// 拒绝访问的地区集合（黑名单）。
		//
		// - 例如：{ "美国", "日本" }
		// - 为空：表示不启用地区限制（不拒绝任何地区）。
		// - 若其中地址与 allow_regions_ 同时设置时，由 deny_regions_ 决定。
		// - 生效前提：必须正确配置 ipip_db_。
		std::unordered_set<std::string> deny_regions_;

		// IPIP 数据库文件路径（用于根据客户端 IP 解析地区信息，进而做 allow/deny 判断）。
		//
		// - 数据库可从 https://www.ipip.net 下载或该项目 Release 页面下载。
		// - 为空：表示不启用地区解析与地区限制功能（allow/deny 不生效）。
		std::string ipip_db_;

		// 上游代理（多层代理 / 级联）配置：当前代理把流量转发给下一个代理或直接目标。
		//
		// - 对客户端无感：客户端只连接当前服务端，当前服务端再决定如何转发到下一跳。
		// - 典型示例：
		//   socks5://user:passwd@proxy.server.com:1080
		//   https://user:passwd@proxy.server.com:1080
		//
		// DNS 行为说明：
		// - 当 proxy_pass_ 为 socks5 时，使用 hostname 模式（即域名解析在远端/上游执行）。
		std::optional<urls::url> proxy_pass_;

		// 在多层代理模式下，与“下一个代理服务器”之间是否使用 TLS/SSL 加密。
		//
		// - 通常用于 proxy_pass_ 指向 socks 代理时使用该选项（用于决定 socks 连接的外层是否包 TLS）。
		// - 若 proxy_pass_ 为 HTTP/HTTPS proxy，是否加密同时也由 URL scheme 决定（http vs https）。
		bool proxy_pass_use_ssl_{ false };

		// proxy_pass 预选连接池大小（TUN 模式）。
		//
		// - 预先建立若干条到 proxy_pass 的 TCP(+TLS) 连接并保活，TCP flow
		//   需要走代理时优先从池中取一条已建立的连接，避免重复握手。
		// - 启动后每 5 秒建 1 条连接，直到池满该数量；连接被取走或断开后
		//   立即异步补充，始终维持该数量的空闲连接。
		// - 0 表示不启用预选连接池；默认 20。
		size_t proxy_pass_pool_size_{ 20 };

		// 指定本代理在向外（上游/目标）发起连接时绑定的本地源地址。
		//
		// - 适用于多网卡/多出口场景，通过绑定不同 local_ip_ 控制出站链路。
		// - 为空：不显式绑定，由操作系统路由表/策略自动选择源地址。
		std::string local_ip_;

		// 是否启用 TCP 端口重用（SO_REUSEPORT）。
		//
		// - 仅 Linux kernel 3.9+ 支持。
		// - 典型用途：多进程/多线程同时 bind 同一端口以提升 accept 性能或做负载均衡。
		bool reuse_port_{ false };

		// 是否启用 Happy Eyeballs 连接算法（IPv6/IPv4 并行探测以降低连接延迟）。
		//
		// - true ：启用（默认），通常可改善双栈环境下的首包延迟与可用性。
		// - false：禁用，按系统/解析顺序串行尝试。
		bool happyeyeballs_{ true };

		// 是否仅使用 IPv4 发起出站连接。
		//
		// - true  ：只使用 IPv4
		// - false ：IPv4/IPv6 都可能使用（受解析结果、happyeyeballs_ 等影响）
		bool connect_v4_only_{ false };

		// 是否仅使用 IPv6 发起出站连接。
		//
		// - true  ：只使用 IPv6
		// - false ：IPv4/IPv6 都可能使用（受解析结果、happyeyeballs_ 等影响）
		bool connect_v6_only_{ false };

		// 是否作为透明代理（Transparent Proxy）。
		//
		// - 仅 Linux 场景可用（通常依赖 TPROXY / iptables / nft 等路由策略配套配置）。
		// - 启用后通常意味着需要特殊的 socket 选项与策略路由，才能“伪装”源地址或接管流量。
		bool transparent_{ false };

		// 是否启用 TUN 设备模式（TUN2SOCKS）。
		//
		// - 仅 Linux 场景可用，创建 TUN 设备抓取 IP 数据包，解析 TCP/UDP
		//   后按分流规则转发到上游代理（proxy_pass）或直接连接目标。
		// - 启用时必须同时指定 proxy_pass_，否则启动失败。
		// - 系统路由需要由外部脚本/文档配置，程序只负责创建设备与清理。
		bool tun_{ false };

		// TUN 设备名称，为空使用系统默认分配的名称。
		std::string tun_name_;

		// TUN 设备 MTU，默认 1500。
		int tun_mtu_{ 1500 };

		// 外部注入的 TUN 设备文件描述符（Android VpnService 场景），-1 表示不使用.
		// 非 -1 时 tun_server 直接包装该 fd，不再自行打开 /dev/net/tun.
		int tun_fd_{ -1 };

		// 是否等待外部注入 TUN fd（Android VpnService 场景）.
		// 为 true 时 tun_server 启动后不创建设备，等待 launcher 控制通道
		// 下发 set_tun_fd 后注入 fd 并开始读包.
		bool tun_wait_fd_{ false };

		// 代理域名列表（后缀匹配）：命中该列表的域名解析走上游 DoH，
		// 数据面按解析结果经 proxy_pass_ 转发；未命中走本地解析并直连。
		std::vector<std::string> proxy_domains_;

		// 代理 CIDR 列表（IPv4/IPv6）：命中该列表的目标地址经
		// proxy_pass_ 转发，未命中直连目标。
		std::vector<std::string> proxy_cidr_;

		// DNS 分流配置（Android app 端）.
		//
		// - dns_domestic_：国内 DNS 列表（仅 IP），未命中 proxy_domains_
		//   的 DNS 查询直连这些服务器解析。
		// - dns_foreign_：国外 DNS 列表（仅 IP），命中 proxy_domains_ 的
		//   DNS 查询经 proxy_pass_ 转发到这些服务器解析。
		// - dns_doh_：国外 DoH URL（可选），命中 proxy_domains_ 的查询
		//   以 DoH 方式解析；与 proxy_pass_ 同服务时直连，否则经代理转发。
		std::vector<std::string> dns_domestic_;
		std::vector<std::string> dns_foreign_;
		std::string dns_doh_;

		// 向外连接发起时使用的 SO_MARK（Linux）标记。
		//
		// - 设置后，所有由本代理发起的 TCP/UDP 出站连接都会带上此标记，
		//   可用于策略路由/防火墙匹配。
		// - 未设置表示不打 mark。
		std::optional<uint32_t> so_mark_;

		// TCP 会话/连接超时时间（秒）。
		//
		// - 用于控制连接建立/读写等阶段的超时。
		// - 默认值为 tcp_session_expired_time, 小于等于0表示禁用超时检查。
		int tcp_timeout_{ tcp_session_expired_time };

		// UDP 会话/流过期时间（秒）。
		//
		// - 用于控制 UDP 会话的生命周期，尤其在透明代理模式下用于清理过期的流。
		// - 默认值为 300 秒（可根据实际需求调整）。
		int udp_timeout_{ udp_session_expired_time };

		// TCP 连接速率限制（全局），单位：bytes/second。
		//
		// - -1 表示不限制。
		// - 若 users_rate_limit_ 中对某用户单独设置，通常应以“用户限速优先或覆盖全局”为准（由实现定义）。
		int tcp_rate_limit_{ -1 };

		// 作为服务端时：SSL 证书目录。
		//
		// - 会自动搜索子目录；每个子目录保存一个域名对应的一组证书文件。
		// - 若证书为加密私钥，则需要 password.txt 存储解密密码（按实现约定读取）。
		std::string ssl_cert_path_;

		// 作为客户端时：CA 证书目录或 CA bundle 路径（用于校验上游 TLS 证书）。
		//
		// - 若不指定：默认使用 curl 官方提供的 CA bundle（见 https://curl.se/docs/caextract.html）。
		// - 具体支持目录还是文件取决于 TLS 实现（OpenSSL 等）。
		std::string ssl_cacert_path_;

		// 指定上游代理/服务器的 SNI（Server Name Indication）名字。
		//
		// - 当上游使用“多域名证书”且需要指定匹配域名时，通过该字段明确 SNI。
		// - 为空：通常使用 proxy_pass_ 中的 host 作为默认 SNI（若实现如此）。
		std::string proxy_ssl_name_;

		// 指定允许使用的 TLS 加密套件（cipher suites）。
		//
		// - 格式取决于 TLS 库（例如 OpenSSL 的 cipher list 格式）。
		// - 为空：使用库默认配置。
		std::string ssl_ciphers_;

		// 是否优先使用服务端指定的加密套件顺序（server cipher preference）。
		//
		// - true ：优先服务端顺序
		// - false：可能由客户端偏好决定
		bool ssl_prefer_server_ciphers_;

		// HTTP 文档根目录：用于将代理伪装为 Web 站点（静态文件服务）。
		//
		// - 为空：不启用静态站点功能；遇到 HTTP/HTTPS 文件请求时返回错误信息。
		// - 非空：对该目录提供文件访问能力（需注意安全，可启用 htpasswd_ 确保访问需要认证）。
		std::string doc_directory_;

		// DNS 上游服务器地址，用于 DoH (DNS over HTTPS) 查询转发。
		//
		// - 格式: "ip:port"，如 "8.8.8.8:53"；也支持 DoH URL，如
		//   "https://dns.google/dns-query"。
		std::optional<std::string> dns_upstream_;

		// UDP DNS 服务器监听端口。
		//
		// - 0 表示不启用 UDP DNS 服务器（默认）。
		// - 非 0 时，proxy_server 在启动时监听该 UDP 端口，把接收到的 DNS
		//   查询转发到 dns_upstream_（若配置），否则按系统默认解析流程构造
		//   响应。
		int dns_udp_port_{ 0 };

		// DNS 查询结果缓存条数上限。
		//
		// - 0 表示不启用 DNS 查询结果缓存（默认）。
		// - 非 0 时配合 dns_cache_ttl_ 生效：缓存命中直接返回结果并重置
		//   过期时间，达到上限后按最近最少使用（LRU）淘汰。
		int dns_cache_size_{ 0 };

		// DNS 查询结果缓存过期时间（秒）。
		//
		// - 0 表示不启用 DNS 查询结果缓存（默认）。
		// - 非 0 时，缓存条目在过期后删除；过期前重复查询该域名会重置
		//   过期时间（滑动 TTL）。
		int dns_cache_ttl_{ 0 };

		// 是否禁用 DNS 的 IPv6 解析返回。
		//
		// - true ：开启后 AAAA 查询直接返回空应答（NODATA），只返回 IPv4
		//   地址，且不转发到上游 DNS。
		// - false：不干预，按正常流程处理 AAAA 查询（默认）。
		bool dns_no_ipv6_{ false };

		// 是否启用目录列表（类似 nginx 的 autoindex）。
		//
		// - 仅在 doc_directory_ 非空、启用了静态站点功能时生效。
		// - true ：访问目录时返回目录下文件列表。
		// - false：访问目录时通常返回错误或默认首页。
		bool autoindex_;

		// 是否启用 HTTP Basic Auth（类似 htpasswd 的认证方式）。
		//
		// - 默认 false：不启用。
		// - 若启用：通常需要配置 auth_users_ 用于用户信息认证。
		bool htpasswd_{ false };

		// 是否禁用 HTTP 相关服务入口。
		//
		// - true ：客户端无法使用 HTTP 与服务端通信，包含 HTTP 代理功能。
		bool disable_http_{ false };

		// 是否禁用 SOCKS 代理服务入口。
		//
		// - true ：不提供 SOCKS4/5 代理服务。
		// - false：提供 SOCKS 服务。
		bool disable_socks_{ false };

		// 是否启用 WebSocket CONNECT 隧道入口。
		//
		// - true ：接受 GET + Upgrade: websocket + Proxy-Connect 请求，
		//   在完成握手后按 CONNECT 隧道转发目标 TCP 流量。
		// - false：不启用该私有扩展（默认）。
		bool websocket_proxy_{ false };

		// 是否禁止不安全（明文）连接。
		//
		// - true ：禁止所有非加密连接。
		// - false：允许明文与加密并存，由客户端与服务端协商确定是否加密。
		bool disable_insecure_{ false };


		// 是否禁用 TLS 证书校验（不安全，通常用于测试或特定环境）。
		//
		// - true ：不校验 TLS 证书，允许连接到证书无效/自签的服务器。
		// - false：启用 TLS 证书校验，确保连接安全。
		bool disable_check_cert_{ false };

		// 是否禁用 UDP 代理服务。
		//
		// - true ：不提供 UDP 转发/代理能力（如 UDP ASSOCIATE 等）。
		// - false：提供 UDP 相关服务。
		bool disable_udp_{ false };

		// 是否启用“噪声注入 / 混淆”（scramble）以干扰流量分析。
		//
		// 生效条件：
		// - 必须 server/client 两端同时启用才有效。
		//
		// 工作机制：
		// - TLS 握手完成后，双方互相发送一段随机长度的随机数据作为“噪声”；
		// - 双方收到对方噪声后对整段噪声做 hash，得到后续数据的加密密钥；
		// - 后续数据使用简单异或加密；密钥用完后通过 hash(hash) 派生新密钥；
		// - hash 算法采用 xxhash；密钥长度固定为 16 字节以兼顾性能。
		//
		// 取舍：
		// - 可降低被动流量分析的有效性，但会增加额外流量与时延；
		// - 默认不启用，建议仅在明确存在分析/干扰风险时开启。
		bool scramble_{ false };

		// 噪声注入的最大长度（单位：字节）。
		//
		// - 允许范围：[16, 64K]。
		int64_t noise_length_{ -1 };
	};

}


#endif // INCLUDE__2026_10_13__PROXY_CONFIG_HPP
