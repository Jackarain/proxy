// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/hash2/detail/config.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <limits>
#include <cstdint>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

template<class Hash, class Flavor, class T>
BOOST_CXX14_CONSTEXPR typename Hash::result_type test( T const& v )
{
    Hash h;
    Flavor f{};

    hash_append( h, f, v );

    return h.result();
}

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if defined(BOOST_NO_CXX14_CONSTEXPR) || !defined(BOOST_HASH2_HAS_BUILTIN_BIT_CAST)

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2)

#else

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2); STATIC_ASSERT(x1 == x2)

#endif

int main()
{
    using namespace boost::hash2;

    // float

    TEST_EQ( (test<fnv1a_32, default_flavor>( +0.0f )), 1268118805 );
    TEST_EQ( (test<fnv1a_32, default_flavor>( -0.0f )), 1268118805 );

    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( +1.0f )), 1902468634 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( -1.0f )), 1106845850 );

    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( +3.14f )), 1979491385 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( -3.14f )), 1566646201 );

    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( +std::numeric_limits<float>::infinity() )), 1716885978 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( -std::numeric_limits<float>::infinity() )), 921263194 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( +1.0f )), 458782360 );
    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( -1.0f )), 2606317592 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( +3.14f )), 2486333141 );
    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( -3.14f )), 338797909 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( +std::numeric_limits<float>::infinity() )), 1532549976 );
    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( -std::numeric_limits<float>::infinity() )), 3680085208 );

    // double

    TEST_EQ( (test<fnv1a_32, default_flavor>( +0.0 )), 2615243109 );
    TEST_EQ( (test<fnv1a_32, default_flavor>( -0.0 )), 2615243109 );

    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( +1.0 )), 2880721802 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( -1.0 )), 1348582922 );

    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( +3.14 )), 1565379100 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( -3.14 )), 555586716 );

    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( +std::numeric_limits<double>::infinity() )), 682495434 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( -std::numeric_limits<double>::infinity() )), 3445323850 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( +1.0 )), 2355796088 );
    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( -1.0 )), 208260856 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( +3.14 )), 4023697370 );
    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( -3.14 )), 1876162138 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( +std::numeric_limits<double>::infinity() )), 3429563704 );
    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( -std::numeric_limits<double>::infinity() )), 1282028472 );

    return boost::report_errors();
}
