cmake_minimum_required(VERSION 3.0)

project(socks_server)

add_executable(socks_server
  main.cpp
  io.hpp
  socks_server.hpp
)

find_package(Boost COMPONENTS system filesystem REQUIRED)

target_link_libraries(socks_server
  ${Boost_FILESYSTEM_LIBRARY}
  ${Boost_SYSTEM_LIBRARY}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

install(TARGETS socks_server RUNTIME DESTINATION bin)