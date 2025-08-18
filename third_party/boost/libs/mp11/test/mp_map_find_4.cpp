// Copyright 2016, 2020, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/map.hpp>
#include <boost/mp11/list.hpp>
#include <boost/core/lightweight_test_trait.hpp>

template<class T> struct Wrap
{
    struct type {};
};

int main()
{
    using boost::mp11::mp_map_find;
    using boost::mp11::mp_list;

    using Int = Wrap<int>::type;
    using Long = Wrap<long>::type;

    using Map = mp_list<
        mp_list<Int, void>,
        mp_list<Long, void>
    >;

    BOOST_TEST_TRAIT_SAME( mp_map_find<Map, Int>, mp_list<Int, void> );
    BOOST_TEST_TRAIT_SAME( mp_map_find<Map, Long>, mp_list<Long, void> );
    BOOST_TEST_TRAIT_SAME( mp_map_find<Map, int>, void );

    return boost::report_errors();
}
