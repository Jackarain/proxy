//
// tun_device.hpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_08_22__TUN_DEVICE_HPP
#define INCLUDE__2026_08_22__TUN_DEVICE_HPP

#include "proxy/use_awaitable.hpp"

#include <string>

namespace proxy {

#if defined(__linux__)

// tun_device 封装 Linux TUN 设备（/dev/net/tun）。
//
// - 打开设备后返回可用于读写 IP 数据包的 fd（IFF_TUN | IFF_NO_PI）。
// - 设备名为空时由内核自动分配；非持久化设备在 fd 关闭时自动销毁。
// - 仅创建设备并设置 MTU，IP 地址与路由由外部脚本配置。
class tun_device
{
public:
	tun_device() = default;
	~tun_device();

	tun_device(const tun_device&) = delete;
	tun_device& operator=(const tun_device&) = delete;

	// 打开 TUN 设备并设置 MTU，成功返回空错误码。
	// name 为空时由内核自动分配设备名；mtu 小于等于 0 时保持默认。
	boost::system::error_code open(const std::string& name, int mtu) noexcept;

	// 关闭设备 fd，非持久化设备随之销毁。
	void close() noexcept;

	bool is_open() const noexcept { return m_fd >= 0; }

	int native_handle() const noexcept { return m_fd; }

	const std::string& name() const noexcept { return m_name; }

	int mtu() const noexcept { return m_mtu; }

private:
	int m_fd { -1 };
	std::string m_name;
	int m_mtu { 1500 };
};

#else // !defined(__linux__)

// 非 Linux 平台不提供 TUN 支持.
class tun_device
{
public:
	tun_device() = default;
	~tun_device() = default;

	tun_device(const tun_device&) = delete;
	tun_device& operator=(const tun_device&) = delete;

	boost::system::error_code open(const std::string&, int) noexcept
	{
		return make_error_code(boost::system::errc::not_supported);
	}

	void close() noexcept {}

	bool is_open() const noexcept { return false; }

	int native_handle() const noexcept { return -1; }

	const std::string& name() const noexcept
	{
		static const std::string empty;
		return empty;
	}

	int mtu() const noexcept { return 1500; }
};

#endif // defined(__linux__)

} // namespace proxy

#endif // INCLUDE__2026_08_22__TUN_DEVICE_HPP
