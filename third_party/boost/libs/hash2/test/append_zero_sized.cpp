// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/describe/class.hpp>
#include <boost/array.hpp>
#include <array>

template<class Hash, class Flavor, class T, class R> void test( T const* first, T const* last, R r )
{
    Hash h;
    Flavor f;

    boost::hash2::hash_append_range( h, f, first, last );

    BOOST_TEST_EQ( h.result(), r );
}

struct X
{
};

BOOST_DESCRIBE_STRUCT(X, (), ())

// A hash_append call must always result in a call to Hash::update

int main()
{
    using namespace boost::hash2;

    {
        boost::array<int, 0> v[ 3 ];
    
        test<fnv1a_32, default_flavor>( v + 0, v + 1, 84696351 );
        test<fnv1a_32, default_flavor>( v + 0, v + 2, 292984781 );
        test<fnv1a_32, default_flavor>( v + 0, v + 3, 1253111735 );
    }

    {
        std::array<int, 0> v[ 3 ];
    
        test<fnv1a_32, default_flavor>( v + 0, v + 1, 84696351 );
        test<fnv1a_32, default_flavor>( v + 0, v + 2, 292984781 );
        test<fnv1a_32, default_flavor>( v + 0, v + 3, 1253111735 );
    }

#if defined(BOOST_DESCRIBE_CXX14)

    {
        X v[ 3 ];
    
        test<fnv1a_32, default_flavor>( v + 0, v + 1, 84696351 );
        test<fnv1a_32, default_flavor>( v + 0, v + 2, 292984781 );
        test<fnv1a_32, default_flavor>( v + 0, v + 3, 1253111735 );
    }

#endif

    return boost::report_errors();
}
