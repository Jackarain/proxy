#
# Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
#
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
#

project(boost_mqtt5_tests CXX)

file(GLOB integration_tests "integration/*.cpp")
file(GLOB unit_tests "unit/*.cpp")

add_executable(boost_mqtt5_unittests src/run_tests.cpp ${unit_tests})
add_executable(boost_mqtt5_integrationtests src/run_tests.cpp ${integration_tests})

if(BOOST_MQTT5_MAIN_PROJECT)
  find_package(OpenSSL REQUIRED)
endif()

target_compile_definitions(boost_mqtt5_unittests PRIVATE BOOST_MQTT5_UNIT_TESTS=1)

foreach(BOOST_MQTT5_TEST boost_mqtt5_unittests boost_mqtt5_integrationtests)
  target_include_directories(${BOOST_MQTT5_TEST} PRIVATE include)
  target_compile_definitions(${BOOST_MQTT5_TEST} PRIVATE BOOST_TEST_NO_MAIN=1)

  if(BOOST_MQTT5_MAIN_PROJECT)
    target_compile_definitions(${BOOST_MQTT5_TEST} PRIVATE BOOST_MQTT5_EXTRA_DEPS=1)

    target_link_libraries(
      ${BOOST_MQTT5_TEST} PRIVATE
      Boost::mqtt5
      OpenSSL::SSL
    )
  else()
    target_link_libraries(
      ${BOOST_MQTT5_TEST} PRIVATE
      Boost::mqtt5
      Boost::included_unit_test_framework
    )

    # Follow the Boost convention: don't build test targets by default,
    # and only when explicitly requested by building target tests
    set_target_properties(${BOOST_MQTT5_TEST} PROPERTIES EXCLUDE_FROM_ALL ON)
  endif()
endforeach()

include(CTest)
add_test(NAME boost_mqtt5_unittests COMMAND boost_mqtt5_unittests)

add_dependencies(tests boost_mqtt5_unittests)

if (BOOST_MQTT5_MAIN_PROJECT)
  add_test(NAME boost_mqtt5_integrationtests COMMAND boost_mqtt5_integrationtests)
  if (BOOST_MQTT5_PUBLIC_BROKER_TESTS)
    set_property(TEST boost_mqtt5_integrationtests PROPERTY ENVIRONMENT "BOOST_MQTT5_PUBLIC_BROKER_TESTS=1")
  endif()
endif()
