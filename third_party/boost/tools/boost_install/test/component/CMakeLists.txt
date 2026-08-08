# Copyright 2018, 2019, 2026 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at http://boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.5...3.31)

if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

project(CmakeConfigComponentTest LANGUAGES CXX)

include(${CMAKE_CURRENT_LIST_DIR}/../BoostVersion.cmake)

string(MAKE_C_IDENTIFIER "${COMPONENT}" COMPONENT) # numeric/conversion -> numeric_conversion

find_package(Boost ${BOOST_VERSION} EXACT REQUIRED COMPONENTS ${COMPONENT})

add_executable(main main.cpp)
target_link_libraries(main Boost::${COMPONENT})

enable_testing()
add_custom_target(check VERBATIM COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --no-tests=error -C $<CONFIG>)

add_test(main main)
