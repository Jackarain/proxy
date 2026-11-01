//
// tun_device_wintun.hpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "tunio/detail/completion.hpp"
#include "tunio/packet_buffer.hpp"
#include "tunio/tun_config.hpp"

#include <boost/asio.hpp>

#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#include <cfgmgr32.h>
#include <iphlpapi.h>
#include <windows.h>
#include <winioctl.h> /* CTL_CODE, METHOD_BUFFERED */
#include <ws2tcpip.h>
#if defined(__MINGW32__) || defined(__MINGW64__)
#include <ddk/ndisguid.h>
#else
#include <ndisguid.h>
#endif

/* ==========================================================================
 * Wintun 内核驱动用户态接口 — ring_buffer.h + wintun.h 的内联提取版本.
 * 仅保留实际用到的定义.
 * ========================================================================== */

extern "C" {

/** CTL_CODE 所需常量（某些 mingw 头里未导出） */
#ifndef METHOD_BUFFERED
#define METHOD_BUFFERED 0
#endif
#ifndef FILE_READ_DATA
#define FILE_READ_DATA 0x0001
#endif
#ifndef FILE_WRITE_DATA
#define FILE_WRITE_DATA 0x0002
#endif

/* --- Ring buffer 常量 --- */

#define WINTUN_RING_CAPACITY 0x800000
#define WINTUN_RING_TRAILING_BYTES 0x10000
#define WINTUN_MAX_PACKET_SIZE 0xffff
#define WINTUN_MAX_IP_PACKET_SIZE 0xffff
#define WINTUN_PACKET_ALIGN 4

#define TUN_IOCTL_REGISTER_RINGS                                               \
    CTL_CODE(51820U, 0x970U, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)

/** Wintun ring buffer layout. */
struct tun_ring
{
    volatile ULONG head;
    volatile ULONG tail;
    volatile LONG alertable;
    UCHAR data[WINTUN_RING_CAPACITY + WINTUN_RING_TRAILING_BYTES];
};

/** IOCTL register_rings payload. */
struct tun_register_rings
{
    struct
    {
        ULONG ring_size;
        tun_ring* ring;
        HANDLE tail_moved;
    } send, receive;
};

/** Packet stored inside a ring buffer. */
struct TUN_PACKET_HEADER
{
    uint32_t size;
};

struct TUN_PACKET
{
    uint32_t size;
    UCHAR data[WINTUN_MAX_PACKET_SIZE];
};

} /* extern "C" */

/* --- Adapter management function pointer types --- */

typedef struct _WINTUN_ADAPTER* WINTUN_ADAPTER_HANDLE;

typedef WINTUN_ADAPTER_HANDLE(WINAPI* PfnWintunCreateAdapter)(
    LPCWSTR Name, LPCWSTR TunnelType, const GUID* RequestedGUID);

typedef WINTUN_ADAPTER_HANDLE(WINAPI* PfnWintunOpenAdapter)(LPCWSTR Name);

typedef VOID(WINAPI* PfnWintunCloseAdapter)(WINTUN_ADAPTER_HANDLE Adapter);

typedef VOID(WINAPI* PfnWintunGetAdapterLUID)(
    WINTUN_ADAPTER_HANDLE Adapter, NET_LUID* Luid);

namespace tunio {
namespace net = boost::asio;
namespace detail {

template <typename FallbackExecutor, typename HandlerT, typename EcT, typename BytesT>
void async_post_with_result(
    const FallbackExecutor& fallback, HandlerT&& h, EcT ec, BytesT bytes)
{
    post_completion_on(fallback,
        std::forward<HandlerT>(h),
        std::move(ec),
        std::move(bytes));
}

template <typename FallbackExecutor, typename HandlerT>
void async_post_with_error(
    const FallbackExecutor& fallback, HandlerT&& h, boost::system::error_code ec)
{
    post_completion_on(fallback, std::forward<HandlerT>(h), ec, 0);
}

// Wintun DLL 动态加载的 API 指针表。
struct wintun_api
{
    PfnWintunCreateAdapter create_adapter{nullptr};
    PfnWintunOpenAdapter open_adapter{nullptr};
    PfnWintunCloseAdapter close_adapter{nullptr};
    PfnWintunGetAdapterLUID get_adapter_luid{nullptr};

    HMODULE module{nullptr};

    ~wintun_api()
    {
        if (module)
            FreeLibrary(module);
    }

    static std::unique_ptr<wintun_api> load(DWORD& err)
    {
        auto api = std::make_unique<wintun_api>();
        api->module = LoadLibraryW(L"wintun.dll");
        if (!api->module)
        {
            err = GetLastError();
            return nullptr;
        }

#define BIND_WINTUN_FN(symbol, member)                                                    \
    do                                                                                    \
    {                                                                                     \
        FARPROC _fp = GetProcAddress(api->module, symbol);                                \
        if (_fp)                                                                          \
        {                                                                                 \
            std::memcpy(&api->member, &_fp, sizeof(_fp));                                 \
        }                                                                                 \
        else                                                                              \
        {                                                                                 \
            err = GetLastError();                                                         \
            api->member = nullptr;                                                        \
            return nullptr;                                                               \
        }                                                                                 \
    } while (0)

        BIND_WINTUN_FN("WintunCreateAdapter", create_adapter);
        BIND_WINTUN_FN("WintunOpenAdapter", open_adapter);
        BIND_WINTUN_FN("WintunCloseAdapter", close_adapter);
        BIND_WINTUN_FN("WintunGetAdapterLUID", get_adapter_luid);

#undef BIND_WINTUN_FN

        err = ERROR_SUCCESS;
        return api;
    }

    static wintun_api* instance(boost::system::error_code& ec)
    {
        static std::once_flag once;
        static std::unique_ptr<wintun_api> api;
        static DWORD load_error = ERROR_SUCCESS;
        std::call_once(once,
            []
            {
                api = load(load_error);
            });
        ec.clear();
        if (!api)
            ec.assign(static_cast<int>(load_error),
                boost::system::system_category());
        return api.get();
    }
};

class wintun_tun_device_impl
{
public:
    explicit wintun_tun_device_impl(net::io_context& ctx)
        : strand_(ctx.get_executor())
        , recv_timer_(strand_)
        , send_timer_(strand_)
    {
    }
    ~wintun_tun_device_impl()
    {
        close();
    }

    bool open(const device_config& cfg, boost::system::error_code& ec);
    // 失败时不关闭注入句柄（所有权始终归调用方）.
    bool assign(native_handle_type handle,
        size_t mtu,
        bool,
        boost::system::error_code& ec);
    // Wintun 无多队列概念：注入多个句柄不支持。
    bool assign_queues(const std::vector<native_handle_type>&,
        size_t,
        bool,
        boost::system::error_code& ec);
    void close();

    size_t mtu() const noexcept
    {
        return mtu_;
    }
    // 单次读取可能需要的最大字节数
    size_t read_size_hint() const
    {
        return mtu_;
    }
    size_t queue_count() const noexcept
    {
        return 1;
    }
    bool is_open() const noexcept
    {
        return opened_;
    }

    // 设备由 tun_device 按值持有；与 Boost.Asio 异步操作契约一致，调用方
    // 须保证设备、缓冲区和完成处理器在操作完成前有效。
    template <typename Handler>
    void async_read(packet_buffer& buf, Handler&& handler)
    {
        net::post(strand_,
            [this, &buf, h = std::forward<Handler>(handler)]() mutable
            {
                recv_poll_loop(buf, std::move(h));
            });
    }

    template <typename Handler>
    void async_read(packet_buffer& buf, size_t, Handler&& handler)
    {
        async_read(buf, std::forward<Handler>(handler));
    }

    template <typename Handler>
    void async_write(packet_buffer& buf, Handler&& handler)
    {
        net::post(strand_,
            [this, &buf, h = std::forward<Handler>(handler)]() mutable
            {
                send_try_and_maybe_retry(buf, std::move(h));
            });
    }

    template <typename Handler>
    void async_write(packet_buffer& buf, size_t, Handler&& handler)
    {
        async_write(buf, std::forward<Handler>(handler));
    }

private:
    int recv_one(std::string_view buf, boost::system::error_code& ec);
    int send_try(std::string_view buf, boost::system::error_code& ec);
    void cleanup_rings();
    void setup_mtu(int mtu);

    template <typename Handler>
    void recv_poll_loop(packet_buffer& buf, Handler&& handler)
    {
        boost::system::error_code ec;
        int n = recv_one(
            {reinterpret_cast<const char*>(buf.writable_data()),
                buf.writable_size()},
            ec);

        if (n > 0)
        {
            // 与 posix 实现保持一致：不在此 commit，由调用方按
            // packet_buffer 契约在读取成功后 commit（引擎 on_read 与
            // tun_device::async_read_ip 均会 commit），避免同一公共 API
            // 的平台间行为漂移（wintun 内部 commit + 调用方再 commit
            // 会二次推进 data_size_）.
            async_post_with_result(strand_,
                std::forward<Handler>(handler),
                boost::system::error_code{},
                static_cast<size_t>(n));
            return;
        }

        if (n < 0)
        {
            async_post_with_error(strand_, std::forward<Handler>(handler), ec);
            return;
        }

        // 无数据，定时重试。
        recv_timer_.expires_after(std::chrono::milliseconds(1));
        recv_timer_.async_wait(
            [this, &buf, h = std::forward<Handler>(handler)](
                boost::system::error_code timer_ec) mutable
            {
                if (timer_ec)
                {
                    async_post_with_error(
                        strand_, std::move(h), timer_ec);
                    return;
                }
                if (!is_open())
                {
                    async_post_with_error(
                        strand_, std::move(h), net::error::operation_aborted);
                    return;
                }
                recv_poll_loop(buf, std::move(h));
            });
    }

    template <typename Handler>
    void send_try_and_maybe_retry(packet_buffer& buf, Handler&& handler)
    {
        boost::system::error_code ec;
        int n = send_try(
            {reinterpret_cast<const char*>(buf.data()), buf.size()}, ec);

        if (n < 0)
        {
            async_post_with_error(strand_, std::forward<Handler>(handler), ec);
            return;
        }

        if (n > 0)
        {
            async_post_with_result(strand_,
                std::forward<Handler>(handler),
                boost::system::error_code{},
                static_cast<size_t>(n));
            return;
        }

        send_timer_.expires_after(std::chrono::milliseconds(1));
        send_timer_.async_wait(
            [this, &buf, h = std::move(handler)](
                boost::system::error_code timer_ec) mutable
            {
                if (timer_ec)
                {
                    async_post_with_error(
                        strand_, std::move(h), timer_ec);
                    return;
                }
                boost::system::error_code ec;
                int n2 = send_try(
                    {reinterpret_cast<const char*>(buf.data()), buf.size()},
                    ec);
                if (n2 < 0)
                {
                    async_post_with_error(strand_, std::move(h), ec);
                    return;
                }
                if (n2 == 0)
                {
                    send_try_and_maybe_retry(buf, std::move(h));
                    return;
                }
                async_post_with_result(strand_,
                    std::move(h),
                    boost::system::error_code{},
                    static_cast<size_t>(n2));
            });
    }

    net::strand<net::io_context::executor_type> strand_;

    wintun_api* api_{nullptr};
    WINTUN_ADAPTER_HANDLE wintun_handle_{nullptr};
    HANDLE dev_handle_{INVALID_HANDLE_VALUE};

    HANDLE send_ring_fh_{nullptr};
    HANDLE recv_ring_fh_{nullptr};
    HANDLE send_evt_{nullptr};
    HANDLE recv_evt_{nullptr};
    struct tun_ring* send_ring_{nullptr};
    struct tun_ring* recv_ring_{nullptr};

    size_t mtu_ = 1500;
    bool opened_ = false;

    net::steady_timer recv_timer_;
    net::steady_timer send_timer_;
};

} // namespace detail
} // namespace tunio
