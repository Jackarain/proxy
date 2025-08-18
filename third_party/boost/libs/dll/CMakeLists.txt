# Copyright 2020 Peter Dimov
# Copyright Antony Polukhin, 2021-2025
#
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.5...3.16)

project(boost_dll VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

option(BOOST_DLL_USE_STD_FS "Use std::filesystem instead of Boost.Filesystem" OFF)

add_library(boost_dll_base INTERFACE)
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


add_library(boost_dll_std_fs INTERFACE)
target_link_libraries(boost_dll_std_fs
  INTERFACE
    boost_dll_base
)
target_compile_definitions(boost_dll_std_fs INTERFACE BOOST_DLL_USE_STD_FS)
target_compile_features(boost_dll_std_fs INTERFACE cxx_std_17)

add_library(boost_dll_boost_fs INTERFACE)
target_link_libraries(boost_dll_boost_fs
  INTERFACE
    boost_dll_base
    Boost::filesystem
)


add_library(boost_dll INTERFACE)
if(BOOST_DLL_USE_STD_FS)
  target_link_libraries(boost_dll INTERFACE boost_dll_std_fs)
else()
  target_link_libraries(boost_dll INTERFACE boost_dll_boost_fs)
endif()

add_library(Boost::dll ALIAS boost_dll)

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

  add_subdirectory(test)

endif()

