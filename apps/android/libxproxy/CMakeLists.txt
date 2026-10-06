cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0078 NEW)
project(proxy_android)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLE_LOGGER")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")

file(GLOB proxy_android_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/xproxy.hpp
)

file(GLOB proxy_android_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/xproxy.cpp
)

add_library(proxy_android ${proxy_android_SRCS} ${proxy_android_HEADERS})

target_compile_definitions(proxy_android PRIVATE
	-DVERSION_GIT="${PVERSION}"
)

target_link_libraries(proxy_android
	libproxy
	${LOG_LIBRARY}
)

# 编译期开关: ON 时退役对象在停止线程同步析构, 不使用后台回收线程
# (reaper), 用于对比验证 reaper 的必要性. 默认 OFF, 即 reaper 启用.
option(XPROXY_SYNC_DESTROY "退役对象改为同步析构 (禁用后台回收线程)" OFF)
if (XPROXY_SYNC_DESTROY)
	target_compile_definitions(proxy_android PRIVATE -DXPROXY_SYNC_DESTROY)
endif()

if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129
    AND CMAKE_VERSION VERSION_GREATER 3.20.3)
	set_property(TARGET proxy_android PROPERTY CXX_STANDARD 23)
	message(STATUS "proxy_android set cxx standard: C++23")
else()
	set_property(TARGET proxy_android PROPERTY CXX_STANDARD 20)
	message(STATUS "proxy_android set cxx standard: C++20")
endif()

if (SWIG_FOUND)
	# ${SWIG_USE_FILE} 变量在使用 find_package(SWIG) 命令时被设置,
	# 指向 SWIG 的配置文件.
	include(${SWIG_USE_FILE})

	# 设置 SWIG 模块名称, 该名称用于生成的文件名.
	set(SWIG_MODULE_NAME "xproxy")

	# CMAKE_SWIG_FLAGS 用于设置 SWIG 参数, 这里指定生成的 Java 包名,
	# 并启用 director 以支持 Java 端回调接口.
	set(CMAKE_SWIG_FLAGS -package com.jackarain -directors)

	# include_directories 用于添加头文件搜索路径.
	include_directories(${CMAKE_CURRENT_SOURCE_DIR})

	# set_source_files_properties 设置 xproxy.i 的属性, 指定其为 C++ 源文件.
	set_source_files_properties(xproxy.i PROPERTIES CPLUSPLUS ON)

	# swig_add_library 生成 SWIG 模块, 这里生成了一个名为 xproxy 的模块,
	# 指定其语言为 Java, 源文件为 xproxy.i.
	swig_add_library(${SWIG_MODULE_NAME} LANGUAGE java SOURCES xproxy.i)

	# target_link_libraries 用于链接库文件, 这里链接了 proxy_android 和 log 库.
	target_link_libraries(${SWIG_MODULE_NAME} proxy_android ${LOG_LIBRARY})
endif()
