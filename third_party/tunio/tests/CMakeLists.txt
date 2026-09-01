# 测试依赖 POSIX 接口（arpa/inet.h、poll.h、sys/socket.h、socketpair 等），
# Windows（MSVC/mingw）无法编译；Windows 上需以 -DTUNIO_BUILD_TESTS=OFF
# 构建（CI 亦如此），README 已注明.
if(NOT WIN32)
  add_executable(test_packet_buffer test_packet_buffer.cpp)
  add_executable(test_checksum test_checksum.cpp)
  add_executable(test_ip_packet test_ip_packet.cpp)
  add_executable(test_tcp_engine test_tcp_engine.cpp)
  add_executable(test_udp_engine test_udp_engine.cpp)
  add_executable(test_icmp test_icmp.cpp)
  add_executable(test_integration test_integration.cpp)
  add_executable(test_ipv6 test_ipv6.cpp)
  add_executable(test_tun_queue_writer test_tun_queue_writer.cpp)
  add_executable(test_multi_queue test_multi_queue.cpp)

  foreach(t test_packet_buffer test_checksum test_ip_packet test_tcp_engine test_udp_engine test_icmp test_integration test_ipv6 test_tun_queue_writer test_multi_queue)
    target_link_libraries(${t} PRIVATE tunio)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${t} PRIVATE -Wno-stringop-overread)
    endif()
    add_test(NAME ${t} COMMAND ${t})
  endforeach()
endif()
