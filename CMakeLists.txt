cmake_minimum_required(VERSION 3.0)

project(socks_server)

if (NOT MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")
endif()

if(WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-D_WIN32_WINNT=0x0501)
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost COMPONENTS system filesystem REQUIRED)

add_executable(socks_server
  src/main.cpp
  src/io.hpp
  src/socks_server.hpp
)

if(UNIX)
	target_link_libraries(socks_server ${Boost_LIBRARIES} pthread)
else()
	target_link_libraries(socks_server ${Boost_LIBRARIES})
endif()

install(TARGETS socks_server RUNTIME DESTINATION bin)

