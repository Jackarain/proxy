# example http_proxy_client

add_executable(http_proxy_client
    http_proxy_client.cpp
)

target_link_libraries(http_proxy_client
    ${PROXY_LIBRARIES}
)

set_property(TARGET http_proxy_client PROPERTY FOLDER "example/http_proxy_client")

if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129 AND CMAKE_VERSION VERSION_GREATER 3.20.3)
	set_property(TARGET http_proxy_client PROPERTY CXX_STANDARD 23)
	message(STATUS "http_proxy_client set cxx standard: C++23")
else()
	set_property(TARGET http_proxy_client PROPERTY CXX_STANDARD 20)
	message(STATUS "http_proxy_client set cxx standard: C++20")
endif()

if(ENABLE_BUILD_WERROR)
	if(MSVC)
		target_compile_options(http_proxy_client PRIVATE /W4 /WX)
	elseif(UNIX AND NOT APPLE)
		if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
			target_compile_options(http_proxy_client PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated)
		else()
			target_compile_options(http_proxy_client PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated -Wno-deprecated-copy -Wno-error=deprecated-copy -Wno-pessimizing-move)
		endif()
	endif()
endif()
