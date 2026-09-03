# 基于 Boost.Asio 范式的用户态 TUN 虚拟网络引擎
## 架构与设计说明书

**版本**：3.0
**状态**：正式发布版
**适用场景**：tun2socks、透明代理、轻量级 VPN 网关

---

### 1. 概述

本设计定义了一个高性能、跨平台的用户态 TUN 网络引擎（以下简称 `tunio`）。该引擎将 Linux TUN、macOS utun 及 Windows Wintun 设备产生的 L3 原始 IP 包处理全面封装于内部，向上层应用暴露一套完全对齐 Boost.Asio 网络编程范式的现代 C++ 异步接口。

引擎在设计上具备高度的**设备管理灵活性**：既支持由引擎内部自主创建并配置 TUN 设备，也支持接管由外部应用预先打开的平台原生句柄（文件描述符或 HANDLE）。后者对于需要特殊权限提升（如 Linux CAP_NET_ADMIN 提前提权）、多实例共享同一设备或集成于现有网络管理框架的场景尤为关键。

本引擎在 TCP 层面采取 **轻量级转发策略**：接收方向维护乱序重排缓存，缺失段补齐后按序交付并批量确认，避免人为乱序触发对端快速重传；发送方向维护最精简的 RTO 重传（无重传队列，直接重读用户写缓冲）与零窗口持久探测，避免在丢包链路上发送侧永久挂起。这种设计在保持低 CPU 开销与内存占用的同时，保证了虚拟连接在真实网络环境下的基本可靠性。

上层开发者能够像操作普通 `asio::ip::tcp::socket` 一样处理 VPN 拦截流量，无缝接入 C++20 协程（`co_await`），从而极大地简化透明代理、tun2socks 及轻量级 VPN 网关的开发复杂度。

---

### 2. 整体架构

系统采用四层解耦架构，自底向上分别为设备抽象层、协议引擎层、异步 API 层和应用层。引擎内部所有状态变更、NAT 表操作及定时器销毁均强制串行执行，避免锁竞争。串行化有两种模式：单线程模式（默认）直接运行于 `io_context` 执行器上，省去每包 Strand 派发开销，要求 `io_context` 单线程 `run()`；多线程模式（`tunio(io, false)`）运行于一个 **Boost.Asio Strand** 之上，支持 `io_context` 运行于线程池。

```
+-----------------------------------------------------------------------------+
|                        应用层 (Proxy Logic / SOCKS5 Client)                  |
|              (使用 Boost.Asio, co_await, 业务路由逻辑)                       |
+-----------------------------------------------------------------------------+
                                    ▲
               Async API Boundary   │  tun_tcp_socket / tun_tcp_acceptor
                                    │  tun_udp_socket / tun_udp_acceptor
                                    ▼
+-----------------------------------------------------------------------------+
|                           异步 API 接口层                                    |
|  +----------------------------+   +---------------------------------------+ |
|  |       tun_tcp_socket           |   |         tun_udp_socket               | |
|  |  (TCP Virtual Socket)      |   |  (UDP Datagram Socket)              | |
|  +----------------------------+   +---------------------------------------+ |
|  +----------------------------+   +---------------------------------------+ |
|  |       tun_tcp_acceptor         |   |         tun_udp_acceptor             | |
|  |  (TCP Listener)            |   |  (UDP Session Listener)              | |
|  +----------------------------+   +---------------------------------------+ |
+-----------------------------------------------------------------------------+
                                    ▲
                                    ▼
+-----------------------------------------------------------------------------+
|                           协议引擎层 (Core Engine)                           |
|  +-------------------------------------+ +--------------------------------+ |
|  |       TCP Flow Engine               | |     UDP Flow Engine            | |
|  | - 乱序缓存按序交付                  | | - Datagram 收发                 | |
|  | - 基础 SEQ/ACK 校验                 | | - Session 表 (5-Tuple)          | |
|  | - RTO 重传读缓冲直发               | | - Min-Heap 空闲超时             | |
|  | - 动态接收窗口                     | | - 新会话通知队列               | |
|  +-------------------------------------+ +--------------------------------+ |
|  +-----------------------------------------------------------------------+ |
|  |          Flow Dispatcher & NAT Table (five_tuple indexing)            | |
|  |          (串行执行器，无锁访问)                                       | |
|  +-----------------------------------------------------------------------+ |
+-----------------------------------------------------------------------------+
                                    ▲
                                    ▼
+-----------------------------------------------------------------------------+
|                       跨平台设备抽象层 (Packet Device)                       |
|  +-----------------------------------------------------------------------+ |
|  | 支持两种初始化模式:                                                    | |
|  |  ① 自主打开: open(device_config) → 创建并配置设备                    | |
|  |  ② 句柄注入: assign(handle, mtu) → 接管外部已打开的句柄              | |
|  |                                                                       | |
|  |  异步 I/O 完全对齐 Asio 范式:                                        | |
|  |  async_read_packet / async_write_packet (原始字节包)              | |
|  |  async_read_ip / async_write_ip (ip_packet 解析级接口)            | |
|  +-----------------------------------------------------------------------+ |
|  +------------------+  +------------------+  +----------------------------+ |
|  | Linux TUN (fd)   |  | macOS utun       |  | Windows Wintun (Overlapped)| |
|  +------------------+  +------------------+  +----------------------------+ |
+-----------------------------------------------------------------------------+
```

**线程模型**：所有内部数据结构（TCP 控制块表、UDP NAT 表、新会话队列等）均在引擎串行执行器上访问。该执行器在单线程模式下为 `io_context` 的执行器，多线程模式下为 `net::strand<net::any_io_executor>`。任何修改共享状态的操作（包括异步回调中触发的状态变更）都必须通过该执行器的 `dispatch` 或 `post` 提交，确保串行。设备 I/O（`async_read_packet`/`async_write_packet`）由底层 `io_context` 调度，其完成回调在单线程模式下直接于 io 线程执行，多线程模式下经 Strand 串行化。

---

### 3. 核心基础类型定义

#### 3.1 平台原生句柄类型

支持外部句柄注入，定义跨平台句柄别名。

```cpp
#ifdef _WIN32
    using native_handle_type = void*;   // Windows HANDLE
#else
    using native_handle_type = int;     // POSIX 文件描述符
#endif

constexpr native_handle_type invalid_native_handle =
#ifdef _WIN32
    nullptr;
#else
    -1;
#endif
```

#### 3.2 统一五元组

用于 NAT 查表与 Flow 索引，支持 IPv4 与 IPv6。IP 地址以网络字节序原始字节保存
（IPv4 仅前 4 字节有效），`family` 字段区分地址族（4 或 6），端口为网络字节序，
可直接与报文头部字段比较。基于纯内存块进行哈希计算以保证高性能，避免字符串分配。

```cpp
#pragma pack(push, 1)
struct five_tuple {
    std::array<uint8_t, 16> src_ip{};  // 网络字节序（IPv4 仅前 4 字节有效）
    std::array<uint8_t, 16> dst_ip{};
    uint16_t src_port = 0;             // 网络字节序
    uint16_t dst_port = 0;             // 网络字节序
    uint8_t  protocol = 0;             // IPPROTO_TCP (6) 或 IPPROTO_UDP (17)
    uint8_t  family = 0;               // 4 或 6
};
#pragma pack(pop)

// 构造五元组：IP 为网络字节序字节，IPv4 仅拷贝前 4 字节
inline five_tuple make_five_tuple(const uint8_t* src_ip, const uint8_t* dst_ip,
                                  uint16_t src_port, uint16_t dst_port,
                                  uint8_t protocol, uint8_t family) noexcept;
```

#### 3.3 IPv6 报文头与校验和

引擎同时支持 IPv4 与 IPv6 报文。IPv6 头部固定 40 字节，无头部校验和；
TCP/UDP/ICMPv6 的伪头部校验和基于 128 位地址计算，IPv6 下 UDP 与 ICMPv6
校验和强制有效（不得为 0，若计算结果为 0 则以 0xffff 替代）。
校验和求和在支持 SSE2 的平台上使用 8 路 32 位累加器向量化（RFC 1071
语义不变），是收方向每段必经的固定成本之一。

```cpp
struct ipv6_header {
    uint32_t vtc_flow;      // version(4) | traffic class(8) | flow label(20)
    uint16_t payload_len;   // 不含 IPv6 头部的载荷长度
    uint8_t  next_header;   // 上层协议号
    uint8_t  hop_limit;
    uint8_t  src_ip[16];    // 网络字节序
    uint8_t  dst_ip[16];    // 网络字节序
};

// 引擎内统一 IP 层信息：地址族 + 网络字节序地址字节
struct ip_packet_info {
    uint8_t family = 0;     // 4 或 6
    uint8_t protocol = 0;
    uint8_t src_ip[16] = {};
    uint8_t dst_ip[16] = {};
};
```

IPv6 扩展头（Hop-by-Hop、Routing 等）不在支持范围内，携带扩展头的报文
按未知协议丢弃；IPv6 jumbogram（超长载荷）亦不支持。

#### 3.4 零拷贝数据包缓冲区

支持头部预留（Headroom）机制，便于高效封装 IP/TCP 头部，避免频繁的内存分配与拷贝。

```cpp
class packet_buffer {
    std::unique_ptr<uint8_t[]> storage_;
    size_t capacity_;
    size_t headroom_;     // 预设头部空间 (如 128 字节)
    size_t data_offset_;  // 实际数据起始偏移
    size_t data_size_;
public:
    explicit packet_buffer(size_t cap = 2048, size_t headroom = 128);

    uint8_t* data() noexcept { return storage_.get() + data_offset_; }
    const uint8_t* data() const noexcept { return storage_.get() + data_offset_; }
    size_t size() const noexcept { return data_size_; }

    // 实际接口（详见 packet_buffer.hpp，无 prepend/trim/headroom_available）：
    //   commit(len)          读取完成后推进数据长度
    //   resize(len)          直接设定数据长度（写报文场景）
    //   writable_data()/writable_size()  可写区（供异步读取）
    //   headroom()           头部预留大小（macOS utun 写前缀等需要 >= 4）
};
```

#### 3.5 TCP 最小控制块

引擎仅维护最精简的状态信息，用于生成正确的 ACK 与处理 RST/FIN。

```cpp
struct tcp_minimal_state {
    // ---- 序列号跟踪 ----
    uint32_t snd_nxt;      // 本端将要发送的下一个序列号
    uint32_t rcv_nxt;      // 本端期望接收的下一个序列号

    // ---- 初始序列号 ----
    uint32_t iss;          // 本端初始发送序号
    uint32_t irs;          // 对端初始发送序号

    // ---- 接收窗口通告（动态：min(固定上限, 剩余缓冲)）----
    static constexpr uint32_t fixed_rcv_wnd = 1048576;

    // ---- 状态机 ----
    enum State : uint8_t {
        CLOSED, SYN_SENT, SYN_RCVD, SYN_ACK_SENT,
        ESTABLISHED,
        FIN_WAIT_1, FIN_WAIT_2, CLOSE_WAIT,
        LAST_ACK, TIME_WAIT
    } state;
};
```

---

### 4. 跨平台设备抽象层

引擎核心不直接依赖任何平台特定的系统调用：`tun_device` 经类型别名 `detail::tun_device_impl` 持有按平台拆分的实现类（posix/windows/wintun/unsupported，位于 `include/tunio/detail/impl/`），在统一的外壳类中完成 I/O 调度。该外壳支持两种初始化模式：

- **自主打开模式**：传入设备配置（设备名、IP 等），内部根据平台构造对应的实现类。
- **句柄注入模式**：传入外部已打开的平台原生句柄及 MTU，直接构造对应的实现类。

所有异步 I/O 接口完全对齐 Boost.Asio 规范：使用 `CompletionToken` 模板参数，通过 `async_initiate` 实现。

```cpp
struct device_config {
    std::string name;
    std::string ipv4;
    std::string netmask;
    std::string ipv6;              // 可选 IPv6 地址，如 "fd00::1"
    uint8_t ipv6_prefix_len = 64;  // IPv6 前缀长度
    size_t mtu = 1500;
};

class tun_device {
public:
    explicit tun_device(boost::asio::io_context& ctx) : ctx_(ctx) {}

    // ---- 模式 1: 自主打开 ----
    bool open(const device_config& cfg, boost::system::error_code& ec) {
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
        impl_.emplace<posix_impl>(ctx_);
#elif defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)
        impl_.emplace<windows_impl>(ctx_);
#endif
        return std::visit([&](auto& impl) -> bool {
            return impl.open(cfg, ec);
        }, impl_);
    }

    // ---- 模式 2: 句柄注入 ----
    // 注：真实签名含第 4 个参数 utun_prefix（macOS utun 读写携带 4 字节
    // 家族前缀时置 true），且实现按平台拆分于 detail/impl/*.hpp，此处为
    // 简化示意.
    bool assign(native_handle_type handle, size_t mtu, bool utun_prefix, boost::system::error_code& ec) {
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
        impl_.emplace<posix_impl>(ctx_);
#elif defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)
        impl_.emplace<windows_impl>(ctx_);
#endif
        return std::visit([&](auto& impl) -> bool {
            return impl.assign(handle, mtu, ec);
        }, impl_);
    }

    void close() {
        std::visit([](auto& impl) { impl.close(); }, impl_);
    }

    size_t mtu() const {
        return std::visit([](const auto& impl) -> size_t { return impl.mtu(); }, impl_);
    }

    bool is_open() const {
        return std::visit([](const auto& impl) -> bool { return impl.is_open(); }, impl_);
    }

    // ---- 异步读取 ----
    template <typename CompletionToken>
    auto async_read_packet(packet_buffer& buf, CompletionToken&& token) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, size_t)>(
            [this, &buf](auto handler) {
                std::visit([&](auto& impl) {
                    impl.async_read(buf, std::move(handler));
                }, impl_);
            },
            token
        );
    }

    // ---- 异步写入 ----
    template <typename CompletionToken>
    auto async_write_packet(packet_buffer& buf, CompletionToken&& token) {
        return boost::asio::async_initiate<CompletionToken, void(boost::system::error_code, size_t)>(
            [this, &buf](auto handler) {
                std::visit([&](auto& impl) {
                    impl.async_write(buf, std::move(handler));
                }, impl_);
            },
            token
        );
    }

private:
    boost::asio::io_context& ctx_;

    // ---- POSIX 实现 (Linux/macOS) ----
    struct posix_impl {
        boost::asio::posix::stream_descriptor desc_;
        size_t mtu_ = 1500;
        bool open_ = false;

        explicit posix_impl(boost::asio::io_context& ctx) : desc_(ctx) {}

        bool open(const device_config& cfg, boost::system::error_code& ec) {
            // 实际 TUN 打开逻辑 (ioctl / dev/net/tun)
            // 成功后将 fd 通过 desc_.assign(fd, ec) 绑定
            open_ = true;
            mtu_ = cfg.mtu;
            return true;
        }

        bool assign(native_handle_type handle, size_t mtu, boost::system::error_code& ec) {
            desc_.assign(static_cast<int>(reinterpret_cast<intptr_t>(handle)), ec);
            if (!ec) { open_ = true; mtu_ = mtu; }
            return !ec;
        }

        void close() { desc_.close(); open_ = false; }
        size_t mtu() const { return mtu_; }
        bool is_open() const { return open_; }

        void async_read(packet_buffer& buf, std::function<void(boost::system::error_code, size_t)> handler) {
            desc_.async_read_some(boost::asio::buffer(buf.data(), buf.size()), std::move(handler));
        }

        void async_write(packet_buffer& buf, std::function<void(boost::system::error_code, size_t)> handler) {
            desc_.async_write_some(boost::asio::buffer(buf.data(), buf.size()), std::move(handler));
        }
    };

    // ---- Windows 实现 (Wintun / Overlapped) ----
    struct windows_impl {
        boost::asio::windows::overlapped_handle handle_;
        size_t mtu_ = 1500;
        bool open_ = false;

        explicit windows_impl(boost::asio::io_context& ctx) : handle_(ctx) {}

        bool open(const device_config& cfg, boost::system::error_code& ec) {
            // 实际 Wintun 打开逻辑 (WintunCreateAdapter / CreateFile)
            // 成功后将 HANDLE 通过 handle_.assign(h, ec) 绑定
            open_ = true;
            mtu_ = cfg.mtu;
            return true;
        }

        bool assign(native_handle_type handle, size_t mtu, boost::system::error_code& ec) {
            handle_.assign(handle, ec);
            if (!ec) { open_ = true; mtu_ = mtu; }
            return !ec;
        }

        void close() { handle_.close(); open_ = false; }
        size_t mtu() const { return mtu_; }
        bool is_open() const { return open_; }

        void async_read(packet_buffer& buf, std::function<void(boost::system::error_code, size_t)> handler) {
            handle_.async_read_some_at(0, boost::asio::buffer(buf.data(), buf.size()), std::move(handler));
        }

        void async_write(packet_buffer& buf, std::function<void(boost::system::error_code, size_t)> handler) {
            handle_.async_write_some_at(0, boost::asio::buffer(buf.data(), buf.size()), std::move(handler));
        }
    };

    std::variant<posix_impl, windows_impl> impl_;
};
```

#### 4.1 解析级包接口：`ip_packet`

在原始字节包 I/O（`async_read_packet` / `async_write_packet`）之上，
`tun_device` 提供解析级接口 `async_read_ip` / `async_write_ip`，操作对象为
公开类型 `ip_packet`（`include/tunio/ip_packet.hpp`）。该类型内部组合一个
`packet_buffer`，读路径设备将报文直接读入其内部缓冲并就地解析（零拷贝），
写路径既支持原样写出也支持从字段构造报文（自动计算长度与校验和）。

- **解析语义**：读完成即做结构校验并填充类型化视图 —— IP 层（版本/协议号/
  地址/总长/分片字段 + `ipv4_header` / `ipv6_header` 原始视图），传输层
  （TCP/UDP 头部视图与主机序端口、ICMP/ICMPv6 type/code/校验和/Echo id/seq），
  以及零拷贝载荷（`payload()` 为 IP 头之后的传输层报文段，
  `transport_data()` 为传输层头之后的纯应用数据）。
- **错误语义**：`async_read_ip` 完成签名 `void(error_code, size_t)`，`ec`
  仅反映设备 I/O 错误；报文结构非法时 `ec` 为 `no_error`，通过
  `pkt.valid()` / `pkt.error()`（枚举 `ip_packet::parse_error`）判断。
  解析只做结构校验、不验证校验和（转发/中继场景可能需原样处理坏校验和包）。
- **策略差异**（与引擎内部 `handle_packet` 不同，`ip_packet` 是通用解析器）：
  IPv4 分片包解析并暴露 `fragmented()` / `fragment_offset()`，不丢弃（分片
  非首片从流中间开始，不解析传输层视图）；IPv6 扩展头不遍历链，
  `next_header` 为扩展头号时传输层视图为空、`ip_protocol()` 返回原始号。
- **写路径 builder**：`begin_ipv4/begin_ipv6 -> begin_tcp/begin_udp/
  begin_icmp -> [append_payload] -> finalize()`，`finalize()` 回填长度并计算
  IP/TCP/UDP/ICMP 校验和（含伪头部），完成后访问器立即可用。
- **复用**：`src/ip_headers.hpp` 中的报文头部结构体与校验和工具已提升至
  公开头 `tunio/ip_packet.hpp`（`tunio` 命名空间），引擎内部经由
  `detail` 命名空间的 using 声明引用，行为与 ABI 均不变；引擎自身的
  `handle_packet` 解析/丢弃策略保持不变。

#### 4.2 Linux TUN 多队列（`IFF_MULTI_QUEUE`）

Linux TUN 驱动支持多队列模式（`IFF_MULTI_QUEUE`，内核 ≥ 2.6.30）：对同一
设备多次 `TUNSETIFF`（设备名相同、均携带 `IFF_MULTI_QUEUE`）可获得多个
独立 fd，每个 fd 对应内核侧一个收发队列；内核按流哈希把入站包并行投递到
各队列 fd，出站包写任意队列 fd 均可（内核按哈希选择实际出口队列）。

- **打开**：`device_config::num_queues > 1` 时，首个 fd 的 `TUNSETIFF`
  携带 `IFF_MULTI_QUEUE`（空设备名自动命名后回写），随后按回写名逐个
  打开其余队列 fd；任一队列打开失败即回滚关闭全部 fd（内核随之销毁
  设备）。`num_queues` 校验范围 `[1, 256]`（内核 `MAX_TAP_QUEUES`），
  非法或内核不支持时 `open()` 返回 `EINVAL`，不静默降级。单队列（默认）
  保持不带 `IFF_MULTI_QUEUE` 的旧行为，兼容老内核。
- **实现**：`posix_tun_device_impl` 由单个 `stream_descriptor` 改为
  `std::vector<stream_descriptor>`（每个队列一个），`async_read` /
  `async_write` 增加队列参数（越界返回 `bad_descriptor`），`queue_count()`
  返回队列数。注入模式增加 `assign_queues(handles, ...)` 多句柄注入
  （每个句柄一个队列，失败不关闭注入句柄）；非 POSIX 平台恒为单队列。
- **读侧**：`tunio_impl` 并发读槽按队列均分 —— 单队列保持原有 32 槽，
  多队列时每队列 `max(1, 32 / num_queues)` 个槽（队列数 ≤ 32 时总槽数
  保持 32，超过时每队列 1 槽、总槽数 = 队列数）；槽号 `index` 经
  `index / slots_per_queue` 归属队列，各队列读互不阻塞。
- **写侧**：`tun_queue_writer` 按报文五元组（地址族 + 源/目的地址 + 协议
  + 端口）做 FNV-1a 哈希取模把写请求分发到对应队列的独立写链
  （`pick_tx_queue`）：同队列内由 Strand 上的泵串行下发（同流稳定同队列、
  保持流内顺序），不同队列的写链互不阻塞——各队列 fd 独立，可同时处于
  未完成写状态，写吞吐随队列数扩展；单队列短路返回 0，零额外开销。
- **示例**：`tun2socks --queues N`；测试 `tests/test_multi_queue.cpp`
  覆盖写队列选择单元测试、多 socketpair 注入集成测试（含多线程 Strand
  模式）与真实 TUN 设备环回测试（无权限环境自动跳过）。

---

### 5. TCP 协议引擎

本引擎在 TCP 处理上采取轻量级转发策略：接收方向缓存乱序段并按序交付，以最低开销完成 L3/L4 拦截；发送方向提供 RTO 重传与零窗口持久探测，保证写操作在确认前不丢失数据。

#### 5.1 握手阶段

- 收到客户端 SYN 后，引擎分配一个 `tcp_minimal_state` 实例，记录 `irs = SYN.seq`，
  状态为 `SYN_RCVD`，并立即触发 `tun_tcp_acceptor::async_accept` 完成事件（此时连接尚未建立）。
- 引擎不立即回复 SYN-ACK：握手结果由应用在领取流后决定，通过
  `tun_tcp_socket::accept()` 回复 SYN-ACK（携带本端 `iss` 与固定 MSS 值，由 MTU 推导：
  IPv4 默认 `MSS = MTU - 40`，IPv6 默认 `MSS = MTU - 60`），或通过
  `tun_tcp_socket::reject()` 回复 RST；未显式调用时，首次读写视为隐式批准握手。
- 收到客户端 ACK 后，状态切换为 `ESTABLISHED`，数据通路开始工作。
- 若应用层在 `tcp_syn_timeout`（默认 30 秒）内未完成握手（`SYN_RCVD` /
  `SYN_ACK_SENT` 半开连接），引擎发送 RST 中断连接并回收资源，避免未领取连接长期驻留。

#### 5.2 数据接收与转发

- **设备读取**：`tunio` 内部读泵在引擎串行执行器上执行 `async_read_packet`。TUN 设备为包语义，每次读取恰为一个完整 IP 报文，读取成功后直接按报文长度校验并交给协议引擎，无需按流拆包拼接（注入设备同样应按包语义提供完整报文）。
- **IP 分片策略**：引擎不做 IP 重组，收到 IPv4 分片包（带分片偏移或 MF 标志）或带 Fragment 扩展头（Next Header = 44）的 IPv6 报文时直接丢弃并计入 `rx_dropped`。

- **顺序检查**：收到数据段后，检查 `SEQ == rcv_nxt`。
  - **若顺序正确**：提取应用层 Payload，追加到 `tun_tcp_socket` 的连续接收缓冲（`vector<uint8_t>` + 头部消费偏移，应用读取后按需压缩，避免逐字节队列），唤醒挂起的 `async_read` 操作；随后 `rcv_nxt += payload_len`，并**每段立即确认**；引擎发送的任何出段都会捎带最新 `rcv_nxt`，视为完成一次确认。
  - **若序列号超前（`SEQ > rcv_nxt`）**：引擎**缓存**该乱序段（`ooo_cache`，受 `tcp_ooo_max_segments` 与接收缓冲记账限额约束），缺失段补齐后按序交付并批量确认。缓存成功时静默等待（缺失段在并发读场景往往即将到达），避免人为乱序触发对端快速重传与拥塞窗口减半；缓存拒绝（超限/重复）时才发 Dup-ACK 促使对端重传缺失段。
  - **若序列号小于 `rcv_nxt`**：视为重复包，直接丢弃，不回复任何内容。

#### 5.3 数据发送

- **单写模型**：每条连接同一时刻至多持有一个未完成的写操作（符合
  Boost.Asio 串行写规则）。应用层调用 `async_write_some` 时，引擎接受
  任意大小的单次写入（不再按队列空间拒绝），由该流的发送协程持有，
  按客户端接收窗口与 MSS 循环分片。
- **设备写背压**：发送协程每个分片经 `tun_queue_writer::async_write`
  （带完成回调）下发，等待设备写完成后再构造下一分片——设备写通道
  拥塞时协程自然挂起，内存占用受"每流单写 + 设备队列水位"约束，
  不再无界累积。发送缓冲由 `tun_queue_writer` 的串行执行器内自由列表池化
  复用（写完成后回收，容量不足时新建），避免每个报文一次堆分配。
- **窗口挂起**：客户端接收窗口耗尽时，发送协程经每流信号通道挂起，
  收到 ACK 更新窗口后由引擎唤醒继续发送。
- **控制段直通**：SYN/SYN+ACK/FIN/RST/ACK 等控制段不走数据背压路径，
  直接入设备写队列（`async_write_and_forget`），确保连接建立与关闭的
  关键段不被数据拥塞阻塞。
- **重叠写拒绝**：上一写操作未完成时新写入以 `no_buffer_space` 立即
  完成（应用违反串行写规则时的背压兜底）。
- **ACK 确认制写完成**：写操作在所有数据被对端确认后才完成回调（而非
  写入设备即完成），保证应用层收到成功回调时对端已实际收到全部字节；
  用户写缓冲在回调前保持有效，重传无需拷贝。
- **RTO 重传**：发送协程对未确认数据维护 RTO 计时器（默认初始 200ms，
  指数退避，上限 60s），ACK 无进展时重读用户写缓冲重传未确认范围。
  重传次数超过 `tcp_rto_max_retransmits`（默认 8 次）判定发送超时，
  以 RST 关闭连接并以 `connection_reset` 完成挂起写，避免连接永久悬挂。
- **零窗口持久探测**：对端窗口为 0 时周期性发送窗口探测（数据未发完时
  探测下一个字节，已发完时重传未确认段首部），不依赖对端主动发窗口更新。
  探测间隔按指数退避直至 60s 上限；超过 `tcp_persist_max_probes`
  （默认 15 次）仍无窗口恢复时判定对端无恢复能力，以 RST 关闭连接并以
  `connection_reset` 完成挂起写，避免流表条目与挂起写无限期滞留
  （收到窗口更新或有效 ACK 时复位计数）。
- **FIN 推迟**：关闭发送侧时尚有未发送/未确认数据时，FIN 推迟到数据
  全部确认后发送，避免 FIN 与在途数据段序列号重叠导致对端丢弃 FIN；
  `close()` 时仍有未确认数据则改发 RST 快速释放连接。

#### 5.4 接收窗口

- 引擎通告一个**动态接收窗口**：`min(固定上限, 剩余接收缓冲)`（对齐 gVisor `selectWindow`），剩余缓冲耗尽时通告 0 施加背压，避免固定大窗口误导对端超发导致队列积压丢包。
- 窗口缩放（RFC 7323 Window Scale）在握手阶段协商，1MB 窗口上限使用 scale=7；SYN-ACK 阶段窗口未缩放。
- 应用层消费后窗口恢复时，引擎立即发送 ACK 通告新窗口（对齐 gVisor 窗口跨越 ACK 阈值机制），避免零窗口死锁下仅靠对端窗口探测（指数退避）缓慢恢复。

#### 5.5 连接终止

- 收到 FIN 段时，引擎回复 ACK，状态进入 `CLOSE_WAIT`，并向应用层指示 `EOF`。FIN 可与数据同段（其序号为 `SEQ + 载荷长度`），引擎按 RFC 语义一并确认。
- 当应用层关闭 `tun_tcp_socket` 时，引擎发送 FIN 段，完成四次挥手。
- 收到 RST 段时，直接销毁 TCB 并通知应用层连接重置。
- 半开连接（`SYN_RCVD` / `SYN_ACK_SENT`）在 `tcp_syn_timeout`（默认 30 秒）后清理；关闭流程
  （`FIN_WAIT_1` / `FIN_WAIT_2` / `LAST_ACK`）在 `tcp_close_timeout`（默认 30 秒）
  后强制清理，避免对端异常时控制块长期驻留。

---

### 6. UDP 协议引擎

UDP 引擎将无连接的 UDP 协议映射为有状态的会话，采用标准数据报语义。

#### 6.1 Datagram 语义

UDP 是数据报协议，其 API 严格遵循一次收发对应一个完整数据报的语义。一次 `async_receive_from` 返回且仅返回一个完整的原始 UDP Datagram（包含 UDP 头载荷），并通过出参返回该数据报的目标远端端点。

#### 6.2 会话模型（1 对 N）

UDP 无连接，一个客户端套接字可与任意多个远端通信。引擎会话以**客户端三元组**（源 IP、源端口、协议）唯一标识：一个会话对应一个客户端套接字，与远端是 1 对 N 关系。每个数据报独立携带目标远端端点，远端信息不保存在会话中。

当引擎收到一个属于未知客户端三元组的 UDP 数据包时，会自动创建一个新的 `udp_session`，并将其加入一个**新会话通知队列**。上层应用通过 `tun_udp_acceptor::async_accept` 等待并获取这个新会话对应的 `tun_udp_socket` 对象。

#### 6.3 NAT 会话管理

- **映射表**：`std::unordered_map<udp_session_key, std::shared_ptr<udp_session>>`，运行于引擎串行执行器中。
- **会话结构**：

```cpp
struct udp_session {
    udp_session_key key;                 // 客户端三元组
    std::deque<datagram> rx_datagrams;   // datagram = 载荷 + 目标远端端点
    std::chrono::steady_clock::time_point expiry;
    bool active = true;
};
```

- **老化策略**：采用**最小堆（Min-Heap）** 管理会话超时。堆顶元素为最早即将过期的会话，定时器仅等待堆顶超时，避免轮询扫描全表。每次收到数据包时更新会话的 `expiry` 并调整堆位置。

---

### 7. 公开异步 API 定义

所有 API 严格遵循 Boost.Asio 的 `async_initiate` 模型，保证与 `use_awaitable`、`use_future` 及自定义 CompletionToken 的完全兼容。

#### 7.1 `tun_tcp_socket`（TCP 虚拟流套接字）

```cpp
class tun_tcp_socket {
public:
    using executor_type = boost::asio::any_io_executor;

    explicit tun_tcp_socket(executor_type ex);
    ~tun_tcp_socket();

    tun_tcp_socket(tun_tcp_socket&&) noexcept;
    tun_tcp_socket& operator=(tun_tcp_socket&&) noexcept;

    executor_type get_executor() const noexcept;

    // 获取客户端请求的原始目标地址
    boost::asio::ip::tcp::endpoint original_destination() const;

    template <typename MutableBufferSequence, typename CompletionToken>
    auto async_read_some(MutableBufferSequence&& buffers, CompletionToken&& token) {
        return boost::asio::async_initiate<CompletionToken, void(error_code, size_t)>(
            [this](auto handler, auto buffers) mutable {
                this->do_read_some(std::move(buffers), std::move(handler));
            },
            token,
            std::forward<MutableBufferSequence>(buffers)
        );
    }

    template <typename ConstBufferSequence, typename CompletionToken>
    auto async_write_some(ConstBufferSequence&& buffers, CompletionToken&& token) {
        return boost::asio::async_initiate<CompletionToken, void(error_code, size_t)>(
            [this](auto handler, auto buffers) mutable {
                this->do_write_some(std::move(buffers), std::move(handler));
            },
            token,
            std::forward<ConstBufferSequence>(buffers)
        );
    }

    void shutdown(boost::asio::ip::tcp::socket::shutdown_type what, error_code& ec);
    void close();
    bool is_open() const noexcept;

private:
    class impl;
    std::shared_ptr<impl> impl_;
    friend class tun_tcp_acceptor;
};
```

#### 7.2 `tun_tcp_acceptor`（TCP 连接监听器）

`async_accept` 在收到客户端 SYN 时触发完成回调，此时连接处于 `SYN_RCVD` 状态（尚未建立）；
三次握手由应用在领取流后通过 `tun_tcp_socket::accept()`/`reject()` 决定（未显式调用时首次读写隐式批准）。

```cpp
class tun_tcp_acceptor {
public:
    explicit tun_tcp_acceptor(tunio& engine);

    template <typename CompletionToken>
    auto async_accept(tun_tcp_socket& peer, CompletionToken&& token) {
        return boost::asio::async_initiate<CompletionToken, void(error_code)>(
            [this, &peer](auto handler) {
                this->do_accept(peer, std::move(handler));
            },
            token
        );
    }
private:
    tunio& engine_;
};
```

#### 7.3 `tun_udp_socket`（UDP 数据报套接字）

```cpp
class tun_udp_socket {
public:
    using executor_type = boost::asio::any_io_executor;

    explicit tun_udp_socket(executor_type ex);
    ~tun_udp_socket();

    tun_udp_socket(tun_udp_socket&&) noexcept;
    tun_udp_socket& operator=(tun_udp_socket&&) noexcept;

    executor_type get_executor() const noexcept;

    // 异步接收一个完整的数据报；sender 输出该数据报的目标远端端点
    // （发送者恒为会话绑定的客户端），失败路径不保证填充
    template <typename MutableBufferSequence, typename CompletionToken>
    auto async_receive_from(MutableBufferSequence&& buffers,
                            boost::asio::ip::udp::endpoint& sender,
                            CompletionToken&& token);

    // 异步发送一个完整的数据报：构造并注入 src=remote → dst=客户端 的响应
    // 数据报
    template <typename ConstBufferSequence, typename CompletionToken>
    auto async_send_to(const boost::asio::ip::udp::endpoint& remote,
                       ConstBufferSequence&& buffers, CompletionToken&& token);

    // 设置会话空闲超时
    void set_timeout(std::chrono::seconds timeout);
    void close();
    bool is_open() const noexcept;

private:
    class impl;
    std::shared_ptr<impl> impl_;
    friend class tun_udp_acceptor;
};
```

#### 7.4 `tun_udp_acceptor`（UDP 新会话监听器）

`async_accept` 在引擎检测到新的 UDP 客户端三元组流量时触发完成回调，并将新创建的 `tun_udp_socket` 传递给调用者。

```cpp
class tun_udp_acceptor {
public:
    explicit tun_udp_acceptor(tunio& engine);

    template <typename CompletionToken>
    auto async_accept(tun_udp_socket& peer, CompletionToken&& token) {
        return boost::asio::async_initiate<CompletionToken, void(error_code)>(
            [this, &peer](auto handler) {
                this->do_accept(peer, std::move(handler));
            },
            token
        );
    }
private:
    tunio& engine_;
};
```

---

### 8. 错误处理与网络诊断支持

#### 8.1 后端连接失败处理

当引擎尝试连接后端代理失败时（如 `ECONNREFUSED`）：
- 由于应用已批准握手（`accept()`/首次读写回复 SYN-ACK）且客户端已进入 ESTABLISHED 状态，
  **绝不能**静默关闭连接。
- 引擎**必须**向客户端发送 TCP **RST** 包，强制客户端立即中断连接。

#### 8.2 ICMP Echo 响应

引擎自动响应发往自身虚拟 IP 地址的 ICMP/ICMPv6 Echo Request（Ping）：
- IPv4：识别 IP 协议字段为 1（ICMP）且 Type=8，构造 Type=0（Echo Reply）并计算 ICMP 校验和。
- IPv6：识别 Next Header 为 58（ICMPv6）且 Type=128，构造 Type=129（Echo Reply），
  校验和必须包含 IPv6 伪头部（源/目的地址、载荷长度、Next Header=58）。
- 回包不经过上层应用层，直接通过 `tun_device::async_write_packet` 写回；
  仅响应发往引擎本地虚拟 IP 的 Echo Request。

#### 8.3 统计接口

```cpp
struct engine_stats {
    std::atomic<uint64_t> rx_packets;
    std::atomic<uint64_t> tx_packets;
    std::atomic<uint64_t> rx_dropped;
    std::atomic<uint64_t> rx_ooo;        // 乱序缓存段数
    std::atomic<uint64_t> tcp_connections;
    std::atomic<uint64_t> udp_sessions;
    std::atomic<uint64_t> icmp_replies;
};

class tunio {
public:
    const engine_stats& stats() const noexcept;
};
```

---

#### 8.4 双栈（IPv4/IPv6）数据通路

- **收包解析**：`on_read` 依据报文首字节的版本字段（4/6）读取对应的长度字段
  （IPv4 `total_len` / IPv6 `40 + payload_len`）逐包拆解，双栈报文可混合到达。
- **五元组索引**：`five_tuple.family` 参与哈希与相等比较，IPv4 与 IPv6 会话
  天然隔离，互不冲突。
- **发送构造**：TCP/UDP 引擎按流所属地址族构建 IPv4/IPv6 头部；IPv4 计算头部
  校验和，IPv6 无头部校验和但 TCP/UDP/ICMPv6 伪头部校验和强制有效。
- **IPv6 地址配置**：Linux 自主打开模式下通过 netlink（`RTM_NEWADDR`）为 TUN
  接口配置 IPv6 地址与前缀长度（`device_config.ipv6` / `ipv6_prefix_len`）。
- **SOCKS5 代理**：`tun2socks` 示例中 SOCKS5 CONNECT 与 UDP ASSOCIATE 均支持
  IPv6 目标（ATYP=4），引擎的 `original_destination()` 按地址族返回 IPv6
  端点，UDP 会话的远端端点随 `async_receive_from` 的 sender 出参返回。

### 9. 配置与资源限制

```cpp
struct tun_config {
    // ---- 网络配置 ----
    std::string dev_name;
    std::string ipv4_addr;
    std::string netmask;
    std::string ipv6_addr;             // 可选 IPv6 地址，如 "fd00::1"
    uint8_t ipv6_prefix_len = 64;      // IPv6 前缀长度
    size_t mtu = 1500;

    // ---- 外部句柄注入 ----
    native_handle_type external_handle = invalid_native_handle;
    size_t external_mtu = 1500;

    // ---- 资源上限 ----
    size_t max_tcp_flows = 65536;
    size_t max_udp_flows = 65536;
    size_t max_rx_queue_per_flow = 8 * 1024 * 1024;
    size_t max_total_buffer = 512 * 1024 * 1024;

    // ---- 超时策略 ----
    std::chrono::seconds udp_idle_timeout{30};
    std::chrono::seconds tcp_time_wait_timeout{10};
    std::chrono::seconds tcp_accept_timeout{30}; // 兜底：已建立但未被 async_accept 领取的连接超时
    std::chrono::seconds tcp_syn_timeout{30};    // 未完成握手的半开连接超时
    std::chrono::seconds tcp_close_timeout{30};  // 关闭流程（FIN 挥手）未完成时的强制清理超时

};
```

**初始化逻辑**：
1. 若 `external_handle != invalid_native_handle`，引擎调用 `tun_device::assign(external_handle, external_mtu, ec)`。
2. 否则，引擎调用 `tun_device::open(cfg)`。
3. 所有内部表、定时器及 Strand 在构造时初始化。

**资源上限说明**：
- `max_rx_queue_per_flow` 限制每条 TCP 连接的接收队列与每条 UDP 会话的
  数据报队列字节数。TCP 发送采用单写模型，发送背压由"每流单写 +
  设备写完成回调"驱动，不再按排队字节数拒绝写入。
- `max_total_buffer` 为跨 TCP/UDP 接收队列的全局缓冲记账上限；发送侧
  数据由应用缓冲持有（引擎只引用不拷贝），不占用该记账。
- 半开连接（`SYN_RCVD` / `SYN_ACK_SENT`）在 `tcp_syn_timeout` 后清理；关闭流程
  （`FIN_WAIT_1` / `FIN_WAIT_2` / `LAST_ACK`）在 `tcp_close_timeout` 后
  强制清理。

---

### 10. 完整使用示例（C++20 Coroutine）

> 除引擎级示例（本节）外，`examples/tun_packet.cpp` 是**设备层**原始 IP 包
> 中继/打印示例：直接使用 `tun_device` + `ip_packet`，不依赖引擎，读取并
> 打印 TCP/UDP/ICMP 协议详情，`--echo` 时原样写回设备实现包回环中继。

#### 10.1 TCP 全双工数据泵

```cpp
#include <boost/asio.hpp>
#include <iostream>
#include <memory>

namespace net = boost::asio;

net::awaitable<void> bidirectional_bridge(tun_tcp_socket client, net::ip::tcp::socket proxy) {
    auto executor = co_await net::this_coro::executor;
    auto client_ptr = std::make_shared<tun_tcp_socket>(std::move(client));
    auto proxy_ptr  = std::make_shared<net::ip::tcp::socket>(std::move(proxy));

    net::co_spawn(executor, [client_ptr, proxy_ptr]() -> net::awaitable<void> {
        std::array<char, 8192> buf;
        try {
            for (;;) {
                size_t n = co_await client_ptr->async_read_some(net::buffer(buf), net::use_awaitable);
                co_await net::async_write(*proxy_ptr, net::buffer(buf, n), net::use_awaitable);
            }
        } catch (...) { }
    }, net::detached);

    net::co_spawn(executor, [client_ptr, proxy_ptr]() -> net::awaitable<void> {
        std::array<char, 8192> buf;
        try {
            for (;;) {
                size_t n = co_await proxy_ptr->async_read_some(net::buffer(buf), net::use_awaitable);
                co_await net::async_write(*client_ptr, net::buffer(buf, n), net::use_awaitable);
            }
        } catch (...) { }
    }, net::detached);

    co_return;
}

net::awaitable<void> tcp_listener(tunio& engine) {
    tun_tcp_acceptor acceptor(engine);
    auto executor = co_await net::this_coro::executor;

    for (;;) {
        tun_tcp_socket client(executor);
        co_await acceptor.async_accept(client, net::use_awaitable);

        auto dest = client.original_destination();
        net::ip::tcp::socket proxy(executor);
        co_await proxy.async_connect(dest, net::use_awaitable);

        net::co_spawn(executor, bidirectional_bridge(std::move(client), std::move(proxy)), net::detached);
    }
}
```

#### 10.2 UDP 回显服务示例

```cpp
net::awaitable<void> udp_echo_handler(tun_udp_socket session) {
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

net::awaitable<void> udp_listener(tunio& engine) {
    tun_udp_acceptor acceptor(engine);
    auto executor = co_await net::this_coro::executor;

    for (;;) {
        tun_udp_socket session(executor);
        co_await acceptor.async_accept(session, net::use_awaitable);
        net::co_spawn(executor, udp_echo_handler(std::move(session)), net::detached);
    }
}
```

#### 10.3 主函数

```cpp
int main() {
    net::io_context io_context(1);
    tunio engine(io_context);

    tun_config config;
    config.dev_name = "tun0";
    config.ipv4_addr = "10.0.0.1";
    config.netmask = "255.255.255.0";
    config.mtu = 1500;

    boost::system::error_code ec;
    if (!engine.open(config, ec)) {
        std::cerr << "Failed to open TUN: " << ec.message() << std::endl;
        return -1;
    }

    net::co_spawn(io_context, tcp_listener(engine), net::detached);
    net::co_spawn(io_context, udp_listener(engine), net::detached);
    io_context.run();
    return 0;
}
```

---

### 11. 总结

本设计文档定义了一个**极简、高性能且设备管理方式灵活**的用户态 TUN 网络引擎。其核心特征在于：

1. **统一的双模设备管理**：支持引擎自主打开设备与外部句柄注入，句柄注入时需显式指定 MTU 以确保 `tun_device` 返回正确参数。

2. **完全 Asio 风格的异步接口**：`tun_device` 的 I/O 及所有公开 API 均采用 `CompletionToken` 与 `async_initiate`，与 `use_awaitable`、`use_future` 等无缝协作。

3. **TCP 与 UDP 的对称抽象**：TCP 提供 `tun_tcp_socket`/`tun_tcp_acceptor`，UDP 提供 `tun_udp_socket`/`tun_udp_acceptor`，两者均遵循 Asio 的命名与行为习惯，降低学习成本。

4. **极低的协议开销**：接收方向缓存乱序段并按序交付，缺失段补齐后批量确认，避免人为乱序触发对端快速重传；发送方向仅以 RTO 计时器 + 重读用户缓冲实现重传，无重传队列与额外拷贝，实现低 CPU 占用的高速转发。

5. **生产级线程安全**：所有内部状态串行化访问——单线程模式直接运行于 io 线程（省去 Strand 派发开销），多线程模式由 Strand 串行化，支持多线程 `io_context` 运行，均无锁竞争。

该设计尤其适用于对转发延迟敏感、网络环境相对稳定或上层应用已具备重试机制的代理场景。
