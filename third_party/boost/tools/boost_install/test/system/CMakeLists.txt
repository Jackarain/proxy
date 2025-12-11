# Copyright 2018, 2019 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at http://boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.5)

if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

project(CmakeConfigSystemTest LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../BoostVersion.cmake)

set(BOOST_HINTS)

if(USE_STAGED_BOOST)
  set(BOOST_HINTS HINTS ../../../../stage)
endif()

# System is header-only in b2 starting from 1.89, so there's no `system` component anymore

find_package(Boost ${BOOST_VERSION} EXACT REQUIRED OPTIONAL_COMPONENTS system ${BOOST_HINTS})

# Using `include_directories`, `link_directories`, `link_libraries`
# is bad practice, only done here for testing purposes.

include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})
link_libraries(${Boost_LIBRARIES})

add_executable(main quick.cpp)

enable_testing()
add_custom_target(check VERBATIM COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --no-tests=error -C $<CONFIG>)

add_test(main main)
