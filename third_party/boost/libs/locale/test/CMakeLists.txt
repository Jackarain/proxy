# Copyright 2022-2024 Alexander Grund
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

include(BoostTestJamfile)

set(BOOST_TEST_COMPILE_DEFINITIONS "")
set(BOOST_TEST_LINK_LIBRARIES Boost::locale)
set(BOOST_TEST_COMPILE_OPTIONS ${BOOST_LOCALE_WARNING_OPTIONS})
if(MSVC OR
  (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
)
  list(APPEND BOOST_TEST_COMPILE_OPTIONS -Wall -Werror)
endif()

# Library config as determined by configure tests
if(BOOST_LOCALE_ENABLE_ICONV)
  list(APPEND BOOST_TEST_COMPILE_DEFINITIONS BOOST_LOCALE_WITH_ICONV=1)
endif()
if(BOOST_LOCALE_ENABLE_ICU)
  list(APPEND BOOST_TEST_COMPILE_DEFINITIONS BOOST_LOCALE_WITH_ICU=1)
  list(APPEND BOOST_TEST_LINK_LIBRARIES ICU::i18n ICU::uc)
endif()
if(NOT BOOST_LOCALE_ENABLE_STD)
  list(APPEND BOOST_TEST_COMPILE_DEFINITIONS BOOST_LOCALE_NO_STD_BACKEND=1)
endif()
if(NOT BOOST_LOCALE_ENABLE_WINAPI)
  list(APPEND BOOST_TEST_COMPILE_DEFINITIONS BOOST_LOCALE_NO_WINAPI_BACKEND=1)
endif()
if(NOT BOOST_LOCALE_ENABLE_POSIX)
  list(APPEND BOOST_TEST_COMPILE_DEFINITIONS BOOST_LOCALE_NO_POSIX_BACKEND=1)
endif()

boost_test_jamfile(FILE Jamfile.v2)
target_link_libraries(boost_locale-test_util_numeric_convert Boost::charconv)

# Those require to be run in the test directory
foreach(name test_formatting test_message)
  boost_test(SOURCES ${name}.cpp WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endforeach()
