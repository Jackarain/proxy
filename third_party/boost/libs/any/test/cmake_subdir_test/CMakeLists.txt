# Copyright  (c) 2016-2025 Antony Polukhin
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...4.0)

project(any_subdir_test LANGUAGES CXX)

add_subdirectory(../../../assert boostorg/assert)
add_subdirectory(../../../core boostorg/core)
add_subdirectory(../../../config boostorg/config)
add_subdirectory(../../../container_hash boostorg/container_hash)
add_subdirectory(../../../describe boostorg/describe)
add_subdirectory(../../../detail boostorg/detail)
add_subdirectory(../../../integer boostorg/integer)
add_subdirectory(../../../mp11 boostorg/mp11)
add_subdirectory(../../../preprocessor boostorg/preprocessor)
add_subdirectory(../../../static_assert boostorg/static_assert)
add_subdirectory(../../../throw_exception boostorg/throw_exception)
add_subdirectory(../../../type_index boostorg/type_index)
add_subdirectory(../../../type_traits boostorg/type_traits)

add_subdirectory(../../ boostorg/any)

enable_testing()

if (BOOST_USE_MODULES)
    add_executable(boost_any_module_usage ../modules/usage_sample.cpp)
    target_link_libraries(boost_any_module_usage PRIVATE Boost::any)
    add_test(NAME boost_any_module_usage COMMAND boost_any_module_usage)
endif()

list(APPEND RUN_TESTS_SOURCES
    any_test_mplif.cpp
    basic_any_test_small_object.cpp
    any_test_rv.cpp
    basic_any_test.cpp

    unique_any/from_any.cpp
    unique_any/move.cpp
    unique_any/emplace.cpp
    unique_any/base.cpp

    basic_any_test_mplif.cpp
    basic_any_test_rv.cpp
    basic_any_test_large_object.cpp
    # any_test.cpp  # Ambiguous with modules, because all the anys now available
)

foreach (testsourcefile ${RUN_TESTS_SOURCES})
    get_filename_component(testname ${testsourcefile} NAME_WLE)
    add_executable(${PROJECT_NAME}_${testname} ../${testsourcefile})
    target_link_libraries(${PROJECT_NAME}_${testname} Boost::any)
    add_test(NAME ${PROJECT_NAME}_${testname} COMMAND ${PROJECT_NAME}_${testname})
endforeach()

