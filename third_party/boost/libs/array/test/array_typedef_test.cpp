// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/array.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iterator>
#include <cstddef>

template<class T, std::size_t N> void test()
{
    typedef boost::array<T, N> A;

    BOOST_TEST_TRAIT_SAME(typename A::value_type, T);

    BOOST_TEST_TRAIT_SAME(typename A::iterator, T*);
    BOOST_TEST_TRAIT_SAME(typename A::const_iterator, T const*);

    BOOST_TEST_TRAIT_SAME(typename A::reverse_iterator, std::reverse_iterator<T*>);
    BOOST_TEST_TRAIT_SAME(typename A::const_reverse_iterator, std::reverse_iterator<T const*>);

    BOOST_TEST_TRAIT_SAME(typename A::reference, T&);
    BOOST_TEST_TRAIT_SAME(typename A::const_reference, T const&);

    BOOST_TEST_TRAIT_SAME(typename A::size_type, std::size_t);
    BOOST_TEST_TRAIT_SAME(typename A::difference_type, std::ptrdiff_t);

    BOOST_TEST_EQ(A::static_size, N);
}

int main()
{
    test<int, 0>();
    test<int, 1>();
    test<int, 7>();

    test<int const, 0>();
    test<int const, 1>();
    test<int const, 7>();

    return boost::report_errors();
}
