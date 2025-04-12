// Copyright 2017, 2023, 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/is_trivially_equality_comparable.hpp>
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

template<class T> void test( bool exp )
{
    using boost::hash2::is_trivially_equality_comparable;

    if( exp )
    {
        BOOST_TEST_TRAIT_TRUE((is_trivially_equality_comparable<T>));
        BOOST_TEST_TRAIT_TRUE((is_trivially_equality_comparable<T const>));
    }
    else
    {
        BOOST_TEST_TRAIT_FALSE((is_trivially_equality_comparable<T>));
        BOOST_TEST_TRAIT_FALSE((is_trivially_equality_comparable<T const>));
    }
}

int main()
{
    test<bool>( true );

    test<char>( true );
    test<signed char>( true);
    test<unsigned char>( true );

    test<wchar_t>( true );

    test<char16_t>( true );
    test<char32_t>( true );

    test<short>( true );
    test<unsigned short>( true );

    test<int>( true );
    test<unsigned int>( true );

    test<long>( true );
    test<unsigned long>( true );

    test<long long>( true );
    test<unsigned long long>( true );

    //

    test<float>( false );
    test<double>( false );
    test<long double>( false );

    //

    test<std::nullptr_t>( false );

    test<void*>( true );
    test<void const*>( true );

    test<void(*)()>( true );

    test<X*>( true );
    test<Y*>( true );

    //

    test<E>( true );

    //

    test<Y>( false );

    test<int Y::*>( false );
    test<void (Y::*)()>( false );

    //

    return boost::report_errors();
}
