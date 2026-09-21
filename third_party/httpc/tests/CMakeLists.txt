#
# httpc tests
# ~~~~~~~~~~~
#
# Boost.Test 单元测试 (URL/请求辅助函数) 与基于本地回环 HTTP 服务器的集成测试.
#

set(_httpc_test_sources
	test_http_details.cpp
	test_http_client.cpp
)

if(HTTPC_BOOST_ROOT)
	# Boost 源码树: 直接把 Boost.Test 源码编入测试目标, 避免静态库单遍
	# 扫描导致的符号提取问题, 也保证与库使用同一份 Boost.
	# 排除提供其它 main 变体的转发源.
	file(GLOB _httpc_test_framework_sources
		"${HTTPC_BOOST_ROOT}/libs/test/src/*.cpp")
	list(FILTER _httpc_test_framework_sources EXCLUDE REGEX "[/+]cpp_main\\.cpp$")
	list(FILTER _httpc_test_framework_sources EXCLUDE REGEX "[/+]test_main\\.cpp$")
	set(_httpc_test_framework_libs)
elseif(TARGET Boost::unit_test_framework)
	# 已安装的 Boost 且其 CMake 目标已存在.
	set(_httpc_test_framework_sources)
	set(_httpc_test_framework_libs Boost::unit_test_framework)
else()
	# 已安装的 Boost: 通过 find_package 获取 Boost.Test 目标.
	find_package(Boost 1.84 REQUIRED CONFIG COMPONENTS unit_test_framework)
	set(_httpc_test_framework_sources)
	set(_httpc_test_framework_libs Boost::unit_test_framework)
endif()

add_executable(httpc_tests
	${_httpc_test_sources}
	${_httpc_test_framework_sources}
)

target_link_libraries(httpc_tests
	PRIVATE
		httpc::httpc
		${_httpc_test_framework_libs}
)

target_include_directories(httpc_tests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../include
)

if(HTTPC_BOOST_ROOT)
	# Boost.Test 源码直接编入测试目标, 关闭 Boost 的自动链接.
	target_compile_definitions(httpc_tests PRIVATE BOOST_TEST_NO_LIB)
endif()

if(MSVC)
	# 避免 MSVC 下 Boost.Test 自动链接到动态库.
	target_compile_definitions(httpc_tests PRIVATE BOOST_TEST_STATIC_LINK)
	target_compile_options(httpc_tests PRIVATE /W4)
else()
	target_compile_options(httpc_tests PRIVATE -Wall -Wextra)
endif()

add_test(NAME httpc_tests COMMAND httpc_tests --log_level=test_suite)
set_tests_properties(httpc_tests PROPERTIES TIMEOUT 300)
