// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/static_assert.hpp>

template<int A, int B> struct plus
{
    static const int value = A + B;
};

BOOST_CORE_STATIC_ASSERT((plus<1, 2>::value == 3));

template<int C> struct X
{
    BOOST_CORE_STATIC_ASSERT((plus<C, 1>::value == C + 1));
};

int main()
{
    BOOST_CORE_STATIC_ASSERT((plus<3, 4>::value == 7));

    X<4> x;
    (void)x;
}
