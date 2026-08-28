cmake_minimum_required(VERSION 3.20)
project(tunio VERSION 3.0.0 LANGUAGES CXX)

# CMake 3.30+ 移除 FindBoost 模块（CMP0167）：系统提供 BoostConfig 时
# find_package 自动回退到 config 模式，此处显式声明旧策略以抑制告警。
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# 作为独立项目构建时查找系统 Boost；作为第三方库被 superproject（如
# proxy 内置 Boost）引入时复用已存在的 Boost 目标，避免混用版本。
# 优先复用 Boost::asio（内置 Boost 独立编译 asio 时提供头文件与符号来源，
# 与 superproject 的 lib_asio_beast 保持同一版本/ABI），否则回退 Boost::headers。
if(TARGET Boost::asio)
  set(TUNIO_BOOST_LIBS Boost::asio)
elseif(NOT TARGET Boost::headers)
  find_package(Boost 1.74 REQUIRED)
  set(TUNIO_BOOST_LIBS Boost::headers)
else()
  set(TUNIO_BOOST_LIBS Boost::headers)
endif()

set(TUNIO_SOURCES
  src/packet_buffer.cpp
  src/tcp_engine.cpp
  src/udp_engine.cpp
  src/tun_tcp_acceptor.cpp
  src/tunio.cpp
  src/tun_tcp_socket.cpp
  src/tun_udp_acceptor.cpp
  src/tun_udp_socket.cpp
)

# 平台相关实现按平台拆分（参考 Asio 的 impl 布局）：
#   Windows -> packet_device_windows.cpp
#   其他 POSIX -> packet_device_posix.cpp
#   Windows + USE_WINTUN_DRIVER -> packet_device_wintun.cpp
option(USE_WINTUN_DRIVER "Use Wintun driver on Windows" OFF)
if(WIN32 AND USE_WINTUN_DRIVER)
  list(APPEND TUNIO_SOURCES src/packet_device_wintun.cpp)
elseif(WIN32)
  list(APPEND TUNIO_SOURCES src/packet_device_windows.cpp)
else()
  list(APPEND TUNIO_SOURCES src/packet_device_posix.cpp)
endif()

# 环路与本地地址防护默认启用；设置为 ON 时定义
# TUNIO_DISABLE_LOOPBACK_GUARD，关闭对源为本机虚拟 IP 或保留地址
# 入包的丢弃（编译宏也可通过 -DTUNIO_DISABLE_LOOPBACK_GUARD 直接定义）。
option(TUNIO_DISABLE_LOOPBACK_GUARD "Disable loopback/local-address traffic guard" OFF)

add_library(tunio STATIC ${TUNIO_SOURCES})
target_include_directories(tunio PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Windows 下 windows.h 会定义 min/max 宏, 在包含任何 Windows 头之前
# 通过编译定义禁用, 避免破坏 std::min/std::max. PUBLIC 传播给使用方,
# 使用方编译含 tunio 头的代码时同样生效.
if(WIN32)
  target_compile_definitions(tunio PUBLIC NOMINMAX)
endif()

# 环路与本地地址防护默认启用；设置为 ON 时定义
# TUNIO_DISABLE_LOOPBACK_GUARD 关闭防护（见上方 option 注释）。
if(TUNIO_DISABLE_LOOPBACK_GUARD)
  target_compile_definitions(tunio PUBLIC TUNIO_DISABLE_LOOPBACK_GUARD)
endif()

# Wintun 所需编译定义及系统库.
if(WIN32 AND USE_WINTUN_DRIVER)
  # 头文件按 USE_WINTUN_DRIVER 选择设备实现 (packet_device.hpp), 必须
  # 对链接 tunio 的使用方可见, 否则使用方会选到 windows overlapped 实现.
  target_compile_definitions(tunio PUBLIC USE_WINTUN_DRIVER)
  # 链接 Wintun 所需的 Windows 系统库.
  target_link_libraries(tunio PRIVATE iphlpapi cfgmgr32 setupapi ws2_32)
endif()

target_link_libraries(tunio PUBLIC ${TUNIO_BOOST_LIBS})
if(MSVC)
  target_compile_options(tunio PRIVATE /W4)
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
