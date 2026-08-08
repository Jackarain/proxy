# Copyright (c) 2019 Mike Dev
# Copyright (c) 2025-2026 Fedor Osetrov
#
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...4.20)
project(boost_conversion VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

if (BOOST_USE_MODULES)
  add_library(boost_conversion)
  target_sources(boost_conversion PUBLIC
    FILE_SET modules_public
    TYPE CXX_MODULES
    BASE_DIRS modules
    FILES ${CMAKE_CURRENT_LIST_DIR}/modules/boost_conversion.cppm
  )

  target_compile_definitions(boost_conversion PUBLIC BOOST_USE_MODULES)
  get_property(__standard TARGET boost_conversion PROPERTY CXX_STANDARD)
  if (__standard IN_LIST CMAKE_CXX_COMPILER_IMPORT_STD AND CMAKE_CXX_MODULE_STD)
    target_compile_features(boost_conversion PUBLIC cxx_std_23)
    target_compile_definitions(boost_conversion PRIVATE BOOST_CONVERSION_USE_STD_MODULE)
    message(STATUS "Using `import std;`")
  else()
    target_compile_features(boost_conversion PUBLIC cxx_std_20)
    message(STATUS "`import std;` is not available")
  endif()
  unset(__standard)
  set(__scope PUBLIC)
else()
  add_library(boost_conversion INTERFACE)
  set(__scope INTERFACE)
endif()

target_include_directories(boost_conversion ${__scope} include)
add_library(Boost::conversion ALIAS boost_conversion)

target_link_libraries(boost_conversion
    ${__scope}
        Boost::assert
        Boost::config
        Boost::throw_exception
        Boost::smart_ptr
)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()
