// Copyright 2017, 2024, 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_NO_CXX17_HDR_OPTIONAL)

BOOST_PRAGMA_MESSAGE( "Test skipped because BOOST_NO_CXX17_HDR_OPTIONAL is defined" )
int main() {}

#else

#include <boost/core/lightweight_test.hpp>
#include <optional>

template<class Hash, class Flavor, class T, class R> void test( std::optional<T> const& opt, R r )
{
    Flavor f;

    {
        Hash h;

        hash_append( h, f, opt );

        BOOST_TEST_EQ( h.result(), r );
    }

    if( opt )
    {
        Hash h;

        hash_append( h, f, true );
        hash_append( h, f, *opt );

        BOOST_TEST_EQ( h.result(), r );
    }
    else
    {
        Hash h;

        hash_append( h, f, false );

        BOOST_TEST_EQ( h.result(), r );
    }
}

int main()
{
    using namespace boost::hash2;

    {
        std::optional<char> opt;

        test<fnv1a_32, default_flavor>( opt, 84696351 );
        test<fnv1a_32, little_endian_flavor>( opt, 84696351 );
        test<fnv1a_32, big_endian_flavor>( opt, 84696351 );
    }

    {
        std::optional<char> opt( 'x' );

        test<fnv1a_32, default_flavor>( opt, 1400160540 );
        test<fnv1a_32, little_endian_flavor>( opt, 1400160540 );
        test<fnv1a_32, big_endian_flavor>( opt, 1400160540 );
    }

    {
        std::optional<int> opt;

        test<fnv1a_32, default_flavor>( opt, 84696351 );
        test<fnv1a_32, little_endian_flavor>( opt, 84696351 );
        test<fnv1a_32, big_endian_flavor>( opt, 84696351 );
    }

    {
        std::optional<int> opt( 17041 );

        test<fnv1a_32, little_endian_flavor>( opt, 1142529047 );
        test<fnv1a_32, big_endian_flavor>( opt, 667993889 );
    }

    {
        std::optional<double> opt;

        test<fnv1a_32, default_flavor>( opt, 84696351 );
        test<fnv1a_32, little_endian_flavor>( opt, 84696351 );
        test<fnv1a_32, big_endian_flavor>( opt, 84696351 );
    }

    {
        std::optional<double> opt( 3.14 );

        test<fnv1a_32, little_endian_flavor>( opt, 4145498479 );
        test<fnv1a_32, big_endian_flavor>( opt, 2672404081 );
    }

    return boost::report_errors();
}

#endif
