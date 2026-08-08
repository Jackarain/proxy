// Copyright 2024, 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_NO_CXX17_HDR_VARIANT)

BOOST_PRAGMA_MESSAGE( "Test skipped because BOOST_NO_CXX17_HDR_VARIANT is defined" )
int main() {}

#else

#include <boost/core/lightweight_test.hpp>
#include <variant>

template<class Hash, class Flavor, class T, class R> void test( T const* first, T const* last, R r )
{
    Hash h;
    Flavor f;

    boost::hash2::hash_append_range( h, f, first, last );

    BOOST_TEST_EQ( h.result(), r );
}

// A hash_append call must always result in a call to Hash::update

int main()
{
    using namespace boost::hash2;

    std::monostate v[ 3 ];

    test<fnv1a_32, default_flavor>( v + 0, v + 1, 84696351 );
    test<fnv1a_32, default_flavor>( v + 0, v + 2, 292984781 );
    test<fnv1a_32, default_flavor>( v + 0, v + 3, 1253111735 );

    return boost::report_errors();
}

#endif
