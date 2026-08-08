# Copyright 2026 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.8...4.2)

project(quickbook LANGUAGES CXX)

add_executable(quickbook)

file(GLOB_RECURSE sources CONFIGURE_DEPENDS src/*.cpp)
target_sources(quickbook PRIVATE ${sources})

target_include_directories(quickbook PRIVATE src)

find_package(Boost COMPONENTS filesystem program_options REQUIRED)
target_link_libraries(quickbook PRIVATE Boost::filesystem Boost::program_options)

install(TARGETS quickbook)
