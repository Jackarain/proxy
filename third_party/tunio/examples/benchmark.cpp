// benchmark.cpp —— handler 包装方案的分配与吞吐基准
//
// 用于对比异步接口 handler 传递/存储方案（std::function 包装 vs
// any_completion_handler 类型擦除）的每次操作堆分配次数与吞吐。
// 通过全局 operator new 计数统计分配；基于 socketpair 注入的虚拟
// TUN 设备运行引擎，无需 root 权限。
//
// 使用：cmake 构建后运行 examples/benchmark；对比不同版本时可用
// -DBENCH_TAG=\"after\" 之类的宏标记输出，并保证两次运行使用相同
// 的构建配置（Release/Debug 一致）。
#include "tunio/tun_tcp_acceptor.hpp"
#include "tunio/tun_config.hpp"
#include "tunio/tun_tcp_socket.hpp"
#include "tunio/tun_udp_acceptor.hpp"
#include "tunio/tun_udp_socket.hpp"
#include "tunio/tunio.hpp"

#include <boost/asio.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace net = boost::asio;
using namespace tunio;

#ifndef BENCH_TAG
#define BENCH_TAG "unknown"
#endif

// ---- 全局分配计数（仅测量区间内生效）----
static std::atomic<long long> *g_alloc = nullptr;
static std::atomic<long long> g_alloc_store{0};

void *operator new(std::size_t n)
{
    if (g_alloc)
        g_alloc->fetch_add(1, std::memory_order_relaxed);
    if (void *p = std::malloc(n))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t n)
{
    return ::operator new(n);
}
void operator delete(void *p) noexcept
{
    std::free(p);
}
void operator delete[](void *p) noexcept
{
    std::free(p);
}
void operator delete(void *p, std::size_t) noexcept
{
    std::free(p);
}
void operator delete[](void *p, std::size_t) noexcept
{
    std::free(p);
}

// ---- 独立校验和 ----
inline uint32_t raw_sum(const uint8_t *d, size_t n)
{
    uint32_t sum = 0;
    size_t i = 0;
    for (; i + 1 < n; i += 2) {
        sum += static_cast<uint16_t>((d[i] << 8) | d[i + 1]);
    }
    if (i < n) {
        sum += static_cast<uint16_t>(d[i] << 8);
    }
    return sum;
}
inline uint32_t fold_sum(uint32_t sum)
{
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return sum;
}
inline uint16_t csum16(const uint8_t *d, size_t n, uint32_t init = 0)
{
    return static_cast<uint16_t>(~fold_sum(raw_sum(d, n) + init));
}
inline uint16_t big16(const uint8_t *p)
{
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}
inline uint32_t big32(const uint8_t *p)
{
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}

// ---- 报文构造 ----
inline std::vector<uint8_t> make_ipv4(uint32_t src, uint32_t dst, uint8_t proto,
                                      const std::vector<uint8_t> &payload)
{
    std::vector<uint8_t> pkt(20, 0);
    pkt[0] = 0x45;
    const uint16_t total = static_cast<uint16_t>(20 + payload.size());
    pkt[2] = static_cast<uint8_t>((total >> 8) & 0xff);
    pkt[3] = static_cast<uint8_t>(total & 0xff);
    pkt[8] = 64;
    pkt[9] = proto;
    uint32_t s = htonl(src), d = htonl(dst);
    std::memcpy(&pkt[12], &s, 4);
    std::memcpy(&pkt[16], &d, 4);
    uint16_t c = csum16(pkt.data(), 20);
    pkt[10] = static_cast<uint8_t>(c >> 8);
    pkt[11] = static_cast<uint8_t>(c & 0xff);
    pkt.insert(pkt.end(), payload.begin(), payload.end());
    return pkt;
}
inline std::vector<uint8_t> make_tcp(uint32_t src, uint32_t dst, uint16_t sport,
                                     uint16_t dport, uint8_t flags,
                                     uint32_t seq, uint32_t ack, uint16_t win,
                                     const std::vector<uint8_t> &data,
                                     bool mss = false)
{
    const size_t hlen = mss ? 24 : 20;
    std::vector<uint8_t> seg(hlen + data.size(), 0);
    seg[0] = static_cast<uint8_t>(sport >> 8);
    seg[1] = static_cast<uint8_t>(sport & 0xff);
    seg[2] = static_cast<uint8_t>(dport >> 8);
    seg[3] = static_cast<uint8_t>(dport & 0xff);
    uint32_t s = htonl(seq), a = htonl(ack);
    std::memcpy(&seg[4], &s, 4);
    std::memcpy(&seg[8], &a, 4);
    seg[12] = static_cast<uint8_t>((mss ? 6 : 5) << 4);
    seg[13] = flags;
    seg[14] = static_cast<uint8_t>(win >> 8);
    seg[15] = static_cast<uint8_t>(win & 0xff);
    if (mss) {
        seg[20] = 2;
        seg[21] = 4;
        seg[22] = 0x05;
        seg[23] = 0xb4;
    }
    if (!data.empty()) {
        std::memcpy(seg.data() + hlen, data.data(), data.size());
    }
    const uint32_t pseudo = (src >> 16) + (src & 0xffff) + (dst >> 16) +
                            (dst & 0xffff) + 6 + seg.size();
    uint16_t c = csum16(seg.data(), seg.size(), pseudo);
    seg[16] = static_cast<uint8_t>(c >> 8);
    seg[17] = static_cast<uint8_t>(c & 0xff);
    return make_ipv4(src, dst, 6, seg);
}
inline std::vector<uint8_t> make_udp(uint32_t src, uint32_t dst, uint16_t sport,
                                     uint16_t dport,
                                     const std::vector<uint8_t> &data)
{
    std::vector<uint8_t> seg(8 + data.size(), 0);
    seg[0] = static_cast<uint8_t>(sport >> 8);
    seg[1] = static_cast<uint8_t>(sport & 0xff);
    seg[2] = static_cast<uint8_t>(dport >> 8);
    seg[3] = static_cast<uint8_t>(dport & 0xff);
    const uint16_t ulen = static_cast<uint16_t>(seg.size());
    seg[4] = static_cast<uint8_t>(ulen >> 8);
    seg[5] = static_cast<uint8_t>(ulen & 0xff);
    if (!data.empty()) {
        std::memcpy(seg.data() + 8, data.data(), data.size());
    }
    const uint32_t pseudo = (src >> 16) + (src & 0xffff) + (dst >> 16) +
                            (dst & 0xffff) + 17 + seg.size();
    uint16_t c = csum16(seg.data(), seg.size(), pseudo);
    seg[6] = static_cast<uint8_t>(c >> 8);
    seg[7] = static_cast<uint8_t>(c & 0xff);
    return make_ipv4(src, dst, 17, seg);
}

// 修改 seq/ack 字段后重算 IPv4 TCP 校验和（seg 为完整 IP 包，源/目的为 host
// order）
inline void refresh_tcp_checksum(std::vector<uint8_t> &seg, uint32_t src,
                                 uint32_t dst)
{
    seg[36] = 0;
    seg[37] = 0;
    const size_t tcp_len = seg.size() - 20;
    const uint32_t pseudo = (src >> 16) + (src & 0xffff) + (dst >> 16) +
                            (dst & 0xffff) + 6 + tcp_len;
    const uint16_t c = csum16(seg.data() + 20, tcp_len, pseudo);
    seg[36] = static_cast<uint8_t>(c >> 8);
    seg[37] = static_cast<uint8_t>(c & 0xff);
}

// ---- 虚拟 TUN 设备 ----
class fake_device
{
public:
    fake_device()
    {
        int sv[2];
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) {
            throw std::runtime_error("socketpair failed");
        }
        fd_ = sv[0];
        inject_fd_ = sv[1];
    }
    ~fake_device()
    {
        ::close(fd_);
    }
    int inject_fd() const
    {
        return inject_fd_;
    }
    void send(const std::vector<uint8_t> &pkt)
    {
        size_t off = 0;
        while (off < pkt.size()) {
            const ssize_t n = ::write(fd_, pkt.data() + off, pkt.size() - off);
            if (n <= 0) {
                throw std::runtime_error("fake_device send failed");
            }
            off += static_cast<size_t>(n);
        }
    }
    // 读取一个完整 IP 包（复用 out/stash_ 容量，测量区间内不产生分配）
    bool read_packet(std::vector<uint8_t> &out, int timeout_ms = 3000)
    {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds(timeout_ms);
        for (;;) {
            while (stash_.size() >= 4) {
                size_t total = 0;
                switch (stash_[0] >> 4) {
                case 4:
                    if (stash_.size() < 20)
                        break;
                    total = static_cast<size_t>((stash_[2] << 8) | stash_[3]);
                    break;
                case 6:
                    if (stash_.size() < 40)
                        break;
                    total =
                        40 + static_cast<size_t>((stash_[4] << 8) | stash_[5]);
                    break;
                default:
                    break;
                }
                if (total < 20 || stash_.size() < total)
                    break;
                out.assign(stash_.begin(), stash_.begin() + total);
                stash_.erase(stash_.begin(), stash_.begin() + total);
                return true;
            }
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
                return false;
            const int remaining = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                                      now)
                    .count());
            struct pollfd pfd{fd_, POLLIN, 0};
            const int r = ::poll(&pfd, 1, remaining);
            if (r <= 0)
                return false;
            uint8_t buf[65536];
            const ssize_t n = ::read(fd_, buf, sizeof(buf));
            if (n <= 0)
                return false;
            stash_.insert(stash_.end(), buf, buf + n);
        }
    }

private:
    int fd_ = -1;
    int inject_fd_ = -1;
    std::vector<uint8_t> stash_;
};

// ---- 引擎环境 ----
struct engine_env
{
    net::io_context io;
    ::tunio::tunio engine;
    fake_device dev;
    std::thread thread;
    net::executor_work_guard<net::io_context::executor_type> guard;

    engine_env()
        : engine(io)
        , guard(net::make_work_guard(io))
    {
        tun_config cfg;
        cfg.external_handle = dev.inject_fd();
        cfg.external_mtu = 1500;
        cfg.ipv4_addr = "10.0.0.1";
        cfg.netmask = "255.255.255.0";
        cfg.ipv6_addr = "fd00::1";
        cfg.ipv6_prefix_len = 64;
        cfg.udp_idle_timeout = std::chrono::seconds(30);
        boost::system::error_code ec;
        if (!engine.open(cfg, ec)) {
            throw std::runtime_error("engine open failed: " + ec.message());
        }
        thread = std::thread([this] { io.run(); });
    }
    ~engine_env()
    {
        engine.close();
        guard.reset();
        if (thread.joinable())
            thread.join();
    }
};

// ---- 完成闩 ----
struct latch
{
    std::mutex m;
    std::condition_variable cv;
    bool done = false;
    void post()
    {
        std::lock_guard<std::mutex> lk(m);
        done = true;
        cv.notify_one();
    }
    void wait()
    {
        std::unique_lock<std::mutex> lk(m);
        if (!cv.wait_for(lk, std::chrono::seconds(5), [&] { return done; })) {
            throw std::runtime_error("latch wait timeout");
        }
        done = false;
    }
};

// ---- 测量 ----
template <typename F>
void measure(const char *name, long long n, long long warmup, F &&f)
{
    for (long long i = 0; i < warmup; i++)
        f();
    g_alloc_store.store(0);
    const auto t0 = std::chrono::steady_clock::now();
    for (long long i = 0; i < n; i++)
        f();
    const auto t1 = std::chrono::steady_clock::now();
    const double sec = std::chrono::duration<double>(t1 - t0).count();
    const long long alloc = g_alloc_store.load();
    std::printf("%-16s %8lld ops  %8.1f ms  %10.0f ops/s  %7.2f alloc/op\n",
                name, n, sec * 1000.0, static_cast<double>(n) / sec,
                static_cast<double>(alloc) / static_cast<double>(n));
}

// ---- 常量 ----
constexpr uint32_t CLIENT_IP = 0x0a000002; // 10.0.0.2
constexpr uint32_t DEST_IP = 0x08080808;   // 8.8.8.8
constexpr uint16_t CLIENT_PORT = 12345;
constexpr uint16_t DEST_PORT = 80;

// ---- TCP 握手：返回已建立的 stream ----
tun_tcp_socket establish_tcp(engine_env &env, tun_tcp_acceptor &acc,
                         uint32_t &engine_iss)
{
    tun_tcp_socket peer(env.io.get_executor());
    latch done;
    acc.async_accept(peer, [&](boost::system::error_code) { done.post(); });
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x02,
                          1000, 0, 65535, {}, true));
    std::vector<uint8_t> synack;
    if (!env.dev.read_packet(synack)) {
        throw std::runtime_error("no SYN-ACK");
    }
    engine_iss = big32(synack.data() + 24);
    env.dev.send(make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x10,
                          1001, engine_iss + 1, 65535, {}));
    done.wait();
    return peer;
}

// ---- TCP 读路径 ----
static void bench_tcp_read(long long n, long long warmup)
{
    engine_env env;
    tun_tcp_acceptor acc(env.engine);
    uint32_t engine_iss = 0;
    tun_tcp_socket peer = establish_tcp(env, acc, engine_iss);
    latch done;

    char buf[64];
    auto seg = make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x18, 1001,
                        engine_iss + 1, 65535, std::vector<uint8_t>(16, 0x61));
    uint32_t seq = 1001;
    measure("tcp_read_some", n, warmup, [&] {
        peer.async_read_some(net::buffer(buf), [&](boost::system::error_code,
                                                   size_t) { done.post(); });
        const uint32_t s = htonl(seq);
        seq += 16;
        std::memcpy(seg.data() + 24, &s, 4);
        refresh_tcp_checksum(seg, CLIENT_IP, DEST_IP);
        env.dev.send(seg);
        done.wait();
    });
}

// ---- TCP 写路径 ----
static void bench_tcp_write(long long n, long long warmup)
{
    engine_env env;
    tun_tcp_acceptor acc(env.engine);
    uint32_t engine_iss = 0;
    tun_tcp_socket peer = establish_tcp(env, acc, engine_iss);
    latch done;

    auto ack_seg = make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x10,
                            1001, 0, 65535, {});
    std::vector<uint8_t> pkt;
    char wbuf[16];
    std::memset(wbuf, 0x62, sizeof(wbuf));
    uint32_t client_ack = engine_iss + 1;
    measure("tcp_write_some", n, warmup, [&] {
        peer.async_write_some(net::buffer(wbuf), [&](boost::system::error_code,
                                                     size_t) { done.post(); });
        done.wait();
        if (!env.dev.read_packet(pkt)) {
            throw std::runtime_error("no tcp data segment");
        }
        const uint32_t pkt_seq = big32(pkt.data() + 24);
        const uint32_t pkt_len =
            big16(pkt.data() + 2) - 40; // IP total - 20(IP) - 20(TCP)
        client_ack = pkt_seq + pkt_len;
        const uint32_t a = htonl(client_ack);
        std::memcpy(ack_seg.data() + 28, &a, 4);
        refresh_tcp_checksum(ack_seg, CLIENT_IP, DEST_IP);
        env.dev.send(ack_seg);
    });
}

// ---- UDP 收发路径 ----
static void bench_udp(long long n, long long warmup)
{
    engine_env env;
    tun_udp_acceptor acc(env.engine);
    latch done;
    tun_udp_socket sock(env.io.get_executor());
    acc.async_accept(sock, [&](boost::system::error_code) { done.post(); });
    env.dev.send(make_udp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT,
                          std::vector<uint8_t>(16, 0x63)));
    done.wait();

    auto udp_seg = make_udp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT,
                            std::vector<uint8_t>(16, 0x63));
    std::vector<uint8_t> pkt;
    char buf[64];
    char sbuf[16];
    std::memset(sbuf, 0x64, sizeof(sbuf));
    net::ip::udp::endpoint sender;
    const auto remote =
        net::ip::udp::endpoint(net::ip::make_address_v4("8.8.8.8"), DEST_PORT);

    measure("udp_receive", n, warmup, [&] {
        sock.async_receive_from(net::buffer(buf), sender,
                                [&](boost::system::error_code, size_t) {
                                    done.post();
                                });
        env.dev.send(udp_seg);
        done.wait();
    });

    measure("udp_send", n, warmup, [&] {
        sock.async_send_to(remote, net::buffer(sbuf),
                           [&](boost::system::error_code, size_t) {
                               done.post();
                           });
        done.wait();
        if (!env.dev.read_packet(pkt)) {
            throw std::runtime_error("no udp packet");
        }
    });
}

// ---- accept 路径 ----
static void bench_accept(long long n, long long warmup)
{
    engine_env env;
    tun_tcp_acceptor acc(env.engine);
    auto syn = make_tcp(CLIENT_IP, DEST_IP, CLIENT_PORT, DEST_PORT, 0x02, 1000,
                        0, 65535, {}, true);
    std::vector<uint8_t> synack;
    std::vector<tun_tcp_socket> conns;
    conns.reserve(static_cast<size_t>(n + warmup));
    latch done;
    uint16_t cport = CLIENT_PORT;
    measure("accept", n, warmup, [&] {
        conns.emplace_back(env.io.get_executor());
        tun_tcp_socket &peer = conns.back();
        acc.async_accept(peer, [&](boost::system::error_code) { done.post(); });
        const uint16_t p = htons(cport++);
        std::memcpy(syn.data() + 20, &p, 2);
        refresh_tcp_checksum(syn, CLIENT_IP, DEST_IP);
        env.dev.send(syn);
        if (!env.dev.read_packet(synack)) {
            throw std::runtime_error("no SYN-ACK");
        }
        const uint32_t engine_iss = big32(synack.data() + 24);
        auto ack = make_tcp(CLIENT_IP, DEST_IP, cport - 1, DEST_PORT, 0x10,
                            1001, engine_iss + 1, 65535, {});
        env.dev.send(ack);
        done.wait();
    });
}

int main()
{
    g_alloc = &g_alloc_store;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("=== handler 包装 benchmark tag=%s pid=%d ===\n", BENCH_TAG,
                static_cast<int>(::getpid()));
    std::printf("[bench] tcp_read start\n");
    bench_tcp_read(50000, 3000);
    std::printf("[bench] tcp_write start\n");
    bench_tcp_write(50000, 3000);
    std::printf("[bench] udp start\n");
    bench_udp(50000, 3000);
    std::printf("[bench] accept start\n");
    bench_accept(5000, 300);
    return 0;
}
