//
// packet_device_wintun.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "tunio/detail/impl/packet_device_wintun.hpp"

#include <algorithm>
#include <cstring>
#include <devguid.h>
#include <setupapi.h>

namespace tunio {
namespace detail {

namespace {

boost::system::error_code win_last_error()
{
    DWORD err = GetLastError();
    char buf[512] = {0};
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
                                 FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, err, 0, buf, sizeof(buf), nullptr);
    while (n > 0 &&
           (buf[n - 1] == '\r' || buf[n - 1] == '\n' || buf[n - 1] == ' '))
        buf[--n] = 0;
    return boost::system::error_code(static_cast<int>(err),
                                     boost::system::system_category());
}

static std::string narrow_from_wide(const wchar_t *ws)
{
    if (!ws)
        return {};
    int len =
        WideCharToMultiByte(CP_ACP, 0, ws, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return {};
    std::string result(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_ACP, 0, ws, -1, &result[0],
                        static_cast<int>(result.size()), nullptr, nullptr);
    while (!result.empty() && result.back() == '\0')
        result.pop_back();
    return result;
}

static std::wstring wide_from_narrow(const char *ns)
{
    if (!ns)
        return {};
    int len = MultiByteToWideChar(CP_ACP, 0, ns, -1, nullptr, 0);
    if (len <= 0)
        return {};
    std::wstring result(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_ACP, 0, ns, -1, &result[0],
                        static_cast<int>(result.size()));
    while (!result.empty() && result.back() == L'\0')
        result.pop_back();
    return result;
}

struct windows_driver_info
{
    std::string component_id; // lowercase ASCII
    std::string guid;  // NetCfgInstanceId string (GUID format like "{xxx}")
    std::wstring name; // adapter display name
};

void fill_connection_names(std::vector<windows_driver_info> &result)
{
    const wchar_t net_connections_key[] =
        L"SYSTEM\\CurrentControlSet\\Control\\Network\\"
        L"{4D36E972-E325-11CE-BFC1-08002BE10318}";

    HKEY key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, net_connections_key, 0, KEY_READ,
                      &key) != ERROR_SUCCESS)
        return;

    for (int i = 0;; ++i) {
        wchar_t enum_name[256] = {0};
        DWORD len = static_cast<DWORD>(sizeof(enum_name) / sizeof(wchar_t));
        if (RegEnumKeyExW(key, i, enum_name, &len, nullptr, nullptr, nullptr,
                          nullptr) != ERROR_SUCCESS)
            break;

        std::wstring conn = net_connections_key;
        conn += L"\\";
        conn += enum_name;
        conn += L"\\Connection";

        HKEY conn_key;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, conn.c_str(), 0, KEY_READ,
                          &conn_key) != ERROR_SUCCESS)
            continue;

        wchar_t name_buf[256] = {0};
        len = static_cast<DWORD>(sizeof(name_buf) / sizeof(wchar_t));
        if (RegQueryValueExW(conn_key, L"Name", nullptr, nullptr,
                             reinterpret_cast<LPBYTE>(name_buf),
                             &len) == ERROR_SUCCESS) {
            std::string ename_narrow = narrow_from_wide(enum_name);
            for (auto &dev : result)
                if (dev.guid == ename_narrow)
                    dev.name = name_buf;
        }
        RegCloseKey(conn_key);
    }
    RegCloseKey(key);
}

std::vector<std::pair<std::wstring, std::wstring>> enum_net_device_interfaces()
{
    std::vector<std::pair<std::wstring, std::wstring>> result;

    auto add_from_devinfo = [](HDEVINFO h_devs, const std::wstring &found_guid,
                               std::wstring &out_path) {
        SP_DEVINFO_DATA dev_info{};
        GUID iface_guid = GUID_DEVINTERFACE_NET;
        for (DWORD j = 0; SetupDiEnumDeviceInfo(h_devs, j, &dev_info); ++j) {
            SP_DEVICE_INTERFACE_DATA iface_data{};
            iface_data.cbSize = sizeof(iface_data);
            if (SetupDiEnumDeviceInterfaces(h_devs, &dev_info, &iface_guid, j,
                                            &iface_data)) {
                DWORD required = 0;
                SetupDiGetDeviceInterfaceDetailW(h_devs, &iface_data, nullptr,
                                                 0, &required, nullptr);
                auto buffer = std::make_unique<BYTE[]>(required);
                auto *detail =
                    reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(
                        buffer.get());
                detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) + 2;
                if (SetupDiGetDeviceInterfaceDetailW(h_devs, &iface_data,
                                                     detail, required,
                                                     &required, nullptr)) {
                    HKEY reg_hk = SetupDiOpenDevRegKey(
                        h_devs, &dev_info, DICS_FLAG_GLOBAL, 0, DIREG_DRV,
                        KEY_QUERY_VALUE);
                    if (reg_hk != INVALID_HANDLE_VALUE) {
                        wchar_t nid[256] = {0};
                        DWORD t = 0;
                        DWORD l =
                            static_cast<DWORD>(sizeof(nid) / sizeof(wchar_t));
                        if (RegQueryValueExW(reg_hk, L"NetCfgInstanceId",
                                             nullptr, &t,
                                             reinterpret_cast<LPBYTE>(nid),
                                             &l) == ERROR_SUCCESS) {
                            if (l >= 2 && std::wstring(nid) == found_guid) {
                                out_path = detail->DevicePath;
                            }
                        }
                        RegCloseKey(reg_hk);
                    }
                }
            }
        }
    };

    HDEVINFO h_all = SetupDiGetClassDevsExW(
        const_cast<GUID *>(&GUID_DEVCLASS_NET), nullptr, nullptr, DIGCF_PRESENT,
        nullptr, nullptr, nullptr);
    if (h_all == INVALID_HANDLE_VALUE)
        return result;

    SP_DEVINFO_DATA data{};
    for (DWORD i = 0; SetupDiEnumDeviceInfo(h_all, i, &data); ++i) {
        HKEY dev_key = SetupDiOpenDevRegKey(h_all, &data, DICS_FLAG_GLOBAL, 0,
                                            DIREG_DRV, KEY_QUERY_VALUE);
        if (dev_key != INVALID_HANDLE_VALUE) {
            wchar_t nid[256] = {0};
            DWORD t = 0;
            DWORD l = static_cast<DWORD>(sizeof(nid) / sizeof(wchar_t));
            if (RegQueryValueExW(dev_key, L"NetCfgInstanceId", nullptr, &t,
                                 reinterpret_cast<LPBYTE>(nid),
                                 &l) == ERROR_SUCCESS) {
                std::wstring path;
                add_from_devinfo(h_all, std::wstring(nid), path);
                if (!path.empty())
                    result.emplace_back(std::wstring(nid), std::move(path));
            }
            RegCloseKey(dev_key);
        }
    }

    SetupDiDestroyDeviceInfoList(h_all);
    return result;
}

HANDLE open_wintun_device(const std::wstring &name)
{
    const wchar_t adapter_key[] = L"SYSTEM\\CurrentControlSet\\Control\\Class\\"
                                  L"{4D36E972-E325-11CE-BFC1-08002BE10318}";
    std::vector<windows_driver_info> devs;

    HKEY key;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, adapter_key, 0, KEY_READ, &key) ==
        ERROR_SUCCESS) {
        for (int i = 0;; ++i) {
            wchar_t enum_name[256] = {0};
            DWORD len = static_cast<DWORD>(sizeof(enum_name) / sizeof(wchar_t));
            if (RegEnumKeyExW(key, i, enum_name, &len, nullptr, nullptr,
                              nullptr, nullptr) != ERROR_SUCCESS)
                break;

            std::wstring unit = adapter_key;
            unit += L"\\";
            unit += enum_name;

            HKEY unit_key;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, unit.c_str(), 0, KEY_READ,
                              &unit_key) != ERROR_SUCCESS)
                continue;

            char comp_id[256] = {0};
            char inst_id[256] = {0};
            DWORD type = 0;
            DWORD cplen = static_cast<DWORD>(sizeof(comp_id));
            bool has_comp =
                RegQueryValueExA(unit_key, "ComponentId", nullptr, &type,
                                 reinterpret_cast<LPBYTE>(comp_id),
                                 &cplen) == ERROR_SUCCESS;
            bool has_inst = false;
            if (has_comp) {
                std::transform(comp_id, comp_id + strlen(comp_id), comp_id,
                               [](unsigned char c) {
                                   return static_cast<char>(tolower(c));
                               });
                len = static_cast<DWORD>(sizeof(inst_id));
                has_inst =
                    RegQueryValueExA(unit_key, "NetCfgInstanceId", nullptr,
                                     &type, reinterpret_cast<LPBYTE>(inst_id),
                                     &len) == ERROR_SUCCESS;
            }

            if (has_comp && has_inst) {
                windows_driver_info dev;
                dev.component_id = comp_id;
                dev.guid.assign(inst_id);
                devs.push_back(std::move(dev));
            }
            RegCloseKey(unit_key);
        }
        RegCloseKey(key);
        fill_connection_names(devs);
    }

    windows_driver_info found;
    for (auto &dev : devs)
        if (dev.component_id.find("wintun") != std::string::npos &&
            dev.name == name) {
            found = dev;
            break;
        }

    if (found.guid.empty())
        return INVALID_HANDLE_VALUE;

    std::wstring found_guid_w = wide_from_narrow(found.guid.c_str());

    auto interfaces = enum_net_device_interfaces();
    for (auto &pair : interfaces)
        if (pair.first == found_guid_w)
            return CreateFileW(pair.second.data(), GENERIC_READ | GENERIC_WRITE,
                               0, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED,
                               nullptr);

    return INVALID_HANDLE_VALUE;
}

bool register_rings_ioct(HANDLE dev_handle, struct tun_ring *send_ring,
                         struct tun_ring *recv_ring, HANDLE send_evt,
                         HANDLE recv_evt)
{
    struct tun_register_rings rr;
    ZeroMemory(&rr, sizeof(rr));

    rr.send.ring_size = sizeof(struct tun_ring);
    rr.send.ring = send_ring;
    rr.send.tail_moved = send_evt;

    rr.receive.ring_size = sizeof(struct tun_ring);
    rr.receive.ring = recv_ring;
    rr.receive.tail_moved = recv_evt;

    DWORD bytes_returned;
    return DeviceIoControl(dev_handle, TUN_IOCTL_REGISTER_RINGS, &rr,
                           sizeof(rr), nullptr, 0, &bytes_returned,
                           nullptr) != FALSE;
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// Ring helpers

inline ULONG wintun_ring_wrap(ULONG value)
{
    return value & (WINTUN_RING_CAPACITY - 1);
}

inline ULONG wintun_ring_packet_align(ULONG size)
{
    return (size + (WINTUN_PACKET_ALIGN - 1)) & ~(WINTUN_PACKET_ALIGN - 1);
}

int wintun_packet_device_impl::recv_one(std::string_view buf)
{
    if (!opened_ || !recv_ring_)
        return -1;

    ULONG head = recv_ring_->head;
    ULONG tail = recv_ring_->tail;

    if (head >= WINTUN_RING_CAPACITY || tail >= WINTUN_RING_CAPACITY)
        return -1;
    if (head == tail)
        return 0;

    ULONG content_len = wintun_ring_wrap(tail - head);
    if (content_len < sizeof(struct TUN_PACKET_HEADER))
        return -1;

    auto *pkt = reinterpret_cast<struct TUN_PACKET *>(&recv_ring_->data[head]);
    if (pkt->size > WINTUN_MAX_IP_PACKET_SIZE)
        return -1;

    ULONG aligned =
        wintun_ring_packet_align(sizeof(struct TUN_PACKET_HEADER) + pkt->size);
    if (aligned > content_len)
        return -1;
    if (static_cast<ULONG>(buf.size()) < pkt->size)
        return -1;

    memcpy(const_cast<char *>(buf.data()), pkt->data, pkt->size);
    head = wintun_ring_wrap(head + aligned);
    recv_ring_->head = head;

    return static_cast<int>(pkt->size);
}

int wintun_packet_device_impl::send_try(std::string_view buf)
{
    if (!opened_ || !send_ring_)
        return -1;

    ULONG head = send_ring_->head;
    ULONG tail = send_ring_->tail;

    if (head >= WINTUN_RING_CAPACITY || tail >= WINTUN_RING_CAPACITY)
        return -1;

    ULONG aligned = wintun_ring_packet_align(sizeof(struct TUN_PACKET_HEADER) +
                                             static_cast<ULONG>(buf.size()));
    ULONG avail = wintun_ring_wrap(head - tail - WINTUN_PACKET_ALIGN);
    if (aligned > avail)
        return 0;

    auto *pkt = reinterpret_cast<struct TUN_PACKET *>(&send_ring_->data[tail]);
    pkt->size = static_cast<uint32_t>(buf.size());
    memcpy(pkt->data, buf.data(), buf.size());
    send_ring_->tail = wintun_ring_wrap(tail + aligned);

    if (send_ring_->alertable != 0)
        SetEvent(send_evt_);

    return static_cast<int>(buf.size());
}

//////////////////////////////////////////////////////////////////////////
// Lifecycle

void wintun_packet_device_impl::cleanup_rings()
{
    if (send_ring_) {
        UnmapViewOfFile(send_ring_);
        send_ring_ = nullptr;
    }
    if (recv_ring_) {
        UnmapViewOfFile(recv_ring_);
        recv_ring_ = nullptr;
    }
    if (send_evt_) {
        CloseHandle(send_evt_);
        send_evt_ = INVALID_HANDLE_VALUE;
    }
    if (recv_evt_) {
        CloseHandle(recv_evt_);
        recv_evt_ = INVALID_HANDLE_VALUE;
    }
    if (send_ring_fh_) {
        CloseHandle(send_ring_fh_);
        send_ring_fh_ = INVALID_HANDLE_VALUE;
    }
    if (recv_ring_fh_) {
        CloseHandle(recv_ring_fh_);
        recv_ring_fh_ = INVALID_HANDLE_VALUE;
    }
}

void wintun_packet_device_impl::setup_mtu(int mtu)
{
    if (!g_api || !wintun_handle_)
        return;

    NET_LUID luid;
    g_api->get_adapter_luid(wintun_handle_, &luid);

    NET_IFINDEX index = 0;
    if (ConvertInterfaceLuidToIndex(&luid, &index) != NO_ERROR)
        return;

    auto set_mtu_for_family = [&](ADDRESS_FAMILY family, int value) {
        MIB_IPINTERFACE_ROW row;
        ZeroMemory(&row, sizeof(row));
        InitializeIpInterfaceEntry(&row);
        row.Family = family;
        row.InterfaceIndex = index;
        if (GetIpInterfaceEntry(&row) == NO_ERROR) {
            row.NlMtu = static_cast<ULONG>(value);
            SetIpInterfaceEntry(&row);
        }
    };

    set_mtu_for_family(AF_INET, mtu > 0 ? mtu : 1500);
    set_mtu_for_family(AF_INET6, std::max(mtu > 0 ? mtu : 1500, 1280));
}

bool wintun_packet_device_impl::open(const device_config &cfg,
                                     boost::system::error_code &ec)
{
    close();

    std::string adapter_name = cfg.name.empty() ? "tunio" : cfg.name;
    g_api = wintun_api::load();

    if (!g_api) {
        ec = win_last_error();
        return false;
    }

    std::wstring wname(adapter_name.begin(), adapter_name.end());

    static const GUID adapter_guid = {
        0xDEADBA11,
        0xCAFE,
        0xBEEF,
        {0x01, 0x23, 0x45, 0x67, 0x00, 0x00, 0x00, 0xFF}};

    for (int n = 0; n < 5; ++n) {
        wintun_handle_ =
            g_api->create_adapter(wname.c_str(), L"Wintun", &adapter_guid);
        if (wintun_handle_)
            break;
        wintun_handle_ = g_api->open_adapter(wname.c_str());
        if (wintun_handle_)
            break;
    }

    if (!wintun_handle_) {
        cleanup_rings();
        ec = win_last_error();
        return false;
    }

    dev_handle_ = open_wintun_device(wname);
    if (dev_handle_ == INVALID_HANDLE_VALUE) {
        cleanup_rings();
        g_api->close_adapter(wintun_handle_);
        wintun_handle_ = nullptr;
        ec = win_last_error();
        return false;
    }

    send_ring_fh_ =
        CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                           sizeof(struct tun_ring), nullptr);
    recv_ring_fh_ =
        CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                           sizeof(struct tun_ring), nullptr);
    if (!send_ring_fh_ || !recv_ring_fh_) {
        cleanup_rings();
        CloseHandle(dev_handle_);
        dev_handle_ = INVALID_HANDLE_VALUE;
        g_api->close_adapter(wintun_handle_);
        wintun_handle_ = nullptr;
        ec = win_last_error();
        return false;
    }

    send_ring_ = static_cast<struct tun_ring *>(MapViewOfFile(
        send_ring_fh_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(struct tun_ring)));
    recv_ring_ = static_cast<struct tun_ring *>(MapViewOfFile(
        recv_ring_fh_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(struct tun_ring)));
    if (!send_ring_ || !recv_ring_) {
        cleanup_rings();
        CloseHandle(dev_handle_);
        dev_handle_ = INVALID_HANDLE_VALUE;
        g_api->close_adapter(wintun_handle_);
        wintun_handle_ = nullptr;
        ec = win_last_error();
        return false;
    }

    send_evt_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    recv_evt_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!send_evt_ || !recv_evt_) {
        cleanup_rings();
        CloseHandle(dev_handle_);
        dev_handle_ = INVALID_HANDLE_VALUE;
        g_api->close_adapter(wintun_handle_);
        wintun_handle_ = nullptr;
        ec = win_last_error();
        return false;
    }

    if (!register_rings_ioct(dev_handle_, send_ring_, recv_ring_, send_evt_,
                             recv_evt_)) {
        cleanup_rings();
        CloseHandle(dev_handle_);
        dev_handle_ = INVALID_HANDLE_VALUE;
        g_api->close_adapter(wintun_handle_);
        wintun_handle_ = nullptr;
        ec = win_last_error();
        return false;
    }

    send_ring_->head = 0;
    send_ring_->tail = 0;
    send_ring_->alertable = 1;
    recv_ring_->head = 0;
    recv_ring_->tail = 0;
    recv_ring_->alertable = 1;

    setup_mtu(static_cast<int>(cfg.mtu));

    opened_ = true;
    mtu_ = cfg.mtu > 0 ? cfg.mtu : 1500;
    return true;
}

bool wintun_packet_device_impl::assign(native_handle_type handle, size_t mtu,
                                       boost::system::error_code &ec)
{
    close();
    dev_handle_ = reinterpret_cast<HANDLE>(handle);
    if (dev_handle_ == INVALID_HANDLE_VALUE) {
        ec = win_last_error();
        return false;
    }

    send_ring_fh_ =
        CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                           sizeof(struct tun_ring), nullptr);
    recv_ring_fh_ =
        CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                           sizeof(struct tun_ring), nullptr);
    if (!send_ring_fh_ || !recv_ring_fh_) {
        CloseHandle(dev_handle_);
        dev_handle_ = INVALID_HANDLE_VALUE;
        ec = win_last_error();
        return false;
    }

    send_ring_ = static_cast<struct tun_ring *>(MapViewOfFile(
        send_ring_fh_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(struct tun_ring)));
    recv_ring_ = static_cast<struct tun_ring *>(MapViewOfFile(
        recv_ring_fh_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(struct tun_ring)));
    if (!send_ring_ || !recv_ring_) {
        cleanup_rings();
        CloseHandle(dev_handle_);
        dev_handle_ = INVALID_HANDLE_VALUE;
        ec = win_last_error();
        return false;
    }

    send_evt_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    recv_evt_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!send_evt_ || !recv_evt_) {
        cleanup_rings();
        CloseHandle(dev_handle_);
        dev_handle_ = INVALID_HANDLE_VALUE;
        ec = win_last_error();
        return false;
    }

    {
        struct tun_register_rings rr;
        ZeroMemory(&rr, sizeof(rr));
        rr.send.ring_size = sizeof(struct tun_ring);
        rr.send.ring = send_ring_;
        rr.send.tail_moved = send_evt_;
        rr.receive.ring_size = sizeof(struct tun_ring);
        rr.receive.ring = recv_ring_;
        rr.receive.tail_moved = recv_evt_;
        DWORD bytes_returned;
        DeviceIoControl(dev_handle_, TUN_IOCTL_REGISTER_RINGS, &rr, sizeof(rr),
                        nullptr, 0, &bytes_returned, nullptr);
    }

    send_ring_->head = 0;
    send_ring_->tail = 0;
    send_ring_->alertable = 1;
    recv_ring_->head = 0;
    recv_ring_->tail = 0;
    recv_ring_->alertable = 1;

    opened_ = true;
    mtu_ = mtu;
    return true;
}

void wintun_packet_device_impl::close()
{
    if (!opened_)
        return;
    opened_ = false;

    cleanup_rings();

    if (dev_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(dev_handle_);
        dev_handle_ = INVALID_HANDLE_VALUE;
    }

    if (wintun_handle_) {
        g_api->close_adapter(wintun_handle_);
        wintun_handle_ = nullptr;
    }
}

} // namespace detail
} // namespace tunio
