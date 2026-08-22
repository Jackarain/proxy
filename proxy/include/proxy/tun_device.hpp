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

#if defined(_WIN32)
# include "proxy/wintun_tun_device.hpp"
#else
# include <boost/asio/posix/stream_descriptor.hpp>
#endif

namespace proxy {

#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

// tun_device 封装 TUN 设备，提供跨平台统一的异步读写接口。
//
// 支持：
//   - Linux: /dev/net/tun。
//   - macOS: utun 内核控制。
//   - Windows: wintun 驱动。
//
// 打开设备后返回可用于读写 IP 数据包的句柄（IFF_TUN | IFF_NO_PI）。
// 设备名为空时由系统自动分配；非持久化设备在关闭时自动销毁。
// 仅创建设备并设置 MTU，IP 地址与路由由外部脚本配置。
class tun_device
{
public:
	explicit tun_device(net::any_io_executor executor);
	~tun_device();

	tun_device(const tun_device&) = delete;
	tun_device& operator=(const tun_device&) = delete;

	// 打开 TUN 设备并设置 MTU，成功返回空错误码。
	// name 为空时由系统自动分配设备名；mtu 小于等于 0 时保持默认。
	boost::system::error_code open(const std::string& name, int mtu) noexcept;

	// 包装外部传入的 TUN fd（Android VpnService 建立后 detach 的 fd）.
	// 设备地址/路由/MTU 由外部（VpnService）配置，此处仅接管读写.
	// 成功返回空错误码；fd 小于 0 返回 invalid_argument.
	boost::system::error_code open(int fd, int mtu) noexcept;

	// 关闭设备，非持久化设备随之销毁。
	void close() noexcept;

	bool is_open() const noexcept { return m_opened; }

	// 返回原生句柄（Windows 下无意义，返回 -1）。
	int native_handle() const noexcept;

	const std::string& name() const noexcept { return m_name; }

	int mtu() const noexcept { return m_mtu; }

	// 异步读取 TUN 设备上的 IP 数据包。
	template <typename MutableBufferSequence, typename ReadHandler>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler,
		void(boost::system::error_code, std::size_t))
		async_read_some(const MutableBufferSequence& buffers,
			ReadHandler&& handler)
	{
#if defined(_WIN32)
		return m_wintun.async_read_some(buffers,
			std::forward<ReadHandler>(handler));
#else
		return m_stream.async_read_some(buffers,
			std::forward<ReadHandler>(handler));
#endif
	}

	// 异步写入 IP 数据包到 TUN 设备。
	template <typename ConstBufferSequence, typename WriteHandler>
	BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(WriteHandler,
		void(boost::system::error_code, std::size_t))
		async_write_some(const ConstBufferSequence& buffers,
			WriteHandler&& handler)
	{
#if defined(_WIN32)
		return m_wintun.async_write_some(buffers,
			std::forward<WriteHandler>(handler));
#else
		return m_stream.async_write_some(buffers,
			std::forward<WriteHandler>(handler));
#endif
	}

private:
	net::any_io_executor m_executor;
#if defined(_WIN32)
	wintun_tun_device m_wintun;
#else
	mutable net::posix::stream_descriptor m_stream;
#endif
	std::string m_name;
	int m_mtu { 1500 };
	bool m_opened { false };
};

#else // 不支持的平台

// 非支持的平台不提供 TUN 支持.
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

#endif // defined(__linux__) || defined(__APPLE__) || defined(_WIN32)

} // namespace proxy

#endif // INCLUDE__2026_08_22__TUN_DEVICE_HPP
