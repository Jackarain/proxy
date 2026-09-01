# benchmark 基于 POSIX socketpair 注入虚拟 TUN 设备（fd 语义），依赖
# arpa/inet.h、poll.h、unistd.h 等 POSIX 接口；Windows 无对应注入模型
# （native_handle_type 为 HANDLE），因此仅在非 Windows 平台构建。
set(TUNIO_EXAMPLE_TARGETS tun2socks tun_echo tun_packet)
if(NOT WIN32)
  list(APPEND TUNIO_EXAMPLE_TARGETS benchmark)
endif()

foreach(e ${TUNIO_EXAMPLE_TARGETS})
  add_executable(${e} ${e}.cpp)
  target_link_libraries(${e} PRIVATE tunio)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${e} PRIVATE -Wno-stringop-overread)
  endif()
endforeach()
