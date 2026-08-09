# proxy server

add_executable(proxy_server
    main.cpp
    ../common/main.hpp
    ../common/version.cpp
)

target_link_libraries(proxy_server
    ${OPENSSL_LIBRARIES}
    ${PROXY_LIBRARIES}
)

target_include_directories(proxy_server
    PRIVATE
    ../common
)

if (TARGET IOUring::IOUring)
    target_link_libraries(proxy_server IOUring::IOUring)
endif()

set_property(SOURCE ../common/version.cpp PROPERTY COMPILE_DEFINITIONS "VERSION_GIT=\"${PVERSION}\"")
set_property(TARGET proxy_server PROPERTY FOLDER "server/proxy_server")

if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129 AND CMAKE_VERSION VERSION_GREATER 3.20.3)
	set_property(TARGET proxy_server PROPERTY CXX_STANDARD 23)
	message(STATUS "proxy_server set cxx standard: C++23")
else()
	set_property(TARGET proxy_server PROPERTY CXX_STANDARD 20)
	message(STATUS "proxy_server set cxx standard: C++20")
endif()

if (ANDROID)
    # 查找 Android log 库.
    find_library(
        log-lib
        log
    )

    # target_link_libraries 目标中链接 log 库.
    target_link_libraries(proxy_server ${log-lib})
endif()

if(ENABLE_BUILD_WERROR)
	if(MSVC)
		target_compile_options(proxy_server PRIVATE /W4 /WX)
	elseif(UNIX AND NOT APPLE)
		if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
			target_compile_options(proxy_server PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated)
		else()
			target_compile_options(proxy_server PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated -Wno-deprecated-copy -Wno-error=deprecated-copy -Wno-pessimizing-move)
		endif()
	endif()
endif()
