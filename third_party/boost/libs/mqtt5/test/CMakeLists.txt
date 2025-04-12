#
# Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
#

project(boost_mqtt5_tests CXX)

file(GLOB integration_tests "integration/*.cpp")
file(GLOB unit_tests "unit/*.cpp")

add_executable(boost_mqtt5-tests src/run_tests.cpp ${integration_tests} ${unit_tests})

target_include_directories(boost_mqtt5-tests PRIVATE include)
target_compile_definitions(boost_mqtt5-tests PRIVATE BOOST_TEST_NO_MAIN=1)

if(BOOST_MQTT5_MAIN_PROJECT)
  find_package(OpenSSL REQUIRED)
  target_compile_definitions(boost_mqtt5-tests PRIVATE BOOST_MQTT5_EXTRA_DEPS=1)

  target_link_libraries(
    boost_mqtt5-tests PRIVATE
    Boost::mqtt5
    OpenSSL::SSL
  )
else()
  target_link_libraries(
    boost_mqtt5-tests PRIVATE
    Boost::mqtt5
    Boost::included_unit_test_framework
  )

  # Follow the Boost convention: don't build test targets by default,
  # and only when explicitly requested by building target tests
  set_target_properties(boost_mqtt5-tests PROPERTIES EXCLUDE_FROM_ALL ON)
  add_dependencies(tests boost_mqtt5-tests)
endif()

include(CTest)
add_test(NAME boost_mqtt5-tests COMMAND boost_mqtt5-tests)

if (BOOST_MQTT5_PUBLIC_BROKER_TESTS)
  set_property(TEST boost_mqtt5-tests PROPERTY ENVIRONMENT "BOOST_MQTT5_PUBLIC_BROKER_TESTS=1")
endif()
