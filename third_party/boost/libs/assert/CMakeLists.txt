# Copyright 2018-2025 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

# We support CMake 3.5, but prefer 3.31 policies and behavior
cmake_minimum_required(VERSION 3.5...3.31)

project(boost_assert VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

add_library(boost_assert INTERFACE)
add_library(Boost::assert ALIAS boost_assert)

target_include_directories(boost_assert INTERFACE include)

target_link_libraries(boost_assert
  INTERFACE
    Boost::config
)

# Add headers and .natvis to project, for better IDE integration

if(NOT CMAKE_VERSION VERSION_LESS 3.19)

  # Using target_sources with PRIVATE or PUBLIC on INTERFACE targets requires 3.19

  file(GLOB_RECURSE headers CONFIGURE_DEPENDS include/*.hpp)
  target_sources(boost_assert PRIVATE ${headers})
  unset(headers)

  if(MSVC)

    # Only Visual Studio needs this, but the generator may also be Ninja
    target_sources(boost_assert PUBLIC extra/boost_assert.natvis)

  endif()

  # Make IDE project folders match directory structure

  get_target_property(sources boost_assert SOURCES)
  source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${sources})
  unset(sources)

endif()

# BUILD_TESTING is the standard CTest variable that enables testing

if(BUILD_TESTING)

  add_subdirectory(test)

endif()
