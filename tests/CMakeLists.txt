# 单元测试
#
# 使用 Boost.Test 框架, 依赖 libproxy 提供的头文件与符号.
# 由根 CMakeLists.txt 在 ENABLE_BUILD_TESTS=ON 时加入, 并通过 CTest 注册.

add_executable(proxy_unit_tests
	main.cpp
	test_rfc9298_capsule.cpp
	test_http_date.cpp
	test_proxy_util.cpp
	test_strutil.cpp
	test_url_info.cpp
	test_dns_wire.cpp
)

target_link_libraries(proxy_unit_tests
	${PROXY_LIBRARIES}
	Boost::unit_test_framework
)

add_test(NAME proxy_unit_tests COMMAND proxy_unit_tests)
