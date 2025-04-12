// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>
#include <boost/config/workaround.hpp>
#include <cstddef>

#if defined(__has_include)
# if __has_include(<compare>)
#  define HAS_COMPARE
# endif
#endif

#if !defined(__cpp_impl_three_way_comparison)

BOOST_PRAGMA_MESSAGE( "Test skipped because __cpp_impl_three_way_comparison is not defined" )
int main() {}

#elif !( __cpp_impl_three_way_comparison >= 201907L )

BOOST_PRAGMA_MESSAGE( "Test skipped because __cpp_impl_three_way_comparison is defined to " BOOST_STRINGIZE(__cpp_impl_three_way_comparison) )
int main() {}

#elif !defined(HAS_COMPARE)

BOOST_PRAGMA_MESSAGE( "Test skipped because <compare> is not available" )
int main() {}

#else

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

template<class T, std::size_t N> void test1()
{
    constexpr auto eq = 0 <=> 0;

    constexpr boost::array<T, N> a1 = {};
    constexpr boost::array<T, N> a2 = {};

    STATIC_ASSERT( ( a1 <=> a2 ) == eq );
}

template<class T> void test2()
{
    constexpr auto eq = 0 <=> 0;
    constexpr auto lt = 0 <=> 1;
    constexpr auto gt = 1 <=> 0;

    {
        constexpr boost::array<T, 4> a1 = {{ 1, 2, 3, 4 }};
        constexpr boost::array<T, 4> a2 = {{ 1, 2, 3, 4 }};

        STATIC_ASSERT( ( a1 <=> a2 ) == eq );
    }
    {
        constexpr boost::array<T, 4> a1 = {{ 1, 2, 3, 4 }};
        constexpr boost::array<T, 4> a2 = {{ 1, 2, 3, 5 }};

        STATIC_ASSERT( ( a1 <=> a2 ) == lt );
    }
    {
        constexpr boost::array<T, 4> a1 = {{ 1, 2, 3, 4 }};
        constexpr boost::array<T, 4> a2 = {{ 1, 2, 3, 2 }};

        STATIC_ASSERT( ( a1 <=> a2 ) == gt );
    }
}

int main()
{
    test1<int, 0>();
    test1<int, 1>();
    test1<int, 7>();

    test2<int>();
}

#endif
