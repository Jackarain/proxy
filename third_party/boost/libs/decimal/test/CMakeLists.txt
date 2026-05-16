# Copyright 2018, 2019 Peter Dimov
# Copyright 2023 Matt Borland
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostTestJamfile OPTIONAL RESULT_VARIABLE HAVE_BOOST_TEST)

if(HAVE_BOOST_TEST)

    if(BUILD_DECTEST_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/decimal-dectest/CMakeLists.txt")

        add_subdirectory(decimal-dectest)

    else()

        find_package(fmt CONFIG)
        find_package(bson 2.0.2 CONFIG)
        if(fmt_FOUND AND bson_FOUND)
            message(STATUS "Boost.Decimal: Test with fmtlib and bson")
            boost_test_jamfile(FILE Jamfile LINK_LIBRARIES Boost::decimal Boost::core Boost::math Boost::multiprecision Boost::charconv Boost::random fmt::fmt bson::bson  COMPILE_DEFINITIONS BOOST_DECIMAL_TEST_FMT BOOST_DECIMAL_TEST_BSON)
        elseif(fmt_FOUND)
            message(STATUS "Boost.Decimal: Test with fmtlib")
            boost_test_jamfile(FILE Jamfile LINK_LIBRARIES Boost::decimal Boost::core Boost::math Boost::multiprecision Boost::charconv Boost::random fmt::fmt COMPILE_DEFINITIONS BOOST_DECIMAL_TEST_FMT)
        else()
            message(STATUS "Boost.Decimal: Test without fmtlib and bson")
            boost_test_jamfile(FILE Jamfile LINK_LIBRARIES Boost::decimal Boost::core Boost::math Boost::multiprecision Boost::charconv Boost::random)
        endif()

        boost_test(SOURCES check_cmake_version.cpp ARGUMENTS ${PROJECT_VERSION} LINK_LIBRARIES Boost::decimal Boost::core)

    endif()

endif()
