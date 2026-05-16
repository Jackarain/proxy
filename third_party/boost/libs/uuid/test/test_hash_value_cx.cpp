// Copyright 2024, 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER) && _MSC_VER < 1920
# pragma warning( disable: 4307 ) // '*': integral constant overflow
# pragma warning( disable: 4309 ) // 'static_cast': truncation of constant value
#endif

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>
#include <cstddef>

#if defined(BOOST_NO_CXX14_CONSTEXPR)
BOOST_PRAGMA_MESSAGE( "Test is not constexpr because BOOST_NO_CXX14_CONSTEXPR is defined" )
#endif

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if defined(BOOST_NO_CXX14_CONSTEXPR)
# define TEST_EQ(x, y) BOOST_TEST_EQ(x, y)
#else
# define TEST_EQ(x, y) STATIC_ASSERT((x)==(y))
#endif

using namespace boost::uuids;

int main()
{
    BOOST_CXX14_CONSTEXPR uuid u1;
    TEST_EQ( hash_value( u1 ), 0 );

    BOOST_CXX14_CONSTEXPR uuid u2 = {{ 0x01, 0x02, 0x03, 0x04 }};
    TEST_EQ( hash_value( u2 ), static_cast<std::size_t>( 7362128010791177377ull ) );

    BOOST_CXX14_CONSTEXPR uuid u3 = {{ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10 }};
    TEST_EQ( hash_value( u3 ), static_cast<std::size_t>( 18416781058927374793ull ) );

    return boost::report_errors();
}
