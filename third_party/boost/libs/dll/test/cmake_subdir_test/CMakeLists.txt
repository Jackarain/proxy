# Copyright 2026 Fedor Osetrov
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...4.20)

project(cmake_subdir_test LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(deps

atomic
align
assert
config
core
predef
system
throw_exception
type_index
winapi

compat
container_hash
describe
variant
variant2
integer
mp11
fusion
function_types
detail
preprocessor
mpl
tuple
type_traits
utility
io
typeof
function
bind
filesystem
iterator
concept_check
optional
smart_ptr
scope
unordered

)

foreach(dep IN LISTS deps)

  add_subdirectory(../../../${dep} boostorg/${dep})

endforeach()

enable_testing()
add_subdirectory(../.. boostorg/dll)

if(BOOST_USE_MODULES)
  add_library(dll_sample_plugin_module SHARED)
  target_link_libraries(dll_sample_plugin_module PRIVATE Boost::config)
  target_sources(dll_sample_plugin_module
    PUBLIC
      FILE_SET CXX_MODULES
      BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../../modules
      FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/sample_plugin.cppm
    PUBLIC
      FILE_SET HEADERS
      BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/../../example/tutorial_common
      FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../example/tutorial_common/my_plugin_api.hpp
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/sample_plugin.cpp
  )

  add_executable(boost_dll_module_usage ../../modules/usage_sample.cpp)
  target_link_libraries(boost_dll_module_usage PRIVATE Boost::dll)
  add_test(NAME boost_dll_module_usage COMMAND boost_dll_module_usage "$<TARGET_FILE:dll_sample_plugin_module>")
  add_dependencies(boost_dll_module_usage dll_sample_plugin_module)

endif()
