cmake_minimum_required(VERSION 3.20)
project(tunio VERSION 3.0.0 LANGUAGES CXX)

# 安装路径基准（CMAKE_INSTALL_*，供安装/导出规则与 INSTALL_INTERFACE
# include 目录使用）.
include(GNUInstallDirs)

# CMake 3.30+ 移除 FindBoost 模块（CMP0167）：系统提供 BoostConfig 时
# find_package 自动回退到 config 模式，此处显式声明旧策略（CMake 3.30+
# 对 set-to-OLD 本身仍会打印弃用告警，属预期）.
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  # 未显式指定时默认 Debug：断言与调试符号可用；Release 请显式传入
  # -DCMAKE_BUILD_TYPE=Release（CI 的测试构建须用 Debug，见 Build.yml）
  set(CMAKE_BUILD_TYPE Debug)
endif()

# 作为独立项目构建时查找系统 Boost；作为第三方库被 superproject（如
# proxy 内置 Boost）引入时复用已存在的 Boost 目标，避免混用版本。
# 优先复用 Boost::asio（内置 Boost 独立编译 asio 时提供头文件与符号来源，
# 与 superproject 的 lib_asio_beast 保持同一版本/ABI），否则回退 Boost::headers。
if(TARGET Boost::asio)
  set(TUNIO_BOOST_LIBS Boost::asio)
elseif(NOT TARGET Boost::headers)
  # 代码实际使用 boost::unordered_flat_map（Boost 1.81 引入）与
  # net::any_completion_handler（Boost 1.77 引入），最低要求 1.81
  find_package(Boost 1.81 REQUIRED)
  set(TUNIO_BOOST_LIBS Boost::headers)
else()
  set(TUNIO_BOOST_LIBS Boost::headers)
endif()

set(TUNIO_SOURCES
  src/ip_packet.cpp
  src/tun_device.cpp
  src/tcp_engine.cpp
  src/udp_engine.cpp
  src/tun_tcp_acceptor.cpp
  src/tunio.cpp
  src/tun_tcp_socket.cpp
  src/tun_udp_acceptor.cpp
  src/tun_udp_socket.cpp
)

# 平台相关实现按平台拆分（参考 Asio 的 impl 布局）：
#   Windows -> tun_device_windows.cpp
#   其他 POSIX -> tun_device_posix.cpp
#   Windows + USE_WINTUN_DRIVER -> tun_device_wintun.cpp
option(USE_WINTUN_DRIVER "Use Wintun driver on Windows" OFF)
if(WIN32 AND USE_WINTUN_DRIVER)
  list(APPEND TUNIO_SOURCES src/tun_device_wintun.cpp)
elseif(WIN32)
  list(APPEND TUNIO_SOURCES src/tun_device_windows.cpp)
else()
  list(APPEND TUNIO_SOURCES src/tun_device_posix.cpp)
endif()

# 环路与本地地址防护默认启用；设置为 ON 时定义
# TUNIO_DISABLE_LOOPBACK_GUARD，关闭对源为本机虚拟 IP 或保留地址
# 入包的丢弃（编译宏也可通过 -DTUNIO_DISABLE_LOOPBACK_GUARD 直接定义）。
option(TUNIO_DISABLE_LOOPBACK_GUARD "Disable loopback/local-address traffic guard" OFF)

add_library(tunio STATIC ${TUNIO_SOURCES})
# BUILD_INTERFACE 保持 add_subdirectory 场景行为（含 src 目录，实现头位于
# src/ 下，测试直接包含实现头）；INSTALL_INTERFACE 只暴露公开头目录，使
# 安装导出的目标可重定位，不写死构建机源码路径.
target_include_directories(tunio PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

# Windows 下 windows.h 会定义 min/max 宏, 在包含任何 Windows 头之前
# 通过编译定义禁用, 避免破坏 std::min/std::max. PUBLIC 传播给使用方,
# 使用方编译含 tunio 头的代码时同样生效.
if(WIN32)
  target_compile_definitions(tunio PUBLIC NOMINMAX)
  # 显式声明 Windows 目标版本（Windows 7 及以上）, 消除 windows.h 的
  # "Please define _WIN32_WINNT" 告警并启用对应版本的 API 声明.
  # PUBLIC 传播, 使用方包含 windows.h 的代码同样受益.
  target_compile_definitions(tunio PUBLIC _WIN32_WINNT=0x0601)
endif()

# 环路与本地地址防护默认启用；设置为 ON 时定义
# TUNIO_DISABLE_LOOPBACK_GUARD 关闭防护（见上方 option 注释）。
if(TUNIO_DISABLE_LOOPBACK_GUARD)
  target_compile_definitions(tunio PUBLIC TUNIO_DISABLE_LOOPBACK_GUARD)
endif()

# Wintun 所需编译定义及系统库.
if(WIN32 AND USE_WINTUN_DRIVER)
  # 头文件按 USE_WINTUN_DRIVER 选择设备实现 (tun_device.hpp), 必须
  # 对链接 tunio 的使用方可见, 否则使用方会选到 windows overlapped 实现.
  target_compile_definitions(tunio PUBLIC USE_WINTUN_DRIVER)
  # 链接 Wintun 所需的 Windows 系统库.
  target_link_libraries(tunio PRIVATE iphlpapi cfgmgr32 setupapi ws2_32)
elseif(WIN32)
  # TAP 驱动自主打开 (tun_device_windows.cpp) 所需系统库:
  #   ws2_32    -> inet_pton (ws2tcpip.h)
  #   iphlpapi  -> GetAdapterIndex / GetIpInterfaceEntry / SetIpInterfaceEntry
  target_link_libraries(tunio PRIVATE ws2_32 iphlpapi)
endif()

target_link_libraries(tunio PUBLIC ${TUNIO_BOOST_LIBS})
if(MSVC)
  target_compile_options(tunio PRIVATE /W4)
  # 源文件为 UTF-8（含中文注释）, 简体中文 Windows 默认代码页 936 下
  # MSVC 会误判字符集, 触发 C4819 告警甚至产生乱码; /utf-8 同时设定
  # 源字符集与执行字符集为 UTF-8. PUBLIC 传播给链接 tunio 的示例与
  # 测试, 其包含 tunio 头时同样受益.
  target_compile_options(tunio PUBLIC /utf-8)
else()
  target_compile_options(tunio PRIVATE -Wall -Wextra)
endif()

option(TUNIO_BUILD_TESTS "构建单元测试" ON)
option(TUNIO_BUILD_EXAMPLES "构建示例程序" ON)

if(TUNIO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(TUNIO_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# 安装/导出规则（cmake --install、find_package(tunio)、pkg-config）.
# 默认 OFF：tunio 常以 add_subdirectory 方式被 superproject 集成，对外
# 安装属可选能力；需要对外发布时以 -DTUNIO_INSTALL=ON 开启.
option(TUNIO_INSTALL "Generate install/export rules (find_package(tunio), pkg-config)" OFF)

if(TUNIO_INSTALL)
  include(CMakePackageConfigHelpers)

  install(TARGETS tunio
    EXPORT tunioTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
  install(DIRECTORY include/tunio
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  # 导出目标（tunio::tunio）与包配置：安装侧解析 Boost 依赖.
  install(EXPORT tunioTargets
    FILE tunioTargets.cmake
    NAMESPACE tunio::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tunio
  )
  configure_package_config_file(
    cmake/tunioConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/tunioConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tunio
  )
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/tunioConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
  )
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/tunioConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/tunioConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tunio
  )

  # pkg-config（随安装导出一并提供，按 CMAKE_INSTALL_PREFIX 可重定位）.
  configure_file(cmake/tunio.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/tunio.pc @ONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tunio.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
  )
endif()
