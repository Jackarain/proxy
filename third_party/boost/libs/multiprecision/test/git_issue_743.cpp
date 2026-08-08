///////////////////////////////////////////////////////////////////////////////
//  Copyright 2026 Matt Borland. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See: https://github.com/boostorg/multiprecision/issues/743

#include <boost/multiprecision/float128.hpp>
#include "test.hpp"

int main()
{
    using real = boost::multiprecision::float128;

    constexpr real value {5};
    BOOST_CHECK(value.backend().value() == value.backend().data());

    real mutable_value {42};
    BOOST_CHECK(mutable_value.backend().value() == mutable_value.backend().data());

    return boost::report_errors();
}
