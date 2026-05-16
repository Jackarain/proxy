///////////////////////////////////////////////////////////////////////////////
//  Copyright (C) 2025 James E. King III. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MAIN  "Boost::Rational multiprecision unit tests"

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( issue_27_test )
{
    using namespace boost::multiprecision;

    // Verify that the check to ensure that the denominator is positive works
    // with an arbitrary precision rational.
    cpp_rational uut(1, -2);
    BOOST_CHECK_EQUAL(numerator(uut), -1);
    BOOST_CHECK_EQUAL(denominator(uut), 2);
}
