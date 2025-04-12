// Copyright 2017, 2023 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/is_contiguously_hashable.hpp>
#include <boost/hash2/endian.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <cstddef>

class X;

struct Y
{
};

enum E
{
    v
};

template<class T, boost::hash2::endian N> void test( bool exp )
{
    using boost::hash2::is_contiguously_hashable;

    if( exp )
    {
        BOOST_TEST_TRAIT_TRUE((is_contiguously_hashable<T, N>));
        BOOST_TEST_TRAIT_TRUE((is_contiguously_hashable<T const, N>));
        BOOST_TEST_TRAIT_TRUE((is_contiguously_hashable<T[2], N>));
        BOOST_TEST_TRAIT_TRUE((is_contiguously_hashable<T const [2], N>));
    }
    else
    {
        BOOST_TEST_TRAIT_FALSE((is_contiguously_hashable<T, N>));
        BOOST_TEST_TRAIT_FALSE((is_contiguously_hashable<T const, N>));
        BOOST_TEST_TRAIT_FALSE((is_contiguously_hashable<T[2], N>));
        BOOST_TEST_TRAIT_FALSE((is_contiguously_hashable<T const [2], N>));
    }
}

int main()
{
    using boost::hash2::endian;

    //

    test<bool, endian::native>( true );

    //

    test<char, endian::native>( true );
    test<char, endian::little>( true );
    test<char, endian::big>( true );

    test<signed char, endian::native>( true);
    test<signed char, endian::little>( true );
    test<signed char, endian::big>( true );

    test<unsigned char, endian::native>( true );
    test<unsigned char, endian::little>( true );
    test<unsigned char, endian::big>( true );

    test<wchar_t, endian::native>( true );
    test<wchar_t, endian::little>( endian::native == endian::little );
    test<wchar_t, endian::big>( endian::native == endian::big );

    test<char16_t, endian::native>( true );
    test<char16_t, endian::little>( endian::native == endian::little );
    test<char16_t, endian::big>( endian::native == endian::big );

    test<char32_t, endian::native>( true );
    test<char32_t, endian::little>( endian::native == endian::little );
    test<char32_t, endian::big>( endian::native == endian::big );

    //

    test<short, endian::native>( true );
    test<short, endian::little>( endian::native == endian::little );
    test<short, endian::big>( endian::native == endian::big );

    test<unsigned short, endian::native>( true );
    test<unsigned short, endian::little>( endian::native == endian::little );
    test<unsigned short, endian::big>( endian::native == endian::big );

    test<int, endian::native>( true );
    test<int, endian::little>( endian::native == endian::little );
    test<int, endian::big>( endian::native == endian::big );

    test<unsigned int, endian::native>( true );
    test<unsigned int, endian::little>( endian::native == endian::little );
    test<unsigned int, endian::big>( endian::native == endian::big );

    test<long, endian::native>( true );
    test<long, endian::little>( endian::native == endian::little );
    test<long, endian::big>( endian::native == endian::big );

    test<unsigned long, endian::native>( true );
    test<unsigned long, endian::little>( endian::native == endian::little );
    test<unsigned long, endian::big>( endian::native == endian::big );

    test<long long, endian::native>( true );
    test<long long, endian::little>( endian::native == endian::little );
    test<long long, endian::big>( endian::native == endian::big );

    test<unsigned long long, endian::native>( true );
    test<unsigned long long, endian::little>( endian::native == endian::little );
    test<unsigned long long, endian::big>( endian::native == endian::big );

    //

    test<float, endian::native>( false );
    test<float, endian::little>( false );
    test<float, endian::big>( false );

    test<double, endian::native>( false );
    test<double, endian::little>( false );
    test<double, endian::big>( false );

    test<long double, endian::native>( false );
    test<long double, endian::little>( false );
    test<long double, endian::big>( false );

    //

    test<std::nullptr_t, endian::native>( false );
    test<std::nullptr_t, endian::little>( false );
    test<std::nullptr_t, endian::big>( false );

    test<void*, endian::native>( true );
    test<void*, endian::little>( endian::native == endian::little );
    test<void*, endian::big>( endian::native == endian::big );

    test<void const*, endian::native>( true );
    test<void const*, endian::little>( endian::native == endian::little );
    test<void const*, endian::big>( endian::native == endian::big );

    test<void(*)(), endian::native>( true );
    test<void(*)(), endian::little>( endian::native == endian::little );
    test<void(*)(), endian::big>( endian::native == endian::big );

    //

    test<E, endian::native>( true );
    test<E, endian::little>( endian::native == endian::little || sizeof(E) == 1 );
    test<E, endian::big>( endian::native == endian::big || sizeof(E) == 1 );

    //

    test<X*, endian::native>( true );
    test<X*, endian::little>( endian::native == endian::little );
    test<X*, endian::big>( endian::native == endian::big );

    //

    test<Y, endian::native>( false );
    test<Y, endian::little>( false );
    test<Y, endian::big>( false );

    test<Y*, endian::native>( true );
    test<Y*, endian::little>( endian::native == endian::little );
    test<Y*, endian::big>( endian::native == endian::big );

    test<int Y::*, endian::native>( false );
    test<int Y::*, endian::little>( false );
    test<int Y::*, endian::big>( false );

    test<void (Y::*)(), endian::native>( false );
    test<void (Y::*)(), endian::little>( false );
    test<void (Y::*)(), endian::big>( false );

    //

    return boost::report_errors();
}
