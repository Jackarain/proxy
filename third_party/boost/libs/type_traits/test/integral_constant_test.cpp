// Copyright 2025 DoctorNoobingstoneIPresume
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/type_traits/integral_constant.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/static_assert.hpp>

int main ()
{
    // [2025-01-07] Adapted from example in https://en.cppreference.com/w/cpp/types/integral_constant ...

#ifndef BOOST_NO_CXX11_CONSTEXPR

    using two_t  = boost::integral_constant <int, 2>;
    using four_t = boost::integral_constant <int, 4>;

    BOOST_STATIC_ASSERT ((! boost::is_same <two_t, four_t>::value));
    BOOST_STATIC_ASSERT ((two_t::value * 2 == four_t::value));
    BOOST_STATIC_ASSERT ((two_t () << 1 == four_t ()));
    BOOST_STATIC_ASSERT ((two_t () () << 1 == four_t () ()));

    enum class E {e0, e1};
    using c0 = boost::integral_constant <E, E::e0>;
    using c1 = boost::integral_constant <E, E::e1>;
    BOOST_STATIC_ASSERT ((c0::value != E::e1));
    BOOST_STATIC_ASSERT ((c0 () == E::e0));
    BOOST_STATIC_ASSERT ((boost::is_same <c1, c1>::value));
#endif
}
