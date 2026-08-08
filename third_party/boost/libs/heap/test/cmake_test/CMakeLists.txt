# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.8...3.20)

project(boost_heap_cmake_test LANGUAGES CXX)

if(BOOST_CI_INSTALL_TEST)
    find_package(Boost CONFIG QUIET COMPONENTS heap)
    if(NOT TARGET Boost::heap)
        add_library(Boost::heap INTERFACE IMPORTED)
        set_target_properties(Boost::heap PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}")
    endif()
else()
    set(BOOST_INCLUDE_LIBRARIES heap)
    add_subdirectory(../../../.. deps/boost EXCLUDE_FROM_ALL)
endif()

add_executable(main ../cmake_install_test/main.cpp)
target_link_libraries(main Boost::heap)

enable_testing()
add_test(NAME main COMMAND main)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)
