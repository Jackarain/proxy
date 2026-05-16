# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.31)

project(cmake_subdir_test LANGUAGES CXX)

add_subdirectory(../.. boostorg/align)

set(deps

# Primary dependencies

assert
config
core

# Secondary dependencies

throw_exception
)

foreach(dep IN LISTS deps)

  add_subdirectory(../../../${dep} boostorg/${dep})

endforeach()

# --target check

add_executable(main main.cpp)
target_link_libraries(main Boost::align)

enable_testing()
add_test(main main)

add_custom_target(check VERBATIM COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)
