// Copyright David Abrahams 2003.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#ifndef STATIC_ASSERT_SAME_DWA2003530_HPP
# define STATIC_ASSERT_SAME_DWA2003530_HPP

#include <type_traits>

#define STATIC_ASSERT_SAME( T1,T2 ) static_assert(std::is_same<T1, T2>::value, "T1 and T2 are expected to be the same types.")

template <class T1, class T2>
struct static_assert_same
{
    STATIC_ASSERT_SAME(T1, T2);
    enum { value = 1 };
};

#endif // STATIC_ASSERT_SAME_DWA2003530_HPP
