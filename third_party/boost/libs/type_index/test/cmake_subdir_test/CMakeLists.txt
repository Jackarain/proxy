# Copyright  (c) 2016-2026 Antony Polukhin
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...4.0)

project(type_index_subdir_test LANGUAGES CXX)

foreach(dep IN ITEMS
  assert
  config
  core
  container_hash
  describe
  mp11
  smart_ptr
  throw_exception
  unordered
  predef)
    add_subdirectory(../../../${dep} boostorg/${dep})
endforeach()
add_subdirectory(../../ boostorg/type_index)

enable_testing()

if (BOOST_USE_MODULES)
    add_executable(boost_type_index_module_usage ../../modules/usage_sample.cpp)
    target_link_libraries(boost_type_index_module_usage PRIVATE Boost::type_index)
    add_test(NAME boost_type_index_module_usage COMMAND boost_type_index_module_usage)

    # Make sure that mixing includes and imports is fine for different TU
    add_executable(boost_type_index_module_usage_mu ../../modules/usage_test_mu1.cpp ../../modules/usage_test_mu2.cpp)
    target_link_libraries(boost_type_index_module_usage_mu PRIVATE Boost::type_index)
    add_test(NAME boost_type_index_module_usage_mu COMMAND boost_type_index_module_usage_mu)
endif()

list(APPEND RUN_TESTS_SOURCES
    compare_ctti_stl.cpp
    ctti_print_name.cpp
    track_13621.cpp
    type_index_runtime_cast_test.cpp
    type_index_test.cpp
)

foreach (testsourcefile ${RUN_TESTS_SOURCES})
    get_filename_component(testname ${testsourcefile} NAME_WLE)
    add_executable(${PROJECT_NAME}_${testname} ../${testsourcefile})
    target_link_libraries(${PROJECT_NAME}_${testname} Boost::type_index Boost::smart_ptr)
    add_test(NAME ${PROJECT_NAME}_${testname} COMMAND ${PROJECT_NAME}_${testname})
endforeach()
