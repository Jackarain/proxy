// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/digest.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <sstream>
#include <iomanip>
#include <type_traits>

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

using namespace boost::hash2;

static void test_constructors()
{
    // default

    {
        constexpr int N = 11;

        digest<N> v;

        for( std::size_t i = 0; i < N; ++i )
        {
            BOOST_TEST_EQ( v.data()[ i ], 0 );
        }
    }

    // from init-list/array

    {
        constexpr int N = 5;

        digest<N> v = {{ 0, 1, 2, 3, 4 }};

        for( std::size_t i = 0; i < N; ++i )
        {
            BOOST_TEST_EQ( v.data()[ i ], i );
        }
    }

    {
        constexpr int N = 4;

        digest<N> v1;
        digest<N> v2 = {{ 0, 0, 0, 0 }};

        for( std::size_t i = 0; i < N; ++i )
        {
            BOOST_TEST_EQ( v1.data()[ i ], v2.data()[ i ] );
        }
    }

    // copy

    {
        constexpr int N = 6;

        digest<N> v1 = {{ 0, 1, 2, 3, 4, 5 }};
        digest<N> v2 = v1;

        for( std::size_t i = 0; i < N; ++i )
        {
            BOOST_TEST_EQ( v1.data()[ i ], v2.data()[ i ] );
        }
    }

    {
        BOOST_TEST_TRAIT_FALSE((std::is_constructible< digest<5>, digest<6> const& >));
        BOOST_TEST_TRAIT_FALSE((std::is_constructible< digest<6>, digest<5> const& >));
    }
}

static void test_assignment()
{
    {
        constexpr int N = 7;

        digest<N> v1 = {{ 0, 1, 2, 3, 4, 5, 6 }};
        digest<N> v2;

        v2 = v1;

        for( std::size_t i = 0; i < N; ++i )
        {
            BOOST_TEST_EQ( v1.data()[ i ], v2.data()[ i ] );
        }
    }
}

static void test_typedefs()
{
    BOOST_TEST_TRAIT_SAME(digest<1>::value_type, unsigned char);
    BOOST_TEST_TRAIT_SAME(digest<2>::reference, unsigned char&);
    BOOST_TEST_TRAIT_SAME(digest<3>::const_reference, unsigned char const&);
    BOOST_TEST_TRAIT_SAME(digest<4>::iterator, unsigned char*);
    BOOST_TEST_TRAIT_SAME(digest<5>::const_iterator, unsigned char const*);
    BOOST_TEST_TRAIT_SAME(digest<6>::size_type, std::size_t);
    BOOST_TEST_TRAIT_SAME(digest<7>::difference_type, std::ptrdiff_t);
}

static void test_data()
{
    {
        digest<17> d;
        BOOST_TEST_EQ( d.data(), reinterpret_cast<unsigned char*>( &d ) );
    }

    {
        digest<18> const d;
        BOOST_TEST_EQ( d.data(), reinterpret_cast<unsigned char const*>( &d ) );
    }
}

static void test_size()
{
    {
        digest<37> d;
        BOOST_TEST_EQ( sizeof( d ), 37 );
        BOOST_TEST_EQ( d.size(), 37 );
        BOOST_TEST_EQ( d.max_size(), 37 );
    }

#if !BOOST_WORKAROUND(BOOST_MSVC, < 1910 )

    {
        constexpr digest<51> d;
        STATIC_ASSERT( d.size() == 51 );
        STATIC_ASSERT( d.max_size() == 51 );
    }

#endif
}

static void test_iteration()
{
    {
        digest<23> d;
        BOOST_TEST_EQ( d.begin(), d.data() );
        BOOST_TEST_EQ( d.end(), d.data() + d.size() );
    }

    {
        digest<29> const d;
        BOOST_TEST_EQ( d.begin(), d.data() );
        BOOST_TEST_EQ( d.end(), d.data() + d.size() );
    }
}

static void test_element_access()
{
    {
        digest<17> d;

        for( std::size_t i = 0; i < d.size(); ++i )
        {
            BOOST_TEST_EQ( &d[ i ], d.data() + i );
        }

        BOOST_TEST_EQ( &d.front(), d.data() );
        BOOST_TEST_EQ( &d.back(), d.data() + d.size() - 1 );
    }

    {
        digest<19> const d;

        for( std::size_t i = 0; i < d.size(); ++i )
        {
            BOOST_TEST_EQ( &d[ i ], d.data() + i );
        }

        BOOST_TEST_EQ( &d.front(), d.data() );
        BOOST_TEST_EQ( &d.back(), d.data() + d.size() - 1 );
    }
}

static void test_comparisons()
{
    {
        digest<13> v1, v2;

        BOOST_TEST_EQ( v1, v2 );
        BOOST_TEST_NOT( v1 != v2 );
    }

    {
        digest<9> v1{{ 0, 1, 2, 3, 4, 5, 6, 7, 8 }};
        digest<9> v2{{ 0, 1, 2, 3, 4, 5, 6, 7, 8 }};

        BOOST_TEST_EQ( v1, v2 );
        BOOST_TEST_NOT( v1 != v2 );
    }

    {
        digest<9> v1{{ 0, 1, 2, 3, 4, 5, 6, 7, 8 }};
        digest<9> v2{{ 0, 1, 2, 3, 4, 5, 6, 7, 9 }};

        BOOST_TEST_NE( v1, v2 );
        BOOST_TEST_NOT( v1 == v2 );
    }

    {
        digest<9> v1{{ 0, 1, 2, 3, 4, 5, 6, 7, 8 }};
        digest<9> v2{{ 1, 1, 2, 3, 4, 5, 6, 7, 8 }};

        BOOST_TEST_NE( v1, v2 );
        BOOST_TEST_NOT( v1 == v2 );
    }

    {
        digest<4> v1;
        digest<4> v2{{ 0, 0, 0, 0 }};

        BOOST_TEST_EQ( v1, v2 );
        BOOST_TEST_NOT( v1 != v2 );
    }

    {
        digest<4> v1;
        digest<4> v2{{ 0, 0, 0, 1 }};

        BOOST_TEST_NE( v1, v2 );
        BOOST_TEST_NOT( v1 == v2 );
    }

    {
        digest<4> v1;
        digest<4> v2{{ 1, 0, 0, 0 }};

        BOOST_TEST_NE( v1, v2 );
        BOOST_TEST_NOT( v1 == v2 );
    }
}

static void test_to_chars()
{
    digest<5> const d{{ 0x12, 0x34, 0x56, 0x78, 0x9A }};

    {
        char buffer[ 13 ] = {};

        BOOST_TEST_EQ( to_chars( d, buffer, buffer + 9 ), nullptr );
        for( std::size_t i = 0; i < 13; ++i ) BOOST_TEST_EQ( buffer[i], 0 );

        for( std::size_t i = 0; i < 13; ++i ) buffer[ i ] = 0x7F;

        BOOST_TEST_EQ( to_chars( d, buffer, buffer + 10 ), buffer + 10 );
        for( std::size_t i = 10; i < 13; ++i ) BOOST_TEST_EQ( buffer[i], 0x7F );

        BOOST_TEST_EQ( std::string( buffer, 10 ), std::string( "123456789a" ) );
    }

    {
        char buffer[ 11 ] = {};
        for( std::size_t i = 0; i < 11; ++i ) buffer[ i ] = 0x7F;

        to_chars( d, buffer );

        BOOST_TEST_EQ( std::string( buffer, 10 ), std::string( "123456789a" ) );
        BOOST_TEST_EQ( buffer[ 10 ], 0 );
    }

    {
        char buffer[ 13 ] = {};
        for( std::size_t i = 0; i < 13; ++i ) buffer[ i ] = 0x7F;

        to_chars( d, buffer );

        BOOST_TEST_EQ( std::string( buffer, 10 ), std::string( "123456789a" ) );
        BOOST_TEST_EQ( buffer[ 10 ], 0 );
        BOOST_TEST_EQ( buffer[ 11 ], 0x7F );
        BOOST_TEST_EQ( buffer[ 12 ], 0x7F );
    }
}

static void test_to_string()
{
    digest<8> d{{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF }};

    BOOST_TEST_EQ( to_string( d ), std::string( "0123456789abcdef" ) );
}

static void test_stream_insert()
{
    digest<8> d{{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF }};

    std::ostringstream os;

    os << std::right << std::setfill( '-' ) << std::setw( 19 ) << d;

    BOOST_TEST_EQ( os.str(), std::string( "---0123456789abcdef" ) );
}

int main()
{
    test_constructors();
    test_assignment();
    test_typedefs();
    test_data();
    test_size();
    test_iteration();
    test_element_access();
    test_comparisons();
    test_to_chars();
    test_to_string();
    test_stream_insert();

    return boost::report_errors();
}
