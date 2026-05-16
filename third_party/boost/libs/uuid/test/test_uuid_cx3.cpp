// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define BOOST_UUID_REPORT_IMPLEMENTATION

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_UUID_NO_CXX14_CONSTEXPR_RT)
BOOST_PRAGMA_MESSAGE( "Test is not constexpr because BOOST_UUID_NO_CXX14_CONSTEXPR_RT is defined" )
#endif

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if defined(BOOST_UUID_NO_CXX14_CONSTEXPR_RT)

# define TEST_EQ(x, y) BOOST_TEST_EQ(x, y)
# define TEST_NE(x, y) BOOST_TEST_NE(x, y)
# define TEST_LT(x, y) BOOST_TEST_LT(x, y)
# define TEST(x) BOOST_TEST(x)

#else

# define TEST_EQ(x, y) STATIC_ASSERT((x)==(y))
# define TEST_NE(x, y) STATIC_ASSERT((x)!=(y))
# define TEST_LT(x, y) STATIC_ASSERT((x)<(y))
# define TEST(x) STATIC_ASSERT(x)

#endif

using namespace boost::uuids;

int main()
{
    BOOST_CXX14_CONSTEXPR uuid u1;
    BOOST_CXX14_CONSTEXPR uuid u2 = {{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }};
    BOOST_CXX14_CONSTEXPR uuid u3 = {{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }};

    TEST_EQ( u1.is_nil(), true );
    TEST_EQ( u2.is_nil(), false );
    TEST_EQ( u3.is_nil(), false );

    TEST_EQ( u1, u1 );
    TEST_EQ( u2, u2 );
    TEST_EQ( u3, u3 );

    TEST_NE( u1, u2 );
    TEST_NE( u1, u3 );
    TEST_NE( u2, u3 );

    TEST_LT( u1, u2 );
    TEST_LT( u1, u3 );
    TEST_LT( u3, u2 );

#if defined(BOOST_UUID_HAS_THREE_WAY_COMPARISON)

    constexpr auto eq = std::strong_ordering::equal;
    constexpr auto lt = std::strong_ordering::less;
    constexpr auto gt = std::strong_ordering::greater;

    TEST( ( u1 <=> u1 ) == eq );
    TEST( ( u2 <=> u2 ) == eq );
    TEST( ( u3 <=> u3 ) == eq );

    TEST( ( u1 <=> u2 ) == lt );
    TEST( ( u1 <=> u3 ) == lt );
    TEST( ( u2 <=> u3 ) == gt );

#endif

    return boost::report_errors();
}
