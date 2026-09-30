cmake_minimum_required(VERSION 3.16)
project(proxy_ohos)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLE_LOGGER")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")

# 从 OHOS SDK 的 native/oh-uni-package.json 读取 apiVersion, 供 min_sdk_version() 返回.
if (DEFINED OHOS_SDK_NATIVE AND EXISTS "${OHOS_SDK_NATIVE}/oh-uni-package.json")
	file(READ "${OHOS_SDK_NATIVE}/oh-uni-package.json" _xproxy_ohos_pkg)
	string(REGEX MATCH "\"apiVersion\"[ \t]*:[ \t]*\"([0-9]+)\"" _xproxy_ohos_api "${_xproxy_ohos_pkg}")
	if (CMAKE_MATCH_1)
		set(XPROXY_OHOS_API_VERSION ${CMAKE_MATCH_1})
		message(STATUS "OHOS api version: ${XPROXY_OHOS_API_VERSION}")
	endif()
endif()

file(GLOB proxy_ohos_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/xproxy.hpp
)

file(GLOB proxy_ohos_SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/xproxy.cpp
)

# NAPI 模块, 产物为 libxproxy.so, 供 ArkTS `import xproxy from 'libxproxy.so'`.
add_library(xproxy SHARED ${proxy_ohos_SRCS} ${proxy_ohos_HEADERS})

target_compile_definitions(xproxy PRIVATE
	-DVERSION_GIT="${PVERSION}"
)

if (XPROXY_OHOS_API_VERSION)
	target_compile_definitions(xproxy PRIVATE
		-DXPROXY_OHOS_API_VERSION=${XPROXY_OHOS_API_VERSION}
	)
endif()

set_property(TARGET xproxy PROPERTY CXX_STANDARD 20)
message(STATUS "proxy_ohos set cxx standard: C++20")

target_link_libraries(xproxy PRIVATE
	libproxy
	libace_napi.z.so
	libhilog_ndk.z.so
)
