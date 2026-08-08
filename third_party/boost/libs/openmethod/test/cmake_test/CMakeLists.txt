#
# Copyright (c) 2022 alandefreitas (alandefreitas@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#

cmake_minimum_required(VERSION 3.8...3.20)

project(cmake_test LANGUAGES CXX)
set(__ignore__ ${CMAKE_C_COMPILER})
set(__ignore__ ${CMAKE_C_FLAGS})

if(BOOST_CI_INSTALL_TEST)
# Header-only libraries:
    # B2 does not (yet) create/install CMake targets for them, so users are supposed to use the generic Boost::headers target
    if(BOOST_CI_INSTALLED_BY STREQUAL "B2")
        find_package(Boost REQUIRED)
        add_library(_boost_openmethod INTERFACE)
        target_link_libraries(_boost_openmethod INTERFACE Boost::headers)
        target_compile_features(_boost_openmethod INTERFACE cxx_std_17)
        add_library(Boost::openmethod ALIAS _boost_openmethod)
    else()
      # Boost as a package (https://github.com/boostorg/cmake#using-boost-after-building-and-installing-it-with-cmake)
      find_package(Boost CONFIG REQUIRED COMPONENTS openmethod)
    endif()
elseif(BOOST_CI_INSTALL_MODULE_TEST)
    # Boost.OpenMethod as a package
    find_package(boost_openmethod CONFIG REQUIRED)
elseif(BOOST_CI_BOOST_SUBDIR_TEST)
    # Boost as a subdirectory (https://github.com/boostorg/cmake#using-boost-after-building-and-installing-it-with-cmake)
    if (BUILD_SHARED_LIBS)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    endif()
    set(BOOST_OPENMETHOD_BUILD_TESTS OFF CACHE BOOL "Build the tests." FORCE)
    set(PREV_BUILD_TESTING ${BUILD_TESTING})
    set(BUILD_TESTING OFF CACHE BOOL "Build the tests." FORCE)
    set(BOOST_INCLUDE_LIBRARIES openmethod)
    add_subdirectory(../../../.. boost)
    set(BUILD_TESTING ${PREV_BUILD_TESTING} CACHE BOOL "Build the tests." FORCE)
else()
    # Boost.OpenMethod as a subdirectory (https://github.com/boostorg/cmake#using-an-individual-boost-library-with-add_subdirectory)
    if (BUILD_SHARED_LIBS)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    endif()
    set(BOOST_OPENMETHOD_BUILD_TESTS OFF CACHE BOOL "Build the tests." FORCE)
    set(BOOST_OPENMETHOD_BUILD_EXAMPLES OFF CACHE BOOL "Build the examples." FORCE)
    set(PREV_BUILD_TESTING ${BUILD_TESTING})
    set(BUILD_TESTING OFF CACHE BOOL "Build the tests." FORCE)
    file(GLOB subdirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/../../.. ${CMAKE_CURRENT_SOURCE_DIR}/../../../*)
    foreach(subdir ${subdirs})
        # This is testing the case when the super-project is not available
        # and users want to add libraries as subdirectories.
        # According to the convention above, users should scan all dependencies
        # with boostdep and include each of them with add_subdirectory.
        # For developers, hard-coding all dependencies is impractical
        # and error-prone because the list of transitive dependencies
        # is unstable over time and across Boost versions and branches.
        # For this reason, we list all directories in ../.. and add them
        # as subdirectories. Only directories previously cloned
        # with `depinst.py` in CI will be added. This will unfortunately
        # add test dependencies we don't need.
        if (
            IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../${subdir} AND
            EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../../${subdir}/CMakeLists.txt
        )
            add_subdirectory(../../../${subdir} boostorg/${subdir})
        endif()
    endforeach()
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../../numeric AND IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../numeric)
        # Iterate potential transitive dependencies in libs/numeric
        file(GLOB subdirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/../../../numeric ${CMAKE_CURRENT_SOURCE_DIR}/../../../numeric/*)
        foreach(subdir ${subdirs})
            if (
                IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../../numeric/${subdir} AND
                EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../../numeric/${subdir}/CMakeLists.txt
            )
                add_subdirectory(../../../numeric/${subdir} boostorg/numeric/${subdir})
            endif()
        endforeach()
    endif()
    set(BUILD_TESTING ${PREV_BUILD_TESTING} CACHE BOOL "Build the tests." FORCE)
endif()

add_executable(main main.cpp)
target_link_libraries(main Boost::openmethod)

if (BUILD_TESTING)
    enable_testing()
    add_test(NAME main COMMAND main)
    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>)
endif()
