// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/siphash.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <cstring>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if defined(BOOST_NO_CXX14_CONSTEXPR)

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2)

#else

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2); STATIC_ASSERT(x1 == x2)

#endif

int main()
{
    using namespace boost::hash2;

    {
        constexpr auto r = 95563134;

        TEST_EQ( siphash_32().result(), r );
        TEST_EQ( siphash_32(0).result(), r );
        TEST_EQ( siphash_32(static_cast<unsigned char const*>(nullptr), 0).result(), r );
    }

    {
        constexpr auto r = 2202906307356721367ull;

        TEST_EQ( siphash_64().result(), r );
        TEST_EQ( siphash_64(0).result(), r );
        TEST_EQ( siphash_64(static_cast<unsigned char const*>(nullptr), 0).result(), r );
    }

    return boost::report_errors();
}
