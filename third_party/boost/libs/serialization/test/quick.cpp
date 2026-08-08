// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

struct A
{
    int x, y;

    template<class Ar> void serialize( Ar& ar, unsigned /*version*/ )
    {
        ar & x;
        ar & y;
    }
};

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/core/lightweight_test.hpp>
#include <sstream>

int main()
{
    std::string tmp;

    {
        A a = { 1, 2 };

        std::ostringstream os;
        boost::archive::text_oarchive oa( os );

        oa << a;

        tmp = os.str();
    }

    {
        A a = {};

        std::istringstream is( tmp );
        boost::archive::text_iarchive ia( is );

        ia >> a;

        BOOST_TEST_EQ( a.x, 1 );
        BOOST_TEST_EQ( a.y, 2 );
    }

    return boost::report_errors();
}
