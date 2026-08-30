//
// packet_device_wintun.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

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
        tun_ring *ring;
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

typedef struct _WINTUN_ADAPTER *WINTUN_ADAPTER_HANDLE;

typedef WINTUN_ADAPTER_HANDLE(WINAPI *PfnWintunCreateAdapter)(
    LPCWSTR Name, LPCWSTR TunnelType, const GUID *RequestedGUID);

typedef WINTUN_ADAPTER_HANDLE(WINAPI *PfnWintunOpenAdapter)(LPCWSTR Name);

typedef VOID(WINAPI *PfnWintunCloseAdapter)(WINTUN_ADAPTER_HANDLE Adapter);

typedef VOID(WINAPI *PfnWintunGetAdapterLUID)(
    WINTUN_ADAPTER_HANDLE Adapter, NET_LUID *Luid);

namespace tunio {
namespace net = boost::asio;
namespace detail {

// Post helper that captures ec and bytes into a closure — works with any
// Boost.Asio version (post(executor, token) only).
template <typename ExecutorT, typename HandlerT, typename EcT, typename BytesT>
void async_post_with_result(const ExecutorT &ex, HandlerT &&h, EcT ec,
    BytesT bytes)
{
    net::post(ex, [h = std::forward<HandlerT>(h), ec = std::move(ec),
        bytes = std::move(bytes)]() mutable { h(ec, bytes); });
}

// Post helper that captures an error_code into a closure.
template <typename ExecutorT, typename HandlerT>
void async_post_with_error(const ExecutorT &ex, HandlerT &&h,
    boost::system::error_code ec)
{
    net::post(ex, [h = std::forward<HandlerT>(h), ec]() mutable { h(ec, 0); });
}

// Wintun DLL 动态加载的 API 指针表.
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

    static std::unique_ptr<wintun_api> load()
    {
        auto api = std::make_unique<wintun_api>();
        api->module = LoadLibraryW(L"wintun.dll");
        if (!api->module)
            return nullptr;

#define BIND_WINTUN_FN(symbol, member)                                         \
    do {                                                                       \
        FARPROC _fp = GetProcAddress(api->module, symbol);                     \
        if (_fp) {                                                             \
            std::memcpy(&api->member, &_fp, sizeof(_fp));                      \
        } else {                                                               \
            api->module = nullptr;                                             \
            api->member = nullptr;                                             \
            return nullptr;                                                    \
        }                                                                      \
    } while (0)

        BIND_WINTUN_FN("WintunCreateAdapter", create_adapter);
        BIND_WINTUN_FN("WintunOpenAdapter", open_adapter);
        BIND_WINTUN_FN("WintunCloseAdapter", close_adapter);
        BIND_WINTUN_FN("WintunGetAdapterLUID", get_adapter_luid);

#undef BIND_WINTUN_FN

        return api;
    }
};

class wintun_packet_device_impl
    : public std::enable_shared_from_this<wintun_packet_device_impl>
{
public:
    explicit wintun_packet_device_impl(net::io_context &ctx)
        : strand_(ctx.get_executor())
        , recv_timer_(ctx)
        , send_timer_(ctx)
    {
    }

    bool open(const device_config &cfg, boost::system::error_code &ec);
    bool assign(native_handle_type handle, size_t mtu, bool,
        boost::system::error_code &ec);
    void close();

    size_t mtu() const
    {
        return mtu_;
    }
    bool is_open() const
    {
        return opened_;
    }

    template <typename Handler>
    void async_read(packet_buffer &buf, Handler &&handler)
    {
        net::post(strand_, [self = shared_from_this(), &buf,
            h = std::forward<Handler>(handler)]() mutable {
            self->recv_poll_loop(buf, std::move(h));
        });
    }

    template <typename Handler>
    void async_write(packet_buffer &buf, Handler &&handler)
    {
        net::post(strand_, [self = shared_from_this(), &buf,
            h = std::forward<Handler>(handler)]() mutable {
            self->send_try_and_maybe_retry(buf, std::move(h));
        });
    }

private:
    int recv_one(std::string_view buf);
    int send_try(std::string_view buf);
    void cleanup_rings();
    void setup_mtu(int mtu);

    template <typename Handler>
    void recv_poll_loop(packet_buffer &buf, Handler &&handler)
    {
        int n = recv_one({reinterpret_cast<const char *>(buf.writable_data()),
            buf.writable_size()});

        if (n > 0) {
            buf.commit(static_cast<size_t>(n));
            async_post_with_result(strand_, std::forward<Handler>(handler),
                                   boost::system::error_code{},
                                   static_cast<size_t>(n));
            return;
        }

        if (n < 0) {
            DWORD err = GetLastError();
            async_post_with_error(
                strand_, std::forward<Handler>(handler),
                boost::system::error_code(static_cast<int>(err),
                                          boost::system::system_category()));
            return;
        }

        // 无数据，定时重试.
        recv_timer_.expires_after(std::chrono::milliseconds(1));
        recv_timer_.async_wait([self = shared_from_this(), &buf,
            h = std::forward<Handler>(handler)](
                boost::system::error_code timer_ec) mutable {
            if (!timer_ec && self->is_open())
                self->recv_poll_loop(buf, std::move(h));
            else
                h(timer_ec ? timer_ec : boost::system::error_code{}, 0);
        });
    }

    template <typename Handler>
    void send_try_and_maybe_retry(packet_buffer &buf, Handler &&handler)
    {
        int n =
            send_try({reinterpret_cast<const char *>(buf.data()), buf.size()});

        if (n < 0) {
            DWORD err = GetLastError();
            async_post_with_error(
                strand_, std::forward<Handler>(handler),
                boost::system::error_code(static_cast<int>(err),
                                          boost::system::system_category()));
            return;
        }

        if (n > 0) {
            async_post_with_result(strand_, std::forward<Handler>(handler),
                                   boost::system::error_code{},
                                   static_cast<size_t>(n));
            return;
        }

        send_timer_.expires_after(std::chrono::milliseconds(1));
        send_timer_.async_wait([self = shared_from_this(), &buf,
            h = std::move(handler)](
                boost::system::error_code timer_ec) mutable {
            if (timer_ec) {
                h(timer_ec, 0);
                return;
            }
            int n2 = self->send_try(
                {reinterpret_cast<const char *>(buf.data()), buf.size()});
            if (n2 < 0) {
                DWORD err = GetLastError();
                async_post_with_error(self->strand_, std::move(h),
                                      boost::system::error_code(
                                          static_cast<int>(err),
                                          boost::system::system_category()));
                return;
            }
            if (n2 == 0) {
                self->send_try_and_maybe_retry(buf, std::move(h));
                return;
            }
            async_post_with_result(self->strand_, std::move(h),
                                   boost::system::error_code{},
                                   static_cast<size_t>(n2));
        });
    }

    net::strand<net::io_context::executor_type> strand_;

    static inline std::once_flag init_flag_;
    static inline std::unique_ptr<wintun_api> g_api{nullptr};

    WINTUN_ADAPTER_HANDLE wintun_handle_{nullptr};
    HANDLE dev_handle_{INVALID_HANDLE_VALUE};

    HANDLE send_ring_fh_{INVALID_HANDLE_VALUE};
    HANDLE recv_ring_fh_{INVALID_HANDLE_VALUE};
    HANDLE send_evt_{INVALID_HANDLE_VALUE};
    HANDLE recv_evt_{INVALID_HANDLE_VALUE};
    struct tun_ring *send_ring_{nullptr};
    struct tun_ring *recv_ring_{nullptr};

    size_t mtu_ = 1500;
    bool opened_ = false;

    net::steady_timer recv_timer_;
    net::steady_timer send_timer_;
};

} // namespace detail
} // namespace tunio
