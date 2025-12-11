#
# Copyright (c) 2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.8...3.20)

project(cmake_subdir_test LANGUAGES CXX)

add_subdirectory(../../../mqtt5 boostorg/mqtt5)

# `boostdep --brief mqtt5`
set(deps
  # Primary dependencies
  
  asio
  assert
  # beast # Optional dependency, boostdep finds it because of websocket.hpp
  container
  core
  endian
  random
  range
  smart_ptr
  system
  type_traits
  
  # Secondary dependencies
  
  align
  config
  context
  date_time
  throw_exception
  bind
  container_hash
  intrusive
  # logic # Beast dependency
  mp11
  optional
  preprocessor
  static_assert
  # static_string # Beast dependency
  # type_index # Beast dependency
  winapi
  move
  function_types
  functional
  mpl
  tuple
  typeof
  utility
  array
  dynamic_bitset
  integer
  io
  concept_check
  conversion
  detail
  iterator
  regex
  "function"
  pool
  unordered
  variant2
  describe
  predef
  algorithm
  lexical_cast
  numeric/conversion
  tokenizer
  fusion
  exception
)

foreach(dep IN LISTS deps)
  add_subdirectory(../../../${dep} boostorg/${dep} EXCLUDE_FROM_ALL)
endforeach()

if (BUILD_TESTING)
  add_subdirectory(../../../test boostorg/test EXCLUDE_FROM_ALL)
endif()

add_executable(main ../src/quick.cpp)
target_link_libraries(main Boost::mqtt5)

include(CTest)
add_test(main main)
