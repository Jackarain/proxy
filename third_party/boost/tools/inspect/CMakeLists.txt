# Copyright 2026 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.8...4.2)

project(inspect LANGUAGES CXX)

add_executable(inspect

    src/apple_macro_check.cpp
    src/ascii_check.cpp
    src/assert_macro_check.cpp
    src/copyright_check.cpp
    src/crlf_check.cpp
    src/deprecated_macro_check.cpp
    src/end_check.cpp
    src/inspect.cpp
    src/license_check.cpp
    src/link_check.cpp
    src/minmax_check.cpp
    src/path_name_check.cpp
    src/tab_check.cpp
    src/unnamed_namespace_check.cpp
)

target_include_directories(inspect PRIVATE src)

find_package(Boost COMPONENTS filesystem regex REQUIRED)
target_link_libraries(inspect PRIVATE Boost::filesystem Boost::regex)

install(TARGETS inspect)
