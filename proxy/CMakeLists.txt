cmake_minimum_required(VERSION 3.16)
project(libproxy)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLE_LOGGER")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")

add_library(libproxy STATIC)

target_sources(libproxy
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/async_connect.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/base_stream.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/default_cert.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/fileop.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/strutil.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/logging.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/scoped_exit.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/socks_client.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/socks_enums.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/socks_error_code.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/socks_io.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/proxy_server.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/use_awaitable.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/http_proxy_client.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/xxhash.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/proxy_socket.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/ipip.hpp

	${CMAKE_CURRENT_SOURCE_DIR}/src/lib_asio_beast.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ipip.cpp
)

target_include_directories(libproxy
	PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include/
)

target_compile_definitions(libproxy
	PUBLIC
	-DXXH_INLINE_ALL
)

target_link_libraries(libproxy
	Boost::thread
	Boost::date_time
	Boost::program_options
	Boost::filesystem
	Boost::signals2
	Boost::nowide
	Boost::url
	Boost::json
	Boost::uuid
	Boost::asio
	Boost::beast

	fmt
	${OPENSSL_LIBRARIES}
)

#source_group("Header Files\\proxy" FILES ${libproxy_HEADERS})
#source_group("Source Files" FILES ${libproxy_SRCS})

set_target_properties(libproxy PROPERTIES OUTPUT_NAME proxy PREFIX "lib" CLEAN_DIRECT_OUTPUT 1)

if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129 AND CMAKE_VERSION VERSION_GREATER 3.20.3)
	set_property(TARGET libproxy PROPERTY CXX_STANDARD 23)
	message(STATUS "libproxy set cxx standard: C++23")
else()
	set_property(TARGET libproxy PROPERTY CXX_STANDARD 20)
	message(STATUS "libproxy set cxx standard: C++20")
endif()

set(PROXY_INCLUDE_DIRS
	${CMAKE_CURRENT_SOURCE_DIR}/include
	PARENT_SCOPE
)

set(PROXY_LIBRARIES
	libproxy
	PARENT_SCOPE
)
