//
// c_api.h
// ~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// tunio 的稳定 C API。
//
// 设计说明：
// - 引擎内部维护 io_context 与 io 线程，open() 后数据通路在后台运行；
//   accept/recv/send 等调用为阻塞式，可在任意线程发起（同一连接句柄建议
//   单线程使用，避免重叠读写）。
// - 调用线程阻塞等待期间，引擎仍能通过 io 线程推进协议状态机；引擎
//   close()（或 free()）会以 TUNIO_ECLOSED 唤醒全部阻塞中的调用。
// - 句柄所有权：引擎/连接/会话句柄均由对应的 *_free() 释放；释放引擎时
//   必须先确保没有其他线程正在使用该引擎的接口。
// - 错误处理：除 accept 返回 NULL、free 返回 void 外，其余接口成功返回
//   TUNIO_OK（recv 返回 0 表示对端已关闭，>0 为实际读取字节数）；失败返回
//   负错误码，可用 tunio_last_error_message() 取线程局部错误描述。
//
// 本 API 只有一套句柄与一套函数族：阻塞调用与回调风格异步调用共享同一
// 引擎（tunio_engine*）、连接（tunio_tcp_conn*）与会话（tunio_udp_session*）。
// - 阻塞风格：tunio_tcp_accept/recv/send、tunio_udp_accept/recvfrom/sendto
//   等在调用线程阻塞至完成，引擎 io 线程在后台推进协议状态机。
// - 回调风格（*_async_*）：在同一引擎/连接/会话上注册连续异步 accept，
//   或发起完成回调驱动的读/写；回调在引擎串行执行器上串行派发，绝无
//   并发。异步 accept 下发的连接/会话可再使用阻塞或异步接口（同一连接
//   上建议固定一种风格）。引擎 close()/free() 会以 TUNIO_ECLOSED 完成全部
//   挂起的异步操作回调。
// - 异步读/写语义与 Boost.Asio async_read_some/async_write_some 一致：
//   同一连接同时只允许一个挂起读与一个挂起写（重叠发起返回 TUNIO_EPARAM），
//   引擎零拷贝引用调用方缓冲，缓冲须存活至回调；回调内可续发或释放句柄。
//   对端 RST/FIN 以 err 报告（FIN 为 err == 0 且 n == 0）；两次操作间隙
//   发生的关闭在下次操作时才可察觉（无主动关闭事件）。

#ifndef TUNIO_C_API_H
#define TUNIO_C_API_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 与 tunio::max_multi_queues 一致的 Linux TUN 多队列上限
#define TUNIO_MAX_MULTI_QUEUES 256

// 错误码（负值；成功为 TUNIO_OK / 非负字节数）
enum
{
    TUNIO_OK = 0,
    TUNIO_EFAIL = -1,      // 通用错误（详情见 tunio_last_error_message）
    TUNIO_ECLOSED = -2,    // 引擎/连接已关闭，或挂起操作被取消
    TUNIO_ECONNRESET = -3, // TCP 连接被对端重置
    TUNIO_EPARAM = -4,     // 非法参数
    TUNIO_ENOMEM = -5      // 内存不足
};

typedef struct tunio_engine tunio_engine;
typedef struct tunio_tcp_conn tunio_tcp_conn;
typedef struct tunio_udp_session tunio_udp_session;

// ---- 回调风格异步族回调类型（与阻塞族共享同一批句柄）----

// 新 TCP 连接到达（客户端 SYN 已到，连接处于 SYN_RCVD）；err == 0 时
// conn 非空。首次读写会隐式批准握手。回调返回后由调用方负责 tunio_tcp_free。
typedef void (*tunio_tcp_conn_cb)(
    tunio_engine* engine, tunio_tcp_conn* conn, int err, void* user);

// 异步读完成：err == 0 且 n > 0 为数据；err == 0 且 n == 0 表示对端 FIN
// （EOF）；err < 0 为错误（TUNIO_ECONNRESET 等）。
typedef void (*tunio_tcp_recv_cb)(
    tunio_engine* engine, tunio_tcp_conn* conn, int err, size_t n,
    void* user);

// 异步写完成：err == 0 表示 len 字节已被引擎接收；err < 0 为错误。
typedef void (*tunio_tcp_send_cb)(
    tunio_engine* engine, tunio_tcp_conn* conn, int err, size_t n,
    void* user);

// 新 UDP 会话建立（客户端首个数据报已到）；err == 0 时 session 非空。
typedef void (*tunio_udp_session_cb)(
    tunio_engine* engine, tunio_udp_session* session, int err, void* user);

// UDP 异步收包完成：err == 0 时 n 为数据报字节数，remote 为该数据报的
// 目标远端端点（语义与阻塞 tunio_udp_recvfrom 的输出一致）。
typedef void (*tunio_udp_recv_cb)(tunio_engine* engine,
    tunio_udp_session* session, int err, size_t n,
    const struct sockaddr_storage* remote, void* user);

// UDP 异步发包完成：err == 0 表示数据报已提交发送。
typedef void (*tunio_udp_send_cb)(tunio_engine* engine,
    tunio_udp_session* session, int err, size_t n, void* user);

// 引擎配置。字符串字段以 NUL 结尾，长度不超过 63；tunio_config_init()
// 会填充默认值。external_handle >= 0 时注入单句柄；num_external_handles
// > 0 时按数组注入多队列句柄（优先于单句柄），仅 Linux TUN 多队列有意义。
typedef struct tunio_config
{
    char dev_name[64];
    char ipv4_addr[64];
    char netmask[64];
    char ipv6_addr[64];
    uint8_t ipv6_prefix_len;
    uint8_t reserved0[3];
    uint32_t mtu;
    uint32_t num_queues;
    int64_t external_handle;  // < 0 表示不注入
    uint32_t external_mtu;
    uint8_t utun_prefix;      // 注入句柄是否为 macOS utun（读写带家族前缀）
    uint8_t reserved1[7];
    uint32_t num_external_handles;
    int64_t external_handles[TUNIO_MAX_MULTI_QUEUES];

    // 资源上限（与 tun_config 同名同义）
    uint32_t max_tcp_flows;
    uint32_t max_udp_flows;
    uint64_t max_rx_queue_per_flow;
    uint32_t tcp_ooo_max_segments;
    uint64_t max_total_buffer;

    // 超时策略（单位见字段名）
    uint32_t udp_idle_timeout_sec;
    uint32_t tcp_time_wait_timeout_sec;
    uint32_t tcp_accept_timeout_sec;
    uint32_t tcp_syn_timeout_sec;
    uint32_t tcp_close_timeout_sec;
    uint32_t tcp_persist_timeout_ms;
    int32_t tcp_persist_max_probes;
    uint32_t tcp_rto_timeout_ms;
    int32_t tcp_rto_max_retransmits;
} tunio_config;

// 引擎统计信息（对应 engine_stats）
typedef struct tunio_stats
{
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_dropped;
    uint64_t rx_ooo;
    uint64_t tcp_connections;
    uint64_t udp_sessions;
    uint64_t icmp_replies;
} tunio_stats;

// ---- 线程局部错误信息 ----
int tunio_last_error_code(void);
const char* tunio_last_error_message(void);

// ---- 配置 ----
void tunio_config_init(tunio_config* config);

// ---- 引擎生命周期 ----
// io_threads >= 1：io_context 工作线程数；为 1 时引擎运行于单线程模式。
tunio_engine* tunio_engine_new(int io_threads);
void tunio_engine_free(tunio_engine* engine);
int tunio_engine_open(tunio_engine* engine, const tunio_config* config);
// 关闭数据通路并唤醒全部阻塞调用；引擎句柄仍可再次 open()。
int tunio_engine_close(tunio_engine* engine);
int tunio_engine_is_open(const tunio_engine* engine);
uint32_t tunio_engine_mtu(const tunio_engine* engine);
uint32_t tunio_engine_queue_count(const tunio_engine* engine);
// 引擎本地虚拟 IP；未配置时输出空串并返回 TUNIO_OK。
int tunio_engine_local_address(const tunio_engine* engine,
    char* ip, size_t ip_size);
int tunio_engine_stats(const tunio_engine* engine, tunio_stats* stats);

// ---- TCP ----
// 阻塞等待一条新的虚拟连接（客户端 SYN 已到达）；引擎关闭返回 NULL。
tunio_tcp_conn* tunio_tcp_accept(tunio_engine* engine);
// 阻塞读取；成功返回读取字节数（>0），对端关闭返回 0，失败返回负错误码。
int tunio_tcp_recv(tunio_tcp_conn* conn, void* buf, size_t len);
// 阻塞发送；成功返回写入字节数（等于 len），失败返回负错误码。
int tunio_tcp_send(tunio_tcp_conn* conn, const void* data, size_t len);
int tunio_tcp_close(tunio_tcp_conn* conn); // 优雅关闭（发送 FIN）
int tunio_tcp_reset(tunio_tcp_conn* conn); // 中止（发送 RST）
int tunio_tcp_is_open(const tunio_tcp_conn* conn);
int tunio_tcp_original_destination(const tunio_tcp_conn* conn,
    char* ip, size_t ip_size, uint16_t* port);
int tunio_tcp_remote_endpoint(const tunio_tcp_conn* conn,
    char* ip, size_t ip_size, uint16_t* port);
void tunio_tcp_free(tunio_tcp_conn* conn);

// ---- UDP ----
// 阻塞等待一条新的 UDP 会话（客户端首个数据报已到达）。
tunio_udp_session* tunio_udp_accept(tunio_engine* engine);
// 阻塞接收一个完整数据报；remote_ip/remote_port 输出数据报的目标远端端点。
int tunio_udp_recvfrom(tunio_udp_session* session,
    void* buf, size_t len,
    char* remote_ip, size_t ip_size, uint16_t* remote_port);
// 发送一个完整数据报到指定远端端点（对端经引擎封装后到达客户端）。
int tunio_udp_sendto(tunio_udp_session* session,
    const char* remote_ip, uint16_t remote_port,
    const void* data, size_t len);
int tunio_udp_close(tunio_udp_session* session);
int tunio_udp_is_open(const tunio_udp_session* session);
int tunio_udp_client_endpoint(const tunio_udp_session* session,
    char* ip, size_t ip_size, uint16_t* port);
// seconds > 0 设置会话空闲超时；0 恢复默认（30s）；< 0 返回 TUNIO_EPARAM。
int tunio_udp_set_timeout(tunio_udp_session* session, int64_t seconds);
void tunio_udp_free(tunio_udp_session* session);

// ---- 回调风格异步族（同一套句柄上的异步变体）----

// 错误码 -> 静态描述串（阻塞族与异步族通用）。
const char* tunio_strerror(int code);

// 注册连续异步 TCP accept：引擎为虚拟网内所有目的地址的新连接回调 on_conn
// （透明代理语义，无按地址监听）。cb 为 NULL 时暂停派发（取消挂起 accept）。
// 引擎 close() 后需重新 open() 并再次注册。
int tunio_tcp_async_accept(
    tunio_engine* engine, tunio_tcp_conn_cb on_conn, void* user);

// 异步读（单挂起读）：err == 0 且 n == 0 表示 EOF；回调内可续发。
int tunio_tcp_async_recv(tunio_tcp_conn* conn, void* buf, size_t cap,
    tunio_tcp_recv_cb cb, void* user);

// 异步写（单挂起写）：err == 0 表示 len 字节已被引擎接收。
int tunio_tcp_async_send(tunio_tcp_conn* conn, const void* data, size_t len,
    tunio_tcp_send_cb cb, void* user);

// 注册连续异步 UDP 会话 accept：客户端对未知五元组的首个数据报到达时
// 回调 on_session（1 对 N：会话内可向任意远端收发）。cb 为 NULL 暂停。
int tunio_udp_async_accept(
    tunio_engine* engine, tunio_udp_session_cb on_session, void* user);

// 异步接收一个完整数据报（单挂起 recv）；remote 经回调参数输出。
int tunio_udp_async_recvfrom(tunio_udp_session* session,
    void* buf, size_t cap,
    tunio_udp_recv_cb cb, void* user);

// 异步发送一个完整数据报到指定远端端点（可多个重叠）。
int tunio_udp_async_sendto(tunio_udp_session* session,
    const struct sockaddr_storage* remote,
    const void* data, size_t len,
    tunio_udp_send_cb cb, void* user);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // TUNIO_C_API_H
