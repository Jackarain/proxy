// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/static_assert.hpp>

BOOST_CORE_STATIC_ASSERT( sizeof(char[1]) == 1 );

struct X
{
    BOOST_CORE_STATIC_ASSERT( sizeof(char[2]) == 2 );
};

int main()
{
    BOOST_CORE_STATIC_ASSERT( sizeof(char[3]) == 3 );
}
