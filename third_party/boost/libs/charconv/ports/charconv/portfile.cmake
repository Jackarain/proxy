# Copyright 2023 Matt Borland
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt
#
# See: https://devblogs.microsoft.com/cppblog/registries-bring-your-own-libraries-to-vcpkg/

vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO cppalliance/charconv
        REF v1.1.1
        SHA512 414f66ef45347d030c43309f9063e385bc74cc31d4b415dde2231e4b273673b930f5f5eb40db401a724a595589a09fad6d039257f5b7fd8e305f935b18ad7e46
        HEAD_REF master
)

vcpkg_replace_string("${SOURCE_PATH}/build/Jamfile"
        "import ../../config/checks/config"
        "import ../config/checks/config"
)

file(COPY "${CURRENT_INSTALLED_DIR}/share/boost-config/checks" DESTINATION "${SOURCE_PATH}/config")
include(${CURRENT_HOST_INSTALLED_DIR}/share/boost-build/boost-modular-build.cmake)
boost_modular_build(
        SOURCE_PATH ${SOURCE_PATH}
        BOOST_CMAKE_FRAGMENT "${CMAKE_CURRENT_LIST_DIR}/b2-options.cmake"
)
include(${CURRENT_INSTALLED_DIR}/share/boost-vcpkg-helpers/boost-modular-headers.cmake)
boost_modular_headers(SOURCE_PATH ${SOURCE_PATH})
