cmake_minimum_required(VERSION 3.0)

option(USE_HUNTER "Use hunter" OFF)

if (USE_HUNTER)
	include("cmake/HunterGate.cmake")
	HunterGate(
		URL "https://github.com/cpp-pm/hunter/archive/v0.23.253.tar.gz"
		SHA1 "88ea6d37c897a81a080eb9ae0f69d7807bbb3c73"
	)
endif()

project(socks_server)

if (USE_HUNTER)
	hunter_add_package(Boost COMPONENTS thread system filesystem coroutine context date_time regex program_options)
endif()


include(CheckCXXCompilerFlag)

CHECK_CXX_COMPILER_FLAG(-std=c++17 LIBCXX_HAS_STDCXX17_FLAG)

if (NOT MSVC)
	if(NOT LIBCXX_HAS_STDCXX17_FLAG)
		message(FATAL_ERROR "need at least GCC 9 or clang 9")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")
else()
	set(CompilerFlags
			CMAKE_CXX_FLAGS_DEBUG
			CMAKE_CXX_FLAGS_RELEASE
			CMAKE_CXX_FLAGS_MINSIZEREL
			CMAKE_CXX_FLAGS_RELWITHDEBINFO
			CMAKE_C_FLAGS_DEBUG
			CMAKE_C_FLAGS_RELEASE
			CMAKE_C_FLAGS_MINSIZEREL
			CMAKE_C_FLAGS_RELWITHDEBINFO
		)

	foreach(CompilerFlag ${CompilerFlags})
		string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
	endforeach()

	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	add_definitions(-D_WIN32_WINNT=0x0501)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()


set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
#set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost COMPONENTS thread system filesystem coroutine context date_time regex program_options REQUIRED)

message(STATUS "Find boost version: ${Boost_VERSION}")
if (NOT Boost_VERSION VERSION_LESS 106700)
	message(STATUS "Use boost coroutines v2")
	add_definitions(-DBOOST_COROUTINES_V2)
endif()

link_libraries(${Boost_LIBRARIES})
include_directories(${Boost_INCLUDE_DIRS})

add_executable(socks_server
  src/server.cpp
  src/v7.c

  src/io.hpp
  src/socks_server.hpp
  src/async_connect.hpp
  src/v7.h
)

add_executable(socks_client
  src/client.cpp

  src/io.hpp
  src/socks_client.hpp
)

if(UNIX)
	target_link_libraries(socks_server ${Boost_LIBRARIES} pthread)
	target_link_libraries(socks_client ${Boost_LIBRARIES} pthread)
else()
	target_link_libraries(socks_server ${Boost_LIBRARIES})
	target_link_libraries(socks_client ${Boost_LIBRARIES})
endif()

install(TARGETS socks_server RUNTIME DESTINATION bin)
install(TARGETS socks_client RUNTIME DESTINATION bin)

