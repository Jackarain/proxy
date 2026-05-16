// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iostream>

using namespace boost::decimal;
using namespace boost::decimal::literals;

int main()
{
    char buffer[64]{};
    const auto print_val {0.000001_df};

    const auto r = boost::decimal::to_chars(
            buffer,
            buffer + sizeof(buffer),
            print_val,
            boost::decimal::chars_format::fixed
    );
    *r.ptr = '\0';

    BOOST_TEST_CSTR_EQ(buffer, "0.000001");

    return boost::report_errors();
}
