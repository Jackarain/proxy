# Copyright 2018, 2019, 2021 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.20)

project(cmake_subdir_test LANGUAGES CXX)

add_subdirectory(../.. boostorg/hash2)

# boostdep --brief hash2

set(deps

# Primary dependencies

assert
config
container_hash
describe
mp11

# Secondary dependencies

)

foreach(dep IN LISTS deps)

    add_subdirectory(../../../${dep} boostorg/${dep})

endforeach()

add_executable(main main.cpp)
target_link_libraries(main Boost::hash2)

enable_testing()
add_test(main main)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)
