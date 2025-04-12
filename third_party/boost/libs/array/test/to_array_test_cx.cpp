// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>
#include <boost/config/workaround.hpp>
#include <cstddef>

#if defined(BOOST_NO_CXX14_CONSTEXPR)

BOOST_PRAGMA_MESSAGE("Test skipped because BOOST_NO_CXX14_CONSTEXPR is defined")

#else

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

int main()
{
    {
        constexpr int a[] = { 1, 2, 3, 4 };
        constexpr boost::array<int, 4> b = boost::to_array( a );

        STATIC_ASSERT( b[0] == 1 );
        STATIC_ASSERT( b[1] == 2 );
        STATIC_ASSERT( b[2] == 3 );
        STATIC_ASSERT( b[3] == 4 );
    }
}

#endif
