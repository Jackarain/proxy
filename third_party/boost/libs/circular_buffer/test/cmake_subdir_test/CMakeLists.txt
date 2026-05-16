# Copyright 2018, 2019, 2026 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.31)

project(cmake_subdir_test LANGUAGES CXX)

add_subdirectory(../.. boostorg/circular_buffer)

# boostdep --brief circular_buffer

set(deps

# Primary dependencies

assert
concept_check
config
core
move
throw_exception
type_traits

# Secondary dependencies

preprocessor

)

foreach(dep IN LISTS deps)

    add_subdirectory(../../../${dep} boostorg/${dep})

endforeach()

add_executable(main main.cpp)
target_link_libraries(main Boost::circular_buffer)

enable_testing()
add_test(main main)

add_custom_target(check VERBATIM COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)
