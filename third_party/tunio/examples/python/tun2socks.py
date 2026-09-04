#!/usr/bin/env python3
#
# tun2socks：tunio Python 绑定的 SOCKS5 透明代理示例（对应 examples/tun2socks.cpp）。
#
# TCP：引擎终止虚拟连接，应用层经 SOCKS5 CONNECT 连到代理后全双工桥接；
# UDP：引擎维护 NAT 会话，应用层经 SOCKS5 UDP ASSOCIATE 中继转发；
# 后端连接失败时向客户端发送 RST。
#
# 用法示例：
#   sudo python3 examples/python/tun2socks.py --tun tun0 \
#       --ip 10.0.0.1 --netmask 255.255.255.0 \
#       --proxy 127.0.0.1:1080
#
# 需要 root（或 CAP_NET_ADMIN）创建 TUN 设备；也可 --inject-fd 注入外部
# 已打开的 TUN 文件描述符。

import argparse
import ipaddress
import signal
import socket
import struct
import sys
import threading
import time
from pathlib import Path

# 保证从源码树任意位置运行都能 import 到 bindings/python/ 下的 tunio 包
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "bindings" / "python"))

import tunio

MAX_MULTI_QUEUES = 256


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="tunio Python 绑定 SOCKS5 透明代理（TCP CONNECT + UDP ASSOCIATE）",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--tun", default="tun0", help="TUN 设备名")
    parser.add_argument("--ip", default="10.0.0.1", help="本地虚拟 IPv4 地址")
    parser.add_argument("--netmask", default="255.255.255.0", help="子网掩码")
    parser.add_argument("--ip6", default="", help="本地虚拟 IPv6 地址（可选）")
    parser.add_argument("--ip6-prefix", type=int, default=64, help="IPv6 前缀长度")
    parser.add_argument("--mtu", type=int, default=1500, help="设备 MTU")
    parser.add_argument("--queues", type=int, default=1,
                        help="Linux TUN 多队列数（IFF_MULTI_QUEUE）")
    parser.add_argument("--proxy", default="127.0.0.1:1080",
                        help="SOCKS5 代理地址 host:port")
    parser.add_argument("--no-udp", action="store_true", help="禁用 UDP 转发")
    parser.add_argument("--utun-prefix", action="store_true",
                        help="注入的 fd 为 macOS utun（读写带 4 字节家族前缀）")
    parser.add_argument("--inject-fd", type=int, default=-1,
                        help="注入外部已打开的 TUN 文件描述符")
    parser.add_argument("--threads", type=int, default=1, help="io_context 线程数")
    return parser.parse_args(argv)


def _recv_exact(sock, n):
    """从 socket 精确读取 n 字节；连接提前关闭抛 OSError。"""
    data = b""
    while len(data) < n:
        chunk = sock.recv(n - len(data))
        if not chunk:
            raise OSError("socks5: connection closed")
        data += chunk
    return data


def _encode_address(host, port):
    """按 SOCKS5 ATYP 编码地址：IPv4/IPv6 优先，否则域名。"""
    try:
        ipaddress.ip_address(host)
    except ValueError:
        data = host.encode("utf-8")
        return bytes((0x03, len(data))) + data + struct.pack(">H", port)
    addr = socket.inet_pton(socket.AF_INET6 if ":" in host else socket.AF_INET, host)
    atyp = 0x04 if ":" in host else 0x01
    return bytes((atyp,)) + addr + struct.pack(">H", port)


def _read_reply_addr(sock, atyp):
    """读取 SOCKS5 回复的 BND.ADDR，返回 (host, port)。"""
    if atyp == 0x01:  # IPv4
        data = _recv_exact(sock, 6)
        host = socket.inet_ntop(socket.AF_INET, data[:4])
        port = struct.unpack(">H", data[4:6])[0]
    elif atyp == 0x04:  # IPv6
        data = _recv_exact(sock, 18)
        host = socket.inet_ntop(socket.AF_INET6, data[:16])
        port = struct.unpack(">H", data[16:18])[0]
    elif atyp == 0x03:  # 域名
        (dlen,) = _recv_exact(sock, 1)
        data = _recv_exact(sock, dlen + 2)
        host = data[:dlen].decode("utf-8")
        port = struct.unpack(">H", data[dlen:dlen + 2])[0]
    else:
        raise OSError("socks5: bad atyp")
    return host, port


def socks5_connect(proxy_host, proxy_port, target_host, target_port):
    """建立到代理的 SOCKS5 CONNECT 隧道，返回已连接的上游 socket。"""
    sock = socket.create_connection((proxy_host, proxy_port))
    try:
        sock.sendall(b"\x05\x01\x00")  # 版本 5 / 1 种方法 / NO AUTH
        resp = _recv_exact(sock, 2)
        if resp[0] != 5 or resp[1] != 0:
            raise OSError("socks5: auth method not accepted")
        sock.sendall(b"\x05\x01\x00" + _encode_address(target_host, target_port))
        head = _recv_exact(sock, 4)
        if head[1] != 0:
            raise OSError(f"socks5: connect failed (rep={head[1]})")
        _read_reply_addr(sock, head[3])
        return sock
    except Exception:
        sock.close()
        raise


class Socks5UdpRelay:
    """SOCKS5 UDP ASSOCIATE 中继；控制连接随对象存活而保持。"""

    def __init__(self, proxy_host, proxy_port):
        self._control = None
        self._sock = None
        self._associate(proxy_host, proxy_port)

    def _associate(self, proxy_host, proxy_port):
        control = socket.create_connection((proxy_host, proxy_port))
        try:
            control.sendall(b"\x05\x01\x00")
            resp = _recv_exact(control, 2)
            if resp[0] != 5 or resp[1] != 0:
                raise OSError("socks5-udp: auth method not accepted")
            # UDP ASSOCIATE：BND.ADDR 为 0.0.0.0:0
            control.sendall(b"\x05\x03\x00\x01\x00\x00\x00\x00\x00\x00")
            head = _recv_exact(control, 4)
            if head[1] != 0:
                raise OSError(f"socks5-udp: associate failed (rep={head[1]})")
            relay_host, relay_port = _read_reply_addr(control, head[3])
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.connect((relay_host, relay_port))  # 仅与中继通信，符合 SOCKS5 语义
        except Exception:
            control.close()
            raise
        self._control = control
        self._sock = sock

    def send_to(self, payload, target_host, target_port):
        """封装 SOCKS5 UDP 头并发送到中继。"""
        self._sock.sendall(
            b"\x00\x00\x00" + _encode_address(target_host, target_port) + payload)

    def receive_from(self):
        """接收并剥离封装，返回 (payload, (原始目标地址, 端口))。"""
        pkt = self._sock.recv(65535)
        if len(pkt) < 4 or pkt[0] != 0 or pkt[1] != 0 or pkt[2] != 0:
            raise OSError("socks5-udp: bad header")
        off = 3
        atyp = pkt[off]
        off += 1
        if atyp == 0x01:  # IPv4
            if len(pkt) < off + 6:
                raise OSError("socks5-udp: short packet")
            host = socket.inet_ntop(socket.AF_INET, pkt[off:off + 4])
            off += 4
        elif atyp == 0x04:  # IPv6
            if len(pkt) < off + 18:
                raise OSError("socks5-udp: short packet")
            host = socket.inet_ntop(socket.AF_INET6, pkt[off:off + 16])
            off += 16
        elif atyp == 0x03:  # 域名
            dlen = pkt[off]
            off += 1
            if len(pkt) < off + dlen + 2:
                raise OSError("socks5-udp: short packet")
            host = pkt[off:off + dlen].decode("utf-8")
            off += dlen
        else:
            raise OSError("socks5-udp: bad atyp")
        port = struct.unpack(">H", pkt[off:off + 2])[0]
        off += 2
        return pkt[off:], (host, port)

    def close(self):
        if self._sock:
            self._sock.close()
        if self._control:
            self._control.close()


def tcp_bridge(client, proxy_host, proxy_port):
    """把一条虚拟 TCP 连接经 SOCKS5 CONNECT 全双工桥接到代理。"""
    dest_host, dest_port = client.original_destination
    try:
        upstream = socks5_connect(proxy_host, proxy_port, dest_host, dest_port)
    except OSError as exc:
        print(f"[tun2socks] {dest_host}:{dest_port} -> {proxy_host}:{proxy_port}: "
              f"{exc}", file=sys.stderr)
        client.reset()  # 后端失败：立即 RST 客户端
        return

    def pump_client_to_upstream():
        try:
            while True:
                data = client.recv(65536)
                if not data:
                    break  # 对端关闭
                upstream.sendall(data)
        except (tunio.TunioError, OSError):
            pass
        finally:
            try:
                upstream.shutdown(socket.SHUT_WR)
            except OSError:
                pass

    def pump_upstream_to_client():
        try:
            while True:
                data = upstream.recv(65536)
                if not data:
                    break
                view = memoryview(data)
                offset = 0
                while offset < len(view):
                    offset += client.send(view[offset:])
        except (tunio.TunioError, OSError):
            pass
        finally:
            try:
                client.close()
            except tunio.TunioError:
                pass
            upstream.close()

    pumps = [
        threading.Thread(target=pump_client_to_upstream, daemon=True),
        threading.Thread(target=pump_upstream_to_client, daemon=True),
    ]
    for thread in pumps:
        thread.start()
    for thread in pumps:
        thread.join()


def tcp_accept_loop(engine, proxy_host, proxy_port, workers):
    while True:
        try:
            client = engine.accept_tcp()
        except tunio.Closed:
            return
        thread = threading.Thread(target=tcp_bridge,
                                  args=(client, proxy_host, proxy_port),
                                  daemon=True)
        thread.start()
        workers.append(thread)


def udp_bridge(session, proxy_host, proxy_port):
    """把一条 UDP 会话经 SOCKS5 UDP ASSOCIATE 中继转发。"""
    try:
        relay = Socks5UdpRelay(proxy_host, proxy_port)
    except OSError as exc:
        print(f"[tun2socks] udp associate {proxy_host}:{proxy_port}: {exc}",
              file=sys.stderr)
        session.close()
        return

    def pump_session_to_relay():
        try:
            while True:
                data, sender = session.recvfrom(2048)
                relay.send_to(data, sender[0], sender[1])
        except (tunio.TunioError, OSError):
            pass
        finally:
            relay.close()

    def pump_relay_to_session():
        try:
            while True:
                payload, target = relay.receive_from()
                session.sendto(payload, target[0], target[1])
        except (tunio.TunioError, OSError):
            pass
        finally:
            try:
                session.close()
            except tunio.TunioError:
                pass
            relay.close()

    pumps = [
        threading.Thread(target=pump_session_to_relay, daemon=True),
        threading.Thread(target=pump_relay_to_session, daemon=True),
    ]
    for thread in pumps:
        thread.start()
    for thread in pumps:
        thread.join()


def udp_accept_loop(engine, proxy_host, proxy_port, workers):
    while True:
        try:
            session = engine.accept_udp()
        except tunio.Closed:
            return
        thread = threading.Thread(target=udp_bridge,
                                  args=(session, proxy_host, proxy_port),
                                  daemon=True)
        thread.start()
        workers.append(thread)


def main(argv=None):
    args = parse_args(argv)

    if args.threads < 1:
        print("error: --threads 必须 >= 1", file=sys.stderr)
        return 1
    if args.queues < 1 or args.queues > MAX_MULTI_QUEUES:
        print(f"error: --queues 需在 1..{MAX_MULTI_QUEUES} 之间", file=sys.stderr)
        return 1

    if ":" in args.proxy:
        proxy_host, _, proxy_port = args.proxy.rpartition(":")
    else:
        proxy_host, proxy_port = args.proxy, "1080"
    try:
        proxy_port = int(proxy_port)
    except ValueError:
        print(f"error: 非法代理端口: {args.proxy}", file=sys.stderr)
        return 1
    if not proxy_host or not 0 < proxy_port < 65536:
        print(f"error: 代理地址需要 host:port 格式: {args.proxy}", file=sys.stderr)
        return 1

    engine = tunio.Engine(threads=args.threads)
    config = {
        "dev_name": args.tun,
        "ipv4_addr": args.ip,
        "netmask": args.netmask,
        "ipv6_addr": args.ip6,
        "ipv6_prefix_len": args.ip6_prefix,
        "mtu": args.mtu,
        "num_queues": args.queues,
    }
    if args.inject_fd >= 0:
        config["external_handle"] = args.inject_fd
        config["external_mtu"] = args.mtu
        if args.utun_prefix:
            config["utun_prefix"] = True

    try:
        engine.open(config)
    except tunio.TunioError as exc:
        print(f"open TUN failed: {exc}", file=sys.stderr)
        return 1

    print(f"tun2socks: {args.tun} {args.ip}"
          + (f" / {args.ip6}" if args.ip6 else "")
          + f" -> {proxy_host}:{proxy_port} (队列 x{engine.queue_count})")
    print(f"引擎信息: mtu={engine.mtu}, 本地地址={engine.local_address}")
    print("按 Ctrl-C 退出")

    workers = []
    acceptors = [
        threading.Thread(target=tcp_accept_loop,
                         args=(engine, proxy_host, proxy_port, workers),
                         daemon=True),
    ]
    if not args.no_udp:
        acceptors.append(threading.Thread(
            target=udp_accept_loop,
            args=(engine, proxy_host, proxy_port, workers),
            daemon=True))
    for thread in acceptors:
        thread.start()

    def on_signal(signum, frame):
        engine.close()

    signal.signal(signal.SIGINT, on_signal)
    signal.signal(signal.SIGTERM, on_signal)

    try:
        while engine.is_open:
            time.sleep(0.2)
    except KeyboardInterrupt:
        engine.close()

    for thread in acceptors:
        thread.join()
    for thread in workers:
        thread.join(timeout=3)
    engine.close()
    print("已退出")
    return 0


if __name__ == "__main__":
    sys.exit(main())