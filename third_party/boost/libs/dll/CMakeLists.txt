# Copyright 2020 Peter Dimov
# Copyright Antony Polukhin, 2021-2025
#
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_dll VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

option(BOOST_DLL_USE_STD_FS "Use std::filesystem instead of Boost.Filesystem" OFF)

# Base library

add_library(boost_dll_base INTERFACE)
add_library(Boost::dll_base ALIAS boost_dll_base)
target_include_directories(boost_dll_base INTERFACE include)
target_link_libraries(boost_dll_base
  INTERFACE
    Boost::assert
    Boost::config
    Boost::core
    Boost::predef
    Boost::system
    Boost::throw_exception
    Boost::type_index
    Boost::winapi
    ${CMAKE_DL_LIBS}
)

# Library with std::filesystem

add_library(boost_dll_std_fs INTERFACE)
add_library(Boost::dll_std_fs ALIAS boost_dll_std_fs)
target_include_directories(boost_dll_std_fs INTERFACE include)
target_link_libraries(boost_dll_std_fs INTERFACE Boost::dll_base)
target_compile_definitions(boost_dll_std_fs INTERFACE BOOST_DLL_USE_STD_FS)
target_compile_features(boost_dll_std_fs INTERFACE cxx_std_17)

# Library with Boost.Filesystem

add_library(boost_dll_boost_fs INTERFACE)
add_library(Boost::dll_boost_fs ALIAS boost_dll_boost_fs)
target_include_directories(boost_dll_boost_fs INTERFACE include)
target_link_libraries(boost_dll_boost_fs
  INTERFACE
    Boost::dll_base # Internal dependency
    Boost::filesystem
)

# Detault library

add_library(boost_dll INTERFACE)
add_library(Boost::dll ALIAS boost_dll)
target_include_directories(boost_dll INTERFACE include)
if (BOOST_DLL_USE_STD_FS)
  target_link_libraries(boost_dll INTERFACE Boost::dll_std_fs)
else()
  target_link_libraries(boost_dll INTERFACE Boost::dll_boost_fs)
endif()

if (BOOST_SUPERPROJECT_VERSION AND NOT CMAKE_VERSION VERSION_LESS 3.13)
  boost_install(
    TARGETS
      boost_dll
      boost_dll_base
      boost_dll_std_fs
      boost_dll_boost_fs
    VERSION
      "${BOOST_SUPERPROJECT_VERSION}"
    HEADER_DIRECTORY
      include
  )
endif()

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()
