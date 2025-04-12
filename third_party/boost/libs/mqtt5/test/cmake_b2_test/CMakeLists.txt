#
# Copyright (c) 2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.8...3.20)

project(cmake_b2_test LANGUAGES CXX)

find_package(Boost REQUIRED COMPONENTS headers)
find_package(Threads REQUIRED)

add_executable(main ../src/quick.cpp)
target_link_libraries(main PRIVATE Boost::headers Threads::Threads)

include(CTest)
add_test(main main)
