// Copyright 2017, 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/fnv1a.hpp>
#include <boost/hash2/siphash.hpp>
#include <boost/hash2/xxhash.hpp>
#include <boost/hash2/md5.hpp>
#include <boost/hash2/sha1.hpp>
#include <boost/hash2/sha2.hpp>
#include <boost/hash2/ripemd.hpp>
#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/get_integral_result.hpp>
#include <boost/core/type_name.hpp>
#include <chrono>
#include <typeinfo>
#include <cstddef>
#include <cstdio>
#include <string>
#include <limits>
#include <cmath>

template<class R, class H> void test_( int N )
{
    typedef std::chrono::steady_clock clock_type;

    clock_type::time_point t1 = clock_type::now();

    double r = 0;

    H h;

    for( int i = 0; i < N; ++i )
    {
        using boost::hash2::get_integral_result;
        r += get_integral_result<R>( h );
        r += 0.5;
    }

    clock_type::time_point t2 = clock_type::now();

    long long ms = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();

    r /= N;

    // Standard deviation of Bates distribution
    double stddev = static_cast<double>( std::numeric_limits<R>::max() ) - static_cast<double>( std::numeric_limits<R>::min() ) / std::sqrt( 12.0 * N );

    r /= stddev;

    printf( "%s: r=%e, %lld ms\n", boost::core::type_name<H>().c_str(), r, ms );
}

template<class R> void test( int N )
{
    printf( "Integral result type `%s`:\n\n", boost::core::type_name<R>().c_str() );

    test_<R, boost::hash2::fnv1a_32>( N );
    test_<R, boost::hash2::fnv1a_64>( N );
    test_<R, boost::hash2::xxhash_32>( N );
    test_<R, boost::hash2::xxhash_64>( N );
    test_<R, boost::hash2::siphash_32>( N );
    test_<R, boost::hash2::siphash_64>( N );
    test_<R, boost::hash2::md5_128>( N );
    test_<R, boost::hash2::sha1_160>( N );
    test_<R, boost::hash2::sha2_256>( N );
    test_<R, boost::hash2::sha2_224>( N );
    test_<R, boost::hash2::sha2_512>( N );
    test_<R, boost::hash2::sha2_384>( N );
    test_<R, boost::hash2::sha2_512_224>( N );
    test_<R, boost::hash2::sha2_512_256>( N );
    test_<R, boost::hash2::ripemd_160>( N );
    test_<R, boost::hash2::ripemd_128>( N );

    puts( "" );
}

int main()
{
    int const N = 64 * 1024 * 1024;

    test<signed char>( N );
    test<short>( N );
    test<int>( N );
    test<long long>( N );
}
