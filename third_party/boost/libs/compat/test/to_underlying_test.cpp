// Copyright 2025 Braden Ganetsky
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/compat/to_underlying.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <cstdint>

enum unscoped
{
    unscoped_value = 1
};

enum unscoped_int8_t : std::int8_t
{
    unscoped_int8_t_value = 2
};

enum unscoped_uint16_t : std::uint16_t
{
    unscoped_uint16_t_value = 3
};

enum class scoped
{
    value = 4
};

enum class scoped_int8_t : std::int8_t
{
    value = 5
};

enum class scoped_uint16_t : std::uint16_t
{
    value = 6
};

int main()
{
    {
        auto output = boost::compat::to_underlying(unscoped_value);
        // Per the standard [dcl.enum], the underlying type of an unscoped enumeration
        // is unspecified, unless it is explicitly specified.
        BOOST_TEST_EQ(output, 1);
    }
    {
        auto output = boost::compat::to_underlying(unscoped_int8_t_value);
        BOOST_TEST_TRAIT_SAME(decltype(output), std::int8_t);
        BOOST_TEST_EQ(output, 2);
    }
    {
        auto output = boost::compat::to_underlying(unscoped_uint16_t_value);
        BOOST_TEST_TRAIT_SAME(decltype(output), std::uint16_t);
        BOOST_TEST_EQ(output, 3);
    }
    {
        auto output = boost::compat::to_underlying(scoped::value);
        BOOST_TEST_TRAIT_SAME(decltype(output), int);
        BOOST_TEST_EQ(output, 4);
    }
    {
        auto output = boost::compat::to_underlying(scoped_int8_t::value);
        BOOST_TEST_TRAIT_SAME(decltype(output), std::int8_t);
        BOOST_TEST_EQ(output, 5);
    }
    {
        auto output = boost::compat::to_underlying(scoped_uint16_t::value);
        BOOST_TEST_TRAIT_SAME(decltype(output), std::uint16_t);
        BOOST_TEST_EQ(output, 6);
    }

    return boost::report_errors();
}
