#
# tunio Python 绑定
#
# 基于 bindings/c/include/tunio/c_api.h 的稳定 C API，通过 ctypes 薄封装实现，无
# 编译期语言绑定依赖。用法：
#
#     import tunio
#
#     engine = tunio.Engine()
#     engine.open(dev_name="tun0", ipv4_addr="10.0.0.1",
#                 netmask="255.255.255.0")
#     while True:
#         client = engine.accept_tcp()   # 阻塞等待虚拟连接
#         data = client.recv(4096)
#         client.send(data)
#
# accept_tcp()/accept_udp()/recv()/send() 均为阻塞调用：内部由 tunio_c
# 的 io 线程推进协议状态机，调用线程（可多个）在完成前释放 GIL。
# engine.close() 会唤醒所有阻塞调用并抛出 tunio.Closed。

from __future__ import annotations

import ctypes
from typing import Any

from tunio import _ffi
from tunio._ffi import (
    Closed,
    ConnectionReset,
    TunioError,
    raise_last_error,
    tunio_config_init,
    tunio_engine_close,
    tunio_engine_free,
    tunio_engine_is_open,
    tunio_engine_local_address,
    tunio_engine_mtu,
    tunio_engine_new,
    tunio_engine_open,
    tunio_engine_queue_count,
    tunio_engine_stats,
    tunio_tcp_accept,
    tunio_tcp_close,
    tunio_tcp_free,
    tunio_tcp_is_open,
    tunio_tcp_original_destination,
    tunio_tcp_recv,
    tunio_tcp_remote_endpoint,
    tunio_tcp_reset,
    tunio_tcp_send,
    tunio_udp_accept,
    tunio_udp_client_endpoint,
    tunio_udp_close,
    tunio_udp_free,
    tunio_udp_is_open,
    tunio_udp_recvfrom,
    tunio_udp_sendto,
    tunio_udp_set_timeout,
)

__version__ = "3.0.0"

__all__ = [
    "Engine",
    "TcpSocket",
    "UdpSocket",
    "TunioError",
    "Closed",
    "ConnectionReset",
    "__version__",
]


def _encode_string(value: str, field_name: str, size: int) -> bytes:
    if not isinstance(value, str):
        raise TypeError(f"{field_name} 必须是字符串")
    data = value.encode("utf-8")
    if len(data) >= size:
        raise ValueError(f"{field_name} 过长（最多 {size - 1} 字节）")
    return data


# Python 关键字与 tun_config 字段的映射。
# 字符串字段在未提供时采用与 examples/tun_echo.cpp 一致的便捷默认值。
_STRING_DEFAULTS = {
    "dev_name": "tun0",
    "ipv4_addr": "10.0.0.1",
    "netmask": "255.255.255.0",
    "ipv6_addr": "",
}

_STRING_FIELDS = ("dev_name", "ipv4_addr", "netmask", "ipv6_addr")

# (python key, struct 字段, C 类型转换函数)
_INT_FIELDS = {
    "ipv6_prefix_len": ("ipv6_prefix_len", int),
    "mtu": ("mtu", int),
    "num_queues": ("num_queues", int),
    "external_mtu": ("external_mtu", int),
    "max_tcp_flows": ("max_tcp_flows", int),
    "max_udp_flows": ("max_udp_flows", int),
    "max_rx_queue_per_flow": ("max_rx_queue_per_flow", int),
    "tcp_ooo_max_segments": ("tcp_ooo_max_segments", int),
    "max_total_buffer": ("max_total_buffer", int),
    "udp_idle_timeout": ("udp_idle_timeout_sec", int),
    "tcp_time_wait_timeout": ("tcp_time_wait_timeout_sec", int),
    "tcp_accept_timeout": ("tcp_accept_timeout_sec", int),
    "tcp_syn_timeout": ("tcp_syn_timeout_sec", int),
    "tcp_close_timeout": ("tcp_close_timeout_sec", int),
    "tcp_persist_timeout": ("tcp_persist_timeout_ms", int),
    "tcp_persist_max_probes": ("tcp_persist_max_probes", int),
    "tcp_rto_timeout": ("tcp_rto_timeout_ms", int),
    "tcp_rto_max_retransmits": ("tcp_rto_max_retransmits", int),
}

_INT_DEFAULTS = {
    "mtu": 1500,
    "num_queues": 1,
    "max_tcp_flows": 65536,
    "max_udp_flows": 65536,
    "max_rx_queue_per_flow": 8 * 1024 * 1024,
    "tcp_ooo_max_segments": 4096,
    "max_total_buffer": 512 * 1024 * 1024,
    "udp_idle_timeout": 30,
    "tcp_time_wait_timeout": 10,
    "tcp_accept_timeout": 30,
    "tcp_syn_timeout": 30,
    "tcp_close_timeout": 30,
    "tcp_persist_timeout": 5000,
    "tcp_persist_max_probes": 15,
    "tcp_rto_timeout": 200,
    "tcp_rto_max_retransmits": 8,
}


def _build_config(config: Any, kwargs: dict[str, Any]) -> _ffi.TunioConfig:
    """合并 config dict 与关键字参数为 C 配置结构。"""
    if config is None:
        values: dict[str, Any] = {}
    elif isinstance(config, dict):
        values = dict(config)
    else:
        raise TypeError("config 必须是 dict")
    values.update(kwargs)

    cfg = _ffi.TunioConfig()
    tunio_config_init(ctypes.byref(cfg))

    for key in _STRING_FIELDS:
        value = values.pop(key, _STRING_DEFAULTS[key])
        setattr(cfg, key, _encode_string(value, key, 64))

    for key, (field, cast) in _INT_FIELDS.items():
        value = values.pop(key, _INT_DEFAULTS.get(key))
        if value is not None:
            setattr(cfg, field, cast(value))

    if "external_handle" in values:
        cfg.external_handle = int(values.pop("external_handle"))

    if "utun_prefix" in values:
        cfg.utun_prefix = 1 if values.pop("utun_prefix") else 0

    if "external_handles" in values:
        handles = values.pop("external_handles")
        try:
            seq = list(handles)
        except TypeError:
            raise TypeError("external_handles 必须是可迭代的整型集合") from None
        if len(seq) > 256:
            raise ValueError("external_handles 数量超过上限 256")
        for i, fd in enumerate(seq):
            cfg.external_handles[i] = int(fd)
        cfg.num_external_handles = len(seq)

    if values:
        unknown = "、".join(sorted(values))
        raise ValueError(f"未知配置项: {unknown}")
    return cfg


class Engine:
    """tunio 用户态 TUN 网络引擎。

    Engine 内部维护 io_context 与 io 线程；open() 成功后数据通路在后台
    运行。线程参数 threads 指定 io 线程数（1 为单线程模式，多线程模式
    引擎内部使用 Strand 串行化）。
    """

    def __init__(self, threads: int = 1) -> None:
        if threads < 1:
            raise ValueError("threads 必须 >= 1")
        self._handle = tunio_engine_new(threads)
        if not self._handle:
            raise_last_error("创建引擎失败: ")

    def open(self, config: dict[str, Any] | None = None, **kwargs: Any) -> None:
        """打开 TUN 设备并启动数据通路。

        配置项与 tun_config 字段一一对应，见 c_api.h 的 tunio_config。
        便捷默认（未指定时）：dev_name="tun0"、ipv4_addr="10.0.0.1"、
        netmask="255.255.255.0"、mtu=1500。
        """
        if not self._handle:
            raise Closed("引擎已释放")
        cfg = _build_config(config, kwargs)
        rc = tunio_engine_open(self._handle, ctypes.byref(cfg))
        if rc < 0:
            raise_last_error("打开引擎失败: ")

    def close(self) -> None:
        """关闭数据通路，唤醒全部阻塞调用；可再次 open()。"""
        if self._handle:
            tunio_engine_close(self._handle)

    @property
    def is_open(self) -> bool:
        return bool(self._handle) and bool(tunio_engine_is_open(self._handle))

    @property
    def mtu(self) -> int:
        return int(tunio_engine_mtu(self._handle)) if self._handle else 0

    @property
    def queue_count(self) -> int:
        return int(tunio_engine_queue_count(self._handle)) if self._handle else 0

    @property
    def local_address(self) -> str | None:
        if not self._handle:
            return None
        buf = ctypes.create_string_buffer(64)
        rc = tunio_engine_local_address(self._handle, buf, len(buf))
        if rc < 0:
            raise_last_error("读取本地地址失败: ")
        value = buf.value
        return value.decode("utf-8") if value else None

    @property
    def stats(self) -> dict[str, int]:
        """引擎统计信息字典（rx_packets/tx_packets/... 等 7 项）。"""
        if not self._handle:
            return {}
        stats = _ffi.TunioStats()
        rc = tunio_engine_stats(self._handle, ctypes.byref(stats))
        if rc < 0:
            raise_last_error("读取统计失败: ")
        return {
            "rx_packets": int(stats.rx_packets),
            "tx_packets": int(stats.tx_packets),
            "rx_dropped": int(stats.rx_dropped),
            "rx_ooo": int(stats.rx_ooo),
            "tcp_connections": int(stats.tcp_connections),
            "udp_sessions": int(stats.udp_sessions),
            "icmp_replies": int(stats.icmp_replies),
        }

    def accept_tcp(self) -> "TcpSocket":
        """阻塞等待一条新的虚拟 TCP 连接；引擎关闭时抛 tunio.Closed。"""
        if not self._handle:
            raise Closed("引擎已释放")
        conn = tunio_tcp_accept(self._handle)
        if not conn:
            raise_last_error("accept_tcp: ")
        return TcpSocket(self, conn)

    def accept_udp(self) -> "UdpSocket":
        """阻塞等待一个新的 UDP 会话；引擎关闭时抛 tunio.Closed。"""
        if not self._handle:
            raise Closed("引擎已释放")
        session = tunio_udp_accept(self._handle)
        if not session:
            raise_last_error("accept_udp: ")
        return UdpSocket(self, session)

    def __del__(self) -> None:
        try:
            handle = getattr(self, "_handle", None)
            if handle:
                tunio_engine_free(handle)
                self._handle = None
        except Exception:
            pass


def _read_endpoint(call, handle: int) -> tuple[str, int]:
    ip = ctypes.create_string_buffer(64)
    port = ctypes.c_uint16()
    rc = call(handle, ip, len(ip), ctypes.byref(port))
    if rc < 0:
        raise_last_error()
    return ip.value.decode("utf-8"), int(port.value)


class _SocketBase:
    """持有引擎引用与 C 句柄的基类，保证引擎在套接字存活期间不被释放。"""

    def __init__(self, engine: Engine, handle: int) -> None:
        self._engine = engine
        self._handle = handle

    def _closed_guard(self, name: str) -> None:
        if not self._handle:
            raise Closed(f"{name}: 句柄已释放")

    def _free(self) -> None:
        handle = getattr(self, "_handle", None)
        if handle:
            self._free_fn(handle)
            self._handle = None

    def __del__(self) -> None:
        try:
            self._free()
        except Exception:
            pass


class TcpSocket(_SocketBase):
    """一条由引擎识别的虚拟 TCP 连接（对应 tun_tcp_socket）。"""

    _free_fn = staticmethod(tunio_tcp_free)

    def recv(self, size: int = 65536) -> bytes:
        """阻塞读取；返回 0..size 字节，对端关闭时返回 b''。"""
        self._closed_guard("recv")
        if size <= 0:
            raise ValueError("size 必须 > 0")
        buf = ctypes.create_string_buffer(size)
        rc = tunio_tcp_recv(self._handle, buf, size)
        if rc < 0:
            raise_last_error("recv: ")
        return buf.raw[:rc]

    def send(self, data: bytes | bytearray | memoryview | str) -> int:
        """阻塞发送；成功返回写入字节数。传入 str 时按 UTF-8 编码。"""
        self._closed_guard("send")
        if isinstance(data, str):
            data = data.encode("utf-8")
        payload = bytes(data)
        if not payload:
            return 0
        rc = tunio_tcp_send(self._handle, payload, len(payload))
        if rc < 0:
            raise_last_error("send: ")
        return rc

    def close(self) -> None:
        """优雅关闭（发送 FIN，完成四次挥手）。"""
        if self._handle:
            tunio_tcp_close(self._handle)

    def reset(self) -> None:
        """中止连接（立即向客户端发送 RST）。"""
        if self._handle:
            tunio_tcp_reset(self._handle)

    @property
    def is_open(self) -> bool:
        return bool(self._handle) and bool(tunio_tcp_is_open(self._handle))

    @property
    def original_destination(self) -> tuple[str, int]:
        """客户端请求的目标远端 (地址, 端口)。"""
        self._closed_guard("original_destination")
        return _read_endpoint(tunio_tcp_original_destination, self._handle)

    @property
    def remote_endpoint(self) -> tuple[str, int]:
        """虚拟网内客户端端点 (地址, 端口)。"""
        self._closed_guard("remote_endpoint")
        return _read_endpoint(tunio_tcp_remote_endpoint, self._handle)


class UdpSocket(_SocketBase):
    """一个 UDP 会话（对应 tun_udp_socket，1 对 N：可向任意远端收发）。"""

    _free_fn = staticmethod(tunio_udp_free)

    def recvfrom(self, size: int = 65535) -> tuple[bytes, tuple[str, int]]:
        """阻塞接收一个完整数据报，返回 (数据, (远端地址, 端口))。"""
        self._closed_guard("recvfrom")
        if size <= 0:
            raise ValueError("size 必须 > 0")
        buf = ctypes.create_string_buffer(size)
        ip = ctypes.create_string_buffer(64)
        port = ctypes.c_uint16()
        rc = tunio_udp_recvfrom(self._handle, buf, size, ip, len(ip), ctypes.byref(port))
        if rc < 0:
            raise_last_error("recvfrom: ")
        return buf.raw[:rc], (ip.value.decode("utf-8"), int(port.value))

    def sendto(
        self, data: bytes | bytearray | memoryview | str, remote_ip: str, remote_port: int
    ) -> int:
        """阻塞发送一个完整数据报到 (remote_ip, remote_port)。"""
        self._closed_guard("sendto")
        if isinstance(data, str):
            data = data.encode("utf-8")
        payload = bytes(data)
        if not payload:
            return 0
        rc = tunio_udp_sendto(self._handle, remote_ip.encode("utf-8"), int(remote_port),
                              payload, len(payload))
        if rc < 0:
            raise_last_error("sendto: ")
        return rc

    def close(self) -> None:
        if self._handle:
            tunio_udp_close(self._handle)

    def set_timeout(self, seconds: int) -> None:
        """设置会话空闲超时（秒）；0 恢复默认 30s。"""
        if self._handle:
            rc = tunio_udp_set_timeout(self._handle, int(seconds))
            if rc < 0:
                raise_last_error("set_timeout: ")

    @property
    def is_open(self) -> bool:
        return bool(self._handle) and bool(tunio_udp_is_open(self._handle))

    @property
    def client_endpoint(self) -> tuple[str, int]:
        """虚拟网内客户端端点 (地址, 端口)。"""
        self._closed_guard("client_endpoint")
        return _read_endpoint(tunio_udp_client_endpoint, self._handle)
