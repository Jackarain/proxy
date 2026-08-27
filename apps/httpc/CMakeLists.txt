#
# httpc  –  Modern C++20 HTTP client library
#

file(GLOB httpc_sources
	include/httpc/*.hpp
	src/*.cpp
)

add_library(httpc STATIC
	${httpc_sources}
)

target_include_directories(httpc
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
)

target_link_libraries(httpc
	PUBLIC
		Boost::asio
		Boost::beast
		Boost::url
		Boost::variant2
		OpenSSL::SSL
		OpenSSL::Crypto
	PRIVATE
		Boost::system
)

target_compile_definitions(httpc
	PRIVATE
		BOOST_ASIO_SEPARATE_COMPILATION
		BOOST_BEAST_SEPARATE_COMPILATION
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
	target_compile_definitions(httpc PUBLIC BOOST_ASIO_HAS_CO_AWAIT)
endif()

target_compile_features(httpc PUBLIC cxx_std_20)

set_target_properties(httpc PROPERTIES
	FOLDER                          "incubator"
	POSITION_INDEPENDENT_CODE       ON
	CXX_VISIBILITY_PRESET           "hidden"
	VISIBILITY_INLINES_HIDDEN       ON
)

# 别名
add_library(httpc::httpc ALIAS httpc)
