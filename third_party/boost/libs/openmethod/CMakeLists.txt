#
# Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
# Copyright (c) 2021 DMitry Arkhipov (grisumbras@gmail.com)
# Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/openmethod
#

#-------------------------------------------------
#
# Project
#
#-------------------------------------------------

cmake_minimum_required(VERSION 3.8...3.20)

set(BOOST_OPENMETHOD_VERSION 1)

if (BOOST_SUPERPROJECT_VERSION)
    set(BOOST_OPENMETHOD_VERSION ${BOOST_SUPERPROJECT_VERSION})
endif ()

project(boost_openmethod VERSION "${BOOST_OPENMETHOD_VERSION}" LANGUAGES CXX)

set(BOOST_OPENMETHOD_IS_ROOT OFF)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(BOOST_OPENMETHOD_IS_ROOT ON)
endif ()
set(__ignore__ ${CMAKE_C_COMPILER})

#-------------------------------------------------
#
# Options
#
#-------------------------------------------------
option(BOOST_OPENMETHOD_BUILD_TESTS "Build boost::openmethod tests even if BUILD_TESTING is OFF" OFF)
option(BOOST_OPENMETHOD_BUILD_EXAMPLES "Build boost::openmethod examples" ${BOOST_OPENMETHOD_IS_ROOT})
option(BOOST_OPENMETHOD_MRDOCS_BUILD "Build the target for MrDocs: see mrdocs.yml" OFF)
option(BOOST_OPENMETHOD_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

# Check if environment variable BOOST_SRC_DIR is set
if (NOT DEFINED BOOST_SRC_DIR AND DEFINED ENV{BOOST_SRC_DIR})
    set(DEFAULT_BOOST_SRC_DIR "$ENV{BOOST_SRC_DIR}")
else ()
    set(DEFAULT_BOOST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
endif ()
set(BOOST_SRC_DIR ${DEFAULT_BOOST_SRC_DIR} CACHE STRING "Boost source dir to use when running CMake from this directory")

#-------------------------------------------------
#
# Boost modules
#
#-------------------------------------------------
# The boost super-project requires one explicit dependency per-line.

set(
    BOOST_OPENMETHOD_DEPENDENCIES
    Boost::assert
    Boost::config
    Boost::core
    Boost::dynamic_bitset
    Boost::mp11
    Boost::preprocessor
)


foreach (BOOST_OPENMETHOD_DEPENDENCY ${BOOST_OPENMETHOD_DEPENDENCIES})
    if (BOOST_OPENMETHOD_DEPENDENCY MATCHES "^[ ]*Boost::([A-Za-z0-9_]+)[ ]*$")
        list(APPEND BOOST_OPENMETHOD_INCLUDE_LIBRARIES ${CMAKE_MATCH_1})
    endif ()
endforeach ()
# Conditional dependencies
if (NOT BOOST_OPENMETHOD_MRDOCS_BUILD)
    if (BUILD_TESTING OR BOOST_OPENMETHOD_BUILD_TESTS)
        set(BOOST_OPENMETHOD_UNIT_TEST_LIBRARIES test)
    endif()
    if (BOOST_OPENMETHOD_BUILD_EXAMPLES)
        set(BOOST_OPENMETHOD_EXAMPLE_LIBRARIES dll)
    endif()
endif()
# Complete dependency list
set(BOOST_INCLUDE_LIBRARIES ${BOOST_OPENMETHOD_INCLUDE_LIBRARIES} ${BOOST_OPENMETHOD_UNIT_TEST_LIBRARIES} ${BOOST_OPENMETHOD_EXAMPLE_LIBRARIES})
set(BOOST_EXCLUDE_LIBRARIES openmethod)

#-------------------------------------------------
#
# Add Boost Subdirectory
#
#-------------------------------------------------
if (BOOST_OPENMETHOD_IS_ROOT)
    set(CMAKE_FOLDER Dependencies)
    # Find absolute BOOST_SRC_DIR
    if (NOT IS_ABSOLUTE ${BOOST_SRC_DIR})
        set(BOOST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${BOOST_SRC_DIR}")
    endif ()

    # Validate BOOST_SRC_DIR
    set(BOOST_SRC_DIR_IS_VALID ON)
    foreach (F "CMakeLists.txt" "Jamroot" "boost-build.jam" "bootstrap.sh" "libs")
        if (NOT EXISTS "${BOOST_SRC_DIR}/${F}")
            message(STATUS "${BOOST_SRC_DIR}/${F} does not exist. Fallback to find_package.")
            set(BOOST_SRC_DIR_IS_VALID OFF)
            break()
        endif ()
    endforeach ()

    # Create Boost interface targets
    if (BOOST_SRC_DIR_IS_VALID)
        # From BOOST_SRC_DIR
        if (BUILD_SHARED_LIBS)
            set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
        endif()
        set(BOOST_EXCLUDE_LIBRARIES ${PROJECT_NAME})
        set(PREV_BUILD_TESTING ${BUILD_TESTING})
        set(BUILD_TESTING OFF CACHE BOOL "Build the tests." FORCE)
        add_subdirectory(${BOOST_SRC_DIR} Dependencies/boost EXCLUDE_FROM_ALL)
        set(BUILD_TESTING ${PREV_BUILD_TESTING} CACHE BOOL "Build the tests." FORCE)
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${BOOST_SRC_DIR}/tools/cmake/include")
    else ()
        # From Boost Package
        find_package(Boost REQUIRED COMPONENTS container)
        foreach (BOOST_INCLUDE_LIBRARY ${BOOST_INCLUDE_LIBRARIES})
            if (NOT TARGET Boost::${BOOST_INCLUDE_LIBRARY})
                add_library(Boost::${BOOST_INCLUDE_LIBRARY} ALIAS Boost::headers)
            endif ()
        endforeach ()
    endif ()
    unset(CMAKE_FOLDER)
endif ()

#-------------------------------------------------
#
# Library
#
#-------------------------------------------------
file(GLOB_RECURSE BOOST_OPENMETHOD_HEADERS CONFIGURE_DEPENDS include/boost/*.hpp)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include/boost PREFIX "" FILES ${BOOST_OPENMETHOD_HEADERS})

function(boost_openmethod_setup_properties target)
    target_link_libraries(${target} INTERFACE ${BOOST_OPENMETHOD_DEPENDENCIES})
endfunction()

add_library(boost_openmethod INTERFACE)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
  target_sources(boost_openmethod PRIVATE ${BOOST_OPENMETHOD_HEADERS})
endif()

add_library(Boost::openmethod ALIAS boost_openmethod)
target_include_directories(boost_openmethod INTERFACE "${PROJECT_SOURCE_DIR}/include")
boost_openmethod_setup_properties(boost_openmethod)
target_compile_features(boost_openmethod INTERFACE cxx_std_17)

if (BOOST_OPENMETHOD_MRDOCS_BUILD)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mrdocs.cpp" "#include <boost/openmethod.hpp>\n")
    foreach (HEADER ${BOOST_OPENMETHOD_HEADERS})
        string(REPLACE "${PROJECT_SOURCE_DIR}/include/" "" HEADER "${HEADER}")
        file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/mrdocs.cpp" "#include <${HEADER}>\n")
    endforeach()
    add_library(boost_openmethod_mrdocs "${CMAKE_CURRENT_BINARY_DIR}/mrdocs.cpp")
    target_include_directories(boost_openmethod_mrdocs PUBLIC "${PROJECT_SOURCE_DIR}/include")
    boost_openmethod_setup_properties(boost_openmethod_mrdocs)
    target_link_libraries(boost_openmethod_mrdocs PUBLIC Boost::openmethod)
    target_compile_definitions(boost_openmethod_mrdocs PUBLIC BOOST_OPENMETHOD_MRDOCS)
    return()
endif()

#-------------------------------------------------
#
# Tests
#
#-------------------------------------------------
if (BUILD_TESTING OR BOOST_OPENMETHOD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
    if (BOOST_OPENMETHOD_IS_ROOT)
        add_custom_target(all_with_tests ALL DEPENDS tests)
    endif()
endif ()

#-------------------------------------------------
#
# Examples
#
#-------------------------------------------------
if (BOOST_OPENMETHOD_BUILD_EXAMPLES)
    enable_testing()
    add_subdirectory(doc/modules/ROOT/examples)
endif ()
