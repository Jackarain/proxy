// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
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

#if defined(BOOST_NO_CXX14_CONSTEXPR) || ( defined(BOOST_GCC) && BOOST_GCC < 60000 )

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2)

#else

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2); STATIC_ASSERT(x1 == x2)

#endif

int main()
{
    using namespace boost::hash2;

    // 8 bit

    TEST_EQ( (test<fnv1a_32, default_flavor>( std::int8_t( 0 ) )), 84696351 );
    TEST_EQ( (test<fnv1a_32, default_flavor>( std::uint8_t( 0 ) )), 84696351 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( std::int8_t( -2 ) )), 2064352225 );
    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( std::uint8_t( +1 ) )), 67918732 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( std::int8_t( -2 ) )), 2064352225 );
    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( std::uint8_t( +1 ) )), 67918732 );

    // 16 bit

    TEST_EQ( (test<fnv1a_32, default_flavor>( std::int16_t( 0 ) )), 292984781 );
    TEST_EQ( (test<fnv1a_32, default_flavor>( std::uint16_t( 0 ) )), 292984781 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( std::int16_t( -2 ) )), 3508496442 );
    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( std::uint16_t( +1 ) )), 3950255460 );

    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( std::int16_t( -2 ) )), 3491674896 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( std::uint16_t( +1 ) )), 276207162 );

    // 32 bit

    TEST_EQ( (test<fnv1a_32, default_flavor>( std::int32_t( 0 ) )), 1268118805 );
    TEST_EQ( (test<fnv1a_32, default_flavor>( std::uint32_t( 0 ) )), 1268118805 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( std::int32_t( -2 ) )), 2388331168 );
    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( std::uint32_t( +1 ) )), 4218009092 );

    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( std::int32_t( -2 ) )), 3793096222 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( std::uint32_t( +1 ) )), 1251341186 );

    // 64 bit

    TEST_EQ( (test<fnv1a_32, default_flavor>( std::int64_t( 0 ) )), 2615243109 );
    TEST_EQ( (test<fnv1a_32, default_flavor>( std::uint64_t( 0 ) )), 2615243109 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( std::int64_t( -2 ) )), 751220028 );
    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( std::uint64_t( +1 ) )), 1048580676 );

    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( std::int64_t( -2 ) )), 1806567626 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( std::uint64_t( +1 ) )), 2598465490 );

    return boost::report_errors();
}
