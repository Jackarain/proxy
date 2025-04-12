// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/xxhash.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <cstring>

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

    constexpr unsigned char v21[ 21 ] = {};
    constexpr unsigned char v45[ 45 ] = {};

    TEST_EQ( test<xxhash_32>( 0, v21 ), 86206869 );
    TEST_EQ( test<xxhash_32>( 0, v45 ), 747548280 );

    TEST_EQ( test<xxhash_64>( 0, v21 ), 8680240691998137788 );
    TEST_EQ( test<xxhash_64>( 0, v45 ), 4352694002423811028 );

    TEST_EQ( test<xxhash_32>( 7, v21 ), 2135174986 );
    TEST_EQ( test<xxhash_32>( 7, v45 ), 1547773082 );

    TEST_EQ( test<xxhash_64>( 7, v21 ), 16168826474312362322ull );
    TEST_EQ( test<xxhash_64>( 7, v45 ), 14120916949766558435ull );

    return boost::report_errors();
}
