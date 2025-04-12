// Copyright 2017, 2023, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/has_constant_size.hpp>
#include <boost/hash2/digest.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/array.hpp>
#include <array>
#include <string>
#include <vector>
#include <set>

template<class T> void test( bool exp )
{
    using boost::hash2::has_constant_size;

    if( exp )
    {
        BOOST_TEST_TRAIT_TRUE((has_constant_size<T>));
        BOOST_TEST_TRAIT_TRUE((has_constant_size<T const>));
    }
    else
    {
        BOOST_TEST_TRAIT_FALSE((has_constant_size<T>));
        BOOST_TEST_TRAIT_FALSE((has_constant_size<T const>));
    }
}

struct X
{
};

int main()
{
    test< void >( false );
    test< X >( false );

    test< std::vector<int> >( false );
    test< std::vector<X> >( false );
    test< std::string >( false );
    test< std::set<int> >( false );

    test< std::array<int, 3> >( true );
    test< std::array<int, 0> >( true );

    test< std::array<X, 3> >( true );
    test< std::array<X, 0> >( true );

    test< boost::array<int, 5> >( true );
    test< boost::array<int, 0> >( true );

    test< boost::array<X, 7> >( true );
    test< boost::array<X, 0> >( true );

    test< std::array<int, 9> >( true );
    test< std::array<int, 0> >( true );

    using boost::hash2::digest;

    test< digest<1> >( true );
    test< digest<16> >( true );
    test< digest<20> >( true );

    return boost::report_errors();
}
