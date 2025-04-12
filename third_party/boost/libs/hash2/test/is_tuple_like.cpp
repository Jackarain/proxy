// Copyright 2017 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/container_hash/is_tuple_like.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <utility>
#include <array>
#include <tuple>

struct X
{
};

int main()
{
    using boost::container_hash::is_tuple_like;

    BOOST_TEST_TRAIT_FALSE((is_tuple_like<void>));
    BOOST_TEST_TRAIT_FALSE((is_tuple_like<void const>));

    BOOST_TEST_TRAIT_FALSE((is_tuple_like<int>));
    BOOST_TEST_TRAIT_FALSE((is_tuple_like<int const>));

    BOOST_TEST_TRAIT_FALSE((is_tuple_like<X>));
    BOOST_TEST_TRAIT_FALSE((is_tuple_like<X const>));

    BOOST_TEST_TRAIT_FALSE((is_tuple_like<int[2]>));
    BOOST_TEST_TRAIT_FALSE((is_tuple_like<int const[2]>));

    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::pair<int, X> >));
    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::pair<int, X> const >));

    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::array<X, 0> >));
    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::array<X, 0> const >));

    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::array<X, 1> >));
    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::array<X, 1> const >));

    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::array<X, 2> >));
    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::array<X, 2> const >));

    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::array<X, 3> >));
    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::array<X, 3> const >));

    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::tuple<> >));
    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::tuple<> const >));

    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::tuple<X> >));
    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::tuple<X> const >));

    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::tuple<X, X> >));
    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::tuple<X, X> const >));

    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::tuple<X, X, X> >));
    BOOST_TEST_TRAIT_TRUE((is_tuple_like< std::tuple<X, X, X> const >));

    return boost::report_errors();
}
