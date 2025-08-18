#
# Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/url
#

# This file defines a modular target for the Boost URL test suite.
# Thus, it can be used in other projects or subdirectories that want to
# define test targets without needing to redeclare the test suite.

cmake_minimum_required(VERSION 3.8...3.20)
project(boost_url_test_suite LANGUAGES CXX)

# Library target with test_suite.cpp and test_suite.hpp
add_library(boost_url_test_suite STATIC test_suite.cpp test_suite.hpp)
target_include_directories(boost_url_test_suite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(boost_url_test_suite PUBLIC Boost::assert)
target_compile_features(boost_url_test_suite PUBLIC cxx_constexpr)

# Library target that links the previous and adds test_main.cpp
add_library(boost_url_test_suite_with_main STATIC test_main.cpp)
target_link_libraries(boost_url_test_suite_with_main PUBLIC boost_url_test_suite)
target_include_directories(boost_url_test_suite_with_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(boost_url_test_suite_with_main PUBLIC cxx_constexpr)

include(DiscoverTests.cmake)