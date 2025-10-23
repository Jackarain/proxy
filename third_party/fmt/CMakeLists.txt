cmake_minimum_required(VERSION 3.5)

file(GLOB FMT_SRC
	src/format.cc
	src/os.cc
	include/fmt/*.h
)

add_library(fmt STATIC ${FMT_SRC})

if (MSVC)
	target_compile_options(fmt PUBLIC $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:MSVC>>:/utf-8>)
endif()

target_include_directories(fmt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include/)
set_target_properties(fmt PROPERTIES FOLDER "third_party")
set_target_properties(fmt PROPERTIES POSITION_INDEPENDENT_CODE ON)
#set_target_properties(fmt PROPERTIES OUTPUT_NAME z PREFIX "fmt" CLEAN_DIRECT_OUTPUT 1)
