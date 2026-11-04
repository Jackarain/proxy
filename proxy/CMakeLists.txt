cmake_minimum_required(VERSION 3.16)
project(libproxy)

set(libproxy_headers
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/authority.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/async_connect.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/variant_stream.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/default_cert.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/fileop.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/strutil.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/logging.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/launcher_log.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/socks_client.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/socks_enums.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/socks_error_code.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/network_order.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/scramble.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/ssl_stream.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/stdio_stream.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/proxy_server.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/udp_tproxy_flow.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/proxy_session.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/proxy.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/use_awaitable.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/proxy_config.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/http_error_body.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/http_proxy_client.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/xxhash.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/proxy_util.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/net_socket_option.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/rfc9298_capsule.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/ssl_client.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/ssl_connect.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/http_date.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/path_util.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/proxy_stream.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/proxy_websocket_client.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/websocket_tunnel.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/tcp_socket.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/ip_database.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/url_info.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/dns_cache.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/dns_response_cache.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/dns_server.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/uds_socket.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/tun_server.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/doh_client.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/doh_http.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_detail.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/tun_server_detail.hpp
	$<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/include/proxy/wintun_install.hpp>
)

set(libproxy_core_sources
	${CMAKE_CURRENT_SOURCE_DIR}/src/lib_asio_beast.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ip_database.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/launcher_log.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_dns.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_socks.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_connect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_resolve.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_transfer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_auth.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_server.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_server_launcher.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_server_launcher.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/dns_server.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/default_cert.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/xxhash.cpp
	$<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/src/wintun_install.cpp>
)

set(libproxy_http_sources
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_dir.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_get.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_response.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_auth.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_connect.cpp
)

set(libproxy_http_udp_sources
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_websocket.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_websocket_udp.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_websocket_udp_bridge.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_websocket_udp_capsule.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_websocket_udp_connect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_udp.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_udp_direct.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_udp_bridge.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_udp_proxy.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_udp_proxy_websocket.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_udp_proxy_http.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_udp_socks.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/proxy_session_http_proxy_udp_socks_connect.cpp
)

set(libproxy_tun_doh_sources
	${CMAKE_CURRENT_SOURCE_DIR}/src/tun_server.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/tun_server_detail.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/tun_tcp_flow.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/tun_udp_flow.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/doh_client.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/doh_connection.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/doh_connection_connect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/doh_proxy_pass_pool.cpp
)

if (MSVC)
	# Visual Studio 生成器会把 OBJECT 库再次归档为 .lib, 仍会触发 COFF
	# 归档的 4 GiB 上限. 拆成多个静态库, 由接口目标统一对外提供.
	add_library(libproxy_core STATIC ${libproxy_headers} ${libproxy_core_sources})
	add_library(libproxy_http STATIC ${libproxy_http_sources})
	add_library(libproxy_http_udp STATIC ${libproxy_http_udp_sources})
	add_library(libproxy_tun_doh STATIC ${libproxy_tun_doh_sources})

	set(libproxy_targets
		libproxy_core
		libproxy_http
		libproxy_http_udp
		libproxy_tun_doh
	)

	add_library(libproxy INTERFACE)
	target_link_libraries(libproxy INTERFACE ${libproxy_targets})
else()
	add_library(libproxy STATIC
		${libproxy_headers}
		${libproxy_core_sources}
		${libproxy_http_sources}
		${libproxy_http_udp_sources}
		${libproxy_tun_doh_sources}
	)
	set(libproxy_targets libproxy)
endif()

foreach(libproxy_target IN LISTS libproxy_targets)
	target_include_directories(${libproxy_target}
		PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include/
		# tinyrpc jsonrpc 头文件（launcher 控制通道直接使用）.
		${CMAKE_SOURCE_DIR}/third_party/tinyrpc/include
	)

	target_compile_definitions(${libproxy_target}
		PUBLIC
		-DXXH_STATIC_LINKING_ONLY
		-DFORCE_USE_FMT_FORMAT
	)

	if (ENABLE_SYSTEM_BOOST)
		target_link_libraries(${libproxy_target}
			Boost::date_time
			Boost::program_options
			Boost::filesystem
			Boost::nowide
			Boost::url
			Boost::json

			fmt
			tunio
			${OPENSSL_LIBRARIES}
		)
	else()
		target_link_libraries(${libproxy_target}
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
			Boost::scope

			fmt
			tunio
			${OPENSSL_LIBRARIES}
		)
	endif()

	if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129 AND CMAKE_VERSION VERSION_GREATER 3.20.3)
		set_property(TARGET ${libproxy_target} PROPERTY CXX_STANDARD 23)
	else()
		set_property(TARGET ${libproxy_target} PROPERTY CXX_STANDARD 20)
	endif()
endforeach()

if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129 AND CMAKE_VERSION VERSION_GREATER 3.20.3)
	message(STATUS "libproxy set cxx standard: C++23")
else()
	message(STATUS "libproxy set cxx standard: C++20")
endif()

#source_group("Header Files\\proxy" FILES ${libproxy_HEADERS})
#source_group("Source Files" FILES ${libproxy_SRCS})

if (NOT MSVC)
	set_target_properties(libproxy PROPERTIES OUTPUT_NAME proxy PREFIX "lib" CLEAN_DIRECT_OUTPUT 1)
endif()

set(PROXY_INCLUDE_DIRS
	${CMAKE_CURRENT_SOURCE_DIR}/include
	PARENT_SCOPE
)

set(PROXY_LIBRARIES
	libproxy
	PARENT_SCOPE
)
