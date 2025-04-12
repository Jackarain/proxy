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

template<class T, class Hash> std::size_t test_sample()
{
    using boost::hash2::get_integral_result;

    std::set<T> dist;

    for( unsigned i = 0; i <= std::numeric_limits<T>::max(); ++i )
    {
        T t1 = static_cast<T>( i );

        Hash h;
        boost::hash2::hash_append( h, {}, t1 );

        T t2 = get_integral_result<T>( h );

        dist.insert( t2 );
    }

    return dist.size();
}

using boost::hash2::fnv1a_32;
using boost::hash2::fnv1a_64;

struct fnv1a_16: private fnv1a_32
{
    using result_type = std::uint16_t;

    using fnv1a_32::update;

    result_type result()
    {
        std::uint32_t r = fnv1a_32::result();
        return static_cast<std::uint16_t>( r ^ ( r >> 16 ) );
    }
};

struct fnv1a_8: private fnv1a_16
{
    using result_type = std::uint8_t;

    using fnv1a_16::update;

    result_type result()
    {
        std::uint16_t r = fnv1a_16::result();
        return static_cast<std::uint8_t>( r ^ ( r >> 8 ) );
    }
};

int main()
{
    test_identity<fnv1a_8>();

    // EV(256 samples in 256 buckets) = 162 (256 * (1-e^-1)), stddev ~= 7.7 (sqrt(256) * 0.482)

    BOOST_TEST_EQ( (test_sample<std::uint8_t, fnv1a_8>()), 168u ); // get_integral_result is identity

    BOOST_TEST_GE( (test_sample<std::uint8_t, fnv1a_16>()), 154u );
    BOOST_TEST_GE( (test_sample<std::uint8_t, fnv1a_32>()), 154u );
    BOOST_TEST_GE( (test_sample<std::uint8_t, fnv1a_64>()), 154u );

    test_identity<fnv1a_16>();

    // EV(65536 samples in 65536 buckets) = 41427 (65536 * (1-e^-1)), stddev ~= 123.4 (sqrt(65536) * 0.482)

    BOOST_TEST_EQ( (test_sample<std::uint16_t, fnv1a_16>()), 40718u ); // get_integral_result is identity

    BOOST_TEST_GE( (test_sample<std::uint16_t, fnv1a_32>()), 41303u );
    BOOST_TEST_GE( (test_sample<std::uint16_t, fnv1a_64>()), 41303u );

    return boost::report_errors();
}
