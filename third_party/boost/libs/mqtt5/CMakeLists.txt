#
# Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.8...3.20)

project(boost_mqtt5 VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

include(cmake/project-is-top-level.cmake)

# Determine if this is the superproject or called from add_subdirectory.
if(NOT DEFINED BOOST_MQTT5_MAIN_PROJECT)
  set(BOOST_MQTT5_MAIN_PROJECT OFF)
  if(PROJECT_IS_TOP_LEVEL)
    set(BOOST_MQTT5_MAIN_PROJECT ON)
  endif()
endif()

add_library(boost_mqtt5 INTERFACE)
add_library(Boost::mqtt5 ALIAS boost_mqtt5)

# If non-Boost dependencies are not found, we just bail out.
find_package(Threads)
if(NOT Threads_FOUND)
  message(STATUS "Boost.MQTT5 has been disabled, because the required package Threads hasn't been found")
  return()
endif()

target_include_directories(boost_mqtt5 INTERFACE include)
target_compile_features(boost_mqtt5 INTERFACE cxx_std_17)

if(BOOST_MQTT5_MAIN_PROJECT)
  find_package(Boost 1.82 REQUIRED)
  if (NOT Boost_FOUND)
    message(STATUS "Cannot find Boost!")
    return()
  endif()
  target_link_libraries(boost_mqtt5 INTERFACE Boost::headers Threads::Threads)
else()
  target_link_libraries(
    boost_mqtt5
    INTERFACE
    Boost::asio
    Boost::assert
    # Boost::beast # Optional, only used for MQTT connections over WebSocket.
    Boost::container
    Boost::core
    Boost::endian
    Boost::fusion
    Boost::optional
    Boost::random
    Boost::range
    Boost::smart_ptr
    Boost::spirit
    Boost::system
    Boost::type_traits
    Threads::Threads
  )
endif()

option(BOOST_MQTT5_PUBLIC_BROKER_TESTS OFF "Whether to run tests requiring a public MQTT broker")
mark_as_advanced(BOOST_MQTT5_PUBLIC_BROKER_TESTS)

if(BUILD_TESTING)
  # Custom target tests; required by the Boost superproject
  if(NOT TARGET tests)
      add_custom_target(tests)
  endif()
  add_subdirectory(test)
endif()

if(BOOST_MQTT5_MAIN_PROJECT)
  option(BUILD_EXAMPLES "Whether to build examples")
  if(BUILD_EXAMPLES)
    add_subdirectory(example)
  endif()
endif()
