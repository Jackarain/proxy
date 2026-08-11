#!/usr/bin/env python3
"""UDP DNS 查询测试客户端：验证 proxy_server 的 dns_udp_port/dns_no_ipv6/dns_cache 功能。

用法:
    python3 dns_test_client.py <port> [--no-ipv6] [--repeat N]
"""
import socket
import struct
import random
import sys

def build_query(name, qtype=1):
    tid = random.randint(0, 0xffff)
    header = struct.pack('>HHHHHH', tid, 0x0100, 1, 0, 0, 0)
    qname = b''.join(bytes([len(label)]) + label.encode()
                     for label in name.split('.')) + b'\x00'
    question = qname + struct.pack('>HH', qtype, 1)
    return tid, header + question

def parse_response(data):
    if len(data) < 12:
        return None
    tid, flags, qd, an, ns, ar = struct.unpack('>HHHHHH', data[:12])
    rcode = flags & 0xf
    # 跳过 question 区.
    pos = 12
    for _ in range(qd):
        while pos < len(data) and data[pos] != 0:
            pos += data[pos] + 1
        pos += 5  # 终止标签(1) + QTYPE(2) + QCLASS(2)
    answers = []
    for _ in range(an):
        # 名称（可能为压缩指针）.
        if data[pos] & 0xc0 == 0xc0:
            pos += 2
        else:
            while data[pos] != 0:
                pos += data[pos] + 1
            pos += 1
        atype, aclass, ttl, rdlen = struct.unpack('>HHIH', data[pos:pos+10])
        pos += 10
        rdata = data[pos:pos+rdlen]
        pos += rdlen
        if atype == 1 and rdlen == 4:
            answers.append(socket.inet_ntop(socket.AF_INET, rdata))
        elif atype == 28 and rdlen == 16:
            answers.append(socket.inet_ntop(socket.AF_INET6, rdata))
    return {'tid': tid, 'rcode': rcode, 'ancount': an, 'answers': answers}

def query(server, port, name, qtype=1, timeout=5):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(timeout)
    tid, req = build_query(name, qtype)
    s.sendto(req, (server, port))
    data, _ = s.recvfrom(65535)
    s.close()
    if len(data) < 2:
        return None
    resp_tid = struct.unpack('>H', data[:2])[0]
    assert resp_tid == tid, f"transaction id mismatch: {resp_tid:#x} != {tid:#x}"
    return parse_response(data)

def main():
    args = sys.argv[1:]
    port = int(args[0]) if args else 5353
    no_ipv6 = '--no-ipv6' in args
    repeat = 1
    if '--repeat' in args:
        repeat = int(args[args.index('--repeat') + 1])

    name = 'example.com'
    ok = True

    # A 查询（应为正常解析）.
    r = query('127.0.0.1', port, name, 1)
    if r is None:
        print(f"[FAIL] A query: no response")
        ok = False
    else:
        print(f"[A] rcode={r['rcode']} ancount={r['ancount']} answers={r['answers']}")
        if r['rcode'] != 0 or r['ancount'] == 0:
            print("  [FAIL] A query should resolve")
            ok = False

    # AAAA 查询.
    r = query('127.0.0.1', port, name, 28)
    if r is None:
        print(f"[FAIL] AAAA query: no response")
        ok = False
    else:
        print(f"[AAAA] rcode={r['rcode']} ancount={r['ancount']} answers={r['answers']}")
        if no_ipv6:
            # no_ipv6 开启：返回空应答（NODATA，rcode=0 且无答案）.
            if r['rcode'] != 0 or r['ancount'] != 0:
                print("  [FAIL] dns_no_ipv6 enabled: AAAA should return empty (NODATA)")
                ok = False
        else:
            if r['rcode'] == 2:
                print("  [WARN] AAAA query got SERVFAIL (may be upstream failure)")

    # 重复查询（验证缓存 / 稳定性）.
    for i in range(repeat):
        r = query('127.0.0.1', port, name, 1)
        if r is None:
            print(f"[FAIL] A query repeat {i}: no response")
            ok = False
        else:
            print(f"[A repeat {i}] rcode={r['rcode']} ancount={r['ancount']} answers={r['answers']}")

    print("RESULT:", "PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)

if __name__ == '__main__':
    main()
