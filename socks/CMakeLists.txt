cmake_minimum_required(VERSION 3.16)
project(libsocks)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLE_LOGGER")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")

file(GLOB libsocks_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/include/socks/async_connect.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/socks/base_stream.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/socks/default_cert.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/socks/fileop.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/socks/logging.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/socks/scoped_exit.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/socks/socks_client.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/socks/socks_enums.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/socks/socks_error_code.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/socks/socks_io.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/socks/socks_server.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/socks/use_awaitable.hpp
)

file(GLOB url_HEADERS
	${CMAKE_CURRENT_SOURCE_DIR}/include/url/url_view.hpp
)

add_library(libsocks INTERFACE)

target_sources(libsocks
	PRIVATE

	${libsocks_HEADERS}
	${url_HEADERS}
)

source_group("Header Files\\socks" FILES ${libsocks_HEADERS})
source_group("Header Files\\url" FILES ${url_HEADERS})

set_target_properties(libsocks PROPERTIES OUTPUT_NAME socks PREFIX "lib" CLEAN_DIRECT_OUTPUT 1)

if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129 AND CMAKE_VERSION VERSION_GREATER 3.20.3)
	set_property(TARGET libsocks PROPERTY CXX_STANDARD 23)
	message(STATUS "libsocks set cxx standard: C++23")
else()
	set_property(TARGET libsocks PROPERTY CXX_STANDARD 20)
	message(STATUS "libsocks set cxx standard: C++20")
endif()

set(BASE_SOURCE_DIR
	${CMAKE_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(libsocks
	INTERFACE

	${BASE_SOURCE_DIR}
	${Boost_INCLUDE_DIRS}
)

set(LIBSOCKS_INCLUDE_DIRS
	${CMAKE_CURRENT_SOURCE_DIR}/include
	PARENT_SCOPE
)
