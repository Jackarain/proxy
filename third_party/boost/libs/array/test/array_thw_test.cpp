// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>
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

template<class T, std::size_t N> void test()
{
    constexpr auto eq = 0 <=> 0;
    constexpr auto lt = 0 <=> 1;
    constexpr auto gt = 1 <=> 0;

    {
        boost::array<T, N> const a1 = {};
        boost::array<T, N> const a2 = {};

        BOOST_TEST( ( a1 <=> a2 ) == eq );
    }

    {
        boost::array<T, N> a1;
        boost::array<T, N> a2;

        a1.fill( 1 );
        a2.fill( 1 );

        BOOST_TEST( ( a1 <=> a2 ) == eq );
    }

    for( std::size_t i = 0; i < N; ++i )
    {
        boost::array<T, N> a1;
        boost::array<T, N> a2;

        a1.fill( 1 );
        a2.fill( 1 );

        a1[ i ] = 0;

        BOOST_TEST( ( a1 <=> a2 ) == lt );

        {
            boost::array<T, N> const a3 = a1;
            boost::array<T, N> const a4 = a2;

            BOOST_TEST( ( a3 <=> a4 ) == lt );
        }

        a1[ i ] = 2;

        BOOST_TEST( ( a1 <=> a2 ) == gt );

        {
            boost::array<T, N> const a3 = a1;
            boost::array<T, N> const a4 = a2;

            BOOST_TEST( ( a3 <=> a4 ) == gt );
        }
    }
}

int main()
{
    test<int, 0>();
    test<int, 1>();
    test<int, 7>();

    test<float, 0>();
    test<float, 1>();
    test<float, 7>();

    return boost::report_errors();
}

#endif
