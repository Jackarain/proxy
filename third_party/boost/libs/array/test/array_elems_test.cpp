// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstddef>

template<class T, std::size_t N> void test()
{
    boost::array<T, N> a = {{}};

    T (&e)[ N ] = a.elems;

    BOOST_TEST_EQ( static_cast<void const*>( e ), static_cast<void const*>( &a ) );
}

int main()
{
    test<int, 1>();
    test<int, 7>();

    test<int const, 1>();
    test<int const, 7>();

    return boost::report_errors();
}
