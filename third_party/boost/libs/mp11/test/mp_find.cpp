// Copyright 2015, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/algorithm.hpp>
#include <boost/mp11/list.hpp>
#include <boost/mp11/integral.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>
#include <tuple>
#include <utility>

struct X1 {};
struct X2 {};
struct X3 {};

int main()
{
    using boost::mp11::mp_list;
    using boost::mp11::mp_find;
    using boost::mp11::mp_size_t;

    {
        using L1 = mp_list<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_find<L1, void>, mp_size_t<0>>));

        using L2 = mp_list<X1, X2, X2, X3, X3, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_find<L2, void>, mp_size_t<6>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_find<L2, X1>, mp_size_t<0>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_find<L2, X2>, mp_size_t<1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_find<L2, X3>, mp_size_t<3>>));
    }

    {
        using L3 = std::tuple<>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_find<L3, void>, mp_size_t<0>>));

        using L4 = std::tuple<X1, X2, X2, X3, X3, X3>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_find<L4, void>, mp_size_t<6>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_find<L4, X1>, mp_size_t<0>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_find<L4, X2>, mp_size_t<1>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_find<L4, X3>, mp_size_t<3>>));
    }

    {
        using L5 = std::pair<X1, X2>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_find<L5, void>, mp_size_t<2>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_find<L5, X1>, mp_size_t<0>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_find<L5, X2>, mp_size_t<1>>));
    }

    {
        using boost::mp11::mp_iota_c;

#if !defined( BOOST_MP11_HAS_CXX14_CONSTEXPR )

        int const N = 257;

#else

        int const N = 1089;

#endif

        using L = mp_iota_c<N>;
        using R = mp_find<L, mp_size_t<N/2>>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<R, mp_size_t<N/2>>));
    }

    return boost::report_errors();
}
