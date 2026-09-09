# C 示例使用 POSIX socket 与 pthread 接口（getaddrinfo、arpa/inet.h 等），
# Windows 无对应原生模型，仅在非 Windows 平台构建。
if(NOT WIN32)
  enable_language(C)
  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED ON)

  find_package(Threads REQUIRED)
  add_executable(tun2socks_c tun2socks.c)
  target_link_libraries(tun2socks_c PRIVATE tunio_c Threads::Threads)
endif()
