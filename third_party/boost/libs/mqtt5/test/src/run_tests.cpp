//
// Copyright (c) 2023-2025 Ivica Siladic, Bruno Iljazovic, Korina Simicevic
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#define BOOST_TEST_MODULE boost_mqtt5_tests

#include <boost/test/included/unit_test.hpp>

boost::unit_test::test_suite* init_tests(
    int /*argc*/, char* /*argv*/[]
) {
    return nullptr;
}

int main(int argc, char* argv[]) {
    return boost::unit_test::unit_test_main(&init_tests, argc, argv);
}

/*
* usage: ./mqtt-test [boost test --arg=val]*
* example: ./mqtt-test --log_level=test_suite
*
* all boost test parameters can be found here:
* https://www.boost.org/doc/libs/master/libs/test/doc/html/boost_test/runtime_config/summary.html
*/
