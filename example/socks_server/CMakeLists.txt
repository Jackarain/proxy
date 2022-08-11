
add_executable(socks_server
    socks_server.cpp
)

target_link_libraries(socks_server
    libsocks
	Boost::asio
	Boost::beast
)

target_include_directories(socks_server PUBLIC ${BASE_SOURCE_DIR})

set_property(TARGET socks_server PROPERTY FOLDER "example/socks_server")

if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129 AND CMAKE_VERSION VERSION_GREATER 3.20.3)
	set_property(TARGET socks_server PROPERTY CXX_STANDARD 23)
	message(STATUS "socks_server set cxx standard: C++23")
else()
	set_property(TARGET socks_server PROPERTY CXX_STANDARD 20)
	message(STATUS "socks_server set cxx standard: C++20")
endif()

if(ENABLE_BUILD_WERROR)
	if(MSVC)
		target_compile_options(socks_server PRIVATE /W4 /WX)
	elseif(UNIX AND NOT APPLE)
		if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
			target_compile_options(socks_server PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated)
		else()
			target_compile_options(socks_server PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated -Wno-deprecated-copy -Wno-error=deprecated-copy -Wno-pessimizing-move)
		endif()
	endif()
endif()
