# example dns_server_test

add_executable(dns_server_test
    dns_server_test.cpp
)

target_link_libraries(dns_server_test
    ${OPENSSL_LIBRARIES}
    ${PROXY_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

if (TARGET IOUring::IOUring)
    target_link_libraries(dns_server_test IOUring::IOUring)
endif()

set_property(TARGET dns_server_test PROPERTY FOLDER "example/dns_server_test")

if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129 AND CMAKE_VERSION VERSION_GREATER 3.20.3)
	set_property(TARGET dns_server_test PROPERTY CXX_STANDARD 23)
	message(STATUS "dns_server_test set cxx standard: C++23")
else()
	set_property(TARGET dns_server_test PROPERTY CXX_STANDARD 20)
	message(STATUS "dns_server_test set cxx standard: C++20")
endif()

if(ENABLE_BUILD_WERROR)
	if(MSVC)
		target_compile_options(dns_server_test PRIVATE /W4 /WX)
	elseif(UNIX AND NOT APPLE)
		if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
			target_compile_options(dns_server_test PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated)
		else()
			target_compile_options(dns_server_test PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated -Wno-deprecated-copy -Wno-error=deprecated-copy -Wno-pessimizing-move)
		endif()
	endif()
endif()
