# Copyright 2018 Mike Dev
# Copyright 2018, 2026 Peter Dimov
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.8...4.2)

project(boostdep LANGUAGES CXX)

add_executable(boostdep src/boostdep.cpp)

find_package(Boost COMPONENTS filesystem REQUIRED)
target_link_libraries(boostdep PRIVATE Boost::filesystem)

install(TARGETS boostdep)
