#
# Copyright (c) 2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.8...3.20)

project(cmake_install_test LANGUAGES CXX)

find_package(boost_mqtt5 REQUIRED)

add_executable(main ../src/quick.cpp)
target_link_libraries(main Boost::mqtt5)

include(CTest)
add_test(main main)
