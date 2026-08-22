//
// wintun_tun_device.cpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2026 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "proxy/wintun_tun_device.hpp"

#include <setupapi.h>
#include <devguid.h>

#include <algorithm>
#include <filesystem>
#include <vector>
#include <string>
#include <cctype>
#include <chrono>
#include <cstring>

namespace proxy {

	namespace {

		// Windows 错误码转字符串.
		std::string win_error(DWORD code)
		{
			char buf[512] = { 0 };
			DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
				buf, sizeof(buf), nullptr);
			while (n > 0 && (buf[n - 1] == '\r' || buf[n - 1] == '\n' ||
				buf[n - 1] == ' '))
				buf[--n] = 0;
			return std::string(buf, n);
		}

		struct windows_driver
		{
			std::string component_id_;	// 小写.
			std::string guid_;
			std::string name_;
		};

		// 枚举已安装网卡驱动 (registry), 参考 avpn utils::enum_windows_devices.
		std::vector<windows_driver> enum_windows_devices()
		{
			const char* adapter_key = "SYSTEM\\CurrentControlSet\\Control\\"
				"Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}";
			const char* network_connections_key = "SYSTEM\\CurrentControlSet\\"
				"Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}";
			std::vector<windows_driver> result;

			HKEY key;
			if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, adapter_key, 0,
					KEY_READ, &key) != ERROR_SUCCESS)
				return result;

			for (int i = 0;; i++)
			{
				char enum_name[256] = { 0 };
				DWORD len = 256;
				if (RegEnumKeyExA(key, i, enum_name, &len, nullptr,
						nullptr, nullptr, nullptr) != ERROR_SUCCESS)
					break;

				std::string unit = std::string(adapter_key) + "\\" + enum_name;
				HKEY unit_key;
				if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, unit.data(), 0,
						KEY_READ, &unit_key) != ERROR_SUCCESS)
					continue;

				char component_id[256] = { 0 };
				char instance_id[256] = { 0 };
				DWORD type = 0;
				len = 256;
				if (RegQueryValueExA(unit_key, "ComponentId", nullptr,
						&type, reinterpret_cast<LPBYTE>(component_id),
						&len) != ERROR_SUCCESS)
				{
					RegCloseKey(unit_key);
					continue;
				}
				len = 256;
				if (RegQueryValueExA(unit_key, "NetCfgInstanceId", nullptr,
						&type, reinterpret_cast<LPBYTE>(instance_id),
						&len) != ERROR_SUCCESS)
				{
					RegCloseKey(unit_key);
					continue;
				}
				RegCloseKey(unit_key);

				windows_driver dev;
				dev.component_id_ = component_id;
				std::transform(dev.component_id_.begin(),
					dev.component_id_.end(), dev.component_id_.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				dev.guid_ = instance_id;
				result.push_back(std::move(dev));
			}
			RegCloseKey(key);

			// 从网络连接信息中获取适配器名称 (如 "AVPN").
			if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, network_connections_key, 0,
					KEY_READ, &key) != ERROR_SUCCESS)
				return result;

			for (int i = 0;; i++)
			{
				char enum_name[256] = { 0 };
				DWORD len = 256;
				if (RegEnumKeyExA(key, i, enum_name, &len, nullptr,
						nullptr, nullptr, nullptr) != ERROR_SUCCESS)
					break;

				std::string conn = std::string(network_connections_key) +
					"\\" + enum_name + "\\Connection";
				HKEY conn_key;
				if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, conn.data(), 0,
						KEY_READ, &conn_key) != ERROR_SUCCESS)
					continue;

				char name[256] = { 0 };
				len = 256;
				if (RegQueryValueExA(conn_key, "Name", nullptr, nullptr,
						reinterpret_cast<LPBYTE>(name), &len) != ERROR_SUCCESS)
				{
					RegCloseKey(conn_key);
					continue;
				}
				RegCloseKey(conn_key);

				std::string guid = enum_name;
				for (auto& dev : result)
					if (dev.guid_ == guid)
						dev.name_ = name;
			}
			RegCloseKey(key);

			return result;
		}

		// 枚举网卡设备接口, 返回 (NetCfgInstanceId, 设备路径) 列表.
		std::vector<std::pair<std::string, std::string>>
		enum_device_interfaces()
		{
			std::vector<std::pair<std::string, std::string>> result;
			HDEVINFO set = SetupDiGetClassDevsEx(
				const_cast<LPGUID>(&GUID_DEVCLASS_NET), nullptr,
				nullptr, DIGCF_PRESENT, nullptr, nullptr, nullptr);
			if (set == INVALID_HANDLE_VALUE)
				return result;

			for (DWORD i = 0;; i++)
			{
				SP_DEVINFO_DATA data;
				std::memset(&data, 0, sizeof(data));
				data.cbSize = sizeof(data);
				if (!SetupDiEnumDeviceInfo(set, i, &data))
				{
					if (GetLastError() == ERROR_NO_MORE_ITEMS)
						break;
					continue;
				}

				HKEY dev_key = SetupDiOpenDevRegKey(set, &data,
					DICS_FLAG_GLOBAL, 0, DIREG_DRV, KEY_QUERY_VALUE);
				if (dev_key == INVALID_HANDLE_VALUE)
					continue;

				char instance_id[256] = { 0 };
				DWORD len = 256;
				DWORD type = REG_SZ;
				LONG status = RegQueryValueExA(dev_key, "NetCfgInstanceId",
					nullptr, &type, reinterpret_cast<LPBYTE>(instance_id), &len);
				RegCloseKey(dev_key);
				if (status != ERROR_SUCCESS)
					continue;

				char device_id[256] = { 0 };
				len = 256;
				if (!SetupDiGetDeviceInstanceIdA(set, &data, device_id,
						len, &len))
					continue;

				ULONG size = 0;
				if (CM_Get_Device_Interface_List_SizeA(&size,
						const_cast<LPGUID>(&GUID_DEVINTERFACE_NET),
						device_id,
						CM_GET_DEVICE_INTERFACE_LIST_PRESENT) != CR_SUCCESS)
					continue;

				std::string list(size, '\0');
				if (CM_Get_Device_Interface_ListA(
						const_cast<LPGUID>(&GUID_DEVINTERFACE_NET),
						device_id, list.data(), size,
						CM_GET_DEVICE_INTERFACE_LIST_PRESENT) != CR_SUCCESS)
					continue;

				// 接口列表为多字符串, 取第一个.
				const char* p = list.data();
				if (p && *p)
					result.emplace_back(instance_id, p);
			}

			SetupDiDestroyDeviceInfoList(set);
			return result;
		}

		// 将 exe 内嵌资源解压到文件.
		bool resource_copy_to_file(const char* name, const std::wstring& dest)
		{
			HRSRC res = FindResourceA(nullptr, name,
				reinterpret_cast<LPCSTR>(RT_RCDATA));
			if (!res)
				return false;
			HGLOBAL data = LoadResource(nullptr, res);
			if (!data)
				return false;
			void* ptr = LockResource(data);
			DWORD size = SizeofResource(nullptr, res);
			if (!ptr || size == 0)
				return false;

			HANDLE file = CreateFileW(dest.data(), GENERIC_WRITE, 0, nullptr,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file == INVALID_HANDLE_VALUE)
				return false;

			DWORD written = 0;
			BOOL ok = WriteFile(file, ptr, size, &written, nullptr);
			CloseHandle(file);
			return ok && written == size;
		}

		// 执行命令并等待结束, 返回退出码是否为 0.
		bool run_command(const std::wstring& cmdline)
		{
			STARTUPINFOW si;
			std::memset(&si, 0, sizeof(si));
			si.cb = sizeof(si);
			PROCESS_INFORMATION pi;
			std::memset(&pi, 0, sizeof(pi));

			std::wstring cmd = cmdline;
			if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
					CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
				return false;

			WaitForSingleObject(pi.hProcess, INFINITE);
			DWORD code = 0;
			GetExitCodeProcess(pi.hProcess, &code);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			return code == 0;
		}
	}

	namespace wintun_detail {

		// wintun.dll 适配器管理 API.
		struct wintun_api
		{
			WINTUN_CREATE_ADAPTER_FUNC* create_adapter{ nullptr };
			WINTUN_OPEN_ADAPTER_FUNC* open_adapter{ nullptr };
			WINTUN_CLOSE_ADAPTER_FUNC* close_adapter{ nullptr };
			WINTUN_GET_ADAPTER_LUID_FUNC* get_adapter_luid{ nullptr };

			HMODULE module{ nullptr };

			~wintun_api()
			{
				if (module)
					FreeLibrary(module);
			}

			static std::unique_ptr<wintun_api> load()
			{
				auto api = std::make_unique<wintun_api>();
				api->module = LoadLibraryW(L"wintun.dll");
				if (!api->module)
					return nullptr;

				api->create_adapter = reinterpret_cast<WINTUN_CREATE_ADAPTER_FUNC*>(
					GetProcAddress(api->module, "WintunCreateAdapter"));
				api->open_adapter = reinterpret_cast<WINTUN_OPEN_ADAPTER_FUNC*>(
					GetProcAddress(api->module, "WintunOpenAdapter"));
				api->close_adapter = reinterpret_cast<WINTUN_CLOSE_ADAPTER_FUNC*>(
					GetProcAddress(api->module, "WintunCloseAdapter"));
				api->get_adapter_luid = reinterpret_cast<WINTUN_GET_ADAPTER_LUID_FUNC*>(
					GetProcAddress(api->module, "WintunGetAdapterLUID"));

				if (!api->create_adapter || !api->open_adapter ||
					!api->close_adapter || !api->get_adapter_luid)
					return nullptr;

				return api;
			}
		};

		HANDLE open_wintun(const std::string& name)
		{
			auto devs = enum_windows_devices();
			auto ifs = enum_device_interfaces();

			windows_driver wintun;
			for (auto& dev : devs)
			{
				if (dev.component_id_.find("wintun") != std::string::npos &&
					dev.name_ == name)
				{
					wintun = dev;
					break;
				}
			}

			if (wintun.guid_.empty())
				return INVALID_HANDLE_VALUE;

			std::string path;
			for (auto& [guid, p] : ifs)
			{
				if (guid == wintun.guid_)
				{
					path = p;
					break;
				}
			}

			if (path.empty())
				return INVALID_HANDLE_VALUE;

			return CreateFileA(const_cast<char*>(path.data()),
				GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
				FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, nullptr);
		}

		bool install_wintun()
		{
			// 已安装 wintun 驱动则直接返回.
			auto devs = enum_windows_devices();
			for (auto& dev : devs)
				if (dev.component_id_.find("wintun") != std::string::npos)
					return true;

			// 解压驱动文件到临时目录.
			std::error_code ec;
			auto tmp = std::filesystem::temp_directory_path(ec);
			if (ec)
			{
				XLOG_ERR << "install wintun, temp path: " << ec.message();
				return false;
			}
			tmp /= std::to_string(GetCurrentProcessId());
			std::filesystem::create_directories(tmp, ec);
			if (ec)
			{
				XLOG_ERR << "install wintun, create dir: " << ec.message();
				return false;
			}

			auto sys_path = tmp / "wintun.sys";
			auto inf_path = tmp / "wintun.inf";
			auto cat_path = tmp / "wintun.cat";
			if (!resource_copy_to_file("wintun.sys", sys_path) ||
				!resource_copy_to_file("wintun.inf", inf_path) ||
				!resource_copy_to_file("wintun.cat", cat_path))
			{
				XLOG_ERR << "install wintun, extract resource failed";
				std::filesystem::remove_all(tmp, ec);
				return false;
			}

			// 使用 pnputil 安装驱动.
			wchar_t win_dir[MAX_PATH] = { 0 };
			GetWindowsDirectoryW(win_dir, MAX_PATH);
			std::wstring pnputil = std::wstring(win_dir) +
				L"\\System32\\pnputil.exe";
			std::wstring cmd = L"\"" + pnputil + L"\" /add-driver \"" +
				inf_path.wstring() + L"\"";
			bool ok = run_command(cmd);

			std::filesystem::remove_all(tmp, ec);
			if (!ok)
				XLOG_ERR << "install wintun, pnputil add-driver failed";
			else
				XLOG_INFO << "wintun driver installed";
			return ok;
		}
	}

	//////////////////////////////////////////////////////////////////////////

	wintun_tun_device::wintun_tun_device(net::any_io_executor executor)
		: m_executor(std::move(executor))
		, m_receive_object_moved(m_executor)
	{}

	wintun_tun_device::~wintun_tun_device()
	{
		close();
	}

	boost::system::error_code wintun_tun_device::open(
		const std::string& name, int mtu)
	{
		// 释放上次打开可能残留的资源, 避免句柄/映射泄漏.
		close();

		std::string adapter_name = name.empty() ? "proxy" : name;
		m_devname = adapter_name;

		static const GUID adapter_guid = {
			0xdeadbab1, 0xcafe, 0xbeef,
			{ 0x01, 0x23, 0x45, 0x67, 0x00, 0x00, 0x00, 0xff }
		};

		// 提前标记可关闭, 使 open() 中途失败时 close() 能正确清理已创建资源.
		m_abort = false;

		// 失败时记录错误并清理已创建资源.
		auto fail = [this](const char* msg) -> boost::system::error_code
		{
			DWORD err = GetLastError();
			XLOG_ERR << msg << ": " << win_error(err);
			close();
			return boost::system::error_code(err,
				boost::system::system_category());
		};

		// 安装驱动并加载 wintun.dll.
		static bool wintun_inited = false;
		if (!wintun_inited)
		{
			wintun_detail::install_wintun();
			m_api = wintun_detail::wintun_api::load();
			if (!m_api)
				XLOG_ERR << "load wintun.dll failed";
			wintun_inited = true;
		}
		if (!m_api)
			return boost::system::error_code(
				GetLastError(), boost::system::system_category());

		std::wstring wname(adapter_name.begin(), adapter_name.end());
		for (int n = 0; n < 5; n++)
		{
			m_wintun_handle = m_api->create_adapter(
				wname.data(), L"Wintun", &adapter_guid);
			if (!m_wintun_handle)
				XLOG_WARN << "WintunCreateAdapter fail: "
					<< win_error(GetLastError());
			if (!m_wintun_handle)
				m_wintun_handle = m_api->open_adapter(wname.data());
			if (m_wintun_handle)
				break;
		}
		if (!m_wintun_handle)
		{
			XLOG_ERR << "wintun adapter open failed: "
				<< win_error(GetLastError());
			return boost::system::error_code(
				GetLastError(), boost::system::system_category());
		}

		m_wintun_file = wintun_detail::open_wintun(adapter_name);
		if (m_wintun_file == INVALID_HANDLE_VALUE)
			return fail("open wintun device failed");

		m_send_ring_handle = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
			PAGE_READWRITE, 0, sizeof(struct tun_ring), nullptr);
		m_receive_ring_handle = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
			PAGE_READWRITE, 0, sizeof(struct tun_ring), nullptr);
		if (!m_send_ring_handle || !m_receive_ring_handle)
			return fail("wintun create file mapping failed");

		m_send_ring = static_cast<struct tun_ring*>(MapViewOfFile(
			m_send_ring_handle, FILE_MAP_ALL_ACCESS, 0, 0,
			sizeof(struct tun_ring)));
		m_receive_ring = static_cast<struct tun_ring*>(MapViewOfFile(
			m_receive_ring_handle, FILE_MAP_ALL_ACCESS, 0, 0,
			sizeof(struct tun_ring)));
		if (!m_send_ring || !m_receive_ring)
			return fail("wintun map view failed");

		m_send_event_moved = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		m_receive_event_moved = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (!m_send_event_moved || !m_receive_event_moved)
			return fail("wintun create event failed");
		m_receive_object_moved.assign(m_receive_event_moved);

		struct tun_register_rings rr;
		std::memset(&rr, 0, sizeof(rr));
		rr.send.ring = m_receive_ring;
		rr.send.ring_size = sizeof(struct tun_ring);
		rr.send.tail_moved = m_receive_event_moved;
		rr.receive.ring = m_send_ring;
		rr.receive.ring_size = sizeof(struct tun_ring);
		rr.receive.tail_moved = m_send_event_moved;

		DWORD bytes_returned = 0;
		BOOL res = DeviceIoControl(m_wintun_file, TUN_IOCTL_REGISTER_RINGS,
			&rr, sizeof(rr), nullptr, 0, &bytes_returned, nullptr);
		if (!res)
			return fail("wintun register rings failed");

		// 设置 MTU (IPv4 与 IPv6).
		NET_IFINDEX index = 0;
		NET_LUID luid;
		m_api->get_adapter_luid(m_wintun_handle, &luid);
		if (ConvertInterfaceLuidToIndex(&luid, &index) == NO_ERROR)
		{
			auto set_mtu = [&](short family, int value) {
				MIB_IPINTERFACE_ROW ipiface;
				std::memset(&ipiface, 0, sizeof(ipiface));
				InitializeIpInterfaceEntry(&ipiface);
				ipiface.Family = family;
				ipiface.InterfaceIndex = index;
				if (GetIpInterfaceEntry(&ipiface) == NO_ERROR)
				{
					ipiface.NlMtu = value;
					SetIpInterfaceEntry(&ipiface);
				}
			};
			set_mtu(AF_INET, mtu > 0 ? mtu : 1500);
			set_mtu(AF_INET6, std::max(mtu > 0 ? mtu : 1500, 1280));
		}

		m_opened = true;
		XLOG_INFO << "wintun opened: " << m_devname;
		return {};
	}

	void wintun_tun_device::close()
	{
		if (m_abort)
			return;

		m_abort = true;
		m_opened = false;

		if (m_send_event_moved != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_send_event_moved);
			m_send_event_moved = INVALID_HANDLE_VALUE;
		}
		boost::system::error_code ignore_ec;
		m_receive_object_moved.close(ignore_ec);

		if (m_send_ring)
		{
			UnmapViewOfFile(m_send_ring);
			m_send_ring = nullptr;
		}
		if (m_receive_ring)
		{
			UnmapViewOfFile(m_receive_ring);
			m_receive_ring = nullptr;
		}
		if (m_send_ring_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_send_ring_handle);
			m_send_ring_handle = INVALID_HANDLE_VALUE;
		}
		if (m_receive_ring_handle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_receive_ring_handle);
			m_receive_ring_handle = INVALID_HANDLE_VALUE;
		}
		if (m_wintun_file != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_wintun_file);
			m_wintun_file = INVALID_HANDLE_VALUE;
		}
		if (m_wintun_handle)
		{
			m_api->close_adapter(m_wintun_handle);
			m_wintun_handle = nullptr;
		}

		XLOG_INFO << "wintun closed";
	}

	int wintun_tun_device::read_wintun(std::string_view buf)
	{
		struct tun_ring* ring = m_receive_ring;
		if (m_abort || !ring)
			return -1;

		ULONG head = ring->head;
		ULONG tail = ring->tail;

		if (head >= WINTUN_RING_CAPACITY || tail >= WINTUN_RING_CAPACITY)
			return -1;

		if (head == tail)
			return 0;

		auto content_len = wintun_detail::wintun_ring_wrap(tail - head);
		if (content_len < sizeof(struct TUN_PACKET_HEADER))
			return -1;

		auto packet = reinterpret_cast<struct TUN_PACKET*>(&ring->data[head]);
		if (packet->size > WINTUN_MAX_PACKET_SIZE)
			return -1;

		auto aligned = wintun_detail::wintun_ring_packet_align(
			static_cast<ULONG>(sizeof(struct TUN_PACKET_HEADER) + packet->size));
		if (aligned > content_len)
			return -1;

		if (buf.size() < packet->size)
			return -1;

		std::memcpy(const_cast<char*>(buf.data()), packet->data, packet->size);
		head = wintun_detail::wintun_ring_wrap(head + aligned);
		ring->head = head;

		return static_cast<int>(packet->size);
	}

	int wintun_tun_device::write_wintun(std::string_view buf)
	{
		struct tun_ring* ring = m_send_ring;
		if (m_abort || !ring)
			return -1;

		ULONG head = ring->head;
		ULONG tail = ring->tail;

		if (head >= WINTUN_RING_CAPACITY || tail >= WINTUN_RING_CAPACITY)
			return -1;

		auto aligned = wintun_detail::wintun_ring_packet_align(
			static_cast<ULONG>(sizeof(struct TUN_PACKET_HEADER) + buf.size()));
		auto buf_space = wintun_detail::wintun_ring_wrap(
			head - tail - WINTUN_PACKET_ALIGN);
		if (aligned > buf_space)
			return 0;

		auto packet = reinterpret_cast<struct TUN_PACKET*>(&ring->data[tail]);
		packet->size = static_cast<uint32_t>(buf.size());
		std::memcpy(packet->data, buf.data(), buf.size());
		ring->tail = wintun_detail::wintun_ring_wrap(tail + aligned);

		if (ring->alertable != 0)
			SetEvent(m_send_event_moved);

		return static_cast<int>(buf.size());
	}

	net::awaitable<std::size_t> wintun_tun_device::do_read(
		std::string_view buf, boost::system::error_code& ec)
	{
		if (m_abort)
		{
			ec = net::error::operation_aborted;
			co_return 0;
		}

		for (;;)
		{
			co_await m_receive_object_moved.async_wait(net_awaitable[ec]);
			if (ec)
				co_return 0;

			int n = read_wintun(buf);
			if (n == 0)
				continue;
			if (n > 0)
				co_return static_cast<std::size_t>(n);

			ec = net::error::operation_aborted;
			co_return 0;
		}
	}

	net::awaitable<std::size_t> wintun_tun_device::do_write(
		std::string_view buf, boost::system::error_code& ec)
	{
		if (m_abort)
		{
			ec = net::error::operation_aborted;
			co_return 0;
		}

		int n = write_wintun(buf);
		if (n < 0)
		{
			ec = net::error::operation_aborted;
			co_return 0;
		}

		while (n == 0)
		{
			// send ring 已满, 定时重试写入.
			net::steady_timer wait_timer(m_executor);
			wait_timer.expires_after(std::chrono::milliseconds(1));
			co_await wait_timer.async_wait(net_awaitable[ec]);
			if (ec)
				co_return 0;
			if (m_abort)
			{
				ec = net::error::operation_aborted;
				co_return 0;
			}

			n = write_wintun(buf);
			if (n < 0)
			{
				ec = net::error::operation_aborted;
				co_return 0;
			}
		}

		co_return static_cast<std::size_t>(n);
	}

} // namespace proxy
