#!/usr/bin/env python3
#
# tun_echo：tunio Python 绑定回显示例（对应 examples/tun_echo.cpp）。
#
# TCP：把虚拟连接收到的数据原样写回（引擎层面回显，省略 C++ 示例中
#      桥接本机 echo 服务的后端连接）；
# UDP：直接把数据报回显给发送方。
#
# 用法示例：
#   sudo python3 examples/python/tun_echo.py --tun tun0 \
#       --ip 10.0.0.1 --netmask 255.255.255.0
#
# 需要 root（或 CAP_NET_ADMIN）创建 TUN 设备；也可 --inject-fd 注入外部
# 已打开的 TUN 文件描述符。

import argparse
import signal
import sys
import threading
import time
from pathlib import Path

# 保证从源码树任意位置运行都能 import 到 bindings/python/ 下的 tunio 包
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "bindings" / "python"))

import tunio


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="tunio Python 绑定回显示例（TCP/UDP 回显）",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--tun", default="tun0", help="TUN 设备名")
    parser.add_argument("--ip", default="10.0.0.1", help="本地虚拟 IPv4 地址")
    parser.add_argument("--netmask", default="255.255.255.0", help="子网掩码")
    parser.add_argument("--ip6", default="", help="本地虚拟 IPv6 地址（可选）")
    parser.add_argument("--ip6-prefix", type=int, default=64, help="IPv6 前缀长度")
    parser.add_argument("--mtu", type=int, default=1500, help="设备 MTU")
    parser.add_argument("--threads", type=int, default=1, help="io_context 线程数")
    parser.add_argument("--inject-fd", type=int, default=-1,
                        help="注入外部已打开的 TUN 文件描述符")
    return parser.parse_args(argv)


def tcp_echo_handler(client):
    """回显一条虚拟 TCP 连接的数据。"""
    try:
        while True:
            data = client.recv(65536)
            if not data:
                break  # 对端关闭
            view = memoryview(data)
            offset = 0
            while offset < len(view):
                offset += client.send(view[offset:])
    except tunio.TunioError:
        pass
    finally:
        try:
            client.close()
        except tunio.TunioError:
            pass


def udp_echo_handler(session):
    """回显一个 UDP 会话的数据报。"""
    try:
        while True:
            data, sender = session.recvfrom(65535)
            session.sendto(data, sender[0], sender[1])
    except tunio.TunioError:
        pass
    finally:
        try:
            session.close()
        except tunio.TunioError:
            pass


def tcp_accept_loop(engine, workers):
    while True:
        try:
            client = engine.accept_tcp()
        except tunio.Closed:
            return
        thread = threading.Thread(target=tcp_echo_handler, args=(client,),
                                  daemon=True)
        thread.start()
        workers.append(thread)


def udp_accept_loop(engine, workers):
    while True:
        try:
            session = engine.accept_udp()
        except tunio.Closed:
            return
        thread = threading.Thread(target=udp_echo_handler, args=(session,),
                                  daemon=True)
        thread.start()
        workers.append(thread)


def main(argv=None):
    args = parse_args(argv)

    if args.threads < 1:
        print("error: --threads 必须 >= 1", file=sys.stderr)
        return 1

    engine = tunio.Engine(threads=args.threads)
    config = {
        "dev_name": args.tun,
        "ipv4_addr": args.ip,
        "netmask": args.netmask,
        "ipv6_addr": args.ip6,
        "ipv6_prefix_len": args.ip6_prefix,
        "mtu": args.mtu,
    }
    if args.inject_fd >= 0:
        config["external_handle"] = args.inject_fd
        config["external_mtu"] = args.mtu

    try:
        engine.open(config)
    except tunio.TunioError as exc:
        print(f"open TUN failed: {exc}", file=sys.stderr)
        return 1

    print(f"tun_echo: {args.tun} {args.ip}"
          + (f" / {args.ip6}" if args.ip6 else ""))
    print(f"引擎信息: mtu={engine.mtu}, 队列数={engine.queue_count}, "
          f"本地地址={engine.local_address}")
    print("按 Ctrl-C 退出")

    workers = []
    acceptors = [
        threading.Thread(target=tcp_accept_loop, args=(engine, workers),
                         daemon=True),
        threading.Thread(target=udp_accept_loop, args=(engine, workers),
                         daemon=True),
    ]
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
