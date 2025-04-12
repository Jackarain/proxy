// Copyright 2017, 2018, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/get_integral_result.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/hash2/hash_append.hpp>
#include <boost/core/lightweight_test.hpp>
#include <set>
#include <limits>
#include <cstdint>

template<class Hash> void test_identity()
{
    using boost::hash2::get_integral_result;

    using R = typename Hash::result_type;

    Hash h;
    Hash h2( h );

    for( int i = 0; i < 1024; ++i )
    {
        R r = h.result();
        R t = get_integral_result<R>( h2 );

        BOOST_TEST_EQ( t, r );
    }
}

template<class T, class Hash> void test_sample()
{
    using boost::hash2::get_integral_result;

    std::set<T> dist;

    for( unsigned i = 0; i < 65536; ++i )
    {
        T t1 = static_cast<T>( i );

        Hash h;
        boost::hash2::hash_append( h, {}, t1 );

        T t2 = get_integral_result<T>( h );

        dist.insert( t2 );
    }

    BOOST_TEST_EQ( dist.size(), 65536u );
}

int main()
{
    test_identity< boost::hash2::fnv1a_32 >();
    test_identity< boost::hash2::fnv1a_64 >();

    test_sample<std::uint32_t, boost::hash2::fnv1a_32>();
    test_sample<std::uint32_t, boost::hash2::fnv1a_64>();
    test_sample<std::uint64_t, boost::hash2::fnv1a_32>();
    test_sample<std::uint64_t, boost::hash2::fnv1a_64>();

    return boost::report_errors();
}
