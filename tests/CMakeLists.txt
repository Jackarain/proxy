# 单元测试
#
# 使用 Boost.Test 框架, 依赖 libproxy 提供的头文件与符号.
# 由根 CMakeLists.txt 在 ENABLE_BUILD_TESTS=ON 时加入, 并通过 CTest 注册.

add_executable(proxy_unit_tests
	main.cpp
	test_rfc9298_capsule.cpp
	test_authority.cpp
	test_http_date.cpp
	test_http_error_body.cpp
	test_http_proxy_client.cpp
	test_logging.cpp
	test_proxy_util.cpp
	test_socks_io.cpp
	test_strutil.cpp
	test_strutil_extra.cpp
	test_url_info.cpp
	test_websocket_tunnel.cpp
	test_dns_wire.cpp
	test_dns_rdata.cpp
	test_dns_cache.cpp
	test_scramble.cpp
	test_fileop.cpp
	test_path_util.cpp
	test_net_socket_option.cpp
	test_socks_error_code.cpp
	test_ssl_client.cpp
	test_udp_tproxy_flow.cpp
	test_doh_http.cpp
)

target_link_libraries(proxy_unit_tests
	${PROXY_LIBRARIES}
	${ZLIB_LIBRARIES}
	Boost::unit_test_framework
)

add_test(NAME proxy_unit_tests COMMAND proxy_unit_tests)
