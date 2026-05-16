add_library(examples_main STATIC main.cpp)
target_link_libraries(examples_main PRIVATE boost_redis_project_options)

function(make_example EXAMPLE_NAME)
  add_executable(${EXAMPLE_NAME} ${EXAMPLE_NAME}.cpp)
  target_link_libraries(${EXAMPLE_NAME} PRIVATE boost_redis_src)
  target_link_libraries(${EXAMPLE_NAME} PRIVATE boost_redis_project_options)
  if (ARGN)
    target_link_libraries(${EXAMPLE_NAME} PRIVATE ${ARGN})
  endif()
endfunction()

function(make_testable_example EXAMPLE_NAME)
  make_example(${EXAMPLE_NAME} ${ARGN})
  add_test(${EXAMPLE_NAME} ${EXAMPLE_NAME} $ENV{BOOST_REDIS_TEST_SERVER} 6379)
endfunction()

make_testable_example(cpp17_intro)
make_testable_example(cpp17_intro_sync)

make_testable_example(cpp20_intro        examples_main)
make_testable_example(cpp20_containers   examples_main)
make_testable_example(cpp20_json         examples_main Boost::json Boost::container_hash)
make_testable_example(cpp20_unix_sockets examples_main)
make_testable_example(cpp20_timeouts     examples_main)
make_testable_example(cpp20_sentinel     examples_main)

make_example(cpp20_subscriber  examples_main)
make_example(cpp20_streams     examples_main)
make_example(cpp20_echo_server examples_main)
make_example(cpp20_intro_tls   examples_main)

# We test the protobuf example only on gcc.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  find_package(Protobuf)
  if (Protobuf_FOUND)
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS person.proto)
    make_testable_example(cpp20_protobuf examples_main ${Protobuf_LIBRARIES})
    target_sources(cpp20_protobuf PUBLIC ${PROTO_SRCS} ${PROTO_HDRS})
    target_include_directories(cpp20_protobuf PUBLIC ${Protobuf_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
  endif()
endif()

if (NOT MSVC)
  make_example(cpp20_chat_room examples_main)
endif()

# We build and test the spdlog integration example only if the library is found
find_package(spdlog)
if (spdlog_FOUND)
  make_testable_example(cpp17_spdlog spdlog::spdlog)
else()
  message(STATUS "Skipping the spdlog example because the spdlog package couldn't be found")
endif()
