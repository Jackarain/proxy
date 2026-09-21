#
# httpc  –  Modern C++20 HTTP client library
# ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#
# Standalone C++20 HTTP client library.
#
# Dependencies:
#   - Boost 1.84+ (asio, beast, url, variant2, system)
#   - OpenSSL
#
# Boost can be provided either as an installed package (find_package) or as a
# Boost source tree (e.g. a vendored third_party/boost).  A vendored source
# tree is auto-detected under third_party/boost, or can be given explicitly via
# HTTPC_BOOST_ROOT.
#

cmake_minimum_required(VERSION 3.16)

project(httpc
	VERSION 1.0.0
	DESCRIPTION "Modern C++20 HTTP client library built on Boost.Asio and Boost.Beast"
	LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(HTTPC_BUILD_EXAMPLES      "Build httpc examples"                          OFF)
option(HTTPC_BUILD_TESTS         "Build httpc tests"                             OFF)
option(HTTPC_SEPARATE_COMPILATION "Compile Boost.Asio/Beast separately"          ON)
option(HTTPC_INSTALL             "Generate httpc install rules"                  ON)
set(HTTPC_BOOST_ROOT "" CACHE PATH
	"Path to a Boost source tree (e.g. third_party/boost). When set, its per-library include dirs are used instead of find_package(Boost).")

# 单独构建时才设置默认构建类型；作为 third_party 被 add_subdirectory 引入时
# 不覆盖宿主工程的 CMAKE_BUILD_TYPE.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR
	AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

# 允许通过环境变量提供 Boost 源码树 (用于 CI 或外部构建脚本).
if(NOT HTTPC_BOOST_ROOT AND DEFINED ENV{HTTPC_BOOST_ROOT})
	set(HTTPC_BOOST_ROOT "$ENV{HTTPC_BOOST_ROOT}"
		CACHE PATH "Path to a Boost source tree (e.g. third_party/boost)" FORCE)
endif()

# 若项目自带 third_party/boost 源码树, 默认使用它.
if(NOT HTTPC_BOOST_ROOT AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost/libs/asio/include")
	set(HTTPC_BOOST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/boost"
		CACHE PATH "Path to a Boost source tree" FORCE)
	message(STATUS "Using vendored Boost source tree: ${HTTPC_BOOST_ROOT}")
endif()

set(HTTPC_BOOST_TARGETS "")
set(HTTPC_BOOST_INCLUDE_DIRS "")

# 作为 third_party 被宿主工程引入且宿主已提供 Boost 目标（如 proxy 内置的
# Boost superproject 或系统 Boost）时，直接复用，避免重复编译 Boost.URL 以及
# 头文件/ABI 混用.
if(TARGET Boost::url)
	set(HTTPC_BOOST_DEFINITIONS "")
	set(HTTPC_BOOST_TARGETS Boost::url Boost::system)
	foreach(_comp asio beast variant2)
		if(TARGET Boost::${_comp})
			list(APPEND HTTPC_BOOST_TARGETS Boost::${_comp})
		endif()
	endforeach()
	if(TARGET Boost::headers)
		list(APPEND HTTPC_BOOST_TARGETS Boost::headers)
	endif()
elseif(HTTPC_BOOST_ROOT)
	# Boost source tree: collect the per-library include dirs.
	# 头文件位于 libs/<lib>/include, 少量模块为 libs/<group>/<lib>/include
	# (例如 numeric/conversion), 这里一并收集, 并排除各库的测试头文件目录.
	file(GLOB HTTPC_BOOST_INCLUDE_DIRS
		"${HTTPC_BOOST_ROOT}/libs/*/include")
	file(GLOB _httpc_boost_nested_include_dirs
		"${HTTPC_BOOST_ROOT}/libs/*/*/include")
	list(FILTER _httpc_boost_nested_include_dirs EXCLUDE REGEX "/test/include$")
	list(APPEND HTTPC_BOOST_INCLUDE_DIRS ${_httpc_boost_nested_include_dirs})
	if(NOT HTTPC_BOOST_INCLUDE_DIRS)
		message(FATAL_ERROR "No Boost include dirs found under ${HTTPC_BOOST_ROOT}/libs/*/include")
	endif()

	# Boost.URL 的 url_view / pct_string_view 等接口需要其编译库, 这里从源码构建.
	# Boost.URL 由本工程自行编译, 关闭 Boost 的自动链接.
	set(HTTPC_BOOST_DEFINITIONS BOOST_URL_NO_LIB=1 BOOST_URL_STATIC_LINK=1)

	file(GLOB_RECURSE _httpc_boost_url_sources
		"${HTTPC_BOOST_ROOT}/libs/url/src/*.cpp")
	add_library(httpc_boost_url STATIC ${_httpc_boost_url_sources})
	target_include_directories(httpc_boost_url PUBLIC ${HTTPC_BOOST_INCLUDE_DIRS})
	target_compile_definitions(httpc_boost_url PUBLIC ${HTTPC_BOOST_DEFINITIONS})
	set_target_properties(httpc_boost_url PROPERTIES
		POSITION_INDEPENDENT_CODE ON
	)
	target_compile_features(httpc_boost_url PUBLIC cxx_std_20)
	set(HTTPC_BOOST_TARGETS httpc_boost_url)
else()
	set(HTTPC_BOOST_DEFINITIONS "")
	# Installed Boost (Boost.Asio / Beast / Variant2 are header-only and
	# exposed through Boost::headers; a Boost source superproject provides
	# dedicated Boost::asio / Boost::beast / Boost::variant2 targets).
	find_package(Boost 1.84 REQUIRED CONFIG COMPONENTS url system)

	set(HTTPC_BOOST_TARGETS Boost::url Boost::system)
	foreach(_comp asio beast variant2)
		if(TARGET Boost::${_comp})
			list(APPEND HTTPC_BOOST_TARGETS Boost::${_comp})
		endif()
	endforeach()
	if(TARGET Boost::headers)
		list(APPEND HTTPC_BOOST_TARGETS Boost::headers)
	endif()
endif()

if(NOT TARGET OpenSSL::SSL)
	find_package(OpenSSL REQUIRED)
endif()
if(NOT TARGET Threads::Threads)
	find_package(Threads REQUIRED)
endif()

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------
add_library(httpc STATIC
	include/httpc/httpc.hpp
	src/httpc.cpp
)

if(HTTPC_SEPARATE_COMPILATION)
	# 单独编译 Boost.Asio / Boost.Beast, 避免每个使用者重复实例化, 同时避免
	# 头文件模式与独立编译模式混用导致的符号冲突.
	target_sources(httpc PRIVATE src/lib_asio_beast.cpp)
	target_compile_definitions(httpc
		PUBLIC
			BOOST_ASIO_SEPARATE_COMPILATION
			BOOST_BEAST_SEPARATE_COMPILATION
	)
endif()

target_include_directories(httpc
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)

if(HTTPC_BOOST_INCLUDE_DIRS)
	# Boost 源码树的 include 目录以普通 -I 传入 (非系统目录, 无 include_next
	# 问题).
	target_include_directories(httpc PUBLIC ${HTTPC_BOOST_INCLUDE_DIRS})
endif()

if(HTTPC_BOOST_DEFINITIONS)
	target_compile_definitions(httpc PUBLIC ${HTTPC_BOOST_DEFINITIONS})
endif()

target_link_libraries(httpc
	PUBLIC
		${HTTPC_BOOST_TARGETS}
		OpenSSL::SSL
		OpenSSL::Crypto
		Threads::Threads
)

if(WIN32)
	# Boost.Asio 在 Windows 上依赖 Winsock/BCrypt; MinGW 不会通过
	# #pragma comment 自动链接这些系统库.
	target_link_libraries(httpc PUBLIC ws2_32 mswsock bcrypt)
endif()

target_compile_features(httpc PUBLIC cxx_std_20)

set_target_properties(httpc PROPERTIES
	POSITION_INDEPENDENT_CODE       ON
	CXX_VISIBILITY_PRESET           "hidden"
	VISIBILITY_INLINES_HIDDEN       ON
)

add_library(httpc::httpc ALIAS httpc)

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
if(HTTPC_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(HTTPC_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

if(HTTPC_INSTALL)
	# ---------------------------------------------------------------------------
	# Install
	# ---------------------------------------------------------------------------
	include(GNUInstallDirs)
	include(CMakePackageConfigHelpers)

	install(TARGETS httpc EXPORT httpcTargets
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	)
	if(TARGET httpc_boost_url)
		install(TARGETS httpc_boost_url EXPORT httpcTargets
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		)
	endif()

	install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

	configure_package_config_file(
		cmake/httpcConfig.cmake.in
		"${CMAKE_CURRENT_BINARY_DIR}/httpcConfig.cmake"
		INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/httpc
	)
	write_basic_package_version_file(
		"${CMAKE_CURRENT_BINARY_DIR}/httpcConfigVersion.cmake"
		VERSION ${PROJECT_VERSION}
		COMPATIBILITY SameMajorVersion
	)

	install(EXPORT httpcTargets
		NAMESPACE   httpc::
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/httpc
	)
	install(FILES
		"${CMAKE_CURRENT_BINARY_DIR}/httpcConfig.cmake"
		"${CMAKE_CURRENT_BINARY_DIR}/httpcConfigVersion.cmake"
		DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/httpc
	)
endif()
