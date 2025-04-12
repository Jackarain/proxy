// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/md5.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <array>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

template<class H, std::size_t N> BOOST_CXX14_CONSTEXPR typename H::result_type test( std::uint64_t seed, unsigned char const (&v)[ N ] )
{
    H h( seed );

    h.update( v, N / 3 );
    h.update( v + N / 3, N - N / 3 );

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

    constexpr unsigned char v[ 95 ] = {};

    BOOST_CXX14_CONSTEXPR digest<16> r1 = {{ 10, 29, 252, 24, 200, 200, 56, 31, 5, 248, 173, 157, 43, 69, 9, 181 }};
    BOOST_CXX14_CONSTEXPR digest<16> r2 = {{ 181, 92, 80, 91, 73, 60, 132, 154, 25, 168, 65, 211, 8, 142, 193, 207 }};

    TEST_EQ( test<md5_128>( 0, v ), r1 );
    TEST_EQ( test<md5_128>( 7, v ), r2 );

    return boost::report_errors();
}
