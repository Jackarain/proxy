# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(cmake_install_test LANGUAGES CXX)

if(BOOST_CI_INSTALL_TEST)
    find_package(Boost CONFIG QUIET COMPONENTS heap)
    if(NOT TARGET Boost::heap)
        add_library(Boost::heap INTERFACE IMPORTED)
        set_target_properties(Boost::heap PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}")
    endif()
else()
    find_package(boost_heap REQUIRED)
endif()

add_executable(main main.cpp)
target_link_libraries(main Boost::heap)

enable_testing()
add_test(main main)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)
