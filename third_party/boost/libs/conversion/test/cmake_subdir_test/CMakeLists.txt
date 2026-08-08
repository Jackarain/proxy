# Copyright (c) 2025-2026 Antony Polukhin
# Copyright (c) 2025-2026 Fedor Osetrov
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...4.2)

project(conversion_subdir_test LANGUAGES CXX)

add_subdirectory(../../../assert boostorg/assert)
add_subdirectory(../../../core boostorg/core)
add_subdirectory(../../../config boostorg/config)
add_subdirectory(../../../smart_ptr boostorg/smart_ptr)
add_subdirectory(../../../throw_exception boostorg/throw_exception)


enable_testing()
add_subdirectory(../../ boostorg/conversion)

if (BOOST_USE_MODULES)
    add_executable(boost_conversion_module_usage ../../modules/usage_sample.cpp)
    target_link_libraries(boost_conversion_module_usage PRIVATE Boost::conversion)
    add_test(NAME boost_conversion_module_usage COMMAND boost_conversion_module_usage)

    # Make sure that mixing includes and imports is fine for different TU
    add_executable(boost_conversion_module_usage_mu ../../modules/usage_test_mu1.cpp ../../modules/usage_test_mu2.cpp)
    target_link_libraries(boost_conversion_module_usage_mu PRIVATE Boost::conversion)
    add_test(NAME boost_conversion_module_usage_mu COMMAND boost_conversion_module_usage_mu)
endif()
