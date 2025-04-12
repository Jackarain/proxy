#
# Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
#

cmake_minimum_required(VERSION 3.8...3.20)

project(boost-mqtt5-examples LANGUAGES CXX)

function(add_example name)
  add_executable("${name}" ${ARGN})
  target_link_libraries("${name}" PRIVATE Boost::mqtt5)
  string(FIND "${example_name}" "tls" found_tls)
  if(found_tls GREATER -1)
    target_link_libraries("${name}" PRIVATE OpenSSL::SSL)
  endif()
endfunction()

file(GLOB examples "*.cpp")

foreach(file_path ${examples})
  get_filename_component(example_name "${file_path}" NAME_WE)
  add_example("${example_name}" "${file_path}")
endforeach()

find_package(OpenSSL REQUIRED)
