// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/date_time.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iostream>

int main()
{
    using namespace boost::gregorian;
    using namespace boost::posix_time; 

    date d( 2026, Apr, 27 );
    ptime t( d, hours(20) + minutes(26) + seconds(11) );

    std::cout << t << std::endl;

    BOOST_TEST_EQ( t.date().day(), 27 );
    BOOST_TEST_EQ( t.date().month(), Apr );
    BOOST_TEST_EQ( t.date().year(), 2026 );

    BOOST_TEST_EQ( t.time_of_day().hours(), 20 );
    BOOST_TEST_EQ( t.time_of_day().minutes(), 26 );
    BOOST_TEST_EQ( t.time_of_day().seconds(), 11 );

    return boost::report_errors();
}
