// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>

template<class T, std::size_t N> void test()
{
    {
        boost::array<T, N> a = {{}};

        T* p1 = a.data();
        T* p2 = a.elems;

        BOOST_TEST_EQ( p1, p2 );
    }

    {
        boost::array<T, N> const a = {{}};

        T const* p1 = a.data();
        T const* p2 = a.elems;

        BOOST_TEST_EQ( p1, p2 );
    }
}

int main()
{
    // test<int, 0>();
    test<int, 1>();
    test<int, 7>();

    // test<int const, 0>();
    test<int const, 1>();
    test<int const, 7>();

    return boost::report_errors();
}
