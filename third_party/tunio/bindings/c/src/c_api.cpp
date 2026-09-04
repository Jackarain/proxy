//
// c_api.cpp
// ~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// tunio 稳定 C API 的实现（见 bindings/c/include/tunio/c_api.h）。
//
// 线程模型：每个引擎句柄内部持有一个 io_context 与若干 io 线程，协议状态
// 机按 tunio 约定串行运行；accept/recv/send 等阻塞调用从任意线程发起异步
// 操作后等待完成槽。引擎 close()/free() 先置关闭标志并等待在途操作完成
// 注册，再在串行执行器上执行引擎清理，保证无操作悬挂。

#include "tunio/c_api.h"

#include "tunio/tun_config.hpp"
#include "tunio/tun_tcp_acceptor.hpp"
#include "tunio/tun_udp_acceptor.hpp"
#include "tunio/tunio.hpp"

#include <boost/asio.hpp>

#ifndef _WIN32
#include <arpa/inet.h>
#endif

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
namespace net = boost::asio;

// ---- 线程局部错误信息 ----
thread_local int g_last_error = TUNIO_OK;
thread_local std::string g_last_error_message;

void set_error(int code, const std::string& message)
{
    g_last_error = code;
    g_last_error_message = message;
}

void clear_error()
{
    g_last_error = TUNIO_OK;
    g_last_error_message.clear();
}

// ---- 一次阻塞操作的等待槽 ----
struct wait_slot
{
    std::mutex m;
    std::condition_variable cv;
    bool done = false;
    boost::system::error_code ec;
    size_t n = 0;
    net::ip::udp::endpoint sender;
};

using wait_slot_ptr = std::shared_ptr<wait_slot>;

void slot_finish(const wait_slot_ptr& slot,
    const boost::system::error_code& ec, size_t n = 0)
{
    std::lock_guard<std::mutex> lk(slot->m);
    slot->ec = ec;
    slot->n = n;
    slot->done = true;
    slot->cv.notify_all();
}

void wait_completion(const wait_slot_ptr& slot)
{
    std::unique_lock<std::mutex> lk(slot->m);
    slot->cv.wait(lk, [&slot] { return slot->done; });
}

// 错误码转换：eof 视为对端关闭（recv 返回 0），其余错误设置线程局部信息。
int error_from_ec(const boost::system::error_code& ec, const char* what)
{
    if (!ec)
        return TUNIO_OK;
    if (ec == net::error::eof)
    {
        clear_error();
        return TUNIO_OK;
    }
    if (ec == net::error::operation_aborted || ec == net::error::bad_descriptor)
    {
        set_error(TUNIO_ECLOSED, std::string(what) + ": 引擎或连接已关闭");
        return TUNIO_ECLOSED;
    }
    if (ec == net::error::connection_reset)
    {
        set_error(
            TUNIO_ECONNRESET, std::string(what) + ": 连接被对端重置");
        return TUNIO_ECONNRESET;
    }
    set_error(TUNIO_EFAIL, std::string(what) + ": " + ec.message());
    return TUNIO_EFAIL;
}

// 把句柄整型转换为平台原生句柄（与 native_handle_from_int 语义一致，
// 但以 64 位承载，Windows 下句柄可完整表达）。
tunio::native_handle_type handle_from_int64(int64_t value)
{
#ifdef _WIN32
    return reinterpret_cast<tunio::native_handle_type>(
        static_cast<intptr_t>(value));
#else
    return static_cast<tunio::native_handle_type>(value);
#endif
}

// ---- 引擎核心：io 线程 + 阻塞操作协调 ----
struct engine_core
{
    net::io_context io;
    tunio::tunio engine;
    std::unique_ptr<tunio::tun_tcp_acceptor> tcp_acceptor_;
    std::unique_ptr<tunio::tun_udp_acceptor> udp_acceptor_;
    net::executor_work_guard<net::io_context::executor_type> guard;
    std::atomic<bool> closed{true}; // true = 未打开或已关闭

    std::mutex op_mu;               // 保护 closed 检查与在途操作计数
    std::condition_variable op_cv;
    size_t active = 0;              // 正在注册（尚未发起完成）的操作数
    size_t io_threads = 1;
    std::vector<std::thread> threads;

    // 回调风格异步族状态（与阻塞族共享同一引擎句柄；未使用时保持默认值）。
    std::mutex aio_mu;               // 保护下述异步监听配置
    tunio_tcp_conn_cb tcp_cb = nullptr;
    void* tcp_user = nullptr;
    tunio_udp_session_cb udp_cb = nullptr;
    void* udp_user = nullptr;
    bool tcp_accepting = false;      // 有挂起异步 accept
    bool udp_accepting = false;
    tunio_engine* aio_self = nullptr; // 回调引擎参数（句柄，engine_free 后无效）
    std::atomic<bool> aio_gone{false}; // engine_free 后不再派发任何回调

    engine_core(size_t n)
        : io(static_cast<int>(n))
        , engine(io, n == 1)
        , guard(net::make_work_guard(io))
        , io_threads(n)
    {
    }

    ~engine_core()
    {
        // 防御：若仍持有 io 线程（正常流程由 shutdown 回收），在此兜底。
        if (!threads.empty())
            shutdown();
    }

    // 在引擎串行执行器上执行 fn 并等待其完成；fn 返回非 void 时回传结果。
    template <typename Fn>
    auto on_io(Fn&& fn) -> decltype(fn())
    {
        using result_type = decltype(fn());
        auto done = std::make_shared<std::promise<result_type>>();
        net::dispatch(engine.get_executor(),
            [done, fn = std::forward<Fn>(fn)]() mutable
            {
                try
                {
                    if constexpr (std::is_void_v<result_type>)
                    {
                        fn();
                        done->set_value();
                    }
                    else
                    {
                        done->set_value(fn());
                    }
                }
                catch (...)
                {
                    // 引擎清理接口不抛异常；此处兜底避免 promise 悬挂。
                    if constexpr (std::is_void_v<result_type>)
                        done->set_value();
                    else
                        done->set_exception(std::current_exception());
                }
            });
        return done->get_future().get();
    }

    // 注册一个在途阻塞操作；引擎已关闭时返回 TUNIO_ECLOSED。
    template <typename Fn>
    int begin_op(Fn&& initiate, const char* what)
    {
        {
            std::unique_lock<std::mutex> lk(op_mu);
            if (closed.load(std::memory_order_acquire))
            {
                set_error(TUNIO_ECLOSED, std::string(what) + ": 引擎已关闭");
                return TUNIO_ECLOSED;
            }
            ++active;
        }
        try
        {
            initiate();
        }
        catch (...)
        {
            std::lock_guard<std::mutex> lk(op_mu);
            --active;
            op_cv.notify_all();
            set_error(TUNIO_EFAIL, std::string(what) + ": 发起异步操作失败");
            return TUNIO_EFAIL;
        }
        {
            std::lock_guard<std::mutex> lk(op_mu);
            --active;
            op_cv.notify_all();
        }
        return TUNIO_OK;
    }

    bool open(const tunio::tun_config& cfg, std::string& err)
    {
        if (engine.is_open())
        {
            err = "引擎已打开，请先 close()";
            return false;
        }
        boost::system::error_code ec;
        if (!engine.open(cfg, ec))
        {
            err = ec.message();
            return false;
        }
        tcp_acceptor_ = std::make_unique<tunio::tun_tcp_acceptor>(engine);
        udp_acceptor_ = std::make_unique<tunio::tun_udp_acceptor>(engine);
        if (threads.empty())
        {
            io.restart();
            for (size_t i = 0; i < io_threads; ++i)
                threads.emplace_back([this] { io.run(); });
        }
        closed.store(false, std::memory_order_release);
        return true;
    }

    // 关闭数据通路并唤醒全部阻塞调用；保留 io 线程以便再次 open()。
    void close_data()
    {
        if (closed.exchange(true, std::memory_order_acq_rel))
            return;
        {
            std::unique_lock<std::mutex> lk(op_mu);
            op_cv.wait(lk, [this] { return active == 0; });
        }
        if (engine.is_open())
            on_io([this] { engine.close(); });
    }

    // 彻底停止：关闭数据通路并回收 io 线程（引擎句柄释放时调用）。
    void shutdown()
    {
        if (!closed.exchange(true, std::memory_order_acq_rel))
        {
            std::unique_lock<std::mutex> lk(op_mu);
            op_cv.wait(lk, [this] { return active == 0; });
            if (engine.is_open())
                on_io([this] { engine.close(); });
        }
        guard.reset();
        for (auto& t : threads)
        {
            if (t.joinable())
                t.join();
        }
        threads.clear();
    }
};

// 回调内释放引擎（tunio_engine_free）时判定当前线程是否属于引擎 io 线程，
// 避免 join 自身；定义见文件尾部异步族实现。
bool is_engine_io_thread(const engine_core& core);

// ---- 配置转换 ----
std::string config_string(const char* buf, size_t size)
{
    const void* end = std::memchr(buf, '\0', size);
    const size_t len =
        end ? static_cast<const char*>(end) - buf : size;
    return std::string(buf, len);
}

bool config_to_tun(const tunio_config& from,
    tunio::tun_config& to, std::string& err)
{
    to.dev_name = config_string(from.dev_name, sizeof(from.dev_name));
    to.ipv4_addr = config_string(from.ipv4_addr, sizeof(from.ipv4_addr));
    to.netmask = config_string(from.netmask, sizeof(from.netmask));
    to.ipv6_addr = config_string(from.ipv6_addr, sizeof(from.ipv6_addr));
    to.ipv6_prefix_len = from.ipv6_prefix_len;
    to.mtu = from.mtu;
    to.num_queues = from.num_queues;
    to.external_mtu = from.external_mtu;
    to.utun_prefix = from.utun_prefix != 0;
    to.max_tcp_flows = from.max_tcp_flows;
    to.max_udp_flows = from.max_udp_flows;
    to.max_rx_queue_per_flow = from.max_rx_queue_per_flow;
    to.tcp_ooo_max_segments = from.tcp_ooo_max_segments;
    to.max_total_buffer = from.max_total_buffer;
    to.udp_idle_timeout = std::chrono::seconds(from.udp_idle_timeout_sec);
    to.tcp_time_wait_timeout =
        std::chrono::seconds(from.tcp_time_wait_timeout_sec);
    to.tcp_accept_timeout =
        std::chrono::seconds(from.tcp_accept_timeout_sec);
    to.tcp_syn_timeout = std::chrono::seconds(from.tcp_syn_timeout_sec);
    to.tcp_close_timeout = std::chrono::seconds(from.tcp_close_timeout_sec);
    to.tcp_persist_timeout =
        std::chrono::milliseconds(from.tcp_persist_timeout_ms);
    to.tcp_persist_max_probes = from.tcp_persist_max_probes;
    to.tcp_rto_timeout = std::chrono::milliseconds(from.tcp_rto_timeout_ms);
    to.tcp_rto_max_retransmits = from.tcp_rto_max_retransmits;

    if (from.num_external_handles > 0)
    {
        if (from.num_external_handles > tunio::max_multi_queues)
        {
            err = "external_handles 数量超过上限";
            return false;
        }
        to.external_handles.reserve(from.num_external_handles);
        for (uint32_t i = 0; i < from.num_external_handles; ++i)
        {
            to.external_handles.push_back(
                handle_from_int64(from.external_handles[i]));
        }
    }
    else if (from.external_handle >= 0)
    {
        to.external_handle = handle_from_int64(from.external_handle);
    }
    return true;
}

// 端点地址输出
bool write_endpoint(const net::ip::address& addr, uint16_t port,
    char* ip, size_t ip_size, uint16_t* port_out)
{
    if (!ip || ip_size == 0)
    {
        set_error(TUNIO_EPARAM, "地址缓冲区非法");
        return false;
    }
    const std::string text = addr.to_string();
    if (text.size() >= ip_size)
    {
        set_error(TUNIO_EPARAM, "地址缓冲区不足");
        return false;
    }
    std::memcpy(ip, text.c_str(), text.size() + 1);
    if (port_out)
        *port_out = port;
    return true;
}

} // namespace

// ---- C 句柄类型（对应 c_api.h 中前置声明的不透明类型）----
struct tunio_tcp_conn_aio;
struct tunio_udp_session_aio;

struct tunio_engine
{
    std::shared_ptr<engine_core> core;

    explicit tunio_engine(std::shared_ptr<engine_core> c)
        : core(std::move(c))
    {
    }
};

struct tunio_tcp_conn
{
    std::shared_ptr<engine_core> core;
    std::unique_ptr<tunio::tun_tcp_socket> sock;
    std::shared_ptr<tunio_tcp_conn_aio> aio; // 异步操作状态（异步 accept 下发）

    tunio_tcp_conn(std::shared_ptr<engine_core> c,
        std::unique_ptr<tunio::tun_tcp_socket> s)
        : core(std::move(c))
        , sock(std::move(s))
    {
    }
};

struct tunio_udp_session
{
    std::shared_ptr<engine_core> core;
    std::unique_ptr<tunio::tun_udp_socket> sock;
    std::shared_ptr<tunio_udp_session_aio> aio; // 异步操作状态（异步 accept 下发）

    tunio_udp_session(std::shared_ptr<engine_core> c,
        std::unique_ptr<tunio::tun_udp_socket> s)
        : core(std::move(c))
        , sock(std::move(s))
    {
    }
};

// 异步操作状态：异步 accept 下发的连接/会话携带；阻塞调用创建的同名句柄
// aio 为空，不参与异步接口。
struct tunio_tcp_conn_aio
{
    tunio_engine* engine = nullptr;  // 回调引擎参数；engine_free 前全部回调已派发
    std::atomic<bool> alive{true};   // conn free 后不再派发回调
    std::atomic<bool> read_pending{false};
    std::atomic<bool> write_pending{false};
};

struct tunio_udp_session_aio
{
    tunio_engine* engine = nullptr;
    std::atomic<bool> alive{true};
    std::atomic<bool> recv_pending{false};
};



// ---- 线程局部错误信息 ----

extern "C" int tunio_last_error_code(void)
{
    return g_last_error;
}

extern "C" const char* tunio_last_error_message(void)
{
    return g_last_error_message.c_str();
}

// ---- 配置 ----

extern "C" void tunio_config_init(tunio_config* config)
{
    if (!config)
        return;
    std::memset(config, 0, sizeof(*config));
    config->dev_name[0] = '\0';
    config->ipv4_addr[0] = '\0';
    config->netmask[0] = '\0';
    config->ipv6_addr[0] = '\0';
    config->ipv6_prefix_len = 64;
    config->mtu = 1500;
    config->num_queues = 1;
    config->external_handle = -1;
    config->external_mtu = 1500;
    config->max_tcp_flows = 65536;
    config->max_udp_flows = 65536;
    config->max_rx_queue_per_flow = 8 * 1024 * 1024;
    config->tcp_ooo_max_segments = 4096;
    config->max_total_buffer = 512ULL * 1024 * 1024;
    config->udp_idle_timeout_sec = 30;
    config->tcp_time_wait_timeout_sec = 10;
    config->tcp_accept_timeout_sec = 30;
    config->tcp_syn_timeout_sec = 30;
    config->tcp_close_timeout_sec = 30;
    config->tcp_persist_timeout_ms = 5000;
    config->tcp_persist_max_probes = 15;
    config->tcp_rto_timeout_ms = 200;
    config->tcp_rto_max_retransmits = 8;
}

// ---- 引擎 ----

extern "C" tunio_engine* tunio_engine_new(int io_threads)
{
    clear_error();
    if (io_threads < 1)
    {
        set_error(TUNIO_EPARAM, "io_threads 必须 >= 1");
        return nullptr;
    }
    try
    {
        auto core =
            std::make_shared<engine_core>(static_cast<size_t>(io_threads));
        auto* engine = new tunio_engine(std::move(core));
        engine->core->aio_self = engine;
        return engine;
    }
    catch (const std::bad_alloc&)
    {
        set_error(TUNIO_ENOMEM, "内存不足");
    }
    catch (const std::exception& e)
    {
        set_error(TUNIO_EFAIL, e.what());
    }
    catch (...)
    {
        set_error(TUNIO_EFAIL, "未知错误");
    }
    return nullptr;
}

extern "C" void tunio_engine_free(tunio_engine* engine)
{
    if (!engine)
        return;
    auto core = engine->core;
    core->aio_gone.store(true, std::memory_order_release);

    auto finalize = [core, engine]() {
        core->shutdown();
        delete engine;
    };

    if (is_engine_io_thread(*core))
    {
        // 在 io 线程（回调内）释放：不能 join 自身，交由收尾线程完成。
        std::thread(std::move(finalize)).detach();
    }
    else
    {
        finalize();
    }
}

extern "C" int tunio_engine_open(tunio_engine* engine,
    const tunio_config* config)
{
    clear_error();
    if (!engine || !config)
    {
        set_error(TUNIO_EPARAM, "engine/config 为空");
        return TUNIO_EPARAM;
    }
    tunio::tun_config cfg;
    std::string err;
    if (!config_to_tun(*config, cfg, err))
    {
        set_error(TUNIO_EPARAM, err);
        return TUNIO_EPARAM;
    }
    if (!engine->core->open(cfg, err))
    {
        set_error(TUNIO_EFAIL, "打开引擎失败: " + err);
        return TUNIO_EFAIL;
    }
    return TUNIO_OK;
}

extern "C" int tunio_engine_close(tunio_engine* engine)
{
    clear_error();
    if (!engine)
    {
        set_error(TUNIO_EPARAM, "engine 为空");
        return TUNIO_EPARAM;
    }
    engine->core->close_data();
    return TUNIO_OK;
}

extern "C" int tunio_engine_is_open(const tunio_engine* engine)
{
    if (!engine)
        return 0;
    return engine->core->engine.is_open() ? 1 : 0;
}

extern "C" uint32_t tunio_engine_mtu(const tunio_engine* engine)
{
    if (!engine)
        return 0;
    return static_cast<uint32_t>(engine->core->engine.mtu());
}

extern "C" uint32_t tunio_engine_queue_count(const tunio_engine* engine)
{
    if (!engine)
        return 0;
    return static_cast<uint32_t>(engine->core->engine.queue_count());
}

extern "C" int tunio_engine_local_address(const tunio_engine* engine,
    char* ip, size_t ip_size)
{
    clear_error();
    if (!engine)
    {
        set_error(TUNIO_EPARAM, "engine 为空");
        return TUNIO_EPARAM;
    }
    if (!ip || ip_size == 0)
    {
        set_error(TUNIO_EPARAM, "地址缓冲区非法");
        return TUNIO_EPARAM;
    }
    const net::ip::address addr = engine->core->engine.local_address();
    if (addr.is_unspecified())
    {
        ip[0] = '\0';
        return TUNIO_OK;
    }
    if (!write_endpoint(addr, 0, ip, ip_size, nullptr))
        return g_last_error;
    return TUNIO_OK;
}

extern "C" int tunio_engine_stats(const tunio_engine* engine,
    tunio_stats* stats)
{
    clear_error();
    if (!engine || !stats)
    {
        set_error(TUNIO_EPARAM, "engine/stats 为空");
        return TUNIO_EPARAM;
    }
    const tunio::engine_stats& s = engine->core->engine.stats();
    stats->rx_packets = s.rx_packets.load(std::memory_order_relaxed);
    stats->tx_packets = s.tx_packets.load(std::memory_order_relaxed);
    stats->rx_dropped = s.rx_dropped.load(std::memory_order_relaxed);
    stats->rx_ooo = s.rx_ooo.load(std::memory_order_relaxed);
    stats->tcp_connections = s.tcp_connections.load(std::memory_order_relaxed);
    stats->udp_sessions = s.udp_sessions.load(std::memory_order_relaxed);
    stats->icmp_replies = s.icmp_replies.load(std::memory_order_relaxed);
    return TUNIO_OK;
}

// ---- TCP ----

extern "C" tunio_tcp_conn* tunio_tcp_accept(tunio_engine* engine)
{
    clear_error();
    if (!engine)
    {
        set_error(TUNIO_EPARAM, "engine 为空");
        return nullptr;
    }
    auto core = engine->core;
    if (core->closed.load(std::memory_order_acquire) || !core->tcp_acceptor_)
    {
        set_error(TUNIO_ECLOSED, "引擎未打开或已关闭");
        return nullptr;
    }
    auto slot = std::make_shared<wait_slot>();
    auto peer =
        std::make_unique<tunio::tun_tcp_socket>(core->engine.get_executor());
    auto& acceptor = *core->tcp_acceptor_;
    const int rc = core->begin_op(
        [&acceptor, &peer, slot]()
        {
            acceptor.async_accept(*peer,
                [slot](const boost::system::error_code& ec)
                {
                    slot_finish(slot, ec);
                });
        },
        "accept");
    if (rc != TUNIO_OK)
        return nullptr;
    wait_completion(slot);
    if (slot->ec)
    {
        error_from_ec(slot->ec, "accept");
        return nullptr;
    }
    tunio_tcp_conn* conn = nullptr;
    try
    {
        conn = new tunio_tcp_conn(std::move(core), std::move(peer));
    }
    catch (const std::bad_alloc&)
    {
        set_error(TUNIO_ENOMEM, "内存不足");
        return nullptr;
    }
    return conn;
}

extern "C" int tunio_tcp_recv(tunio_tcp_conn* conn, void* buf, size_t len)
{
    clear_error();
    if (!conn || !buf || len == 0)
    {
        set_error(TUNIO_EPARAM, "conn/buf 非法或 len 为 0");
        return TUNIO_EPARAM;
    }
    if (conn->core->closed.load(std::memory_order_acquire))
    {
        set_error(TUNIO_ECLOSED, "recv: 引擎已关闭");
        return TUNIO_ECLOSED;
    }
    auto slot = std::make_shared<wait_slot>();
    auto* sock = conn->sock.get();
    const int rc = conn->core->begin_op(
        [slot, sock, buf, len]()
        {
            sock->async_read_some(net::buffer(buf, len),
                [slot](const boost::system::error_code& ec, size_t n)
                {
                    slot_finish(slot, ec, n);
                });
        },
        "recv");
    if (rc != TUNIO_OK)
        return rc;
    wait_completion(slot);
    if (!slot->ec)
        return static_cast<int>(slot->n); // n 为 0 表示对端关闭（EOF）
    if (slot->ec == net::error::eof)
    {
        clear_error();
        return slot->n == 0 ? 0 : static_cast<int>(slot->n);
    }
    return error_from_ec(slot->ec, "recv");
}

extern "C" int tunio_tcp_send(tunio_tcp_conn* conn,
    const void* data, size_t len)
{
    clear_error();
    if (!conn || !data || len == 0)
    {
        set_error(TUNIO_EPARAM, "conn/data 非法或 len 为 0");
        return TUNIO_EPARAM;
    }
    if (conn->core->closed.load(std::memory_order_acquire))
    {
        set_error(TUNIO_ECLOSED, "send: 引擎已关闭");
        return TUNIO_ECLOSED;
    }
    auto slot = std::make_shared<wait_slot>();
    auto* sock = conn->sock.get();
    const int rc = conn->core->begin_op(
        [slot, sock, data, len]()
        {
            sock->async_write_some(net::buffer(data, len),
                [slot](const boost::system::error_code& ec, size_t n)
                {
                    slot_finish(slot, ec, n);
                });
        },
        "send");
    if (rc != TUNIO_OK)
        return rc;
    wait_completion(slot);
    if (slot->ec)
        return error_from_ec(slot->ec, "send");
    return static_cast<int>(slot->n);
}

extern "C" int tunio_tcp_close(tunio_tcp_conn* conn)
{
    clear_error();
    if (!conn)
    {
        set_error(TUNIO_EPARAM, "conn 为空");
        return TUNIO_EPARAM;
    }
    if (conn->core->closed.load(std::memory_order_acquire))
        return TUNIO_OK;
    auto* sock = conn->sock.get();
    conn->core->on_io([sock] { sock->close(); });
    return TUNIO_OK;
}

extern "C" int tunio_tcp_reset(tunio_tcp_conn* conn)
{
    clear_error();
    if (!conn)
    {
        set_error(TUNIO_EPARAM, "conn 为空");
        return TUNIO_EPARAM;
    }
    if (conn->core->closed.load(std::memory_order_acquire))
        return TUNIO_OK;
    auto* sock = conn->sock.get();
    conn->core->on_io([sock] { sock->reset(); });
    return TUNIO_OK;
}

extern "C" int tunio_tcp_is_open(const tunio_tcp_conn* conn)
{
    if (!conn)
        return 0;
    if (conn->core->closed.load(std::memory_order_acquire))
        return 0;
    return conn->core->on_io(
        [&conn]() { return conn->sock->is_open() ? 1 : 0; });
}

extern "C" int tunio_tcp_original_destination(const tunio_tcp_conn* conn,
    char* ip, size_t ip_size, uint16_t* port)
{
    clear_error();
    if (!conn)
    {
        set_error(TUNIO_EPARAM, "conn 为空");
        return TUNIO_EPARAM;
    }
    const auto ep = conn->sock->original_destination();
    if (!write_endpoint(ep.address(), ep.port(), ip, ip_size, port))
        return g_last_error;
    return TUNIO_OK;
}

extern "C" int tunio_tcp_remote_endpoint(const tunio_tcp_conn* conn,
    char* ip, size_t ip_size, uint16_t* port)
{
    clear_error();
    if (!conn)
    {
        set_error(TUNIO_EPARAM, "conn 为空");
        return TUNIO_EPARAM;
    }
    const auto ep = conn->sock->remote_endpoint();
    if (!write_endpoint(ep.address(), ep.port(), ip, ip_size, port))
        return g_last_error;
    return TUNIO_OK;
}

extern "C" void tunio_tcp_free(tunio_tcp_conn* conn)
{
    if (!conn)
        return;
    if (conn->aio)
        conn->aio->alive.store(false, std::memory_order_release);
    delete conn;
}

// ---- UDP ----

extern "C" tunio_udp_session* tunio_udp_accept(tunio_engine* engine)
{
    clear_error();
    if (!engine)
    {
        set_error(TUNIO_EPARAM, "engine 为空");
        return nullptr;
    }
    auto core = engine->core;
    if (core->closed.load(std::memory_order_acquire) || !core->udp_acceptor_)
    {
        set_error(TUNIO_ECLOSED, "引擎未打开或已关闭");
        return nullptr;
    }
    auto slot = std::make_shared<wait_slot>();
    auto peer =
        std::make_unique<tunio::tun_udp_socket>(core->engine.get_executor());
    auto& acceptor = *core->udp_acceptor_;
    const int rc = core->begin_op(
        [&acceptor, &peer, slot]()
        {
            acceptor.async_accept(*peer,
                [slot](const boost::system::error_code& ec)
                {
                    slot_finish(slot, ec);
                });
        },
        "accept");
    if (rc != TUNIO_OK)
        return nullptr;
    wait_completion(slot);
    if (slot->ec)
    {
        error_from_ec(slot->ec, "accept");
        return nullptr;
    }
    tunio_udp_session* session = nullptr;
    try
    {
        session = new tunio_udp_session(std::move(core), std::move(peer));
    }
    catch (const std::bad_alloc&)
    {
        set_error(TUNIO_ENOMEM, "内存不足");
        return nullptr;
    }
    return session;
}

extern "C" int tunio_udp_recvfrom(tunio_udp_session* session,
    void* buf, size_t len,
    char* remote_ip, size_t ip_size, uint16_t* remote_port)
{
    clear_error();
    if (!session || !buf || len == 0)
    {
        set_error(TUNIO_EPARAM, "session/buf 非法或 len 为 0");
        return TUNIO_EPARAM;
    }
    if (session->core->closed.load(std::memory_order_acquire))
    {
        set_error(TUNIO_ECLOSED, "recvfrom: 引擎已关闭");
        return TUNIO_ECLOSED;
    }
    auto slot = std::make_shared<wait_slot>();
    auto* sock = session->sock.get();
    const int rc = session->core->begin_op(
        [slot, sock, buf, len]()
        {
            sock->async_receive_from(net::buffer(buf, len), slot->sender,
                [slot](const boost::system::error_code& ec, size_t n)
                {
                    slot_finish(slot, ec, n);
                });
        },
        "recvfrom");
    if (rc != TUNIO_OK)
        return rc;
    wait_completion(slot);
    if (slot->ec)
        return error_from_ec(slot->ec, "recvfrom");
    if (remote_ip && ip_size > 0)
    {
        if (!write_endpoint(slot->sender.address(), slot->sender.port(),
                remote_ip, ip_size, remote_port))
        {
            return g_last_error;
        }
    }
    else if (remote_port)
    {
        *remote_port = slot->sender.port();
    }
    return static_cast<int>(slot->n);
}

extern "C" int tunio_udp_sendto(tunio_udp_session* session,
    const char* remote_ip, uint16_t remote_port,
    const void* data, size_t len)
{
    clear_error();
    if (!session || !remote_ip || !data || len == 0)
    {
        set_error(TUNIO_EPARAM, "session/remote_ip/data 非法或 len 为 0");
        return TUNIO_EPARAM;
    }
    if (session->core->closed.load(std::memory_order_acquire))
    {
        set_error(TUNIO_ECLOSED, "sendto: 引擎已关闭");
        return TUNIO_ECLOSED;
    }
    net::ip::address addr;
    {
        boost::system::error_code parse_ec;
        addr = net::ip::make_address(remote_ip, parse_ec);
        if (parse_ec)
        {
            set_error(TUNIO_EPARAM, "非法远端地址: " +
                std::string(remote_ip));
            return TUNIO_EPARAM;
        }
    }
    const net::ip::udp::endpoint remote(addr, remote_port);
    auto slot = std::make_shared<wait_slot>();
    auto* sock = session->sock.get();
    const int rc = session->core->begin_op(
        [slot, sock, remote, data, len]()
        {
            sock->async_send_to(remote, net::buffer(data, len),
                [slot](const boost::system::error_code& ec, size_t n)
                {
                    slot_finish(slot, ec, n);
                });
        },
        "sendto");
    if (rc != TUNIO_OK)
        return rc;
    wait_completion(slot);
    if (slot->ec)
        return error_from_ec(slot->ec, "sendto");
    return static_cast<int>(slot->n);
}

extern "C" int tunio_udp_close(tunio_udp_session* session)
{
    clear_error();
    if (!session)
    {
        set_error(TUNIO_EPARAM, "session 为空");
        return TUNIO_EPARAM;
    }
    if (session->core->closed.load(std::memory_order_acquire))
        return TUNIO_OK;
    auto* sock = session->sock.get();
    session->core->on_io([sock] { sock->close(); });
    return TUNIO_OK;
}

extern "C" int tunio_udp_is_open(const tunio_udp_session* session)
{
    if (!session)
        return 0;
    if (session->core->closed.load(std::memory_order_acquire))
        return 0;
    return session->core->on_io(
        [&session]() { return session->sock->is_open() ? 1 : 0; });
}

extern "C" int tunio_udp_client_endpoint(const tunio_udp_session* session,
    char* ip, size_t ip_size, uint16_t* port)
{
    clear_error();
    if (!session)
    {
        set_error(TUNIO_EPARAM, "session 为空");
        return TUNIO_EPARAM;
    }
    const auto ep = session->sock->client_endpoint();
    if (!write_endpoint(ep.address(), ep.port(), ip, ip_size, port))
        return g_last_error;
    return TUNIO_OK;
}

extern "C" int tunio_udp_set_timeout(tunio_udp_session* session,
    int64_t seconds)
{
    clear_error();
    if (!session)
    {
        set_error(TUNIO_EPARAM, "session 为空");
        return TUNIO_EPARAM;
    }
    if (seconds < 0)
    {
        set_error(TUNIO_EPARAM, "seconds 必须 >= 0");
        return TUNIO_EPARAM;
    }
    if (session->core->closed.load(std::memory_order_acquire))
        return TUNIO_OK;
    const int64_t timeout =
        seconds == 0 ? 30 : seconds; // 0 恢复默认 30s
    auto* sock = session->sock.get();
    session->core->on_io(
        [sock, timeout]() { sock->set_timeout(std::chrono::seconds(timeout)); });
    return TUNIO_OK;
}

extern "C" void tunio_udp_free(tunio_udp_session* session)
{
    if (!session)
        return;
    if (session->aio)
        session->aio->alive.store(false, std::memory_order_release);
    delete session;
}

// =====================================================================
// 回调风格异步族：与阻塞族共享同一批句柄（tunio_engine / tunio_tcp_conn /
// tunio_udp_session）与生命周期。异步 accept 注册在引擎上并自动续接；
// 异步 accept 下发的连接/会话携带 aio 状态，支持完成回调风格的读写。
// 关闭/reset/free/端点查询等接口与阻塞族共用。
// =====================================================================

namespace {

bool is_engine_io_thread(const engine_core& core)
{
    const std::thread::id self = std::this_thread::get_id();
    for (const auto& t : core.threads)
    {
        if (t.get_id() == self)
            return true;
    }
    return false;
}

// 回调错误码映射：EOF 以 (0, 0) 表示，与阻塞 recv 返回 0 表示对端关闭的
// 约定一致。
int async_err_code(const boost::system::error_code& ec) noexcept
{
    if (!ec)
        return TUNIO_OK;
    if (ec == net::error::eof)
        return TUNIO_OK;
    if (ec == net::error::operation_aborted ||
        ec == net::error::bad_descriptor)
        return TUNIO_ECLOSED;
    if (ec == net::error::connection_reset)
        return TUNIO_ECONNRESET;
    return TUNIO_EFAIL;
}

bool endpoint_to_sockaddr(const net::ip::address& addr, uint16_t port,
    sockaddr_storage& ss) noexcept
{
    std::memset(&ss, 0, sizeof(ss));
    if (addr.is_v4())
    {
        const auto bytes = addr.to_v4().to_bytes();
        auto* sin = reinterpret_cast<sockaddr_in*>(&ss);
        sin->sin_family = AF_INET;
        sin->sin_port = htons(port);
        std::memcpy(&sin->sin_addr, bytes.data(), bytes.size());
        return true;
    }
    if (addr.is_v6())
    {
        const auto bytes = addr.to_v6().to_bytes();
        auto* sin6 = reinterpret_cast<sockaddr_in6*>(&ss);
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = htons(port);
        std::memcpy(&sin6->sin6_addr, bytes.data(), bytes.size());
        return true;
    }
    return false;
}

bool sockaddr_to_address(const sockaddr_storage& ss,
    net::ip::address& addr, uint16_t& port) noexcept
{
    if (ss.ss_family == AF_INET)
    {
        const auto* sin = reinterpret_cast<const sockaddr_in*>(&ss);
        net::ip::address_v4::bytes_type b{};
        std::memcpy(b.data(), &sin->sin_addr, b.size());
        addr = net::ip::make_address_v4(b);
        port = ntohs(sin->sin_port);
        return true;
    }
    if (ss.ss_family == AF_INET6)
    {
        const auto* sin6 = reinterpret_cast<const sockaddr_in6*>(&ss);
        net::ip::address_v6::bytes_type b{};
        std::memcpy(b.data(), &sin6->sin6_addr, b.size());
        addr = net::ip::make_address_v6(b);
        port = ntohs(sin6->sin6_port);
        return true;
    }
    return false;
}

void arm_tcp_accept(const std::shared_ptr<engine_core>& core);
void arm_udp_accept(const std::shared_ptr<engine_core>& core);

void arm_tcp_accept(const std::shared_ptr<engine_core>& core)
{
    if (core->closed.load(std::memory_order_acquire))
        return;
    const auto ex = core->engine.get_executor();
    net::post(ex, [core]() mutable {
        auto& eng = *core;
        if (eng.closed.load(std::memory_order_acquire) || !eng.tcp_acceptor_)
        {
            std::lock_guard<std::mutex> lk(eng.aio_mu);
            eng.tcp_accepting = false;
            return;
        }
        auto peer = std::make_shared<tunio::tun_tcp_socket>(
            eng.engine.get_executor());
        auto* acceptor = eng.tcp_acceptor_.get();
        std::weak_ptr<engine_core> wcore = core;
        acceptor->async_accept(*peer,
            [wcore, peer](const boost::system::error_code& ec) mutable {
                auto core = wcore.lock();
                if (!core)
                    return;
                {
                    std::lock_guard<std::mutex> lk(core->aio_mu);
                    core->tcp_accepting = false;
                }
                if (ec)
                    return;  // 引擎关闭等中止路径：不自动续接

                tunio_tcp_conn_cb fn = nullptr;
                void* user = nullptr;
                {
                    std::lock_guard<std::mutex> lk(core->aio_mu);
                    if (core->closed.load(std::memory_order_acquire) ||
                        core->aio_gone.load(std::memory_order_acquire) ||
                        !core->tcp_cb)
                    {
                        return;  // 未领取：连接随对象析构以关闭收场
                    }
                    fn = core->tcp_cb;
                    user = core->tcp_user;
                }

                auto* conn = new tunio_tcp_conn(core,
                    std::make_unique<tunio::tun_tcp_socket>(std::move(*peer)));
                conn->aio = std::make_shared<tunio_tcp_conn_aio>();
                conn->aio->engine = core->aio_self;
                fn(core->aio_self, conn, TUNIO_OK, user);

                bool rearm = false;
                {
                    std::lock_guard<std::mutex> lk(core->aio_mu);
                    if (!core->tcp_accepting &&
                        !core->aio_gone.load(std::memory_order_acquire) &&
                        !core->closed.load(std::memory_order_acquire) &&
                        core->tcp_cb)
                    {
                        core->tcp_accepting = true;
                        rearm = true;
                    }
                }
                if (rearm)
                    arm_tcp_accept(core);
            });
    });
}

void arm_udp_accept(const std::shared_ptr<engine_core>& core)
{
    if (core->closed.load(std::memory_order_acquire))
        return;
    const auto ex = core->engine.get_executor();
    net::post(ex, [core]() mutable {
        auto& eng = *core;
        if (eng.closed.load(std::memory_order_acquire) || !eng.udp_acceptor_)
        {
            std::lock_guard<std::mutex> lk(eng.aio_mu);
            eng.udp_accepting = false;
            return;
        }
        auto peer = std::make_shared<tunio::tun_udp_socket>(
            eng.engine.get_executor());
        auto* acceptor = eng.udp_acceptor_.get();
        std::weak_ptr<engine_core> wcore = core;
        acceptor->async_accept(*peer,
            [wcore, peer](const boost::system::error_code& ec) mutable {
                auto core = wcore.lock();
                if (!core)
                    return;
                {
                    std::lock_guard<std::mutex> lk(core->aio_mu);
                    core->udp_accepting = false;
                }
                if (ec)
                    return;

                tunio_udp_session_cb fn = nullptr;
                void* user = nullptr;
                {
                    std::lock_guard<std::mutex> lk(core->aio_mu);
                    if (core->closed.load(std::memory_order_acquire) ||
                        core->aio_gone.load(std::memory_order_acquire) ||
                        !core->udp_cb)
                    {
                        return;
                    }
                    fn = core->udp_cb;
                    user = core->udp_user;
                }

                auto* session = new tunio_udp_session(core,
                    std::make_unique<tunio::tun_udp_socket>(std::move(*peer)));
                session->aio = std::make_shared<tunio_udp_session_aio>();
                session->aio->engine = core->aio_self;
                fn(core->aio_self, session, TUNIO_OK, user);

                bool rearm = false;
                {
                    std::lock_guard<std::mutex> lk(core->aio_mu);
                    if (!core->udp_accepting &&
                        !core->aio_gone.load(std::memory_order_acquire) &&
                        !core->closed.load(std::memory_order_acquire) &&
                        core->udp_cb)
                    {
                        core->udp_accepting = true;
                        rearm = true;
                    }
                }
                if (rearm)
                    arm_udp_accept(core);
            });
    });
}

} // namespace

// ---- 通用 ----

extern "C" const char* tunio_strerror(int code)
{
    switch (code)
    {
    case TUNIO_OK:
        return "成功";
    case TUNIO_EFAIL:
        return "通用错误";
    case TUNIO_ECLOSED:
        return "引擎或连接已关闭，或挂起操作被取消";
    case TUNIO_ECONNRESET:
        return "TCP 连接被对端重置";
    case TUNIO_EPARAM:
        return "非法参数";
    case TUNIO_ENOMEM:
        return "内存不足";
    default:
        return "未知错误码";
    }
}

// ---- 异步 accept（注册在引擎上，自动续接）----

extern "C" int tunio_tcp_async_accept(tunio_engine* engine,
    tunio_tcp_conn_cb on_conn, void* user)
{
    clear_error();
    if (!engine)
    {
        set_error(TUNIO_EPARAM, "engine 为空");
        return TUNIO_EPARAM;
    }
    auto core = engine->core;
    if (core->closed.load(std::memory_order_acquire))
    {
        set_error(TUNIO_ECLOSED,
            "引擎未打开或已关闭（请先 tunio_engine_open）");
        return TUNIO_ECLOSED;
    }
    bool arm = false;
    bool cancel = false;
    {
        std::lock_guard<std::mutex> lk(core->aio_mu);
        core->tcp_cb = on_conn;
        core->tcp_user = user;
        if (on_conn && !core->tcp_accepting)
        {
            core->tcp_accepting = true;
            arm = true;
        }
        else if (!on_conn && core->tcp_accepting)
        {
            cancel = true;  // 暂停：取消挂起 accept，停止领取新连接
        }
    }
    if (arm)
        arm_tcp_accept(core);
    if (cancel)
    {
        const auto ex = core->engine.get_executor();
        net::post(ex, [core]() {
            auto& eng = *core;
            if (!eng.closed.load(std::memory_order_acquire) &&
                eng.tcp_acceptor_)
            {
                eng.tcp_acceptor_->cancel();
            }
        });
    }
    return TUNIO_OK;
}

extern "C" int tunio_udp_async_accept(tunio_engine* engine,
    tunio_udp_session_cb on_session, void* user)
{
    clear_error();
    if (!engine)
    {
        set_error(TUNIO_EPARAM, "engine 为空");
        return TUNIO_EPARAM;
    }
    auto core = engine->core;
    if (core->closed.load(std::memory_order_acquire))
    {
        set_error(TUNIO_ECLOSED,
            "引擎未打开或已关闭（请先 tunio_engine_open）");
        return TUNIO_ECLOSED;
    }
    bool arm = false;
    bool cancel = false;
    {
        std::lock_guard<std::mutex> lk(core->aio_mu);
        core->udp_cb = on_session;
        core->udp_user = user;
        if (on_session && !core->udp_accepting)
        {
            core->udp_accepting = true;
            arm = true;
        }
        else if (!on_session && core->udp_accepting)
        {
            cancel = true;
        }
    }
    if (arm)
        arm_udp_accept(core);
    if (cancel)
    {
        const auto ex = core->engine.get_executor();
        net::post(ex, [core]() {
            auto& eng = *core;
            if (!eng.closed.load(std::memory_order_acquire) &&
                eng.udp_acceptor_)
            {
                eng.udp_acceptor_->cancel();
            }
        });
    }
    return TUNIO_OK;
}

// ---- TCP 异步读写 ----

extern "C" int tunio_tcp_async_recv(tunio_tcp_conn* conn, void* buf,
    size_t cap, tunio_tcp_recv_cb cb, void* user)
{
    clear_error();
    if (!conn || !buf || cap == 0 || !cb)
    {
        set_error(TUNIO_EPARAM, "conn/buf/cap/cb 非法");
        return TUNIO_EPARAM;
    }
    auto aio = conn->aio;
    if (!aio)
    {
        set_error(TUNIO_EPARAM,
            "conn 非异步 accept 下发，不能发起异步读");
        return TUNIO_EPARAM;
    }
    if (!aio->alive.load(std::memory_order_acquire))
    {
        set_error(TUNIO_ECLOSED, "conn: 已释放");
        return TUNIO_ECLOSED;
    }
    auto core = conn->core;
    if (core->aio_gone.load(std::memory_order_acquire) ||
        core->closed.load(std::memory_order_acquire))
    {
        set_error(TUNIO_ECLOSED, "引擎未打开或已关闭");
        return TUNIO_ECLOSED;
    }
    bool expected = false;
    if (!aio->read_pending.compare_exchange_strong(expected, true))
    {
        set_error(TUNIO_EPARAM, "conn: 已有挂起读");
        return TUNIO_EPARAM;
    }
    auto* sock = conn->sock.get();
    try
    {
        sock->async_read_some(net::buffer(buf, cap),
            [core, aio, conn, cb, user](
                const boost::system::error_code& ec, size_t n) mutable {
                aio->read_pending.store(false, std::memory_order_release);
                if (!aio->alive.load(std::memory_order_acquire) ||
                    core->aio_gone.load(std::memory_order_acquire))
                    return;
                cb(aio->engine, conn, async_err_code(ec), n, user);
            });
    }
    catch (...)
    {
        aio->read_pending.store(false, std::memory_order_release);
        set_error(TUNIO_EFAIL, "发起异步读失败");
        return TUNIO_EFAIL;
    }
    return TUNIO_OK;
}

extern "C" int tunio_tcp_async_send(tunio_tcp_conn* conn,
    const void* data, size_t len, tunio_tcp_send_cb cb, void* user)
{
    clear_error();
    if (!conn || !data || len == 0 || !cb)
    {
        set_error(TUNIO_EPARAM, "conn/data/len/cb 非法");
        return TUNIO_EPARAM;
    }
    auto aio = conn->aio;
    if (!aio)
    {
        set_error(TUNIO_EPARAM,
            "conn 非异步 accept 下发，不能发起异步写");
        return TUNIO_EPARAM;
    }
    if (!aio->alive.load(std::memory_order_acquire))
    {
        set_error(TUNIO_ECLOSED, "conn: 已释放");
        return TUNIO_ECLOSED;
    }
    auto core = conn->core;
    if (core->aio_gone.load(std::memory_order_acquire) ||
        core->closed.load(std::memory_order_acquire))
    {
        set_error(TUNIO_ECLOSED, "引擎未打开或已关闭");
        return TUNIO_ECLOSED;
    }
    bool expected = false;
    if (!aio->write_pending.compare_exchange_strong(expected, true))
    {
        set_error(TUNIO_EPARAM, "conn: 已有挂起写");
        return TUNIO_EPARAM;
    }
    auto* sock = conn->sock.get();
    try
    {
        sock->async_write_some(net::buffer(data, len),
            [core, aio, conn, cb, user](
                const boost::system::error_code& ec, size_t n) mutable {
                aio->write_pending.store(false, std::memory_order_release);
                if (!aio->alive.load(std::memory_order_acquire) ||
                    core->aio_gone.load(std::memory_order_acquire))
                    return;
                cb(aio->engine, conn, async_err_code(ec), n, user);
            });
    }
    catch (...)
    {
        aio->write_pending.store(false, std::memory_order_release);
        set_error(TUNIO_EFAIL, "发起异步写失败");
        return TUNIO_EFAIL;
    }
    return TUNIO_OK;
}

// ---- UDP 异步收发 ----

extern "C" int tunio_udp_async_recvfrom(tunio_udp_session* session,
    void* buf, size_t cap, tunio_udp_recv_cb cb, void* user)
{
    clear_error();
    if (!session || !buf || cap == 0 || !cb)
    {
        set_error(TUNIO_EPARAM, "session/buf/cap/cb 非法");
        return TUNIO_EPARAM;
    }
    auto aio = session->aio;
    if (!aio)
    {
        set_error(TUNIO_EPARAM,
            "session 非异步 accept 下发，不能发起异步 recvfrom");
        return TUNIO_EPARAM;
    }
    if (!aio->alive.load(std::memory_order_acquire))
    {
        set_error(TUNIO_ECLOSED, "session: 已释放");
        return TUNIO_ECLOSED;
    }
    auto core = session->core;
    if (core->aio_gone.load(std::memory_order_acquire) ||
        core->closed.load(std::memory_order_acquire))
    {
        set_error(TUNIO_ECLOSED, "引擎未打开或已关闭");
        return TUNIO_ECLOSED;
    }
    bool expected = false;
    if (!aio->recv_pending.compare_exchange_strong(expected, true))
    {
        set_error(TUNIO_EPARAM, "session: 已有挂起 recvfrom");
        return TUNIO_EPARAM;
    }
    auto* sock = session->sock.get();
    auto sender = std::make_shared<net::ip::udp::endpoint>();
    try
    {
        sock->async_receive_from(net::buffer(buf, cap), *sender,
            [core, aio, session, sender, cb, user](
                const boost::system::error_code& ec, size_t n) mutable {
                aio->recv_pending.store(false, std::memory_order_release);
                if (!aio->alive.load(std::memory_order_acquire) ||
                    core->aio_gone.load(std::memory_order_acquire))
                    return;
                sockaddr_storage ss{};
                if (ec || !endpoint_to_sockaddr(
                              sender->address(), sender->port(), ss))
                {
                    std::memset(&ss, 0, sizeof(ss));
                }
                cb(aio->engine, session, async_err_code(ec), n, &ss, user);
            });
    }
    catch (...)
    {
        aio->recv_pending.store(false, std::memory_order_release);
        set_error(TUNIO_EFAIL, "发起异步 recvfrom 失败");
        return TUNIO_EFAIL;
    }
    return TUNIO_OK;
}

extern "C" int tunio_udp_async_sendto(tunio_udp_session* session,
    const struct sockaddr_storage* remote,
    const void* data, size_t len,
    tunio_udp_send_cb cb, void* user)
{
    clear_error();
    if (!session || !remote || !data || len == 0 || !cb)
    {
        set_error(TUNIO_EPARAM, "session/remote/data/len/cb 非法");
        return TUNIO_EPARAM;
    }
    auto aio = session->aio;
    if (!aio)
    {
        set_error(TUNIO_EPARAM,
            "session 非异步 accept 下发，不能发起异步 sendto");
        return TUNIO_EPARAM;
    }
    if (!aio->alive.load(std::memory_order_acquire))
    {
        set_error(TUNIO_ECLOSED, "session: 已释放");
        return TUNIO_ECLOSED;
    }
    auto core = session->core;
    if (core->aio_gone.load(std::memory_order_acquire) ||
        core->closed.load(std::memory_order_acquire))
    {
        set_error(TUNIO_ECLOSED, "引擎未打开或已关闭");
        return TUNIO_ECLOSED;
    }
    net::ip::address addr;
    uint16_t port = 0;
    if (!sockaddr_to_address(*remote, addr, port))
    {
        set_error(TUNIO_EPARAM, "非法远端端点");
        return TUNIO_EPARAM;
    }
    const net::ip::udp::endpoint ep(addr, port);
    auto* sock = session->sock.get();
    try
    {
        sock->async_send_to(ep, net::buffer(data, len),
            [core, aio, session, cb, user](
                const boost::system::error_code& ec, size_t n) mutable {
                if (!aio->alive.load(std::memory_order_acquire) ||
                    core->aio_gone.load(std::memory_order_acquire))
                    return;
                cb(aio->engine, session, async_err_code(ec), n, user);
            });
    }
    catch (...)
    {
        set_error(TUNIO_EFAIL, "发起异步 sendto 失败");
        return TUNIO_EFAIL;
    }
    return TUNIO_OK;
}
