//
// tun_device_windows.cpp
// ~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tunio/tun_device.hpp"

#if defined(BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR)

#include <winsock2.h> // 必须在 windows.h 之前，避免类型冲突

#include <iphlpapi.h>
#include <windows.h>
#include <winioctl.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// TAP-Windows 驱动用户态接口（与 OpenVPN tap-windows 驱动一致）。
// 参照 avpn 的 tuntap_windows_service 实现。
// ---------------------------------------------------------------------------

#ifndef TUNTAP_IOCTL_DEFINED
#define TUNTAP_IOCTL_DEFINED

#define TAP_CONTROL_CODE(request, method)                                      \
    CTL_CODE(FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)

#define TAP_IOCTL_GET_MAC TAP_CONTROL_CODE(1, METHOD_BUFFERED)
#define TAP_IOCTL_GET_VERSION TAP_CONTROL_CODE(2, METHOD_BUFFERED)
#define TAP_IOCTL_GET_MTU TAP_CONTROL_CODE(3, METHOD_BUFFERED)
#define TAP_IOCTL_GET_INFO TAP_CONTROL_CODE(4, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_POINT_TO_POINT TAP_CONTROL_CODE(5, METHOD_BUFFERED)
#define TAP_IOCTL_SET_MEDIA_STATUS TAP_CONTROL_CODE(6, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_DHCP_MASQ TAP_CONTROL_CODE(7, METHOD_BUFFERED)
#define TAP_IOCTL_GET_LOG_LINE TAP_CONTROL_CODE(8, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_DHCP_SET_OPT TAP_CONTROL_CODE(9, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_TUN TAP_CONTROL_CODE(10, METHOD_BUFFERED)

#endif // !TUNTAP_IOCTL_DEFINED

namespace tunio {

namespace detail {

namespace {

// 注册表中网络适配器类（网卡）与网络连接分支。
const wchar_t k_adapter_key[] = L"SYSTEM\\CurrentControlSet\\Control\\Class\\"
                                L"{4D36E972-E325-11CE-BFC1-08002BE10318}";
const wchar_t k_network_connections_key[] =
    L"SYSTEM\\CurrentControlSet\\Control\\Network\\"
    L"{4D36E972-E325-11CE-BFC1-08002BE10318}";

// 单个 TAP 网卡信息。
struct tap_device_info
{
    std::string
        component_id; // ComponentId 归一化（小写、去 root\ 前缀），如 "tap0901"
    std::string guid; // NetCfgInstanceId，如 "{XXXXXXXX-...}"
    std::string name; // 网卡显示名（UTF-8），如 "TAP-Windows Adapter V9"
    std::string driver_desc; // DriverDesc（UTF-8），显示名兜底
};

boost::system::error_code win_last_error()
{
    const DWORD err = ::GetLastError();
    return boost::system::error_code(
        static_cast<int>(err), boost::system::system_category());
}

std::string ascii_lower(std::string s)
{
    std::transform(s.begin(),
        s.end(),
        s.begin(),
        [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
    return s;
}

bool iequals(const std::string& lhs, const std::string& rhs)
{
    if (lhs.size() != rhs.size())
        return false;
    for (size_t i = 0; i < lhs.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
            std::tolower(static_cast<unsigned char>(rhs[i])))
            return false;
    }
    return true;
}

// 去掉 GUID 字符串中的花括号（"{xxx}" -> "xxx"）。
std::string strip_braces(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
        if (c != '{' && c != '}')
            out.push_back(c);
    return out;
}

std::wstring widen(const std::string& s)
{
    if (s.empty())
        return {};
    const int len =
        ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (len <= 0)
        return {};
    std::wstring result(static_cast<size_t>(len), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &result[0], len);
    while (!result.empty() && result.back() == L'\0')
        result.pop_back();
    return result;
}

std::string narrow(const std::wstring& ws)
{
    if (ws.empty())
        return {};

    const int len = ::WideCharToMultiByte(
        CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return {};

    std::string result(static_cast<size_t>(len), '\0');
    ::WideCharToMultiByte(
        CP_UTF8, 0, ws.c_str(), -1, &result[0], len, nullptr, nullptr);

    while (!result.empty() && result.back() == '\0')
        result.pop_back();
    return result;
}

// 归一化 ComponentId：小写并去掉 "root\" 前缀。新版 OpenVPN 安装器
// （2.5.6+）创建的 TAP 适配器是根枚举设备，ComponentId 形如
// "root\tap0901"（参考 yarrick/iodine issue #73）。
std::string normalize_component_id(std::string id)
{
    id = ascii_lower(id);
    if (id.starts_with("root\\"))
        id.erase(0, 5); // strlen("root\")
    return id;
}

// 是否为 TAP 驱动：显式支持 tap0901、tapnordvpn、tap-tb-0901 等常见
// ComponentId，以及任何以 "tap" 开头的变体（tap0801、tapoas 等）。
// 入参为已归一化的 ComponentId（无 "root\" 前缀）。
bool is_tap_component(const std::string& component_id)
{
    static const char* const known[] = {"tap0901", "tapnordvpn", "tap-tb-0901"};
    for (const char* k : known)
        if (component_id == k)
            return true;
    return component_id.starts_with("tap");
}

// 枚举注册表中的 TAP 网卡，返回 {component_id, guid，显示名} 列表。
std::vector<tap_device_info> enum_tap_devices()
{
    std::vector<tap_device_info> result;

    HKEY key;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, k_adapter_key, 0, KEY_READ, &key) !=
        ERROR_SUCCESS)
        return result;

    for (int i = 0;; ++i)
    {
        wchar_t enum_name[256] = {0};
        DWORD len = static_cast<DWORD>(sizeof(enum_name) / sizeof(wchar_t));
        if (::RegEnumKeyExW(
                key, i, enum_name, &len, nullptr, nullptr, nullptr, nullptr) !=
            ERROR_SUCCESS)
            break;

        std::wstring unit = k_adapter_key;
        unit += L"\\";
        unit += enum_name;

        HKEY unit_key;
        if (::RegOpenKeyExW(
                HKEY_LOCAL_MACHINE, unit.c_str(), 0, KEY_READ, &unit_key) !=
            ERROR_SUCCESS)
            continue;

        char comp_id[256] = {0};
        char match_id[256] = {0};
        char inst_id[256] = {0};
        wchar_t desc_id[256] = {0};
        DWORD type = 0;
        DWORD comp_len = static_cast<DWORD>(sizeof(comp_id));
        const bool has_comp = ::RegQueryValueExA(unit_key,
                                  "ComponentId",
                                  nullptr,
                                  &type,
                                  reinterpret_cast<LPBYTE>(comp_id),
                                  &comp_len) == ERROR_SUCCESS;
        // NetCfgInstanceId 独立读取：不依赖 ComponentId 是否存在，
        // 以便 ComponentId 缺失时仍能走 MatchingDeviceId 兜底。
        len = static_cast<DWORD>(sizeof(inst_id));
        const bool has_inst = ::RegQueryValueExA(unit_key,
                                  "NetCfgInstanceId",
                                  nullptr,
                                  &type,
                                  reinterpret_cast<LPBYTE>(inst_id),
                                  &len) == ERROR_SUCCESS;
        bool has_match = false;
        {
            DWORD mlen = static_cast<DWORD>(sizeof(match_id));
            has_match = ::RegQueryValueExA(unit_key,
                            "MatchingDeviceId",
                            nullptr,
                            &type,
                            reinterpret_cast<LPBYTE>(match_id),
                            &mlen) == ERROR_SUCCESS;
        }
        bool has_desc = false;
        {
            // DriverDesc 可能本地化（非 ASCII），用宽字符 API 读取。
            DWORD dlen = static_cast<DWORD>(sizeof(desc_id));
            has_desc = ::RegQueryValueExW(unit_key,
                           L"DriverDesc",
                           nullptr,
                           &type,
                           reinterpret_cast<LPBYTE>(desc_id),
                           &dlen) == ERROR_SUCCESS;
        }
        ::RegCloseKey(unit_key);

        if (!has_inst)
            continue;

        // 硬件 ID 信号：优先 ComponentId，缺失时退回 MatchingDeviceId，
        // 两者都归一化（小写 + 去 "root\" 前缀）。
        std::string component_id = has_comp
            ? normalize_component_id(comp_id)
            : (has_match ? normalize_component_id(match_id) : std::string());
        const bool is_tap = is_tap_component(component_id) ||
            (has_desc &&
                ascii_lower(narrow(desc_id)).find("tap") != std::string::npos);

        if (is_tap)
        {
            tap_device_info dev;
            dev.component_id = std::move(component_id);
            dev.guid.assign(inst_id);
            if (has_desc)
                dev.driver_desc = narrow(desc_id);
            result.push_back(std::move(dev));
        }
    }
    ::RegCloseKey(key);

    // 读取网卡显示名（网络连接分支：...\<GUID>\Connection 下的 Name）。
    if (::RegOpenKeyExW(
            HKEY_LOCAL_MACHINE, k_network_connections_key, 0, KEY_READ, &key) !=
        ERROR_SUCCESS)
        return result;

    for (int i = 0;; ++i)
    {
        wchar_t enum_name[256] = {0};
        DWORD len = static_cast<DWORD>(sizeof(enum_name) / sizeof(wchar_t));
        if (::RegEnumKeyExW(
                key, i, enum_name, &len, nullptr, nullptr, nullptr, nullptr) !=
            ERROR_SUCCESS)
            break;

        std::wstring conn = k_network_connections_key;
        conn += L"\\";
        conn += enum_name;
        conn += L"\\Connection";

        HKEY conn_key;
        if (::RegOpenKeyExW(
                HKEY_LOCAL_MACHINE, conn.c_str(), 0, KEY_READ, &conn_key) !=
            ERROR_SUCCESS)
            continue;

        wchar_t name_buf[256] = {0};
        len = static_cast<DWORD>(sizeof(name_buf) / sizeof(wchar_t));
        if (::RegQueryValueExW(conn_key,
                L"Name",
                nullptr,
                nullptr,
                reinterpret_cast<LPBYTE>(name_buf),
                &len) == ERROR_SUCCESS)
        {
            const std::string guid = narrow(enum_name);
            const std::string name = narrow(name_buf);
            for (auto& dev : result)
                if (iequals(dev.guid, guid))
                    dev.name = name;
        }
        ::RegCloseKey(conn_key);
    }
    ::RegCloseKey(key);

    // 显示名兜底：网络连接分支缺失时退回 DriverDesc（TAP-Windows
    // Adapter V9 的 DriverDesc 与显示名通常一致）。
    for (auto& dev : result)
        if (dev.name.empty())
            dev.name = dev.driver_desc;

    return result;
}

// 根据 device_config.name 解析目标 TAP 网卡的 GUID。支持：
//   ① 空名 -> 第一块 TAP 网卡；
//   ② GUID（带/不带花括号，大小写不敏感）；
//   ③ 设备路径，如 "\\.\Global\{GUID}.tap"；
//   ④ 网卡显示名（如 "TAP-Windows Adapter V9"）；
//   ⑤ ComponentId（如 "tap0901" 或 "root\tap0901" -> 第一块该驱动的网卡）。
std::string resolve_tap_guid(
    const std::vector<tap_device_info>& devs, const std::string& name)
{
    if (devs.empty())
        return {};

    if (name.empty())
        return devs.front().guid;

    // 设备路径形式：\\.\Global\{GUID}.tap
    if (name.size() > 4 && name.compare(0, 4, "\\\\.\\") == 0)
    {
        const size_t open = name.find('{');
        const size_t close = name.find('}', open);
        if (open != std::string::npos && close != std::string::npos)
        {
            const std::string guid = name.substr(open, close - open + 1);
            for (const auto& dev : devs)
                if (iequals(dev.guid, guid))
                    return dev.guid;
        }
        return {};
    }

    const std::string bare = strip_braces(name);

    for (const auto& dev : devs)
    {
        if (iequals(dev.guid, name) || iequals(strip_braces(dev.guid), bare))
            return dev.guid;
    }

    // 显示名 / ComponentId（输入与存储的 component_id 均做归一化，
    // 因此 "tap0901" 与 "root\tap0901" 写法都能匹配）。
    const std::string norm_name = normalize_component_id(name);
    for (const auto& dev : devs)
    {
        if (iequals(dev.name, name) || iequals(dev.component_id, norm_name))
            return dev.guid;
    }

    return {};
}

// 同步 Windows IP 协议栈的接口 MTU（GetAdapterIndex + SetIpInterfaceEntry），
// 使驱动 MTU 与协议栈 MTU 一致；失败时忽略（需要管理员权限等）。
void set_interface_mtu(const std::string& guid, int mtu)
{
    if (mtu <= 0)
        return;

    // GetAdapterIndex 的声明为 LPWSTR（非 const）。
    std::wstring name = L"\\DEVICE\\TCPIP_" + widen(guid);
    ULONG index = 0;
    if (::GetAdapterIndex(&name[0], &index) != NO_ERROR)
        return;

    auto set_family_mtu = [&](ADDRESS_FAMILY family, int value)
    {
        MIB_IPINTERFACE_ROW row;
        ZeroMemory(&row, sizeof(row));
        InitializeIpInterfaceEntry(&row);
        row.Family = family;
        row.InterfaceIndex = index;
        if (GetIpInterfaceEntry(&row) == NO_ERROR)
        {
            row.NlMtu = static_cast<ULONG>(value);
            SetIpInterfaceEntry(&row);
        }
    };

    set_family_mtu(AF_INET, mtu);
    // IPv6 最小 MTU 为 1280，避免低于标准导致 IPv6 不可用。
    set_family_mtu(AF_INET6, std::max(mtu, 1280));
}

} // namespace

bool windows_tun_device_impl::assign(
    native_handle_type handle, size_t mtu, bool, boost::system::error_code& ec)
{
    handle_.assign(handle, ec);
    if (!ec)
    {
        open_ = true;
        mtu_ = mtu;
    }
    return !ec;
}

bool windows_tun_device_impl::assign_queues(
    const std::vector<native_handle_type>&,
    size_t,
    bool,
    boost::system::error_code& ec)
{
    // Windows TAP 驱动无多队列概念：注入多个句柄不支持。
    ec = make_error_code(boost::system::errc::operation_not_supported);
    return false;
}

bool windows_tun_device_impl::open(
    const device_config& cfg, boost::system::error_code& ec)
{
    close();

    const std::vector<tap_device_info> devs = enum_tap_devices();
    const std::string guid = resolve_tap_guid(devs, cfg.name);
    if (guid.empty())
    {
        ec = boost::system::error_code(static_cast<int>(ERROR_FILE_NOT_FOUND),
            boost::system::system_category());
        return false;
    }

    const std::wstring device_path = L"\\\\.\\Global\\" + widen(guid) + L".tap";

    const HANDLE handle = ::CreateFileW(device_path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        ec = win_last_error();
        return false;
    }

    DWORD bytes = 0;

    // TAP 驱动版本校验：确认打开的确实是 TAP 设备。
    struct
    {
        unsigned long major;
        unsigned long minor;
        unsigned long debug;
    } version = {0, 0, 0};
    if (!::DeviceIoControl(handle,
            TAP_IOCTL_GET_VERSION,
            &version,
            sizeof(version),
            &version,
            sizeof(version),
            &bytes,
            nullptr))
    {
        ec = win_last_error();
        ::CloseHandle(handle);
        return false;
    }

    // 切换到 TUN 模式（剥离以太网头，仅透传 IP 报文）。
    // 未配置 IPv4 时传入全零地址：仍启用 TUN 模式，不做子网过滤。
    uint32_t tun_addrs[3] = {0, 0, 0};
    if (!cfg.ipv4.empty())
    {
        const std::string mask =
            cfg.netmask.empty() ? std::string("255.255.255.0") : cfg.netmask;
        if (::inet_pton(AF_INET, cfg.ipv4.c_str(), &tun_addrs[0]) != 1 ||
            ::inet_pton(AF_INET, mask.c_str(), &tun_addrs[1]) != 1)
        {
            ec = boost::system::error_code(
                static_cast<int>(WSAEINVAL), boost::system::system_category());
            ::CloseHandle(handle);
            return false;
        }

        tun_addrs[2] = tun_addrs[1];                // mask
        tun_addrs[1] = tun_addrs[2] & tun_addrs[0]; // network = ip & mask
    }
    if (!::DeviceIoControl(handle,
            TAP_IOCTL_CONFIG_TUN,
            tun_addrs,
            sizeof(tun_addrs),
            tun_addrs,
            sizeof(tun_addrs),
            &bytes,
            nullptr))
    {
        ec = win_last_error();
        ::CloseHandle(handle);
        return false;
    }

    // 查询驱动 MTU。
    ULONG driver_mtu = 0;
    if (!::DeviceIoControl(handle,
            TAP_IOCTL_GET_MTU,
            &driver_mtu,
            sizeof(driver_mtu),
            &driver_mtu,
            sizeof(driver_mtu),
            &bytes,
            nullptr) ||
        driver_mtu == 0)
    {
        driver_mtu = 1500;
    }
    size_t mtu = cfg.mtu > 0 ? cfg.mtu : static_cast<size_t>(driver_mtu);
    mtu = std::max<size_t>(mtu, 576);

    // 模拟 DHCP，让 Windows 自动为适配器分配地址（尽力而为，失败忽略；
    // 不配置时适配器保持无 IP，仍可正常读写 IP 报文）。
    // 参数与 OpenVPN tuntap_dhcp_mask 一致：
    //   ep[0] = 分配的客户端地址（local）
    //   ep[1] = 子网掩码
    //   ep[2] = DHCP 服务器伪装地址（取网络地址，满足驱动
    //           CheckIfDhcpAndTunMode 的归属校验）
    //   ep[3] = 租约时间（秒），必须 > 0
    if (!cfg.ipv4.empty())
    {
        uint32_t ep[4] = {tun_addrs[0], tun_addrs[2], tun_addrs[1], 86400};
        ::DeviceIoControl(handle,
            TAP_IOCTL_CONFIG_DHCP_MASQ,
            ep,
            sizeof(ep),
            ep,
            sizeof(ep),
            &bytes,
            nullptr);
    }

    // 启用链路（media status）。
    {
        ULONG status = TRUE;
        if (!::DeviceIoControl(handle,
                TAP_IOCTL_SET_MEDIA_STATUS,
                &status,
                sizeof(status),
                &status,
                sizeof(status),
                &bytes,
                nullptr))
        {
            ec = win_last_error();
            ::CloseHandle(handle);
            return false;
        }
    }

    // 同步协议栈 MTU（尽力而为）。
    set_interface_mtu(guid, static_cast<int>(mtu));

    // 交给 Asio 接管句柄（Overlapped I/O）。
    handle_.assign(handle, ec);
    if (ec)
    {
        ::CloseHandle(handle);
        return false;
    }

    open_ = true;
    mtu_ = mtu;
    return true;
}

void windows_tun_device_impl::close()
{
    if (!open_)
        return;
    open_ = false;

    const HANDLE handle = handle_.native_handle();
    if (handle != INVALID_HANDLE_VALUE)
    {
        // 关闭前先置链路 down，避免残留的假媒体状态。
        ULONG status = FALSE;
        DWORD bytes = 0;
        ::DeviceIoControl(handle,
            TAP_IOCTL_SET_MEDIA_STATUS,
            &status,
            sizeof(status),
            &status,
            sizeof(status),
            &bytes,
            nullptr);
    }

    boost::system::error_code ignore;
    handle_.cancel(ignore);
    handle_.close(ignore);
}

} // namespace detail

} // namespace tunio

#endif // BOOST_ASIO_HAS_WINDOWS_OVERLAPPED_PTR
