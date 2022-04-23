
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLE_LOGGER")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")

file(GLOB socks_SRCS
	include/socks/*.hpp
	src/socks/*.cpp
	src/main/*.cpp
)

add_library(libsocks
	${socks_SRCS}
)

add_executable(socks
	src/main/main.cpp
)

target_link_libraries(socks
	libsocks
	Boost::asio
	Boost::beast
)

set_target_properties(libsocks PROPERTIES OUTPUT_NAME socks PREFIX "lib" CLEAN_DIRECT_OUTPUT 1)

if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129 AND CMAKE_VERSION VERSION_GREATER 3.20.3)
	set_property(TARGET socks libsocks PROPERTY CXX_STANDARD 23)
	message(STATUS "socks set cxx standard: C++23")
else()
	set_property(TARGET socks libsocks PROPERTY CXX_STANDARD 20)
	message(STATUS "socks set cxx standard: C++20")
endif()

set(BASE_SOURCE_DIR
	${CMAKE_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(socks PUBLIC ${BASE_SOURCE_DIR})
target_include_directories(libsocks PUBLIC ${BASE_SOURCE_DIR})

if(ENABLE_BUILD_WERROR)
	if(MSVC)
		target_compile_options(socks PRIVATE /W4 /WX)
		target_compile_options(libsocks PRIVATE /W4 /WX)
	elseif(UNIX AND NOT APPLE)
		if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
			target_compile_options(socks PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated)
			target_compile_options(libsocks PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated)
		else()
			target_compile_options(socks PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated -Wno-deprecated-copy -Wno-error=deprecated-copy -Wno-pessimizing-move)
			target_compile_options(libsocks PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated -Wno-deprecated-copy -Wno-error=deprecated-copy -Wno-pessimizing-move)
		endif()
	endif()
endif()

install(TARGETS socks DESTINATION bin)
install(TARGETS libsocks DESTINATION bin)
