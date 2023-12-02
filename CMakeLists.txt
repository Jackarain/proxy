cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0091 NEW) # for CMAKE_MSVC_RUNTIME_LIBRARY
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW) # cmake_policy(SET CMP0077 NEW)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(proxy LANGUAGES CXX)

################################################################################

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/cmake")

include(CheckCXXCompilerFlag)
include(CheckLibraryExists)
include(CheckCXXLinkerFlag)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

################################################################################

if (ENABLE_GIT_VERSION)
	find_package(Git)
	gitGetVersion(${CMAKE_CURRENT_SOURCE_DIR} proxy)
	set(VERSION_GIT ${proxy_WC_REVISION_HASH})
	string(TIMESTAMP PVERSION "%Y%m%d-${VERSION_GIT}-${CMAKE_SYSTEM_NAME}")
endif()

# Disable in-source builds to prevent source tree corruption.
if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "
FATAL: In-source builds are not allowed.
       You should create a separate directory for build files.
")
endif()
message(STATUS "CMAKE Version: ${CMAKE_VERSION}")

message(STATUS "Source Dir: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "Host System name: ${CMAKE_HOST_SYSTEM_NAME}")

set(default_build_type "Release")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
        STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                 "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

message(STATUS "System name: ${CMAKE_SYSTEM_NAME}")
message(STATUS "System version: ${CMAKE_SYSTEM_VERSION}")
if (WIN32)
	message(STATUS "Platform version: ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "SYSTEM NAME: ${CMAKE_SYSTEM_NAME}")


################################################################################

option(ENABLE_BUILD_WERROR			"All warnings being treated as errors" OFF)
option(ENABLE_GIT_VERSION			"Enable git version" ON)

option(ENABLE_USE_IO_URING			"Enable auto use io_uring" OFF)
option(ENABLE_BUILD_WITH_LIBCXX		"Build with libc++" OFF)
option(ENABLE_REUSEPORT				"Build with REUSEPORT support" OFF)

option(ENABLE_SYSTEM_ZLIB			"Build with system zlib support" OFF)
option(ENABLE_SYSTEM_OPENSSL		"Build with system openssl support" OFF)

option(ENABLE_USE_WOLFSSL			"Build with wolfssl support" ON)
option(ENABLE_USE_OPENSSL			"Build with openssl support" OFF)

option(ENABLE_MOLD					"build using mold" OFF)
option(ENABLE_LLD					"build using lld" OFF)

option(ENABLE_STATIC_LINK_TO_GCC "Build static link to gcc" ON)

option(ENABLE_TCMALLOC_STATIC		"Build with Tcmalloc support" OFF)
option(ENABLE_JEMALLOC_STATIC		"Build with Jemalloc support" OFF)

option(ENABLE_BUILD_TESTS			"build animals's unit tests" OFF)
option(ENABLE_LINKE_TO_LIBATOMIC	"link to libatomic.so - some platform requires it" OFF)

################################################################################

find_program(MOLD_LINKER mold)
find_program(LLD_LINKER lld)

set(CMAKE_C_STANDARD 11)

################################################################################

if (MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

	add_definitions(
		-D_CRT_SECURE_NO_DEPRECATE
		-D_CRT_SECURE_NO_WARNINGS
		-D_CRT_NONSTDC_NO_DEPRECATE
		-D_CRT_NONSTDC_NO_WARNINGS
		-D_SCL_SECURE_NO_DEPRECATE
		-D_SCL_SECURE_NO_WARNINGS
	)

	add_definitions(
		-DWIN32_LEAN_AND_MEAN
		-D_WIN32_WINNT=0x0601
		-DNOMINMAX
		-DUNICODE
		-D_UNICODE
		-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
		-D_SILENCE_ALL_CXX23_DEPRECATION_WARNINGS
		-D_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
		-D_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
	)

	add_definitions(
		-DBOOST_ALL_STATIC_LINK
		-DBOOST_THREAD_USE_LIB
		-DBOOST_FILESYSTEM_STATIC_LINK
		-DBOOST_USE_WINAPI_VERSION=0x0601
	)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj /MP /Zc:__cplusplus")
	message(STATUS "Using parallel compiling (/MP)")
	set(CMAKE_CXX_STACK_SIZE "100000000") #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /STACK:100000000")
	message(STATUS "Set stack size: 100000000 bytes")

	CHECK_CXX_COMPILER_FLAG(/std:c++20 COMPILER_HAS_STD_CXX20)

	if (COMPILER_HAS_STD_CXX20)
		set(CMAKE_CXX_STANDARD 20)
		set(CMAKE_CXX_STANDARD_REQUIRED ON)
		message(STATUS "Set default cxx standard: C++20")
	elseif (MSVC_VERSION GREATER_EQUAL "1900")
		set(CMAKE_CXX_STANDARD 17)
		set(CMAKE_CXX_STANDARD_REQUIRED ON)
		message(STATUS "Set default cxx standard: C++17")
	endif()
elseif(WIN32)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

	add_definitions(
		-DWIN32_LEAN_AND_MEAN
		-D_WIN32_WINNT=0x0601
		-DNOMINMAX
		-DUNICODE
		-D_UNICODE
		-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
	)

	add_definitions(
		-DBOOST_ALL_STATIC_LINK
		-DBOOST_THREAD_USE_LIB
		-DBOOST_FILESYSTEM_STATIC_LINK
		-DBOOST_USE_WINAPI_VERSION=0x0601
	)

	if (NOT MINGW)
		add_compile_options(/utf8)
	endif()
endif()

################################################################################

CHECK_CXX_COMPILER_FLAG(-fvisibility-inlines-hidden COMPILER_HAS_VISIBILITY_INLINE_HIDDEN)
CHECK_CXX_COMPILER_FLAG(-fvisibility=hidden COMPILER_HAS_VISIBILITY_HIDDEN)
CHECK_CXX_COMPILER_FLAG(-fdiagnostics-color=always COMPILER_HAS_COLOR)
CHECK_CXX_COMPILER_FLAG(-fcoroutines COMPILER_HAS_FCOROUTINES)
CHECK_CXX_COMPILER_FLAG(-fcoroutines-ts COMPILER_HAS_FCOROUTINES_TS)
CHECK_CXX_COMPILER_FLAG(-std=c++20 COMPILER_HAS_STD_CXX20)

if (${COMPILER_HAS_VISIBILITY_INLINE_HIDDEN})
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
endif()

if (${COMPILER_HAS_VISIBILITY_HIDDEN})
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
endif()

if (${COMPILER_HAS_COLOR})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
endif()

if (COMPILER_HAS_FCOROUTINES)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
endif()

#if (COMPILER_HAS_FCOROUTINES_TS)
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts")
#endif()

if (ENABLE_SYSTEMD_LOGGING)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_SYSTEMD_LOGGING")
endif()

################################################################################

if (NOT MSVC)
	if(NOT APPLE)
		if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			if (ENABLE_BUILD_WITH_LIBCXX)
				set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
				CHECK_LINKER_FLAG(CXX "-rtlib=compiler-rt" IS_COMPILER_RT_SUPPORTED)

				if (IS_COMPILER_RT_SUPPORTED)
					message(STATUS "clang supports compiler-rt, use it")
					add_link_options(-rtlib=compiler-rt -unwindlib=libunwind)
				endif()
			else()
				set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -static-libgcc")
			endif()
		elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			if (MINGW)
				set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libstdc++")
			else()
				set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
			endif()
			if (ENABLE_STATIC_LINK_TO_GCC)
				set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc")
			endif()
		endif()

		if (ENABLE_REUSEPORT)
			add_definitions(-DENABLE_REUSEPORT)
		endif()
	endif()
	if (ANDROID)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fPIE -pie")
	endif()
	if(COMPILER_HAS_STD_CXX20)
		set(CMAKE_CXX_STANDARD 20)
		add_definitions(-DUSE_STD_STRING_VIEW)
		message(STATUS "Set default cxx standard: C++20")
	else()
		message(FATAL_ERROR "need at least GCC 11 or clang 14")
	endif()

	if (ENABLE_MOLD)
		if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			if (MOLD_LINKER)
				list(APPEND CMAKE_EXE_LINKER_FLAGS "-fuse-ld=mold")
			endif()
		endif()
	endif()

	if (ENABLE_LLD)
		if (LLD_LINKER)
			list(APPEND CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
		endif()
	endif()
endif(NOT MSVC)

################################################################################

if (ENABLE_USE_OPENSSL)
	if (ENABLE_SYSTEM_OPENSSL)
		find_package(OpenSSL)
	else()
		set(OpenSSL_FOUND TRUE)
		add_subdirectory(third_party/openssl)
		if (NOT MSVC AND NOT APPLE)
			if (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
				set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--exclude-libs,ALL ${CMAKE_SHARED_LINKER_FLAGS}")
				set(CMAKE_MODULE_LINKER_FLAGS "-Wl,--exclude-libs,ALL ${CMAKE_MODULE_LINKER_FLAGS}")
				set(CMAKE_EXE_LINKER_FLAGS "-Wl,--exclude-libs,ALL ${CMAKE_EXE_LINKER_FLAGS}")
			endif()
		endif()
	endif()
	include_directories(${OPENSSL_INCLUDE_DIR})

	message(STATUS "Using openssl...")
endif()

################################################################################

if (ENABLE_USE_WOLFSSL)

	set(WOLFSSL_EXAMPLES OFF)
	set(WOLFSSL_CRYPT_TESTS OFF)

	set(WOLFSSL_ASIO ON)
	set(OpenSSL_FOUND TRUE)

	set(BUILD_SHARED_LIBS OFF)

	add_definitions(-DBOOST_ASIO_USE_WOLFSSL)

	if (NOT MSVC AND NOT APPLE)
		if (NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-stringop-overflow")
		endif()
	endif()

	add_subdirectory(third_party/wolfssl)

	link_libraries(wolfssl::wolfssl)

	set(WOLFSSL_INCLUDE_DIRS
		${CMAKE_CURRENT_SOURCE_DIR}/third_party/wolfssl/wolfssl/
	)
	include_directories(${WOLFSSL_INCLUDE_DIRS})

	message(STATUS "Using wolfssl...")
endif()

################################################################################

if (ENABLE_BUILD_TESTS)
	set(USE_BOOST_TEST ON)
endif()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

#独立编译asio时, 必须要在使用者项目定义, 否则会将
#定义BOOST_ASIO_HEADER_ONLY, 使用者将会导致符号冲突.

add_definitions(-DBOOST_ASIO_SEPARATE_COMPILATION)
add_definitions(-DBOOST_BEAST_SEPARATE_COMPILATION)

#此BOOST_ASIO_DYN_LINK只有在编译asio为动态链接的时候
#才能启用, 它在msvc下将BOOST_ASIO_DECL分别自动作为
#dllimport、dllexport添加.
#add_definitions(-DBOOST_ASIO_DYN_LINK)

add_definitions(-DBOOST_LOCALE_HIDE_AUTO_PTR)
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)
add_definitions(-DBOOST_DISABLE_PRAGMA_MESSAGE)
add_definitions(-DBOOST_COROUTINES_NO_DEPRECATION_WARNING)
add_definitions(-DBOOST_PROCESS_USE_STD_FS)
if (ENABLE_USE_IO_URING)
	find_package(IOUring)

	if (IOUring_FOUND)
		message(STATUS "Linux using io_uring...")
		add_definitions(-DBOOST_ASIO_HAS_IO_URING -DBOOST_ASIO_DISABLE_EPOLL)
		link_libraries(${IOUring_LIBRARIES})
	endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	add_definitions(-DBOOST_ASIO_HAS_CO_AWAIT)
endif()

set(Boost_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost)
add_subdirectory(third_party/boost EXCLUDE_FROM_ALL)

################################################################################

find_package(Threads)
link_libraries(${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

################################################################################

if (ENABLE_TCMALLOC_STATIC)
	find_package(Tcmalloc)
	link_libraries(${Tcmalloc_LIBRARY})
endif()

if (ENABLE_JEMALLOC_STATIC)
	find_package(JeMalloc)
	link_libraries(${JEMALLOC_LIBRARIES})
endif()

if (WIN32 OR NOT ENABLE_SYSTEM_ZLIB)
	add_subdirectory(third_party/zlib EXCLUDE_FROM_ALL)

	set(ZLIB_LIBRARIES zlibstatic)
	set(ZLIB_INCLUDE_DIRS
		${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib
		${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib/contrib/minizip
	)
else()
	find_package(ZLIB REQUIRED)
endif()

################################################################################

add_subdirectory(third_party/fmt)

################################################################################

add_subdirectory(proxy)

################################################################################

include_directories(third_party
	${ZLIB_INCLUDE_DIRS}
	${PROXY_INCLUDE_DIRS}
)

################################################################################

link_libraries(
		Boost::thread
		Boost::date_time
		Boost::program_options

		Boost::url

		fmt

		${OPENSSL_LIBRARIES}
		${ZLIB_LIBRARIES}
		${PROXY_LIBRARIES}
)

################################################################################

if (WIN32)

	if (MINGW)
		link_libraries(
			secur32
			ws2_32
			mswsock
			bcrypt
			iphlpapi
		)
	else()
		link_libraries(
			Secur32.lib
			Bcrypt.lib
			Winmm.lib
			Mswsock.lib
		)
	endif()
elseif (UNIX AND NOT APPLE)
	# aviod link std::filesystem fail.
	set(std_fs_workaround "")
	if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
		set(std_fs_workaround stdc++fs)
	endif()

	link_libraries(
		${std_fs_workaround}
	)
endif()

if (ENABLE_LINKE_TO_LIBATOMIC)
	link_libraries(atomic)
endif()

################################################################################

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (WIN32)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/debug)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/release)
endif()

################################################################################

add_subdirectory(example)
add_subdirectory(server)

################################################################################

if (ENABLE_BUILD_TESTS)
	link_libraries(Boost::test)

	enable_testing()
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/proxy/include)
	#add_subdirectory(unittests)
endif()

################################################################################

configure_file(proxy.service.in proxy.service)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/proxy.service DESTINATION /usr/lib/systemd/system/)
