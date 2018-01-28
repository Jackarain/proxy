cmake_minimum_required(VERSION 3.0)

option(USE_HUNTER "Use hunter" OFF)

if (USE_HUNTER)
	include("cmake/HunterGate.cmake")
	HunterGate(
		URL "https://github.com/ruslo/hunter/archive/v0.19.102.tar.gz"
		SHA1 "49a8333d7efc24c531aed97159fe58a68374185f"
	)
endif()

project(socks_server)

if (USE_HUNTER)
	hunter_add_package(Boost COMPONENTS thread system filesystem coroutine context date_time regex program_options)
endif()

if (NOT MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
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
set(Boost_USE_STATIC_RUNTIME ON)

find_package(Boost COMPONENTS thread system filesystem coroutine context date_time regex program_options REQUIRED)
link_libraries(${Boost_LIBRARIES})
include_directories(${Boost_INCLUDE_DIRS})

add_executable(socks_server
  src/server.cpp
  src/v7.c

  src/io.hpp
  src/socks_server.hpp
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

