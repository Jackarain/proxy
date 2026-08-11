#!/usr/bin/env python3
"""Mock UDP DNS 上游服务器：计数收到的查询，返回固定的 A 记录响应。

用于验证 proxy_server 的 DNS 查询结果缓存：如果缓存生效，重复查询
不会到达本服务器（查询计数不增长）。

用法:
    python3 dns_mock_upstream.py <port>
"""
import socket
import struct
import sys

counter = 0

def build_response(req, ip):
    """构造与 req 同 ID 的 A 记录响应."""
    if len(req) < 12:
        return None
    tid = req[0:2]
    # flags: QR=1 RD=1 RA=1, rcode=0 => 0x8180
    flags = struct.pack('>H', 0x8180)
    qd = struct.pack('>H', 1)
    an = struct.pack('>H', 1)
    ns = struct.pack('>H', 0)
    ar = struct.pack('>H', 0)
    # 回显 question.
    qend = 12
    while req[qend] != 0:
        qend += req[qend] + 1
    question = req[12:qend + 5]  # 含终止标签 + QTYPE + QCLASS
    # answer: 压缩指针指向 offset 12.
    answer = b'\xc0\x0c' + struct.pack('>HHIH', 1, 1, 60, 4) + socket.inet_aton(ip)
    return tid + flags + qd + an + ns + ar + question + answer

def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 15300
    ip = sys.argv[2] if len(sys.argv) > 2 else '93.184.216.34'

    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(('127.0.0.1', port))
    print(f"mock dns upstream listening on 127.0.0.1:{port}, answer A -> {ip}")

    while True:
        data, addr = s.recvfrom(65535)
        global counter
        counter += 1
        print(f"[query #{counter}] from {addr}: {len(data)} bytes")
        resp = build_response(data, ip)
        if resp:
            s.sendto(resp, addr)

if __name__ == '__main__':
    main()
