#
# _ffi.py
# ~~~~~~~
#
# tunio C API（bindings/c/include/tunio/c_api.h）的 ctypes 底层绑定。
#
# 该层只负责：加载 libtunio_c 共享库、声明函数原型、映射错误码。
# 面向用户的类（Engine/TcpSocket/UdpSocket）见包内 __init__.py。

from __future__ import annotations

import ctypes
import ctypes.util
import os
import sys
from pathlib import Path

# 错误码，与 c_api.h 中的 TUNIO_* 保持一致
TUNIO_OK = 0
TUNIO_EFAIL = -1
TUNIO_ECLOSED = -2
TUNIO_ECONNRESET = -3
TUNIO_EPARAM = -4
TUNIO_ENOMEM = -5


class TunioError(RuntimeError):
    """tunio 通用错误。"""


class Closed(TunioError):
    """引擎或连接已关闭，挂起操作被取消。"""


class ConnectionReset(TunioError):
    """TCP 连接被对端重置。"""


def _exception_for(code: int) -> type[Exception]:
    """错误码 -> Python 异常类型。"""
    if code == TUNIO_ECLOSED:
        return Closed
    if code == TUNIO_ECONNRESET:
        return ConnectionReset
    if code == TUNIO_EPARAM:
        return ValueError
    if code == TUNIO_ENOMEM:
        return MemoryError
    return TunioError


_DEFAULT_MESSAGES = {
    TUNIO_EFAIL: "通用错误",
    TUNIO_ECLOSED: "引擎或连接已关闭",
    TUNIO_ECONNRESET: "连接被对端重置",
    TUNIO_EPARAM: "非法参数",
    TUNIO_ENOMEM: "内存不足",
}


def _load_library():
    here = Path(__file__).resolve().parent
    if sys.platform == "win32":
        names = ("tunio_c.dll",)
    elif sys.platform == "darwin":
        names = ("libtunio_c.dylib", "libtunio_c.so")
    else:
        names = ("libtunio_c.so",)

    bases = []
    env = os.environ.get("TUNIO_LIBRARY_PATH")
    if env:
        bases.append(Path(env))
    bases.append(here)

    for base in bases:
        for name in names:
            path = base / name
            if path.is_file():
                return ctypes.CDLL(str(path))

    found = ctypes.util.find_library("tunio_c")
    if found:
        return ctypes.CDLL(found)

    raise ImportError(
        "未找到 tunio_c 共享库。请先构建：cmake -B build -DTUNIO_BUILD_PYTHON=ON "
        "&& cmake --build build，或将 libtunio_c.so 所在目录加入环境变量 "
        "TUNIO_LIBRARY_PATH"
    )


lib = _load_library()


class TunioConfig(ctypes.Structure):
    """与 c_api.h 的 tunio_config 保持一致的镜像结构。"""

    _fields_ = [
        ("dev_name", ctypes.c_char * 64),
        ("ipv4_addr", ctypes.c_char * 64),
        ("netmask", ctypes.c_char * 64),
        ("ipv6_addr", ctypes.c_char * 64),
        ("ipv6_prefix_len", ctypes.c_uint8),
        ("reserved0", ctypes.c_uint8 * 3),
        ("mtu", ctypes.c_uint32),
        ("num_queues", ctypes.c_uint32),
        ("external_handle", ctypes.c_int64),
        ("external_mtu", ctypes.c_uint32),
        ("utun_prefix", ctypes.c_uint8),
        ("reserved1", ctypes.c_uint8 * 7),
        ("num_external_handles", ctypes.c_uint32),
        ("external_handles", ctypes.c_int64 * 256),
        ("max_tcp_flows", ctypes.c_uint32),
        ("max_udp_flows", ctypes.c_uint32),
        ("max_rx_queue_per_flow", ctypes.c_uint64),
        ("tcp_ooo_max_segments", ctypes.c_uint32),
        ("max_total_buffer", ctypes.c_uint64),
        ("udp_idle_timeout_sec", ctypes.c_uint32),
        ("tcp_time_wait_timeout_sec", ctypes.c_uint32),
        ("tcp_accept_timeout_sec", ctypes.c_uint32),
        ("tcp_syn_timeout_sec", ctypes.c_uint32),
        ("tcp_close_timeout_sec", ctypes.c_uint32),
        ("tcp_persist_timeout_ms", ctypes.c_uint32),
        ("tcp_persist_max_probes", ctypes.c_int32),
        ("tcp_rto_timeout_ms", ctypes.c_uint32),
        ("tcp_rto_max_retransmits", ctypes.c_int32),
    ]


class TunioStats(ctypes.Structure):
    """与 c_api.h 的 tunio_stats 保持一致的镜像结构。"""

    _fields_ = [
        ("rx_packets", ctypes.c_uint64),
        ("tx_packets", ctypes.c_uint64),
        ("rx_dropped", ctypes.c_uint64),
        ("rx_ooo", ctypes.c_uint64),
        ("tcp_connections", ctypes.c_uint64),
        ("udp_sessions", ctypes.c_uint64),
        ("icmp_replies", ctypes.c_uint64),
    ]


def _bind(name, restype, argtypes):
    fn = getattr(lib, name)
    fn.restype = restype
    fn.argtypes = argtypes
    return fn


# ---- 错误信息 ----
tunio_last_error_code = _bind("tunio_last_error_code", ctypes.c_int, [])
tunio_last_error_message = _bind("tunio_last_error_message", ctypes.c_char_p, [])

# ---- 配置 ----
tunio_config_init = _bind("tunio_config_init", None, [ctypes.POINTER(TunioConfig)])

# ---- 引擎 ----
tunio_engine_new = _bind("tunio_engine_new", ctypes.c_void_p, [ctypes.c_int])
tunio_engine_free = _bind("tunio_engine_free", None, [ctypes.c_void_p])
tunio_engine_open = _bind(
    "tunio_engine_open", ctypes.c_int, [ctypes.c_void_p, ctypes.POINTER(TunioConfig)]
)
tunio_engine_close = _bind("tunio_engine_close", ctypes.c_int, [ctypes.c_void_p])
tunio_engine_is_open = _bind("tunio_engine_is_open", ctypes.c_int, [ctypes.c_void_p])
tunio_engine_mtu = _bind("tunio_engine_mtu", ctypes.c_uint32, [ctypes.c_void_p])
tunio_engine_queue_count = _bind(
    "tunio_engine_queue_count", ctypes.c_uint32, [ctypes.c_void_p]
)
tunio_engine_local_address = _bind(
    "tunio_engine_local_address", ctypes.c_int, [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
)
tunio_engine_stats = _bind(
    "tunio_engine_stats", ctypes.c_int, [ctypes.c_void_p, ctypes.POINTER(TunioStats)]
)

# ---- TCP ----
tunio_tcp_accept = _bind("tunio_tcp_accept", ctypes.c_void_p, [ctypes.c_void_p])
tunio_tcp_recv = _bind(
    "tunio_tcp_recv", ctypes.c_int, [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
)
tunio_tcp_send = _bind(
    "tunio_tcp_send", ctypes.c_int, [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t]
)
tunio_tcp_close = _bind("tunio_tcp_close", ctypes.c_int, [ctypes.c_void_p])
tunio_tcp_reset = _bind("tunio_tcp_reset", ctypes.c_int, [ctypes.c_void_p])
tunio_tcp_is_open = _bind("tunio_tcp_is_open", ctypes.c_int, [ctypes.c_void_p])
tunio_tcp_original_destination = _bind(
    "tunio_tcp_original_destination",
    ctypes.c_int,
    [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_uint16)],
)
tunio_tcp_remote_endpoint = _bind(
    "tunio_tcp_remote_endpoint",
    ctypes.c_int,
    [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_uint16)],
)
tunio_tcp_free = _bind("tunio_tcp_free", None, [ctypes.c_void_p])

# ---- UDP ----
tunio_udp_accept = _bind("tunio_udp_accept", ctypes.c_void_p, [ctypes.c_void_p])
tunio_udp_recvfrom = _bind(
    "tunio_udp_recvfrom",
    ctypes.c_int,
    [
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_size_t,
        ctypes.c_char_p,
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint16),
    ],
)
tunio_udp_sendto = _bind(
    "tunio_udp_sendto",
    ctypes.c_int,
    [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_uint16, ctypes.c_void_p, ctypes.c_size_t],
)
tunio_udp_close = _bind("tunio_udp_close", ctypes.c_int, [ctypes.c_void_p])
tunio_udp_is_open = _bind("tunio_udp_is_open", ctypes.c_int, [ctypes.c_void_p])
tunio_udp_client_endpoint = _bind(
    "tunio_udp_client_endpoint",
    ctypes.c_int,
    [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_uint16)],
)
tunio_udp_set_timeout = _bind(
    "tunio_udp_set_timeout", ctypes.c_int, [ctypes.c_void_p, ctypes.c_int64]
)
tunio_udp_free = _bind("tunio_udp_free", None, [ctypes.c_void_p])


def last_error_message() -> str:
    """线程最近一次失败的错误描述（无描述时返回空串）。"""
    msg = tunio_last_error_message()
    return msg.decode("utf-8", "replace") if msg else ""


def raise_last_error(prefix: str = "") -> None:
    """按线程最近一次错误码抛出对应异常。"""
    code = tunio_last_error_code()
    raise_exception(code, prefix + last_error_message() or _DEFAULT_MESSAGES.get(code, "未知错误"))


def raise_exception(code: int, message: str) -> None:
    exc = _exception_for(code)
    raise exc(message)
