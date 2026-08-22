//
// wintun_tun_device.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef INCLUDE__2026_08_23__WINTUN_TUN_DEVICE_HPP
#define INCLUDE__2026_08_23__WINTUN_TUN_DEVICE_HPP

#include "proxy/use_awaitable.hpp"
#include "proxy/logging.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/windows/object_handle.hpp>

#include <memory>
#include <string>
#include <string_view>

#if defined(_WIN32)

#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif // !WIN32_LEAN_AND_MEAN

#	ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#		define _WINSOCK_DEPRECATED_NO_WARNINGS
#	endif // _WINSOCK_DEPRECATED_NO_WARNINGS

#	include <windows.h>
#	include <winreg.h>
#	include <winioctl.h>
#	include <ws2tcpip.h>
#	include <iphlpapi.h>
#	include <cfgmgr32.h>
#	if defined(__MINGW32__) || defined(__MINGW64__)
#		include <ddk/ndisguid.h>
#	else
#		include <ndisguid.h>
#	endif

extern "C" {
#	include "wintun.h"
#	include "ring_buffer.h"
}

namespace proxy {

	namespace net = boost::asio;

	namespace wintun_detail {

		inline ULONG wintun_ring_packet_align(ULONG size)
		{
			return (size + (WINTUN_PACKET_ALIGN - 1)) & ~(WINTUN_PACKET_ALIGN - 1);
		}

		inline ULONG wintun_ring_wrap(ULONG value)
		{
			return value & (WINTUN_RING_CAPACITY - 1);
		}

		// wintun.dll 动态加载封装, 仅加载适配器管理所需 API.
		struct wintun_api;

		// 打开 wintun 设备对象文件, 失败返回 INVALID_HANDLE_VALUE.
		HANDLE open_wintun(const std::string& name);

		// 安装 wintun 驱动 (从 exe 资源中解压 wintun.sys/inf/cat, 使用 pnputil 安装).
		bool install_wintun();
	}

	// Windows wintun 设备封装, 提供与 tun_device 一致的接口.
	//
	// 基于 wintun 驱动自带的 ring buffer 实现异步读写:
	//   - 应用 -> 驱动: 写 send ring, 置位发送事件.
	//   - 驱动 -> 应用: 驱动写 receive ring, 置位接收事件, 通过
	//     boost::asio::windows::object_handle 等待事件.
	// 适配器的创建/打开通过动态加载 wintun.dll 完成, 参考 avpn.
	class wintun_tun_device
	{
	public:
		explicit wintun_tun_device(net::any_io_executor executor);
		~wintun_tun_device();

		wintun_tun_device(const wintun_tun_device&) = delete;
		wintun_tun_device& operator=(const wintun_tun_device&) = delete;

	public:
		// 创建/打开 wintun 适配器并注册 ring buffer, 成功返回空错误码.
		boost::system::error_code open(const std::string& name, int mtu);

		// 关闭适配器并释放资源.
		void close();

		// 返回设备名.
		const std::string& device_name() const { return m_devname; }

		bool is_open() const noexcept { return m_opened; }

		net::any_io_executor get_executor() noexcept { return m_executor; }

		// 异步读取 tun 设备上的 IP 数据包.
		template <typename MutableBufferSequence, typename ReadHandler>
		BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler,
			void(boost::system::error_code, std::size_t))
			async_read_some(const MutableBufferSequence& buffers,
				ReadHandler&& handler)
		{
			return async_initiate<ReadHandler,
				void(boost::system::error_code, std::size_t)>(
					initiate_async_read_some(this), handler, buffers);
		}

		// 异步写入 IP 数据包到 tun 设备.
		template <typename ConstBufferSequence, typename WriteHandler>
		BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(WriteHandler,
			void(boost::system::error_code, std::size_t))
			async_write_some(const ConstBufferSequence& buffers,
				WriteHandler&& handler)
		{
			return async_initiate<WriteHandler,
				void(boost::system::error_code, std::size_t)>(
					initiate_async_write_some(this), handler, buffers);
		}

	private:
		// 从 receive ring 读取一个数据包到 buf, 返回读取字节数.
		// 0 表示暂无数据, -1 表示错误.
		int read_wintun(std::string_view buf);

		// 将 buf 中的数据包写入 send ring, 返回写入字节数.
		// 0 表示 ring 已满, -1 表示错误.
		int write_wintun(std::string_view buf);

		struct initiate_async_read_some
		{
			using executor_type = net::any_io_executor;

			explicit initiate_async_read_some(wintun_tun_device* self)
				: self_(self)
			{}

			executor_type get_executor() const noexcept
			{
				return self_->m_executor;
			}

			template <typename Handler, typename MutableBufferSequence>
			void operator()(Handler&& handler,
				const MutableBufferSequence& buffers)
			{
				// wintun 一次只处理一个数据包, 仅使用第一个缓冲区,
				// 避免多缓冲区序列按总大小拷贝导致越界.
				auto buf = net::buffer_sequence_begin(buffers);
				auto bufsize = buf->size();
				auto bufptr = static_cast<uint8_t*>(buf->data());
				std::string_view bufs(
					reinterpret_cast<const char*>(bufptr), bufsize);
				boost::system::error_code ec;

				auto bytes_transferred = self_->read_wintun(bufs);
				if (bytes_transferred == 0)
				{
					// 短暂 spin 等待驱动写入数据, 减少协程切换开销.
					LARGE_INTEGER frequency;
					QueryPerformanceFrequency(&frequency);
					ULONG64 spin_max =
						frequency.QuadPart / 1000 / 10; // 100us.

					LARGE_INTEGER spin_start;
					QueryPerformanceCounter(&spin_start);

					for (; !self_->m_abort;)
					{
						bytes_transferred = self_->read_wintun(bufs);
						if (bytes_transferred > 0)
							break;

						if (bytes_transferred == 0)
						{
							LARGE_INTEGER spin_now;
							QueryPerformanceCounter(&spin_now);
							if (static_cast<ULONG64>(spin_now.QuadPart) -
								static_cast<ULONG64>(spin_start.QuadPart) >= spin_max)
								break;

							Sleep(0);
							continue;
						}
					}

					// 被中止操作.
					if (self_->m_abort)
					{
						ec = net::error::operation_aborted;
						net::post(self_->get_executor(),
							[handler = std::move(handler), ec]() mutable {
								handler(ec, 0);
							});
						return;
					}
				}

				// spin 后仍无数据, 挂起等待接收事件.
				if (bytes_transferred == 0)
				{
					net::co_spawn(this->get_executor(),
						[self = self_, handler = std::move(handler),
							bufs = std::move(bufs)]() mutable -> net::awaitable<void>
						{
							boost::system::error_code ec;
							int bytes_transferred = 0;
							bool done = false;

							auto complete = [&]() mutable
							{
								if (done)
									return;
								done = true;
								net::post(self->get_executor(),
									[handler = std::move(handler), ec,
										bytes_transferred]() mutable {
										handler(ec, bytes_transferred);
									});
							};

							if (self->m_abort)
							{
								ec = net::error::operation_aborted;
								complete();
								co_return;
							}

							auto& object = self->m_receive_object_moved;
							for (;;)
							{
								co_await object.async_wait(net_awaitable[ec]);
								if (ec)
								{
									complete();
									co_return;
								}

								bytes_transferred = self->read_wintun(bufs);
								if (bytes_transferred == 0)
									continue;
								if (bytes_transferred > 0)
									break;
								if (bytes_transferred < 0)
								{
									ec = net::error::operation_aborted;
									bytes_transferred = 0;
									complete();
									co_return;
								}
							}

							ec = {};
							complete();
							co_return;
						}, net::detached);

					return;
				}

				if (bytes_transferred < 0)
				{
					ec = net::error::operation_aborted;
					bytes_transferred = 0;
				}

				net::post(self_->get_executor(),
					[handler = std::move(handler), ec,
						bytes_transferred]() mutable {
						handler(ec, bytes_transferred);
					});
			}

			wintun_tun_device* self_;
		};

		struct initiate_async_write_some
		{
			using executor_type = net::any_io_executor;

			explicit initiate_async_write_some(wintun_tun_device* self)
				: self_(self)
			{}

			executor_type get_executor() const noexcept
			{
				return self_->m_executor;
			}

			template <typename Handler, typename ConstBufferSequence>
			void operator()(Handler&& handler,
				const ConstBufferSequence& buffers)
			{
				// wintun 一次只处理一个数据包, 仅使用第一个缓冲区,
				// 避免多缓冲区序列按总大小拷贝导致越界.
				auto buf = net::buffer_sequence_begin(buffers);
				auto bufptr = static_cast<uint8_t*>(buf->data());
				auto bufsize = buf->size();
				std::string_view bufs(
					reinterpret_cast<const char*>(bufptr), bufsize);
				boost::system::error_code ec;

				auto bytes_transferred = self_->write_wintun(bufs);
				if (bytes_transferred < 0 || self_->m_abort)
				{
					ec = net::error::operation_aborted;
					net::post(self_->get_executor(),
						[handler = std::move(handler), ec]() mutable {
							handler(ec, 0);
						});
					return;
				}

				if (bytes_transferred == 0)
				{
					// send ring 已满, 定时重试写入.
					net::co_spawn(self_->get_executor(),
						[self = self_, handler = std::move(handler),
							bufs = std::move(bufs)]() mutable -> net::awaitable<void>
						{
							auto bytes_transferred = self->write_wintun(bufs);
							boost::system::error_code ec;
							bool done = false;

							auto complete = [&]() mutable
							{
								if (done)
									return;
								done = true;
								net::post(self->get_executor(),
									[handler = std::move(handler), ec,
										bytes_transferred]() mutable {
										handler(ec, bytes_transferred);
									});
							};

							if (bytes_transferred < 0 || self->m_abort)
							{
								ec = net::error::operation_aborted;
								bytes_transferred = 0;
								complete();
								co_return;
							}

							while (bytes_transferred == 0)
							{
								net::steady_timer wait_timer(
									self->get_executor());
								wait_timer.expires_after(
									std::chrono::milliseconds(1));
								co_await wait_timer.async_wait(
									net_awaitable[ec]);
								if (ec)
								{
									complete();
									co_return;
								}

								bytes_transferred = self->write_wintun(bufs);
								if (bytes_transferred < 0 || self->m_abort)
								{
									ec = net::error::operation_aborted;
									bytes_transferred = 0;
									complete();
									co_return;
								}
							}

							ec = {};
							complete();
							co_return;
						}, net::detached);
					return;
				}

				net::post(self_->get_executor(),
					[handler = std::move(handler), ec,
						bytes_transferred]() mutable {
						handler(ec, bytes_transferred);
					});
			}

			wintun_tun_device* self_;
		};

	private:
		net::any_io_executor m_executor;
		std::string m_devname;

		std::unique_ptr<wintun_detail::wintun_api> m_api;

		HANDLE m_send_ring_handle{ INVALID_HANDLE_VALUE };
		HANDLE m_receive_ring_handle{ INVALID_HANDLE_VALUE };
		HANDLE m_send_event_moved{ INVALID_HANDLE_VALUE };
		HANDLE m_receive_event_moved{ INVALID_HANDLE_VALUE };
		net::windows::object_handle m_receive_object_moved;
		HANDLE m_wintun_file{ INVALID_HANDLE_VALUE };

		struct tun_ring* m_send_ring{ nullptr };
		struct tun_ring* m_receive_ring{ nullptr };

		WINTUN_ADAPTER_HANDLE m_wintun_handle{ nullptr };
		MIB_UNICASTIPADDRESS_ROW m_address_row{ 0 };

		bool m_abort{ true };
		bool m_opened{ false };
	};

} // namespace proxy

#endif // defined(_WIN32)

#endif // INCLUDE__2026_08_23__WINTUN_TUN_DEVICE_HPP
