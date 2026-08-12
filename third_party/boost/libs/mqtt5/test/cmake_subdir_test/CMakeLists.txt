#
# Copyright (c) 2025-2026 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.8...3.20)

project(cmake_subdir_test LANGUAGES CXX)

set(BOOST_INCLUDE_LIBRARIES mqtt5)
add_subdirectory(../../../.. boostorg/boost)

add_executable(main ../src/quick.cpp)
target_link_libraries(main Boost::mqtt5)

enable_testing()
add_test(NAME main COMMAND main)
