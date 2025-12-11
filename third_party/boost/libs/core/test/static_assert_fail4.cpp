// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/static_assert.hpp>

template<int C> struct X
{
    BOOST_CORE_STATIC_ASSERT( C >= 0 );
};

int main()
{
    X< -4 > x;
    (void)x;
}
