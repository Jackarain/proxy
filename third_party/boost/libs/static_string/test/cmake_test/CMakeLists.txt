# Copyright 2021-2025 Alexander Grund
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.8...3.16)

project(cmake_subdir_test LANGUAGES CXX)

# Those 2 should work the same
# while using find_package for the installed Boost avoids the need to manually specify dependencies
if(BOOST_CI_INSTALL_TEST)
    find_package(boost_static_string REQUIRED)
else()
    set(BOOST_INCLUDE_LIBRARIES static_string)
    add_subdirectory(../../../.. deps/boost EXCLUDE_FROM_ALL)
endif()

add_executable(main main.cpp)
target_link_libraries(main Boost::static_string)

enable_testing()
add_test(NAME main COMMAND main)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)