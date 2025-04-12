// Copyright 2017, 2023, 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/is_endian_independent.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <cstddef>

class X;

struct Y
{
};

struct Z
{
    int a;
};

enum E
{
    v
};

template<class T> void test( bool exp )
{
    using boost::hash2::is_endian_independent;

    if( exp )
    {
        BOOST_TEST_TRAIT_TRUE((is_endian_independent<T>));
        BOOST_TEST_TRAIT_TRUE((is_endian_independent<T const>));
    }
    else
    {
        BOOST_TEST_TRAIT_FALSE((is_endian_independent<T>));
        BOOST_TEST_TRAIT_FALSE((is_endian_independent<T const>));
    }
}

int main()
{
    test<bool>( true );

    test<char>( true );
    test<signed char>( true);
    test<unsigned char>( true );

    test<wchar_t>( false );

    test<char16_t>( false );
    test<char32_t>( false );

    test<short>( false );
    test<unsigned short>( false );

    test<int>( false );
    test<unsigned int>( false );

    test<long>( false );
    test<unsigned long>( false );

    test<long long>( false );
    test<unsigned long long>( false );

    //

    test<float>( false );
    test<double>( false );
    test<long double>( false );

    //

    test<std::nullptr_t>( false );

    test<void*>( false );
    test<void const*>( false );

    test<void(*)()>( false );

    test<X*>( false );
    test<Y*>( false );

    //

    test<E>( sizeof(E) == 1 );

    //

    test<Y>( true );
    test<Z>( false );

    test<int Y::*>( false );
    test<void (Y::*)()>( false );

    //

    return boost::report_errors();
}
