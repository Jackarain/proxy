// Copyright 2017-2019, 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define _CRT_SECURE_NO_WARNINGS

#include <boost/hash2/fnv1a.hpp>
#include <boost/hash2/siphash.hpp>
#include <boost/hash2/xxhash.hpp>
#include <boost/hash2/md5.hpp>
#include <boost/hash2/sha1.hpp>
#include <boost/hash2/sha2.hpp>
#include <boost/hash2/ripemd.hpp>
#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/get_integral_result.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <boost/core/type_name.hpp>
#include <cstdint>
#include <random>
#include <chrono>
#include <typeinfo>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>
#include <type_traits>

namespace detail
{

template<class Hash, class T> inline void compute_hash_value_impl( Hash& h, T const& v, std::false_type )
{
    boost::hash2::hash_append( h, {}, v );
}

template<class Hash, class T> inline void compute_hash_value_impl( Hash& h, T const& v, std::true_type )
{
    boost::hash2::hash_append_range( h, {}, v.data(), v.data() + v.size() );
}

template<class Hash, class T> inline std::size_t compute_hash_value( Hash& h, T const& v )
{
    detail::compute_hash_value_impl( h, v, boost::container_hash::is_contiguous_range<T>() );
    return boost::hash2::get_integral_result<std::size_t>( h );
}

} // namespace detail

template<class T, class H> class hash_without_seed
{
public:

    using is_avalanching = std::true_type;

    std::size_t operator()( T const& v ) const
    {
        H h;
        return detail::compute_hash_value( h, v );
    }
};

template<class T, class H> class hash_with_uint64_seed
{
private:

    std::uint64_t seed_;

public:

    using is_avalanching = std::true_type;

    explicit hash_with_uint64_seed( std::uint64_t seed ): seed_( seed )
    {
    }

    std::size_t operator()( T const& v ) const
    {
        H h( seed_ );
        return detail::compute_hash_value( h, v );
    }
};

template<class T, class H> class hash_with_byte_seed
{
private:

    H h_;

public:

    using is_avalanching = std::true_type;

    hash_with_byte_seed( unsigned char const* seed, std::size_t n ): h_( seed, n )
    {
    }

    std::size_t operator()( T const& v ) const
    {
        H h( h_ );
        return detail::compute_hash_value( h, v );
    }
};

template<class V, class S> BOOST_NOINLINE void test4( int N, V const& v, char const * hash, S s )
{
    typedef std::chrono::steady_clock clock_type;

    clock_type::time_point t1 = clock_type::now();

    for( int i = 0; i < N; ++i )
    {
        s.insert( v[ i * 16 ] );
    }

    clock_type::time_point t2 = clock_type::now();

    std::size_t q = 0;

    for( int i = 0; i < 16 * N; ++i )
    {
        q += s.count( v[ i ] );
    }

    clock_type::time_point t3 = clock_type::now();

    long long ms1 = std::chrono::duration_cast<std::chrono::milliseconds>( t2 - t1 ).count();
    long long ms2 = std::chrono::duration_cast<std::chrono::milliseconds>( t3 - t2 ).count();

    std::size_t n = s.bucket_count();

    std::printf( "%s: n=%zu, q=%zu, %lld + %lld = %lld ms\n", hash, n, q, ms1, ms2, ms1 + ms2 );
}

template<class K, class H, class V> void test2( int N, V const& v )
{
    {
        std::string name = boost::core::type_name<H>() + " without seed";

        using hash = hash_without_seed<K, H>;
        boost::unordered_flat_set<K, hash> s( 0, hash() );

        test4( N, v, name.c_str(), s );
    }

    {
        constexpr std::uint64_t seed = 0x0102030405060708ull;

        std::string name = boost::core::type_name<H>() + " with uint64 seed";

        using hash = hash_with_uint64_seed<K, H>;
        boost::unordered_flat_set<K, hash> s( 0, hash( seed ) );

        test4( N, v, name.c_str(), s );
    }

    {
        static constexpr unsigned char seed[ 16 ] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10 };

        std::string name = boost::core::type_name<H>() + " with byte seed";

        using hash = hash_with_byte_seed<K, H>;
        boost::unordered_flat_set<K, hash> s( 0, hash( seed, sizeof(seed) ) );

        test4( N, v, name.c_str(), s );
    }

    std::puts( "" );
}

int main()
{
    int const N = 1048576;

    std::vector<std::string> v;

    {
        v.reserve( N * 16 );

        std::mt19937_64 rnd;

        for( int i = 0; i < 16 * N; ++i )
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

    typedef std::string K;

    {
        boost::unordered_flat_set<K> s;
        test4( N, v, "default boost::hash without seed", s );
        std::puts( "" );
    }

    test2<K, boost::hash2::fnv1a_32>( N, v );
    test2<K, boost::hash2::fnv1a_64>( N, v );
    test2<K, boost::hash2::xxhash_32>( N, v );
    test2<K, boost::hash2::xxhash_64>( N, v );
    test2<K, boost::hash2::siphash_32>( N, v );
    test2<K, boost::hash2::siphash_64>( N, v );
    test2<K, boost::hash2::md5_128>( N, v );
}
