// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/workaround.hpp>
#include <memory>
#include <utility>

boost::array<int, 3> f1()
{
    boost::array<int, 3> r = {{ 1, 2, 3 }};
    return r;
}

boost::array<int, 3> const f2()
{
    boost::array<int, 3> r = {{ 1, 2, 3 }};
    return r;
}

int main()
{
    {
        int a[] = { 1, 2, 3 };

        boost::array<int, 3> b = boost::to_array( a );

        BOOST_TEST_EQ( b[0], 1 );
        BOOST_TEST_EQ( b[1], 2 );
        BOOST_TEST_EQ( b[2], 3 );
    }

    {
        int const a[] = { 1, 2, 3 };

        boost::array<int, 3> b = boost::to_array( a );

        BOOST_TEST_EQ( b[0], 1 );
        BOOST_TEST_EQ( b[1], 2 );
        BOOST_TEST_EQ( b[2], 3 );
    }

    {
        boost::array<int, 3> b = boost::to_array( f1().elems );

        BOOST_TEST_EQ( b[0], 1 );
        BOOST_TEST_EQ( b[1], 2 );
        BOOST_TEST_EQ( b[2], 3 );
    }

    {
        boost::array<int, 3> b = boost::to_array( f2().elems );

        BOOST_TEST_EQ( b[0], 1 );
        BOOST_TEST_EQ( b[1], 2 );
        BOOST_TEST_EQ( b[2], 3 );
    }

#if BOOST_CXX_VERSION >= 201103L

#if !BOOST_WORKAROUND(BOOST_MSVC, < 1910)
    {
        int a[] = { 1, 2, 3 };

        boost::array<int, 3> b = boost::to_array( std::move( a ) );

        BOOST_TEST_EQ( b[0], 1 );
        BOOST_TEST_EQ( b[1], 2 );
        BOOST_TEST_EQ( b[2], 3 );
    }

    {
        int const a[] = { 1, 2, 3 };

        boost::array<int, 3> b = boost::to_array( std::move( a ) );

        BOOST_TEST_EQ( b[0], 1 );
        BOOST_TEST_EQ( b[1], 2 );
        BOOST_TEST_EQ( b[2], 3 );
    }
#endif

#if !BOOST_WORKAROUND(BOOST_GCC, < 40900) && !BOOST_WORKAROUND(BOOST_CLANG_VERSION, < 30800)
    {
        boost::array<int, 3> b = boost::to_array({ 1, 2, 3 });

        BOOST_TEST_EQ( b[0], 1 );
        BOOST_TEST_EQ( b[1], 2 );
        BOOST_TEST_EQ( b[2], 3 );
    }
#endif

#if !BOOST_WORKAROUND(BOOST_MSVC, < 1920)
    {
        std::unique_ptr<int> a[] = { {}, {}, {} };

        boost::array<std::unique_ptr<int>, 3> b = boost::to_array( std::move( a ) );

        (void)b;
    }
#endif

#endif

    return boost::report_errors();
}
