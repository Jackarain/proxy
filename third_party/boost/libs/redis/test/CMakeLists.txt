
# Common utilities
add_library(boost_redis_project_options INTERFACE)
target_link_libraries(boost_redis_project_options INTERFACE boost_redis)
if (MSVC)
  # C4459: name hides outer scope variable is issued by Asio
  target_compile_options(boost_redis_project_options INTERFACE /bigobj /W4 /WX /wd4459)
  target_compile_definitions(boost_redis_project_options INTERFACE _WIN32_WINNT=0x0601 _CRT_SECURE_NO_WARNINGS=1)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(boost_redis_project_options INTERFACE -Wall -Wextra -Werror)
endif()

add_library(boost_redis_src STATIC boost_redis.cpp)
target_compile_features(boost_redis_src PRIVATE cxx_std_17)
target_link_libraries(boost_redis_src PRIVATE boost_redis_project_options)

# Test utils
add_library(boost_redis_tests_common STATIC common.cpp)
target_compile_features(boost_redis_tests_common PRIVATE cxx_std_17)
target_link_libraries(boost_redis_tests_common PRIVATE boost_redis_project_options)

macro(make_test TEST_NAME)
  set(EXE_NAME "boost_redis_${TEST_NAME}")
  add_executable(${EXE_NAME} ${TEST_NAME}.cpp)
  target_link_libraries(${EXE_NAME} PRIVATE
    boost_redis_src
    boost_redis_tests_common
    boost_redis_project_options
    Boost::unit_test_framework
  )
  target_compile_definitions(${EXE_NAME} PRIVATE BOOST_ALLOW_DEPRECATED=1) # we need to still test deprecated fns
  add_test(${EXE_NAME} ${EXE_NAME})
endmacro()

# Unit tests
make_test(test_low_level)
make_test(test_request)
make_test(test_low_level_sync_sans_io)
make_test(test_any_adapter)
make_test(test_exec_fsm)
make_test(test_log_to_file)
make_test(test_conn_logging)
make_test(test_reader_fsm)

# Tests that require a real Redis server
make_test(test_conn_quit)
make_test(test_conn_tls)
make_test(test_conn_exec_retry)
make_test(test_conn_exec_error)
make_test(test_run)
make_test(test_conn_check_health)
make_test(test_conn_exec)
make_test(test_conn_push)
make_test(test_conn_reconnect)
make_test(test_conn_exec_cancel)
make_test(test_conn_exec_cancel2)
make_test(test_conn_echo_stress)
make_test(test_issue_50)
make_test(test_issue_181)
make_test(test_conversions)
make_test(test_unix_sockets)

# Coverage
set(
  COVERAGE_TRACE_COMMAND
  lcov --capture
  -output-file "${PROJECT_BINARY_DIR}/coverage.info"
  --directory "${PROJECT_BINARY_DIR}"
  --include "${PROJECT_SOURCE_DIR}/include/*"
)

set(
  COVERAGE_HTML_COMMAND
  genhtml --legend -f -q
  "${PROJECT_BINARY_DIR}/coverage.info"
  --prefix "${PROJECT_SOURCE_DIR}"
  --output-directory "${PROJECT_BINARY_DIR}/coverage_html"
)

add_custom_target(
  coverage
  COMMAND ${COVERAGE_TRACE_COMMAND}
  COMMAND ${COVERAGE_HTML_COMMAND}
  COMMENT "Generating coverage report"
  VERBATIM
)
