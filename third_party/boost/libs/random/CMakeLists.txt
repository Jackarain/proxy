# Copyright 2020, 2021 Peter Dimov
# Copyright 2024 Matt Borland
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_random VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

option(BOOST_RANDOM_ENABLE_RANDOM_DEVICE "Build boost::random_device" ON)

if(BOOST_RANDOM_ENABLE_RANDOM_DEVICE)
  add_library(boost_random
    src/random_device.cpp
  )
  set(_populate PUBLIC)
else()
  add_library(boost_random INTERFACE)
  set(_populate INTERFACE)
endif()

add_library(Boost::random ALIAS boost_random)

target_include_directories(boost_random ${_populate} include)

target_link_libraries(boost_random
  ${_populate}
    Boost::assert
    Boost::config
    Boost::core
    Boost::dynamic_bitset
    Boost::integer
    Boost::io
    Boost::static_assert
    Boost::system
    Boost::throw_exception
    Boost::type_traits
    Boost::utility
)

target_compile_features(boost_random ${_populate} cxx_std_11)

target_compile_definitions(boost_random
  ${_populate} BOOST_RANDOM_NO_LIB
  # Source files already define BOOST_RANDOM_SOURCE
  # PRIVATE BOOST_RANDOM_SOURCE
)

if(BOOST_RANDOM_ENABLE_RANDOM_DEVICE)
  if(BUILD_SHARED_LIBS)
    target_compile_definitions(boost_random PUBLIC BOOST_RANDOM_DYN_LINK)
  else()
    target_compile_definitions(boost_random PUBLIC BOOST_RANDOM_STATIC_LINK)
  endif()
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()
