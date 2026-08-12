# launcher
#
# gproxy launcher 的 C++ 版本：
# WebUI 实例管理器
# （REST API + WebSocket JSON-RPC 控制通道 + instances.json 持久化）。
# JSON-RPC 基于 third_party/tinyrpc 的 jsonrpc.hpp。
#
# 并发模型：C++23 标准 + C++20 协程（boost.asio co_spawn/awaitable）。
# 整个服务运行在单个 io_context 上的小线程池中，无每连接线程、无 poll。
# WebUI 静态资源在构建期内嵌进可执行文件（webui_embedded.cpp 由
# embed_webui.cmake 生成），运行时直接从内存提供。

# 平台相关的子进程管理实现（创建/采集输出/终止）。
if (WIN32)
	set(LAUNCHER_PROC_SRC proc_windows.cpp)
else()
	set(LAUNCHER_PROC_SRC proc_unix.cpp)
endif()

# 构建期内嵌 WebUI：生成 webui_embedded.cpp（静态资源直接编入可执行文件）。
set(WEBUI_EMBEDDED_CPP ${CMAKE_CURRENT_BINARY_DIR}/webui_embedded.cpp)
file(GLOB_RECURSE WEBUI_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/webui
	${CMAKE_CURRENT_SOURCE_DIR}/webui/*)
set(WEBUI_FILE_DEPS)
foreach (f ${WEBUI_FILES})
	list(APPEND WEBUI_FILE_DEPS ${CMAKE_CURRENT_SOURCE_DIR}/webui/${f})
endforeach()
add_custom_command(
	OUTPUT ${WEBUI_EMBEDDED_CPP}
	COMMAND ${CMAKE_COMMAND}
		-DINPUT_DIR=${CMAKE_CURRENT_SOURCE_DIR}/webui
		-DOUTPUT_CPP=${WEBUI_EMBEDDED_CPP}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/embed_webui.cmake
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/embed_webui.cmake ${WEBUI_FILE_DEPS}
	COMMENT "Embedding launcher webui assets"
	VERBATIM
)

add_executable(launcher
	main.cpp
	options.cpp
	${LAUNCHER_PROC_SRC}
	manager.cpp
	http_server.cpp
	version.cpp
	webui_embedded.hpp
	${WEBUI_EMBEDDED_CPP}
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
	${CMAKE_CURRENT_SOURCE_DIR}
)

set_property(SOURCE version.cpp PROPERTY COMPILE_DEFINITIONS "VERSION_GIT=\"${PVERSION}\"")
set_property(TARGET launcher PROPERTY FOLDER "server/launcher")

# 严格要求 C++23 标准（协程 / 现代特性）。
set_property(TARGET launcher PROPERTY CXX_STANDARD 23)
set_property(TARGET launcher PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET launcher PROPERTY CXX_EXTENSIONS OFF)
message(STATUS "launcher set cxx standard: C++23")

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
