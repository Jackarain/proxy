// Copyright 2017, 2018, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/get_integral_result.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/hash2/md5.hpp>
#include <boost/hash2/digest.hpp>
#include <boost/array.hpp>
#include <array>
#include <cstring>

template<class Hash> void test()
{
    using boost::hash2::get_integral_result;

    Hash h;

    get_integral_result<signed char>( h );
    get_integral_result<unsigned char>( h );
    get_integral_result<short>( h );
    get_integral_result<unsigned short>( h );
    get_integral_result<int>( h );
    get_integral_result<unsigned int>( h );
    get_integral_result<long>( h );
    get_integral_result<unsigned long>( h );
    get_integral_result<long long>( h );
    get_integral_result<unsigned long long>( h );

    get_integral_result<char>( h );
    get_integral_result<char16_t>( h );
    get_integral_result<char32_t>( h );
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

template<class R> void test2()
{
    test< H1<R> >();
}

template<class R> void test3()
{
    test< H2<R> >();
}

int main()
{
    using boost::hash2::get_integral_result;

    test2< unsigned char >();
    test2< unsigned short >();
    test2< unsigned int >();
    test2< unsigned long >();
    test2< unsigned long long >();

    test3< std::array<unsigned char, 8> >();
    test3< std::array<unsigned char, 16> >();
    test3< std::array<unsigned char, 20> >();
    test3< std::array<unsigned char, 28> >();
    test3< std::array<unsigned char, 32> >();
    test3< std::array<unsigned char, 48> >();
    test3< std::array<unsigned char, 64> >();

    test3< boost::array<unsigned char, 8> >();
    test3< boost::array<unsigned char, 16> >();
    test3< boost::array<unsigned char, 20> >();
    test3< boost::array<unsigned char, 28> >();
    test3< boost::array<unsigned char, 32> >();
    test3< boost::array<unsigned char, 48> >();
    test3< boost::array<unsigned char, 64> >();

    test3< boost::hash2::digest<8> >();
    test3< boost::hash2::digest<16> >();
    test3< boost::hash2::digest<20> >();
    test3< boost::hash2::digest<28> >();
    test3< boost::hash2::digest<32> >();
    test3< boost::hash2::digest<48> >();
    test3< boost::hash2::digest<64> >();
}
