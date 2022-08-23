
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DENABLE_LOGGER")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")
set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -DENABLE_LOGGER -DDISABLE_LOGGER_TO_CONSOLE -DLOGGER_DBG_VIEW")

file(GLOB libsocks_HEADERS
	include/socks/async_connect.hpp
	include/socks/logging.hpp
	include/socks/scoped_exit.hpp
	include/socks/socks_client.hpp
	include/socks/socks_enums.hpp
	include/socks/socks_error_code.hpp
	include/socks/socks_server.hpp
	include/socks/use_awaitable.hpp
)

file(GLOB url_HEADERS
	include/url/url_view.hpp
)

file(GLOB socks_SRCS
	src/socks/socks_client.cpp
	src/socks/socks_error_code.cpp
	src/socks/socks_server.cpp
)

add_library(libsocks
	${libsocks_HEADERS}
	${url_HEADERS}
	${socks_SRCS}
)

source_group("Header Files\\socks" FILES ${libsocks_HEADERS})
source_group("Header Files\\url" FILES ${url_HEADERS})

set_target_properties(libsocks PROPERTIES OUTPUT_NAME socks PREFIX "lib" CLEAN_DIRECT_OUTPUT 1)

if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129 AND CMAKE_VERSION VERSION_GREATER 3.20.3)
	set_property(TARGET libsocks PROPERTY CXX_STANDARD 23)
	message(STATUS "libsocks set cxx standard: C++23")
else()
	set_property(TARGET libsocks PROPERTY CXX_STANDARD 20)
	message(STATUS "libsocks set cxx standard: C++20")
endif()

set(BASE_SOURCE_DIR
	${CMAKE_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(libsocks PUBLIC ${BASE_SOURCE_DIR})

if(ENABLE_BUILD_WERROR)
	if(MSVC)
		target_compile_options(libsocks PRIVATE /W4 /WX)
	elseif(UNIX AND NOT APPLE)
		if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
			target_compile_options(libsocks PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated)
		else()
			target_compile_options(libsocks PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated -Wno-deprecated-copy -Wno-error=deprecated-copy -Wno-pessimizing-move)
		endif()
	endif()
endif()

install(TARGETS libsocks DESTINATION bin)
