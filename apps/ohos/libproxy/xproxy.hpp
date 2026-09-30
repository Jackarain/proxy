#ifndef INCLUDE__2026_09_30__XPROXY_OHOS_HPP
#define INCLUDE__2026_09_30__XPROXY_OHOS_HPP

#include <cstdint>
#include <memory>
#include <string>

namespace xproxy {

	// 返回 libxproxy 编译时使用的 OHOS API 版本.
	std::string min_sdk_version();

	// 返回 libxproxy 编译时记录的 git commit hash 前 6 位.
	std::string build_version();

	// 以 JSON 配置启动 proxy 服务, 成功返回 0, 失败返回非 0 值.
	int start(const std::string& config);

	// 停止 proxy 服务.
	void stop();

}

#endif // INCLUDE__2026_09_30__XPROXY_OHOS_HPP
