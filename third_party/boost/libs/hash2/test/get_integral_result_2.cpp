// Copyright 2017, 2018, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/get_integral_result.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/hash2/md5.hpp>
#include <boost/hash2/sha1.hpp>
#include <boost/hash2/sha2.hpp>
#include <boost/hash2/digest.hpp>
#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <array>
#include <cstdint>

template<class Hash> void test()
{
    using boost::hash2::get_integral_result;

    Hash h;

    {
        Hash h2( h );
        auto t1 = get_integral_result<signed char>( h );
        auto t2 = get_integral_result<unsigned char>( h2 );

        BOOST_TEST_EQ( static_cast<unsigned char>( t1 ), t2 );
    }

    {
        Hash h2( h );
        auto t1 = get_integral_result<signed short>( h );
        auto t2 = get_integral_result<unsigned short>( h2 );

        BOOST_TEST_EQ( static_cast<unsigned short>( t1 ), t2 );
    }

    {
        Hash h2( h );
        auto t1 = get_integral_result<signed int>( h );
        auto t2 = get_integral_result<unsigned int>( h2 );

        BOOST_TEST_EQ( static_cast<unsigned int>( t1 ), t2 );
    }

    {
        Hash h2( h );
        auto t1 = get_integral_result<signed long>( h );
        auto t2 = get_integral_result<unsigned long>( h2 );

        BOOST_TEST_EQ( static_cast<unsigned long>( t1 ), t2 );
    }

    {
        Hash h2( h );
        auto t1 = get_integral_result<signed long long>( h );
        auto t2 = get_integral_result<unsigned long long>( h2 );

        BOOST_TEST_EQ( static_cast<unsigned long long>( t1 ), t2 );
    }
}

template<class R> struct H1: private boost::hash2::fnv1a_64
{
    using result_type = R;

    result_type result()
    {
        return static_cast<R>( boost::hash2::fnv1a_64::result() );
    }
};

template<class R> struct H2: private boost::hash2::md5_128
{
    using result_type = R;

    result_type result()
    {
        boost::hash2::md5_128::result_type r1 = boost::hash2::md5_128::result();
        R r2 = {{}};

        std::memcpy( &r2[0], &r1[0], std::min( r1.size(), r2.size() ) );
        return r2;
    }
};

int main()
{
    test<boost::hash2::fnv1a_32>();
    test<boost::hash2::fnv1a_64>();
    test<boost::hash2::md5_128>();
    test<boost::hash2::sha1_160>();
    test<boost::hash2::sha2_224>();
    test<boost::hash2::sha2_256>();
    test<boost::hash2::sha2_384>();
    test<boost::hash2::sha2_512>();

    test< H1<std::uint8_t> >();
    test< H1<std::uint16_t> >();
    test< H1<std::uint32_t> >();
    test< H1<std::uint64_t> >();

    test< H2<std::array<unsigned char, 8>> >();
    test< H2<boost::array<unsigned char, 8>> >();
    test< H2<boost::hash2::digest<8>> >();

    return boost::report_errors();
}
