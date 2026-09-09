#define _POSIX_C_SOURCE 200809L

// tun2socks.c：基于 tunio C API（bindings/c，tunio_c 共享库）的 SOCKS5
// 透明代理示例（对应 examples/cpp/tun2socks.cpp 与 examples/python/tun2socks.py）。
//
// 功能：
//   - TCP：引擎终止虚拟连接，应用层经 SOCKS5 CONNECT 连到代理后全双工桥接；
//   - UDP：引擎维护 NAT 会话，应用层经 SOCKS5 UDP ASSOCIATE 中继转发；
//   - 后端连接失败时向客户端发送 RST。
//
// 使用阻塞风格 C API：引擎内部维护 io_context 与 io 线程推进协议状态机，
// accept/recv/send 等调用在调用线程阻塞至完成；本示例为每个连接/会话建立
// 独立工作线程与双向泵线程，数据通路方向与 C++/Python 示例一致。
//
// 用法示例（构建需开启 TUNIO_BUILD_C 或 TUNIO_BUILD_PYTHON）：
//   sudo ./build/bin/tun2socks_c --tun tun0 --ip 10.0.0.1
//       --netmask 255.255.255.0 --proxy 127.0.0.1:1080
//
// 需要 root（或 CAP_NET_ADMIN）创建 TUN 设备；也可 --inject-fd 注入外部
// 已打开的 TUN 文件描述符。
#include "tunio/c_api.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

enum
{
    BUF_SIZE = 65536,   // 单次泵数据缓冲
    FRAME_EXTRA = 512,  // SOCKS5 UDP 帧头预留（RSV + ATYP + 最大域名 + 端口）
    ADDR_BUF = 64,      // 地址文本缓冲（含 IPv6 最长文本与域名）
    MAX_QUEUES = 256
};

static volatile sig_atomic_t g_stop = 0;
static atomic_int g_active_bridges = 0;  // 仍在运行的桥接工作线程数

struct options
{
    char dev_name[64];
    char ipv4_addr[64];
    char netmask[64];
    char ipv6_addr[64];
    uint8_t ipv6_prefix_len;
    uint32_t mtu;
    uint32_t num_queues;
    char proxy_host[256];
    uint16_t proxy_port;
    int udp;          // 1 启用 UDP 转发
    int utun_prefix;  // 注入的 fd 是否为 macOS utun
    int inject_fd;    // 注入的外部 TUN 文件描述符，< 0 表示不注入
    int threads;      // io_context 线程数
};

static void set_error_msg(char* buf, size_t size, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, size, fmt, ap);
    va_end(ap);
}

static void usage(const char* prog)
{
    fprintf(stderr,
        "用法: %s [选项]\n"
        "  --tun <name>           TUN 设备名（默认 tun0）\n"
        "  --ip <addr>            本地虚拟 IP（默认 10.0.0.1）\n"
        "  --netmask <mask>       子网掩码（默认 255.255.255.0）\n"
        "  --ip6 <addr>           本地虚拟 IPv6 地址（可选，如 fd00::1）\n"
        "  --ip6-prefix <len>     IPv6 前缀长度（默认 64）\n"
        "  --mtu <bytes>          MTU（默认 1500）\n"
        "  --queues <n>           Linux TUN 多队列数（IFF_MULTI_QUEUE，默认 1）\n"
        "  --proxy <host:port>    SOCKS5 代理地址（默认 127.0.0.1:1080）\n"
        "  --utun-prefix          注入的 fd 为 macOS utun（读写带 4 字节家族前缀）\n"
        "  --no-udp               禁用 UDP 转发\n"
        "  --inject-fd <fd>       注入外部已打开的 TUN 文件描述符\n"
        "  --threads <n>          io_context 线程数（默认 1）\n",
        prog);
}

static int parse_u32(const char* s, uint32_t* out)
{
    char* end = NULL;
    unsigned long v = strtoul(s, &end, 10);
    if (!s[0] || !end || *end != '\0')
        return -1;
    *out = (uint32_t)v;
    return 0;
}

static int parse_proxy(const char* value, char* host, size_t host_size,
    uint16_t* port)
{
    const char* colon = strrchr(value, ':');
    char host_part[256];
    uint32_t p = 0;
    if (!colon)
        return -1;
    if ((size_t)(colon - value) >= sizeof(host_part))
        return -1;
    memcpy(host_part, value, (size_t)(colon - value));
    host_part[colon - value] = '\0';
    if (host_part[0] == '\0' || parse_u32(colon + 1, &p) < 0 ||
        p < 1 || p > 65535)
        return -1;
    snprintf(host, host_size, "%s", host_part);
    *port = (uint16_t)p;
    return 0;
}

static int parse_args(int argc, char** argv, struct options* opt)
{
    memset(opt, 0, sizeof(*opt));
    snprintf(opt->dev_name, sizeof(opt->dev_name), "%s", "tun0");
    snprintf(opt->ipv4_addr, sizeof(opt->ipv4_addr), "%s", "10.0.0.1");
    snprintf(opt->netmask, sizeof(opt->netmask), "%s", "255.255.255.0");
    snprintf(opt->proxy_host, sizeof(opt->proxy_host), "%s", "127.0.0.1");
    opt->ipv6_prefix_len = 64;
    opt->mtu = 1500;
    opt->num_queues = 1;
    opt->proxy_port = 1080;
    opt->udp = 1;
    opt->inject_fd = -1;
    opt->threads = 1;

    for (int i = 1; i < argc; ++i)
    {
        const char* arg = argv[i];
        const char* value = NULL;
        if (i + 1 < argc)
            value = argv[i + 1];

        if (strcmp(arg, "--tun") == 0 ||
            strcmp(arg, "--ip") == 0 ||
            strcmp(arg, "--netmask") == 0 ||
            strcmp(arg, "--ip6") == 0 ||
            strcmp(arg, "--ip6-prefix") == 0 ||
            strcmp(arg, "--mtu") == 0 ||
            strcmp(arg, "--queues") == 0 ||
            strcmp(arg, "--proxy") == 0 ||
            strcmp(arg, "--inject-fd") == 0 ||
            strcmp(arg, "--threads") == 0)
        {
            if (!value)
            {
                fprintf(stderr, "错误: %s 缺少参数\n", arg);
                return -1;
            }
            ++i;
        }

        uint32_t num = 0;
        if (strcmp(arg, "--tun") == 0)
        {
            snprintf(opt->dev_name, sizeof(opt->dev_name), "%s", value);
        }
        else if (strcmp(arg, "--ip") == 0)
        {
            snprintf(opt->ipv4_addr, sizeof(opt->ipv4_addr), "%s", value);
        }
        else if (strcmp(arg, "--netmask") == 0)
        {
            snprintf(opt->netmask, sizeof(opt->netmask), "%s", value);
        }
        else if (strcmp(arg, "--ip6") == 0)
        {
            snprintf(opt->ipv6_addr, sizeof(opt->ipv6_addr), "%s", value);
        }
        else if (strcmp(arg, "--ip6-prefix") == 0)
        {
            if (parse_u32(value, &num) < 0)
                goto bad_value;
            opt->ipv6_prefix_len = (uint8_t)num;
        }
        else if (strcmp(arg, "--mtu") == 0)
        {
            if (parse_u32(value, &num) < 0)
                goto bad_value;
            opt->mtu = num;
        }
        else if (strcmp(arg, "--queues") == 0)
        {
            if (parse_u32(value, &num) < 0 || num < 1 || num > MAX_QUEUES)
            {
                fprintf(stderr, "错误: --queues 需在 1..%d 之间\n", MAX_QUEUES);
                return -1;
            }
            opt->num_queues = num;
        }
        else if (strcmp(arg, "--proxy") == 0)
        {
            if (parse_proxy(value, opt->proxy_host,
                    sizeof(opt->proxy_host), &opt->proxy_port) < 0)
            {
                fprintf(stderr, "错误: 代理地址需要 host:port 格式: %s\n", value);
                return -1;
            }
        }
        else if (strcmp(arg, "--utun-prefix") == 0)
        {
            opt->utun_prefix = 1;
        }
        else if (strcmp(arg, "--no-udp") == 0)
        {
            opt->udp = 0;
        }
        else if (strcmp(arg, "--inject-fd") == 0)
        {
            if (parse_u32(value, &num) < 0)
                goto bad_value;
            opt->inject_fd = (int)num;
        }
        else if (strcmp(arg, "--threads") == 0)
        {
            if (parse_u32(value, &num) < 0 || num < 1)
            {
                fprintf(stderr, "错误: --threads 必须 >= 1\n");
                return -1;
            }
            opt->threads = (int)num;
        }
        else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
        {
            usage(argv[0]);
            return 1;
        }
        else
        {
            fprintf(stderr, "错误: 未知参数: %s\n", arg);
            return -1;
        }
        continue;
    bad_value:
        fprintf(stderr, "错误: 非法数值: %s %s\n", arg, value);
        return -1;
    }
    return 0;
}

// ---- 基础 socket 工具 ----

static void bridge_begin(void)
{
    atomic_fetch_add(&g_active_bridges, 1);
}

static void bridge_end(void)
{
    atomic_fetch_sub(&g_active_bridges, 1);
}

static int send_all(int fd, const void* data, size_t len)
{
    const unsigned char* p = (const unsigned char*)data;
    while (len > 0)
    {
        ssize_t n = send(fd, p, len, 0);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int recv_exact(int fd, void* data, size_t len)
{
    unsigned char* p = (unsigned char*)data;
    while (len > 0)
    {
        ssize_t n = recv(fd, p, len, 0);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;  // 对端提前关闭
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

// 建立 TCP 连接（host 可为域名或 IP），失败返回 -1 并填充 err。
static int tcp_connect(const char* host, uint16_t port,
    char* err, size_t err_size)
{
    char port_str[8];
    struct addrinfo hints;
    struct addrinfo* res = NULL;
    int fd = -1;
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0)
    {
        set_error_msg(err, err_size, "解析 %s 失败: %s",
            host, gai_strerror(rc));
        return -1;
    }
    for (struct addrinfo* ai = res; ai; ai = ai->ai_next)
    {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0)
            continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }
    if (fd < 0)
        set_error_msg(err, err_size, "连接 %s:%u 失败: %s",
            host, (unsigned)port, strerror(errno));
    freeaddrinfo(res);
    return fd;
}

// 建立 UDP socket 并连接到中继端点（host 可为域名或 IP）。
static int udp_connect(const char* host, uint16_t port,
    char* err, size_t err_size)
{
    char port_str[8];
    struct addrinfo hints;
    struct addrinfo* res = NULL;
    int fd = -1;
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0)
    {
        set_error_msg(err, err_size, "解析 %s 失败: %s",
            host, gai_strerror(rc));
        return -1;
    }
    for (struct addrinfo* ai = res; ai; ai = ai->ai_next)
    {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0)
            continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }
    if (fd < 0)
        set_error_msg(err, err_size, "连接 UDP 中继 %s:%u 失败: %s",
            host, (unsigned)port, strerror(errno));
    freeaddrinfo(res);
    return fd;
}

// ---- SOCKS5 协议 ----

// 编码 SOCKS5 ATYP 地址段（ATYP + ADDR + PORT）：IPv4/IPv6 按二进制，
// 其余按域名（长度 <= 255）。成功返回地址段长度，失败返回 -1。
static int encode_socks_addr(unsigned char* out, size_t cap,
    const char* host, uint16_t port)
{
    struct in_addr a4;
    struct in6_addr a6;
    unsigned char* p = out;
    if (inet_pton(AF_INET, host, &a4) == 1)
    {
        if (cap < 7)
            return -1;
        *p++ = 0x01;
        memcpy(p, &a4, 4);
        p += 4;
    }
    else if (inet_pton(AF_INET6, host, &a6) == 1)
    {
        if (cap < 19)
            return -1;
        *p++ = 0x04;
        memcpy(p, &a6, 16);
        p += 16;
    }
    else
    {
        size_t hlen = strlen(host);
        if (hlen == 0 || hlen > 255 || cap < hlen + 4)
            return -1;
        *p++ = 0x03;
        *p++ = (unsigned char)hlen;
        memcpy(p, host, hlen);
        p += hlen;
    }
    *p++ = (unsigned char)((port >> 8) & 0xff);
    *p++ = (unsigned char)(port & 0xff);
    return (int)(p - out);
}

// 读取并解析 SOCKS5 回复中的 BND.ADDR；bnd_host 可为 NULL（仅消费字节）。
static int read_reply_address(int fd, uint8_t atyp,
    char* bnd_host, size_t bnd_host_size, uint16_t* bnd_port)
{
    uint8_t raw[260];
    size_t fixed = 0;
    int family = AF_UNSPEC;
    if (atyp == 0x01)
    {
        fixed = 4;
        family = AF_INET;
    }
    else if (atyp == 0x04)
    {
        fixed = 16;
        family = AF_INET6;
    }
    else if (atyp == 0x03)
    {
        if (recv_exact(fd, raw, 1) < 0)
            return -1;
        fixed = raw[0];
        family = 0;  // 域名
    }
    else
    {
        return -1;
    }
    if (recv_exact(fd, raw, fixed + 2) < 0)
        return -1;
    if (bnd_host)
    {
        if (family == AF_INET)
            inet_ntop(AF_INET, raw, bnd_host, (socklen_t)bnd_host_size);
        else if (family == AF_INET6)
            inet_ntop(AF_INET6, raw, bnd_host, (socklen_t)bnd_host_size);
        else
            snprintf(bnd_host, bnd_host_size, "%.*s",
                (int)fixed, (const char*)raw);
    }
    if (bnd_port)
    {
        uint16_t net_port;
        memcpy(&net_port, raw + fixed, 2);
        *bnd_port = ntohs(net_port);
    }
    return 0;
}

// 完成 SOCKS5 协商并发送 cmd（1 = CONNECT，3 = UDP ASSOCIATE）；CONNECT 的
// 目标为 target_host:target_port，ASSOCIATE 固定请求 0.0.0.0:0。成功返回 0
// 并输出代理回复的 BND 端点；失败返回 -1 并填充 err。
static int socks5_negotiate(int fd, uint8_t cmd,
    const char* target_host, uint16_t target_port,
    char* bnd_host, size_t bnd_host_size, uint16_t* bnd_port,
    char* err, size_t err_size)
{
    const unsigned char greeting[] = { 0x05, 0x01, 0x00 };
    unsigned char resp[4];
    if (send_all(fd, greeting, sizeof(greeting)) < 0)
    {
        set_error_msg(err, err_size, "发送 SOCKS5 协商失败: %s",
            strerror(errno));
        return -1;
    }
    if (recv_exact(fd, resp, 2) < 0)
    {
        set_error_msg(err, err_size, "读取 SOCKS5 协商响应失败: %s",
            strerror(errno));
        return -1;
    }
    if (resp[0] != 0x05 || resp[1] != 0x00)
    {
        set_error_msg(err, err_size, "代理不接受 NO AUTH");
        return -1;
    }

    unsigned char addr[260];
    int addr_len = 0;
    if (cmd == 0x01)
    {
        addr_len = encode_socks_addr(addr, sizeof(addr), target_host,
            target_port);
        if (addr_len < 0)
        {
            set_error_msg(err, err_size, "目标地址非法: %s", target_host);
            return -1;
        }
        unsigned char head[4] = { 0x05, 0x01, 0x00 };
        if (send_all(fd, head, sizeof(head)) < 0 ||
            send_all(fd, addr, (size_t)addr_len) < 0)
        {
            set_error_msg(err, err_size, "发送 CONNECT 请求失败: %s",
                strerror(errno));
            return -1;
        }
    }
    else
    {
        // UDP ASSOCIATE：BND.ADDR 为 0.0.0.0:0
        const unsigned char req[] = {
            0x05, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        if (send_all(fd, req, sizeof(req)) < 0)
        {
            set_error_msg(err, err_size, "发送 ASSOCIATE 请求失败: %s",
                strerror(errno));
            return -1;
        }
    }

    if (recv_exact(fd, resp, 4) < 0)
    {
        set_error_msg(err, err_size, "读取 SOCKS5 响应头失败: %s",
            strerror(errno));
        return -1;
    }
    if (resp[0] != 0x05)
    {
        set_error_msg(err, err_size, "SOCKS5 版本错误");
        return -1;
    }
    if (resp[1] != 0x00)
    {
        set_error_msg(err, err_size, "SOCKS5 请求失败 (rep=%u)",
            (unsigned)resp[1]);
        return -1;
    }
    if (read_reply_address(fd, resp[3], bnd_host, bnd_host_size,
            bnd_port) < 0)
    {
        set_error_msg(err, err_size, "读取 SOCKS5 BND 地址失败");
        return -1;
    }
    return 0;
}

// ---- TCP 桥接 ----

struct tcp_bridge_arg
{
    tunio_tcp_conn* conn;
    const struct options* opt;
};

struct tcp_pump_arg
{
    tunio_tcp_conn* conn;
    int upstream;
};

// 客户端 -> 上游代理方向：读虚拟连接并写上游 socket。
static void* pump_client_to_upstream(void* arg)
{
    struct tcp_pump_arg* a = (struct tcp_pump_arg*)arg;
    unsigned char buf[BUF_SIZE];
    for (;;)
    {
        int n = tunio_tcp_recv(a->conn, buf, sizeof(buf));
        if (n <= 0)
            break;  // EOF 或错误（引擎关闭等）
        if (send_all(a->upstream, buf, (size_t)n) < 0)
            break;
    }
    shutdown(a->upstream, SHUT_WR);  // 半关闭通知代理；失败可忽略
    return NULL;
}

// 上游代理 -> 客户端方向：读上游 socket 并写虚拟连接。
static void* pump_upstream_to_client(void* arg)
{
    struct tcp_pump_arg* a = (struct tcp_pump_arg*)arg;
    unsigned char buf[BUF_SIZE];
    for (;;)
    {
        ssize_t n = recv(a->upstream, buf, sizeof(buf), 0);
        if (n <= 0)
            break;
        size_t off = 0;
        while (off < (size_t)n)
        {
            int w = tunio_tcp_send(a->conn, buf + off, (size_t)n - off);
            if (w <= 0)
                break;
            off += (size_t)w;
        }
        if (off < (size_t)n)
            break;
    }
    close(a->upstream);
    tunio_tcp_close(a->conn);  // 对端结束，向客户端发送 FIN
    return NULL;
}

static void* tcp_bridge_worker(void* arg)
{
    struct tcp_bridge_arg* a = (struct tcp_bridge_arg*)arg;
    tunio_tcp_conn* conn = a->conn;
    const struct options* opt = a->opt;
    char dest[ADDR_BUF] = "";
    uint16_t dest_port = 0;
    char err[256];

    if (tunio_tcp_original_destination(conn, dest, sizeof(dest),
            &dest_port) != TUNIO_OK)
    {
        fprintf(stderr, "[tun2socks] 获取原始目标地址失败: %s\n",
            tunio_last_error_message());
        tunio_tcp_reset(conn);
        tunio_tcp_free(conn);
        free(a);
        bridge_end();
        return NULL;
    }

    int upstream = tcp_connect(opt->proxy_host, opt->proxy_port, err,
        sizeof(err));
    if (upstream < 0)
    {
        fprintf(stderr, "[tun2socks] %s:%u -> %s:%u : %s\n",
            dest, (unsigned)dest_port,
            opt->proxy_host, (unsigned)opt->proxy_port, err);
        tunio_tcp_reset(conn);  // 后端失败：立即 RST 客户端
        tunio_tcp_free(conn);
        free(a);
        bridge_end();
        return NULL;
    }

    if (socks5_negotiate(upstream, 0x01, dest, dest_port,
            NULL, 0, NULL, err, sizeof(err)) < 0)
    {
        fprintf(stderr, "[tun2socks] %s:%u -> %s:%u : %s\n",
            dest, (unsigned)dest_port,
            opt->proxy_host, (unsigned)opt->proxy_port, err);
        close(upstream);
        tunio_tcp_reset(conn);
        tunio_tcp_free(conn);
        free(a);
        bridge_end();
        return NULL;
    }

    struct tcp_pump_arg p1 = { conn, upstream };
    struct tcp_pump_arg p2 = { conn, upstream };
    pthread_t t1;
    pthread_t t2;
    if (pthread_create(&t1, NULL, pump_client_to_upstream, &p1) != 0)
    {
        fprintf(stderr, "[tun2socks] 创建泵线程失败\n");
        close(upstream);
        tunio_tcp_reset(conn);
        tunio_tcp_free(conn);
        free(a);
        bridge_end();
        return NULL;
    }
    if (pthread_create(&t2, NULL, pump_upstream_to_client, &p2) != 0)
    {
        fprintf(stderr, "[tun2socks] 创建泵线程失败\n");
        pthread_join(t1, NULL);
        close(upstream);
        tunio_tcp_reset(conn);
        tunio_tcp_free(conn);
        free(a);
        bridge_end();
        return NULL;
    }
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    tunio_tcp_free(conn);
    free(a);
    bridge_end();
    return NULL;
}

// ---- UDP 桥接 ----

struct udp_bridge_arg
{
    tunio_udp_session* session;
    const struct options* opt;
};

struct udp_pump_arg
{
    tunio_udp_session* session;
    int relay_fd;
};

// 解析 SOCKS5 UDP 中继数据报（RSV=0 + ATYP 地址 + 载荷），输出目标端点与
// 载荷偏移；仅接受 IPv4/IPv6 目标（中继只会回显本示例发送的数值地址）。
static int parse_relay_datagram(const unsigned char* pkt, size_t len,
    char* host, size_t host_size, uint16_t* port, size_t* payload_off)
{
    if (len < 4 || pkt[0] != 0 || pkt[1] != 0 || pkt[2] != 0)
        return -1;
    size_t off = 3;
    uint8_t atyp = pkt[off++];
    if (atyp == 0x01)
    {
        if (len < off + 6)
            return -1;
        if (!inet_ntop(AF_INET, pkt + off, host, (socklen_t)host_size))
            return -1;
        off += 4;
    }
    else if (atyp == 0x04)
    {
        if (len < off + 18)
            return -1;
        if (!inet_ntop(AF_INET6, pkt + off, host, (socklen_t)host_size))
            return -1;
        off += 16;
    }
    else
    {
        return -1;
    }
    uint16_t net_port;
    memcpy(&net_port, pkt + off, 2);
    *port = ntohs(net_port);
    *payload_off = off + 2;
    if (*payload_off > len)
        return -1;
    return 0;
}

// 会话 -> 中继方向：读虚拟 UDP 数据报，封装 SOCKS5 UDP 头后发往中继。
static void* pump_session_to_relay(void* arg)
{
    struct udp_pump_arg* a = (struct udp_pump_arg*)arg;
    unsigned char* payload = (unsigned char*)malloc(BUF_SIZE);
    unsigned char* frame = (unsigned char*)malloc(BUF_SIZE + FRAME_EXTRA);
    if (!payload || !frame)
    {
        fprintf(stderr, "[tun2socks] UDP 泵内存分配失败\n");
        free(payload);
        free(frame);
        close(a->relay_fd);
        return NULL;
    }
    for (;;)
    {
        char target[ADDR_BUF];
        uint16_t target_port = 0;
        int n = tunio_udp_recvfrom(a->session, payload, BUF_SIZE,
            target, sizeof(target), &target_port);
        if (n <= 0)
            break;  // 引擎关闭等
        int hlen = encode_socks_addr(frame + 3, FRAME_EXTRA, target,
            target_port);
        if (hlen < 0)
            continue;  // 非法目标地址，丢弃
        frame[0] = 0;
        frame[1] = 0;
        frame[2] = 0;
        if (send_all(a->relay_fd, frame, (size_t)(3 + hlen + n)) < 0)
            break;
    }
    free(frame);
    free(payload);
    close(a->relay_fd);  // 唤醒另一方向泵线程
    return NULL;
}

// 中继 -> 会话方向：接收并剥离 SOCKS5 UDP 头，把载荷发回虚拟会话。
static void* pump_relay_to_session(void* arg)
{
    struct udp_pump_arg* a = (struct udp_pump_arg*)arg;
    unsigned char* pkt = (unsigned char*)malloc(BUF_SIZE);
    if (!pkt)
    {
        fprintf(stderr, "[tun2socks] UDP 泵内存分配失败\n");
        close(a->relay_fd);
        tunio_udp_close(a->session);
        return NULL;
    }
    for (;;)
    {
        ssize_t n = recv(a->relay_fd, pkt, BUF_SIZE, 0);
        if (n <= 0)
            break;
        char target[ADDR_BUF];
        uint16_t target_port = 0;
        size_t off = 0;
        if (parse_relay_datagram(pkt, (size_t)n, target, sizeof(target),
                &target_port, &off) < 0)
            continue;
        if (n - off == 0)
            continue;  // 空载荷数据报，跳过
        if (tunio_udp_sendto(a->session, target, target_port,
                pkt + off, n - off) < 0)
            break;
    }
    free(pkt);
    close(a->relay_fd);
    tunio_udp_close(a->session);
    return NULL;
}

static void* udp_bridge_worker(void* arg)
{
    struct udp_bridge_arg* a = (struct udp_bridge_arg*)arg;
    tunio_udp_session* session = a->session;
    const struct options* opt = a->opt;
    char relay_host[ADDR_BUF];
    uint16_t relay_port = 0;
    char err[256];

    int control = tcp_connect(opt->proxy_host, opt->proxy_port, err,
        sizeof(err));
    if (control < 0)
    {
        fprintf(stderr, "[tun2socks] udp associate %s:%u : %s\n",
            opt->proxy_host, (unsigned)opt->proxy_port, err);
        tunio_udp_close(session);
        tunio_udp_free(session);
        free(a);
        bridge_end();
        return NULL;
    }
    if (socks5_negotiate(control, 0x03, NULL, 0,
            relay_host, sizeof(relay_host), &relay_port,
            err, sizeof(err)) < 0)
    {
        fprintf(stderr, "[tun2socks] udp associate %s:%u : %s\n",
            opt->proxy_host, (unsigned)opt->proxy_port, err);
        close(control);
        tunio_udp_close(session);
        tunio_udp_free(session);
        free(a);
        bridge_end();
        return NULL;
    }

    int relay_fd = udp_connect(relay_host, relay_port, err, sizeof(err));
    if (relay_fd < 0)
    {
        fprintf(stderr, "[tun2socks] udp associate %s:%u : %s\n",
            opt->proxy_host, (unsigned)opt->proxy_port, err);
        close(control);
        tunio_udp_close(session);
        tunio_udp_free(session);
        free(a);
        bridge_end();
        return NULL;
    }

    struct udp_pump_arg p1 = { session, relay_fd };
    struct udp_pump_arg p2 = { session, relay_fd };
    pthread_t t1;
    pthread_t t2;
    if (pthread_create(&t1, NULL, pump_session_to_relay, &p1) != 0)
    {
        fprintf(stderr, "[tun2socks] 创建 UDP 泵线程失败\n");
        close(relay_fd);
        close(control);
        tunio_udp_close(session);
        tunio_udp_free(session);
        free(a);
        bridge_end();
        return NULL;
    }
    if (pthread_create(&t2, NULL, pump_relay_to_session, &p2) != 0)
    {
        fprintf(stderr, "[tun2socks] 创建 UDP 泵线程失败\n");
        pthread_join(t1, NULL);
        close(relay_fd);
        close(control);
        tunio_udp_close(session);
        tunio_udp_free(session);
        free(a);
        bridge_end();
        return NULL;
    }
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    close(relay_fd);
    close(control);  // 数据面结束后释放控制连接
    tunio_udp_free(session);
    free(a);
    bridge_end();
    return NULL;
}

// ---- accept 循环与入口 ----

struct engine_arg
{
    tunio_engine* engine;
    const struct options* opt;
};

static int spawn_detached(void* (*fn)(void*), void* arg)
{
    pthread_attr_t attr;
    pthread_t tid;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int rc = pthread_create(&tid, &attr, fn, arg);
    pthread_attr_destroy(&attr);
    return rc;
}

static void* tcp_accept_worker(void* arg)
{
    struct engine_arg* e = (struct engine_arg*)arg;
    for (;;)
    {
        tunio_tcp_conn* conn = tunio_tcp_accept(e->engine);
        if (!conn)
            break;  // 引擎已关闭
        struct tcp_bridge_arg* ctx =
            (struct tcp_bridge_arg*)malloc(sizeof(*ctx));
        if (!ctx)
        {
            tunio_tcp_reset(conn);
            tunio_tcp_free(conn);
            continue;
        }
        ctx->conn = conn;
        ctx->opt = e->opt;
        bridge_begin();
        if (spawn_detached(tcp_bridge_worker, ctx) != 0)
        {
            fprintf(stderr, "[tun2socks] 创建连接工作线程失败\n");
            bridge_end();
            tunio_tcp_reset(conn);
            tunio_tcp_free(conn);
            free(ctx);
        }
    }
    return NULL;
}

static void* udp_accept_worker(void* arg)
{
    struct engine_arg* e = (struct engine_arg*)arg;
    for (;;)
    {
        tunio_udp_session* session = tunio_udp_accept(e->engine);
        if (!session)
            break;  // 引擎已关闭
        struct udp_bridge_arg* ctx =
            (struct udp_bridge_arg*)malloc(sizeof(*ctx));
        if (!ctx)
        {
            tunio_udp_close(session);
            tunio_udp_free(session);
            continue;
        }
        ctx->session = session;
        ctx->opt = e->opt;
        bridge_begin();
        if (spawn_detached(udp_bridge_worker, ctx) != 0)
        {
            fprintf(stderr, "[tun2socks] 创建会话工作线程失败\n");
            bridge_end();
            tunio_udp_close(session);
            tunio_udp_free(session);
            free(ctx);
        }
    }
    return NULL;
}

static void on_signal(int signo)
{
    (void)signo;
    g_stop = 1;
}

static void sleep_ms(long ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int main(int argc, char** argv)
{
    struct options opt;
    int rc = parse_args(argc, argv, &opt);
    if (rc != 0)
    {
        if (rc < 0)
            usage(argv[0]);
        return rc < 0 ? 1 : 0;
    }

    tunio_engine* engine = tunio_engine_new(opt.threads);
    if (!engine)
    {
        fprintf(stderr, "创建引擎失败\n");
        return 1;
    }

    tunio_config cfg;
    tunio_config_init(&cfg);
    snprintf(cfg.dev_name, sizeof(cfg.dev_name), "%s", opt.dev_name);
    snprintf(cfg.ipv4_addr, sizeof(cfg.ipv4_addr), "%s", opt.ipv4_addr);
    snprintf(cfg.netmask, sizeof(cfg.netmask), "%s", opt.netmask);
    snprintf(cfg.ipv6_addr, sizeof(cfg.ipv6_addr), "%s", opt.ipv6_addr);
    cfg.ipv6_prefix_len = opt.ipv6_prefix_len;
    cfg.mtu = opt.mtu;
    cfg.num_queues = opt.num_queues;
    if (opt.inject_fd >= 0)
    {
        cfg.external_handle = opt.inject_fd;
        cfg.external_mtu = opt.mtu;
        if (opt.utun_prefix)
            cfg.utun_prefix = 1;
    }

    if (tunio_engine_open(engine, &cfg) != TUNIO_OK)
    {
        fprintf(stderr, "open TUN 失败: %s\n", tunio_last_error_message());
        tunio_engine_free(engine);
        return 1;
    }

    char local[ADDR_BUF] = "";
    tunio_engine_local_address(engine, local, sizeof(local));
    printf("tun2socks: %s %s -> %s:%u (队列 x%u)\n",
        opt.dev_name, opt.ipv4_addr,
        opt.proxy_host, (unsigned)opt.proxy_port,
        (unsigned)tunio_engine_queue_count(engine));
    printf("引擎信息: mtu=%u, 本地地址=%s\n",
        (unsigned)tunio_engine_mtu(engine), local);
    printf("按 Ctrl-C 退出\n");

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);  // socket 写入已关闭连接时不终止进程

    struct engine_arg earg = { engine, &opt };
    pthread_t tcp_acceptor;
    pthread_t udp_acceptor;
    int have_udp = 0;
    if (pthread_create(&tcp_acceptor, NULL, tcp_accept_worker, &earg) != 0)
    {
        fprintf(stderr, "创建 TCP accept 线程失败\n");
        tunio_engine_close(engine);
        tunio_engine_free(engine);
        return 1;
    }
    if (opt.udp)
    {
        have_udp = 1;
        if (pthread_create(&udp_acceptor, NULL, udp_accept_worker,
                &earg) != 0)
        {
            fprintf(stderr, "创建 UDP accept 线程失败\n");
            have_udp = 0;
        }
    }

    while (!g_stop)
        sleep_ms(200);

    printf("正在退出...\n");
    tunio_engine_close(engine);  // 唤醒全部阻塞中的调用
    pthread_join(tcp_acceptor, NULL);
    if (have_udp)
        pthread_join(udp_acceptor, NULL);
    // 等待仍在收尾的桥接工作线程退出，再释放引擎句柄
    for (int i = 0; i < 60 && atomic_load(&g_active_bridges) > 0; ++i)
        sleep_ms(50);
    tunio_engine_free(engine);
    printf("已退出\n");
    return 0;
}
