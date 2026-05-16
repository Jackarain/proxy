# Copyright 2023 - 2025 Matt Borland
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.10...3.20)

project(boost_decimal VERSION 1.91.0 LANGUAGES CXX)

add_library(boost_decimal INTERFACE)

add_library(Boost::decimal ALIAS boost_decimal)

if(NOT BOOST_SUPERPROJECT_VERSION)

    target_include_directories(boost_decimal INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

else()

    target_include_directories(boost_decimal INTERFACE include)

endif()

target_compile_features(boost_decimal INTERFACE cxx_std_14)

if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt")

    add_subdirectory(test)

endif()

# Installation and config-file package generation
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install headers
    install(DIRECTORY "include/"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    # Install targets
    install(TARGETS boost_decimal
            EXPORT boost_decimalTargets
            INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    # Install export targets
    install(EXPORT boost_decimalTargets
            FILE boost_decimalTargets.cmake
            NAMESPACE Boost::
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/boost_decimal"
    )

    # Generate config files
    configure_package_config_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/boost_decimalConfig.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/boost_decimalConfig.cmake"
            INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/boost_decimal"
    )

    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/boost_decimalConfigVersion.cmake"
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion
    )

    # Install config files
    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/boost_decimalConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/boost_decimalConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/boost_decimal"
    )

    configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/boost_decimal.pc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/boost_decimal.pc"
            @ONLY
    )

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/boost_decimal.pc"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    )

endif()
