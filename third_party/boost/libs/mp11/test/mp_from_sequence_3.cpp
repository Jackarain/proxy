// Copyright 2017, 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/algorithm.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

template<class T, T... I> struct S;

enum class E { one, two, three };

template<E e> using constant = std::integral_constant<E, e>;

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_from_sequence;

    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<S<E>>, mp_list<>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<S<E, E::one>>, mp_list<constant<E::one>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<S<E, E::one, E::three>>, mp_list<constant<E::one>, constant<E::three>>>));
    BOOST_TEST_TRAIT_TRUE((std::is_same<mp_from_sequence<S<E, E::one, E::two, E::three>>, mp_list<constant<E::one>, constant<E::two>, constant<E::three>>>));

    return boost::report_errors();
}
