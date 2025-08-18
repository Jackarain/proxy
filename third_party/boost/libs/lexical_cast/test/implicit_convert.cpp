//  Copyright Antony Polukhin, 2023-2025.
//
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt).

#include <boost/lexical_cast.hpp>

#include <boost/core/lightweight_test.hpp>

struct oops {
    operator int () const {
        return 41;
    }
};

inline std::ostream& operator<<(std::ostream& os, const oops&) {
    return os << 42;
}

struct oops2 {
    operator double () const {
        return 1.12;
    }
};

inline std::ostream& operator<<(std::ostream& os, const oops2&) {
    return os << 2;
}

struct Value {
    explicit Value(double x) : x(x) {}

    operator double() const {
        return x;
    }

    double x;
};

int main() {
    BOOST_TEST_EQ(boost::lexical_cast<int>(oops{}), 42);
    BOOST_TEST_EQ(boost::lexical_cast<std::string>(oops{}), "42");

    BOOST_TEST_EQ(boost::lexical_cast<double>(oops2{}), 2);
    BOOST_TEST_EQ(boost::lexical_cast<std::string>(oops2{}), "2");

    Value longer_that_1_digit(128);
    BOOST_TEST_EQ(boost::lexical_cast<std::string>(longer_that_1_digit), "128");

    // iostreams have default precision 6, see [basic.ios.cons]
    const double precision_more_than_6_digits = 1.23456789123;
    // At the moment we do not set the precision for types that use conversion
    // operators for ostreaming, so the below test pass. We do not set precision
    // for user provided `operator<<`, types with conversion operators follow
    // the same design decision for now.
    BOOST_TEST_NE(
        boost::lexical_cast<std::string>(Value(precision_more_than_6_digits)),
        boost::lexical_cast<std::string>(precision_more_than_6_digits)
    );

    return boost::report_errors();
}
