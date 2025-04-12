// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/sha1.hpp>
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

    BOOST_CXX14_CONSTEXPR digest<20> r1 = {{ 0xBD, 0x05, 0x7D, 0x7F, 0x49, 0x14, 0x38, 0x24, 0xE4, 0x52, 0x63, 0x14, 0x7A, 0x02, 0xA5, 0x80, 0x13, 0x7F, 0xAB, 0xEF }};
    BOOST_CXX14_CONSTEXPR digest<20> r2 = {{ 0xB8, 0x90, 0x42, 0x8C, 0x88, 0xBB, 0x53, 0xD1, 0x19, 0xB9, 0x34, 0x18, 0x0D, 0xA6, 0x26, 0x5B, 0xB1, 0xEF, 0xF7, 0x58 }};

    TEST_EQ( test<sha1_160>( 0, v ), r1 );
    TEST_EQ( test<sha1_160>( 7, v ), r2 );

    return boost::report_errors();
}
