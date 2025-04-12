// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>

template<class Hash, class Flavor, class T> void test( T const& v, unsigned char const (&ref)[ sizeof(T) ] )
{
    {
        typename Hash::result_type r1, r2;

        {
            Hash h;
            Flavor f;

            hash_append( h, f, v );

            r1 = h.result();
        }

        {
            Hash h;

            h.update( ref, sizeof(ref) );

            r2 = h.result();
        }

        BOOST_TEST_EQ( r1, r2 );
    }
}

struct X;

int main()
{
    using namespace boost::hash2;

    test<fnv1a_32, default_flavor>( nullptr, {} );
    test<fnv1a_32, default_flavor>( (void*)nullptr, {} );
    test<fnv1a_32, default_flavor>( (void const*)nullptr, {} );
    test<fnv1a_32, default_flavor>( (void(*)())nullptr, {} );
    test<fnv1a_32, default_flavor>( (X*)nullptr, {} );

    return boost::report_errors();
}
