# tunio

基于 Boost.Asio 范式的用户态 TUN 虚拟网络引擎。引擎将 Linux TUN、macOS
utun 及 Windows Wintun 设备产生的 L3 原始 IP 包处理全面封装于内部，向上层
暴露一套完全对齐 Boost.Asio 网络编程范式的现代 C++ 异步接口，可无缝接入
C++20 协程（`co_await`），适用于 tun2socks、透明代理与轻量级 VPN 网关等
场景。

协议栈采用轻量级转发策略：接收方向维护乱序重排缓存并按序交付，动态
接收窗口（按剩余缓冲通告）避免对端超发；发送方向以最精简的 RTO 重传
（直接重读用户写缓冲，无重传队列）与零窗口持久探测保证写操作在丢包链路
上仍能完成，从而兼顾低 CPU 开销与基本可靠性。所有内部状态变更强制串行执行：
单线程模式（默认）直接运行于 `io_context` 执行器上，省去每包 Strand 派发
开销；多线程模式运行于单个 Strand 上，无锁竞争。构造时通过
`tunio(io, single_thread)` 选择。

## 特性

- **双模设备管理**：引擎自主创建并配置 TUN 设备，或接管外部应用已打开的
  平台原生句柄（文件描述符 / HANDLE），便于提权前置、多实例共享设备。
- **完全 Asio 风格异步接口**：所有公开 API 采用 `CompletionToken` 与
  `async_initiate` 实现，与 `use_awaitable`、`use_future` 及自定义
  CompletionToken 无缝协作。
- **TCP/UDP 对称抽象**：TCP 提供 `tun_tcp_socket`/`tun_tcp_acceptor`，UDP 提供
  `tun_udp_socket`/`tun_udp_acceptor`，命名与行为习惯对齐 Boost.Asio，
  降低学习成本。
- **极低协议开销**：接收方向缓存乱序段并静默等待补齐，避免人为乱序触发
  快速重传；发送方向仅以 RTO 计时器重读用户缓冲实现重传，无重传队列与拷贝。
- **IPv4/IPv6 双栈**：双栈报文解析与构造，内置 ICMP/ICMPv6 回显响应，
  丢弃分片与扩展头报文。
- **生产级健壮性**：资源上限（流数、队列字节数、总缓冲）、空闲超时与
  半开连接清理、环路与本地地址防护，均可通过 `tun_config` 或编译宏调整。
- **统计接口**：`engine_stats` 原子计数，实时暴露收发包、丢弃、活动连接
  与会话等指标。

## 平台支持

| 平台 | 设备实现 | 说明 |
| :--- | :--- | :--- |
| Linux | TUN（`posix::stream_descriptor`） | 需 root 或 `CAP_NET_ADMIN` |
| macOS | utun | 需 root；当前仅支持句柄注入（自主打开 Phase 3 未实现） |
| Windows | TAP（overlapped I/O）或 Wintun | 自主打开支持 `tap0901` 等 TAP 驱动；编译时 `USE_WINTUN_DRIVER` 切换 Wintun |

## 构建

### 依赖

- CMake 3.20+
- C++20 编译器
- Boost 1.81+（asio 头文件；代码使用 `boost::unordered_flat_map` 与
  `net::any_completion_handler`，最低要求 1.81；作为第三方库被 superproject
  引入时可复用其内置 Boost 目标）
- Windows + Wintun 还需链接 `iphlpapi`、`cfgmgr32`、`setupapi`、`ws2_32`
- Windows + TAP 驱动需链接 `ws2_32`、`iphlpapi`（CMake 已自动处理）

### 编译与测试

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

常用选项：

- `TUNIO_BUILD_TESTS`（默认 `ON`）：构建单元测试（基于 Boost.Test，
  header-only 模式，无需额外链接测试库）。
- `TUNIO_BUILD_EXAMPLES`（默认 `ON`）：构建示例程序。
- `USE_WINTUN_DRIVER`（默认 `OFF`，仅 Windows）：使用 Wintun 驱动。
- `TUNIO_DISABLE_LOOPBACK_GUARD`（默认 `OFF`）：关闭环路与本地地址防护
  （定义编译宏 `TUNIO_DISABLE_LOOPBACK_GUARD` 效果相同）。

## 快速上手

以下是一个完整的最小示例：打开 TUN 设备，将虚拟网内的 TCP 连接桥接到本机
回环端口的 echo 服务，并在引擎层直接回显 UDP 数据报。完整版本见
`examples/tun_echo.cpp`。

```cpp
#include "tunio/tun_tcp_acceptor.hpp"
#include "tunio/tun_config.hpp"
#include "tunio/tun_tcp_socket.hpp"
#include "tunio/tun_udp_acceptor.hpp"
#include "tunio/tun_udp_socket.hpp"
#include "tunio/tunio.hpp"

#include <boost/asio.hpp>

#include <array>
#include <iostream>
#include <memory>

namespace net = boost::asio;
using tunio::tun_tcp_acceptor;
using tunio::tun_tcp_socket;
using tunio::tun_udp_acceptor;
using tunio::tun_udp_socket;

// ---- TCP 全双工桥接：虚拟连接 <-> 本机 echo 服务 ----
net::awaitable<void> bidirectional_bridge(tun_tcp_socket client,
                                          net::ip::tcp::endpoint target)
{
    auto ex = co_await net::this_coro::executor;
    auto proxy = std::make_shared<net::ip::tcp::socket>(ex);
    boost::system::error_code ec;
    co_await proxy->async_connect(target,
                                  net::redirect_error(net::use_awaitable, ec));
    if (ec) {
        client.reset(); // 后端不可达：立即向客户端发送 RST
        co_return;
    }
    auto c = std::make_shared<tun_tcp_socket>(std::move(client));

    net::co_spawn(
        ex,
        [c, proxy]() -> net::awaitable<void> {
            std::array<char, 8192> buf;
            try {
                for (;;) {
                    size_t n = co_await c->async_read_some(net::buffer(buf),
                                                           net::use_awaitable);
                    co_await net::async_write(*proxy, net::buffer(buf, n),
                                              net::use_awaitable);
                }
            } catch (...) {
            }
            boost::system::error_code sec;
            proxy->shutdown(net::ip::tcp::socket::shutdown_send, sec);
        },
        net::detached);

    net::co_spawn(
        ex,
        [c, proxy]() -> net::awaitable<void> {
            std::array<char, 8192> buf;
            try {
                for (;;) {
                    size_t n = co_await proxy->async_read_some(
                        net::buffer(buf), net::use_awaitable);
                    co_await net::async_write(*c, net::buffer(buf, n),
                                              net::use_awaitable);
                }
            } catch (...) {
                c->close();
            }
        },
        net::detached);
}

// ---- TCP 监听：每个虚拟连接派生一个桥接协程 ----
net::awaitable<void> tcp_listener(tunio::tunio &engine, uint16_t echo_port)
{
    auto ex = co_await net::this_coro::executor;
    tun_tcp_acceptor acceptor(engine);
    for (;;) {
        tun_tcp_socket client(ex);
        boost::system::error_code ec;
        co_await acceptor.async_accept(
            client, net::redirect_error(net::use_awaitable, ec));
        if (ec) {
            co_return;
        }
        const auto dest = client.original_destination();
        const net::ip::tcp::endpoint target =
            dest.address().is_v6()
                ? net::ip::tcp::endpoint(net::ip::address_v6::loopback(),
                                         echo_port)
                : net::ip::tcp::endpoint(net::ip::address_v4::loopback(),
                                         echo_port);
        net::co_spawn(ex, bidirectional_bridge(std::move(client), target),
                      net::detached);
    }
}

// ---- UDP 回显会话：一个虚拟客户端会话对应一个协程 ----
net::awaitable<void> udp_echo_handler(tun_udp_socket session)
{
    std::array<char, 2048> buf;
    try {
        for (;;) {
            net::ip::udp::endpoint sender;
            size_t n = co_await session.async_receive_from(
                net::buffer(buf), sender, net::use_awaitable);
            co_await session.async_send_to(sender, net::buffer(buf, n),
                                           net::use_awaitable);
        }
    } catch (...) {
        session.close();
    }
}

net::awaitable<void> udp_listener(tunio::tunio &engine)
{
    auto ex = co_await net::this_coro::executor;
    tun_udp_acceptor acceptor(engine);
    for (;;) {
        tun_udp_socket session(ex);
        boost::system::error_code ec;
        co_await acceptor.async_accept(
            session, net::redirect_error(net::use_awaitable, ec));
        if (ec) {
            co_return;
        }
        net::co_spawn(ex, udp_echo_handler(std::move(session)), net::detached);
    }
}

int main()
{
    net::io_context io(1);
    tunio::tunio engine(io);

    tunio::tun_config cfg;
    cfg.dev_name = "tun0";
    cfg.ipv4_addr = "10.0.0.1";
    cfg.netmask = "255.255.255.0";
    cfg.mtu = 1500;

    boost::system::error_code ec;
    if (!engine.open(cfg, ec)) {
        std::cerr << "open TUN failed: " << ec.message() << std::endl;
        return 1;
    }

    net::co_spawn(io, tcp_listener(engine, 7), net::detached);
    net::co_spawn(io, udp_listener(engine), net::detached);
    io.run();
    return 0;
}
```

以 root 运行并配置路由后，虚拟网内客户端即可访问本机 echo 服务：

```sh
sudo ./tun_echo --tun tun0 --ip 10.0.0.1 --netmask 255.255.255.0
sudo ip route add 10.0.0.0/24 dev tun0   # 或由外部路由/策略路由注入流量
```

## API 设计

### 架构概览

系统采用四层解耦架构：设备抽象层（`tun_device`）负责跨平台 TUN I/O；
协议引擎层（TCP/UDP Flow Engine）在引擎串行执行器上维护流表与 NAT；
异步 API 层向上层暴露四个套接字抽象；应用层通过协程实现业务逻辑。所有
内部状态变更均串行执行：单线程模式（默认）要求 `io_context` 单线程
`run()`；多线程模式（`tunio(io, false)`）使用 Strand，多线程 `io_context`
下无锁竞争。

```
应用层 (Proxy Logic / SOCKS5 Client)
    |  co_await / CompletionToken
异步 API 层
    tun_tcp_socket / tun_tcp_acceptor   tun_udp_socket / tun_udp_acceptor
协议引擎层
    TCP Flow Engine             UDP Flow Engine
    Flow Dispatcher & NAT 表 (串行执行器)
设备抽象层
    tun_device (Linux TUN / macOS utun / Windows Wintun)
      └─ async_read_packet / async_write_packet  (原始字节包)
      └─ async_read_ip    / async_write_ip      (ip_packet 解析级接口)
```

### 核心类型

| 类型 | 头文件 | 说明 |
| :--- | :--- | :--- |
| `tunio::tunio` | `tunio/tunio.hpp` | 引擎入口：打开/关闭设备、查询 MTU/本地 IP/统计 |
| `tunio::tun_config` | `tunio/tun_config.hpp` | 引擎配置：网络、句柄注入、资源上限、超时 |
| `tunio::engine_stats` | `tunio/tun_config.hpp` | 原子统计计数（收发包/丢弃/连接/会话/ICMP） |
| `tunio::tun_tcp_socket` | `tunio/tun_tcp_socket.hpp` | 虚拟 TCP 流，可读写、握手批准/拒绝、RST |
| `tunio::tun_tcp_acceptor` | `tunio/tun_tcp_acceptor.hpp` | 虚拟 TCP 监听器，SYN 到达时触发 accept |
| `tunio::tun_udp_socket` | `tunio/tun_udp_socket.hpp` | 虚拟 UDP 数据报会话，一次一报 |
| `tunio::tun_udp_acceptor` | `tunio/tun_udp_acceptor.hpp` | 新 UDP 会话监听器 |
| `tunio::tun_device` | `tunio/tun_device.hpp` | 跨平台设备抽象：自主打开/句柄注入，原始字节包与解析级 IP 包异步 I/O |
| `tunio::ip_packet` | `tunio/ip_packet.hpp` | 解析后的 IP 报文：IP 头信息 + TCP/UDP/ICMP 传输层视图 + 载荷；支持字段构造与校验和计算 |

### 引擎入口 `tunio`

```cpp
explicit tunio(net::io_context &ctx, bool single_thread = true);
bool open(const tun_config &config, boost::system::error_code &ec);
void close();
bool is_open() const noexcept;
size_t mtu() const noexcept;
net::ip::address local_address() const noexcept;
const engine_stats &stats() const noexcept;
executor_type get_executor() const noexcept;   // 引擎内部串行执行器
```

`open()` 同步完成设备创建与配置；`close()` 停止数据通路并清理全部会话与
挂起操作；`get_executor()` 返回引擎内部串行执行器（单线程模式为 io 执行器，
多线程模式为 Strand），应用层可借其提交任务与引擎状态串行化。

### 套接字抽象

四个套接字类型的行为与 Boost.Asio 对应类型对齐，全部异步操作均支持
CompletionToken（协程 `co_await` 或 `net::use_future` 等）：

- `tun_tcp_socket`：`async_read_some` / `async_write_some` /
  `original_destination()` / `remote_endpoint()` / `accept()` / `reject()` /
  `reset()` / `shutdown()` / `close()` / `is_open()`。
- `tun_tcp_acceptor`：`async_accept(tun_tcp_socket &peer, token)` / `cancel()`。
- `tun_udp_socket`：`async_receive_from` / `async_send_to(remote, ...)` /
  `client_endpoint()` / `set_timeout()` / `close()` / `is_open()`。
- `tun_udp_acceptor`：`async_accept(tun_udp_socket &peer, token)` /
  `cancel()`。

握手语义：收到客户端 SYN 后引擎不立即回复，由 `accept()`/`reject()`（或
首次读写隐式批准）决定握手结果；三次握手完成前的读写操作会缓冲，完成后
交付。TCP 转发为按序交付（乱序段经重排缓存补齐），超时与资源上限见
`tun_config`。

### 设备层 `tun_device` 与 `ip_packet`

`tun_device` 可脱离引擎独立使用：打开真实 TUN 设备或注入外部句柄后，直接
读写 IP 报文。除原始字节包接口 `async_read_packet` / `async_write_packet`
外，还提供解析级接口 `async_read_ip` / `async_write_ip`，操作对象为
`ip_packet` —— 一次读取即得到一个完整解析的 IP 报文：

```cpp
#include "tunio/tun_device.hpp"
#include "tunio/ip_packet.hpp"

tunio::tun_device dev(io);
boost::system::error_code ec;
if (!dev.open(cfg, ec)) { /* ... */ }

// 读：解析 IP 头 + 传输层（TCP/UDP/ICMP）视图 + 载荷，零拷贝
tunio::ip_packet pkt;
size_t n = co_await dev.async_read_ip(pkt, net::use_awaitable);
if (pkt.valid()) {
    const net::ip::address src = pkt.source_address();
    const net::ip::address dst = pkt.destination_address();
    if (pkt.is_tcp()) {
        uint16_t sport = pkt.source_port();      // 主机字节序
        const auto *tcp = pkt.tcp();             // 原始 TCP 头视图
        const uint8_t *data = pkt.transport_data();
    } else if (pkt.is_udp()) {
        // ...
    } else if (pkt.is_icmp() && pkt.icmp_type() == 8) {
        uint16_t id = pkt.icmp_echo_id();
    }
}

// 写：从字段构造报文（自动计算长度与 IP/TCP/UDP/ICMP 校验和）后写出
tunio::ip_packet out;
out.begin_ipv4(src_v4, dst_v4);
out.begin_udp(12345, 53);
out.append_payload(data, len);
out.finalize();
co_await dev.async_write_ip(out, net::use_awaitable);
```

行为约定：

- `async_read_ip` 完成签名为 `void(error_code, size_t)`，`ec` 仅反映设备
  I/O 错误；报文结构非法时 `ec` 为 `no_error`，通过 `pkt.valid()` /
  `pkt.error()` 判断（解析失败原因含非法版本、报文过短、IHL/total_len 非法、
  传输层头非法等）。
- 解析只做结构校验，不验证校验和；IPv4 分片包解析并暴露
  `fragmented()` / `fragment_offset()`（分片非首片不解析传输层视图）；
  IPv6 扩展头不遍历，`ip_protocol()` 返回原始扩展头号。
- 每个未完成的 `async_read_ip` 需要独立的 `ip_packet` 对象（自持缓冲），
  同一对象不可并发发起多次读取；`packet_buffer` 默认容量 2048，大 MTU
  场景构造 `ip_packet(dev.mtu() + 64)`。
- macOS utun 的 4 字节家族前缀在平台实现层透明剥离/附加，`ip_packet`
  始终看到纯 IP 报文。

### `tun_config` 关键配置

| 字段 | 默认值 | 说明 |
| :--- | :--- | :--- |
| `dev_name` / `ipv4_addr` / `netmask` | 空 / 空 | 自主打开模式下的设备名（默认空，Linux 内核自动命名）与 IPv4 配置 |
| `ipv6_addr` / `ipv6_prefix_len` | 空 / `64` | 可选 IPv6 地址 |
| `mtu` | `1500` | 自主打开模式 MTU |
| `num_queues` | `1` | Linux TUN 多队列数（`IFF_MULTI_QUEUE`，上限 256；其他平台忽略） |
| `external_handle` / `external_mtu` | `invalid` / `1500` | 外部句柄注入（优先于自主打开） |
| `external_handles` | 空 | 多句柄注入：每元素一个队列 fd（非空时优先于 `external_handle`；仅 Linux TUN 多队列有意义） |
| `max_tcp_flows` / `max_udp_flows` | `65536` | 流/会话数上限 |
| `max_rx_queue_per_flow` | `8 MiB` | 每流接收队列字节上限 |
| `max_total_buffer` | `512 MiB` | 全局缓冲上限 |
| `udp_idle_timeout` | `30s` | UDP 会话空闲超时 |
| `tcp_time_wait_timeout` / `tcp_accept_timeout` | `10s` / `30s` | TCP 清理超时 |
| `tcp_syn_timeout` / `tcp_close_timeout` | `30s` / `30s` | 半开/关闭流程超时 |

### 生命周期与线程安全

- 首次 `open()` 必须在 `io_context` 开始运行（`io.run()`）之前调用。
- 引擎必须在所有 `tun_tcp_socket` / `tun_udp_socket` 销毁之后、且
  `io_context` 停止运行（所有 `run()` 已返回、io 线程已 join）之后销毁。
  若在 io 线程仍在执行完成回调时销毁引擎，引擎内部状态与在途异步操作将
  与销毁线程并发访问（数据竞争），设备层读回调引用的内部缓冲也可能已
  释放。运行期间需要关闭请调用 `close()`，其内部在串行执行器上完成清理。
- 对已打开（或 close 后尚未完成异步清理）的引擎再次 `open()` 时，
  `io_context` 必须正在运行：`open()` 会在串行执行器上同步收尾上一代实例，
  `io_context` 未运行时该收尾任务无法执行，将导致调用线程阻塞等待。
- 运行期间需要重新 `open()` 时，请通过 `get_executor()` 派发屏障任务，
  确保与引擎内部任务串行。
- 所有异步操作完成回调在调用方绑定的执行器上触发；引擎内部状态由
  串行执行器串行化。单线程模式要求 `io_context` 单线程 `run()`；多线程
  模式由 Strand 串行化，多线程运行 `io_context` 是安全的。
- `is_open()` 等同步状态查询仅在 io 线程（或与引擎串行执行器同步的
  上下文）调用（与 Boost.Asio 对共享 socket 对象"并发访问不安全"的约定
  一致）；多线程模式下从任意线程调用会构成数据竞争。
- `async_write_some` / `async_send_to` 的缓冲区必须保持有效至完成回调
  触发（与 Boost.Asio 语义一致，引擎只引用不拷贝）。

## 更多示例

### Linux TUN 多队列（IFF_MULTI_QUEUE）

Linux 下可通过 `num_queues` 以多队列模式打开 TUN 设备（内核 ≥ 2.6.30，
需 `CAP_NET_ADMIN`）：每个队列一个独立 fd，内核按流哈希并行投递入站包，
引擎为每个队列分配并发读槽、出站包按五元组哈希分发到各队列 fd，读写
吞吐随队列数扩展（建议配合多线程 `io_context` 使用）：

```cpp
tunio::tun_config cfg;
cfg.dev_name = "tun0";
cfg.ipv4_addr = "10.0.0.1";
cfg.netmask = "255.255.255.0";
cfg.num_queues = 4;  // 4 队列多队列模式

boost::system::error_code ec;
if (!engine.open(cfg, ec)) {
    std::cerr << "open failed: " << ec.message() << std::endl;
    return 1;
}
std::cout << "queues: " << engine.queue_count() << std::endl;  // 4
```

多队列也可以经 `external_handles` 注入外部已打开的队列 fd（每个句柄对应
一个队列，队列数 = 句柄数）；非 Linux 平台忽略多队列配置，`queue_count()`
恒为 1。若内核不支持 `IFF_MULTI_QUEUE`（老内核），显式请求多队列打开会
失败并返回 `EINVAL`，不会静默降级。

### 外部句柄注入

需要特殊权限前置（如提前获取 `CAP_NET_ADMIN`）或接管外部已打开的设备时，
通过 `external_handle` 注入，此时必须显式指定 MTU：

```cpp
int fd = open("/dev/net/tun", O_RDWR);  // 外部已打开并配置好的 TUN fd

tunio::tun_config cfg;
cfg.external_handle = fd;
cfg.external_mtu = 1500;

boost::system::error_code ec;
if (!engine.open(cfg, ec)) {
    std::cerr << "open failed: " << ec.message() << std::endl;
    return 1;
}
```

### 多线程运行

引擎内部全部状态由 Strand 串行化，可直接以线程池运行 `io_context`（每个
线程执行 `io.run()`，线程数即并发度）：

```cpp
net::io_context io(4);
tunio::tunio engine(io);
// ... open + 注册监听协程 ...

std::vector<std::thread> threads;
for (size_t i = 1; i < 4; ++i) {
    threads.emplace_back([&io] { io.run(); });
}
io.run(); // 主线程也参与事件循环
for (auto &t : threads) {
    t.join();
}
```

## 示例程序

| 程序 | 说明 |
| :--- | :--- |
| `tun_echo` | 最小示例：TCP 桥接本机 echo 服务 + UDP 回显 |
| `tun2socks` | SOCKS5 透明代理：TCP CONNECT + UDP ASSOCIATE |
| `tun_packet` | 原始 IP 包中继/打印：直接使用 `tun_device` + `ip_packet`，解析并打印 TCP/UDP/ICMP 协议详情，`--echo` 回环中继 |
| `benchmark` | 异步接口每操作堆分配与吞吐基准（基于 socketpair 注入） |

## 许可证

Boost Software License 1.0，见 `LICENSE_1_0.txt`。
