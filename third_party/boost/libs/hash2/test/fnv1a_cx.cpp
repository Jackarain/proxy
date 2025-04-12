// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <cstring>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

template<class H, std::size_t N> BOOST_CXX14_CONSTEXPR typename H::result_type test( std::uint64_t seed, unsigned char const (&v)[ N ] )
{
    H h( seed );

    h.update( v, N );

    return h.result();
}

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if defined(BOOST_NO_CXX14_CONSTEXPR)

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2)

#else

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2); STATIC_ASSERT(x1 == x2)

#endif

int main()
{
    using namespace boost::hash2;

    constexpr unsigned char v1[] = { 0 };
    constexpr unsigned char v4[] = { 0, 1, 2, 3 };

    TEST_EQ( test<fnv1a_32>( 0, v1 ), 84696351 );
    TEST_EQ( test<fnv1a_32>( 0, v4 ), 3282719153 );

    TEST_EQ( test<fnv1a_64>( 0, v1 ), 12638153115695167455ull );
    TEST_EQ( test<fnv1a_64>( 0, v4 ), 4932904490461320209 );

    TEST_EQ( test<fnv1a_32>( 7, v1 ), 1695235878 );
    TEST_EQ( test<fnv1a_32>( 7, v4 ), 4085431250 );

    TEST_EQ( test<fnv1a_64>( 7, v1 ), 3154070194012243846 );
    TEST_EQ( test<fnv1a_64>( 7, v4 ), 9028993744456876338 );

    return boost::report_errors();
}
