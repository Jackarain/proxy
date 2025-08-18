// Copyright 2025 Joaquin M Lopez Munoz.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/container_hash/hash_is_avalanching.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

struct X
{
    using is_avalanching = void;
};

struct Y
{
    using is_avalanching = std::true_type;
};

struct Z
{
    using is_avalanching = std::false_type;
};

struct W
{
};

int main()
{
    using boost::hash_is_avalanching;

    BOOST_TEST_TRAIT_TRUE(( hash_is_avalanching< X > ));
    BOOST_TEST_TRAIT_TRUE(( hash_is_avalanching< Y > ));
    BOOST_TEST_TRAIT_FALSE(( hash_is_avalanching< Z > ));
    BOOST_TEST_TRAIT_FALSE(( hash_is_avalanching< W > ));

    return boost::report_errors();
}
