# launcher server

add_executable(launcher
    main.cpp
    ../common/main.hpp
    ../common/version.cpp
)

target_link_libraries(launcher
    ${OPENSSL_LIBRARIES}
    ${PROXY_LIBRARIES}
)

target_include_directories(launcher
    PRIVATE
    ../common
)

if (TARGET IOUring::IOUring)
    target_link_libraries(launcher IOUring::IOUring)
endif()

set_property(SOURCE ../common/version.cpp PROPERTY COMPILE_DEFINITIONS "VERSION_GIT=\"${PVERSION}\"")
set_property(TARGET launcher PROPERTY FOLDER "server/launcher")

if (MSVC AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.29.30129 AND CMAKE_VERSION VERSION_GREATER 3.20.3)
	set_property(TARGET launcher PROPERTY CXX_STANDARD 23)
	message(STATUS "launcher set cxx standard: C++23")
else()
	set_property(TARGET launcher PROPERTY CXX_STANDARD 20)
	message(STATUS "launcher set cxx standard: C++20")
endif()

if (ANDROID)
    # 查找 Android log 库.
    find_library(
        log-lib
        log
    )

    # target_link_libraries 目标中链接 log 库.
    target_link_libraries(launcher ${log-lib})
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
