# launcher
#
# gproxy launcher 的 C++ 版本：
# WebUI 实例管理器
# （REST API + WebSocket JSON-RPC 控制通道 + instances.json 持久化）。
# JSON-RPC 基于 third_party/tinyrpc 的 jsonrpc.hpp。

add_executable(launcher
    main.cpp
    options.cpp
    proc.cpp
    manager.cpp
    http_server.cpp
    version.cpp
)

target_link_libraries(launcher
    ${OPENSSL_LIBRARIES}
    ${PROXY_LIBRARIES}
)

# 启用 io_uring 时链接 liburing（与 proxy_server 一致），
# 否则 asio 的 io_uring_service 会产生未定义符号链接错误。
if (TARGET IOUring::IOUring)
    target_link_libraries(launcher IOUring::IOUring)
endif()

# tinyrpc jsonrpc 头文件.
target_include_directories(launcher
    PRIVATE
    ${PROJECT_SOURCE_DIR}/third_party/tinyrpc/include
)

# 构建后把 webui 静态资源复制到可执行文件旁边，运行时直接从磁盘提供。
add_custom_command(TARGET launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/webui
        $<TARGET_FILE_DIR:launcher>/webui
    COMMENT "Copy launcher webui assets"
)

set_property(SOURCE version.cpp PROPERTY COMPILE_DEFINITIONS "VERSION_GIT=\"${PVERSION}\"")
set_property(TARGET launcher PROPERTY FOLDER "server/launcher")

if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129 AND CMAKE_VERSION VERSION_GREATER 3.20.3)
	set_property(TARGET launcher PROPERTY CXX_STANDARD 23)
	message(STATUS "launcher set cxx standard: C++23")
else()
	set_property(TARGET launcher PROPERTY CXX_STANDARD 20)
	message(STATUS "launcher set cxx standard: C++20")
endif()

if(ENABLE_BUILD_WERROR)
	if(MSVC)
		target_compile_options(launcher PRIVATE /W4 /WX)
	elseif(UNIX AND NOT APPLE)
		if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
			target_compile_options(launcher PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated)
		else()
			target_compile_options(launcher PRIVATE -Werror -Wall -Wextra -Wno-unknown-pragmas -Wno-deprecated -Wno-deprecated-copy -Wno-error=deprecated-copy -Wno-pessimizing-move)
		endif()
	endif()
endif()
