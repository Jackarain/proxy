// Copyright 2017-2020 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _CRT_SECURE_NO_WARNINGS

#include <boost/hash2/fnv1a.hpp>
#include <boost/hash2/siphash.hpp>
#include <boost/hash2/xxhash.hpp>
#include <boost/hash2/md5.hpp>
#include <boost/hash2/sha1.hpp>
#include <boost/hash2/ripemd.hpp>
#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/get_integral_result.hpp>
#include <boost/core/type_name.hpp>
#include <cstdint>
#include <chrono>
#include <random>
#include <typeinfo>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

template<class T, class H> class hasher
{
private:

    H h_;

private:

    template<class = void> static void hash_append_impl( H& h, T const& v, std::false_type )
    {
        boost::hash2::hash_append( h, {}, v );
    }

    template<class = void> static void hash_append_impl( H& h, T const& v, std::true_type )
    {
        boost::hash2::hash_append_range( h, {}, v.data(), v.data() + v.size() );
    }

public:

    hasher(): h_()
    {
    }

    explicit hasher( std::uint64_t seed ): h_( seed )
    {
    }

    hasher( unsigned char const* seed, std::size_t n ): h_( seed, n )
    {
    }

    std::size_t operator()( T const& v ) const
    {
        H h( h_ );

        hash_append_impl( h, v, boost::container_hash::is_contiguous_range<T>() );

        return boost::hash2::get_integral_result<std::size_t>( h );
    }
};

template<class H, class V> void test3( int N, V const& v, std::size_t seed )
{
    typedef std::chrono::steady_clock clock_type;

    clock_type::time_point t1 = clock_type::now();

    std::size_t q = 0;

    hasher<std::string, H> const h( seed );

    for( int i = 0; i < N; ++i )
    {
        q += h( v[i] );
    }

    clock_type::time_point t2 = clock_type::now();

    long long ms1 = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();

    std::string hash = boost::core::type_name<H>();

    std::printf( "%s: q=%zu, %lld ms\n", hash.c_str(), q, ms1 );
}

template<class H, class V> void test2( int N, V const& v )
{
    test3<H>( N, v, 0x9e3779b9 );
}

int main()
{
    int const N = 16 * 1048576;

    std::vector<std::string> v;

    {
        v.reserve( N );

        std::mt19937_64 rnd;

        for( int i = 0; i < N; ++i )
        {
            char buffer[ 64 ];

            unsigned long long k = rnd();

            if( k & 1 )
            {
                std::snprintf( buffer, sizeof( buffer ), "prefix_%llu_suffix", k );
            }
            else
            {
                std::snprintf( buffer, sizeof( buffer ), "{%u}", static_cast<unsigned>( k ) );
            }

            v.push_back( buffer );
        }
    }

    using namespace boost::hash2;

    test2<fnv1a_32>( N, v );
    test2<fnv1a_64>( N, v );
    test2<xxhash_32>( N, v );
    test2<xxhash_64>( N, v );
    test2<siphash_32>( N, v );
    test2<siphash_64>( N, v );

    std::puts( "" );
}
