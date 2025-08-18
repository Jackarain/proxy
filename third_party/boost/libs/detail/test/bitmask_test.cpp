/*
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * Copyright (c) 2025 Andrey Semashev
 */

#include <boost/detail/bitmask.hpp>
#include <boost/core/lightweight_test.hpp>

enum unscoped_enum
{
    zero = 0,
    one = 1,
    two = 2,
    four = 4
};

BOOST_BITMASK(unscoped_enum)

void test_unscoped_enum()
{
    BOOST_TEST((one | two) == ((unscoped_enum)(1 | 2)));
    BOOST_TEST((one & two) == ((unscoped_enum)(1 & 2)));
    BOOST_TEST((one ^ two) == ((unscoped_enum)(1 ^ 2)));
    BOOST_TEST((one | one) == ((unscoped_enum)(1 | 1)));
    BOOST_TEST((one & one) == ((unscoped_enum)(1 & 1)));
    BOOST_TEST((one ^ one) == ((unscoped_enum)(1 ^ 1)));
    BOOST_TEST((~one) == ((unscoped_enum)(~1)));
    {
        unscoped_enum val = one;
        val |= two;
        BOOST_TEST(val == ((unscoped_enum)(1 | 2)));
    }
    {
        unscoped_enum val = one;
        val &= two;
        BOOST_TEST(val == ((unscoped_enum)(1 & 2)));
    }
    {
        unscoped_enum val = one;
        val ^= two;
        BOOST_TEST(val == ((unscoped_enum)(1 ^ 2)));
    }
    {
        unscoped_enum val = one;
        val |= one;
        BOOST_TEST(val == ((unscoped_enum)(1 | 1)));
    }
    {
        unscoped_enum val = one;
        val &= one;
        BOOST_TEST(val == ((unscoped_enum)(1 & 1)));
    }
    {
        unscoped_enum val = one;
        val ^= one;
        BOOST_TEST(val == ((unscoped_enum)(1 ^ 1)));
    }
    BOOST_TEST(!zero);
    BOOST_TEST(!!one);
}


enum class scoped_enum
{
    none = 0,
    x = 8,
    y = 16,
    z = 32
};

BOOST_BITMASK(scoped_enum)

void test_scoped_enum()
{
    BOOST_TEST((scoped_enum::x | scoped_enum::y) == ((scoped_enum)(8 | 16)));
    BOOST_TEST((scoped_enum::x & scoped_enum::y) == ((scoped_enum)(8 & 16)));
    BOOST_TEST((scoped_enum::x ^ scoped_enum::y) == ((scoped_enum)(8 ^ 16)));
    BOOST_TEST((scoped_enum::x | scoped_enum::x) == ((scoped_enum)(8 | 8)));
    BOOST_TEST((scoped_enum::x & scoped_enum::x) == ((scoped_enum)(8 & 8)));
    BOOST_TEST((scoped_enum::x ^ scoped_enum::x) == ((scoped_enum)(8 ^ 8)));
    BOOST_TEST((~scoped_enum::x) == ((scoped_enum)(~8)));
    {
        scoped_enum val = scoped_enum::x;
        val |= scoped_enum::y;
        BOOST_TEST(val == ((scoped_enum)(8 | 16)));
    }
    {
        scoped_enum val = scoped_enum::x;
        val &= scoped_enum::y;
        BOOST_TEST(val == ((scoped_enum)(8 & 16)));
    }
    {
        scoped_enum val = scoped_enum::x;
        val ^= scoped_enum::y;
        BOOST_TEST(val == ((scoped_enum)(8 ^ 16)));
    }
    {
        scoped_enum val = scoped_enum::x;
        val |= scoped_enum::x;
        BOOST_TEST(val == ((scoped_enum)(8 | 8)));
    }
    {
        scoped_enum val = scoped_enum::x;
        val &= scoped_enum::x;
        BOOST_TEST(val == ((scoped_enum)(8 & 8)));
    }
    {
        scoped_enum val = scoped_enum::x;
        val ^= scoped_enum::x;
        BOOST_TEST(val == ((scoped_enum)(8 ^ 8)));
    }
    BOOST_TEST(!scoped_enum::none);
    BOOST_TEST(!!scoped_enum::x);
}


namespace my_namespace {

enum class namespaced_enum : unsigned int
{
    empty = 0,
    a = 64,
    b = 128,
    c = 256
};

BOOST_BITMASK(namespaced_enum)

} // namespace my_namespace

void test_namespaced_enum()
{
    BOOST_TEST((my_namespace::namespaced_enum::a | my_namespace::namespaced_enum::b) == ((my_namespace::namespaced_enum)(64u | 128u)));
    BOOST_TEST((my_namespace::namespaced_enum::a & my_namespace::namespaced_enum::b) == ((my_namespace::namespaced_enum)(64u & 128u)));
    BOOST_TEST((my_namespace::namespaced_enum::a ^ my_namespace::namespaced_enum::b) == ((my_namespace::namespaced_enum)(64u ^ 128u)));
    BOOST_TEST((my_namespace::namespaced_enum::a | my_namespace::namespaced_enum::a) == ((my_namespace::namespaced_enum)(64u | 64u)));
    BOOST_TEST((my_namespace::namespaced_enum::a & my_namespace::namespaced_enum::a) == ((my_namespace::namespaced_enum)(64u & 64u)));
    BOOST_TEST((my_namespace::namespaced_enum::a ^ my_namespace::namespaced_enum::a) == ((my_namespace::namespaced_enum)(64u ^ 64u)));
    BOOST_TEST((~my_namespace::namespaced_enum::a) == ((my_namespace::namespaced_enum)(~64u)));
    {
        my_namespace::namespaced_enum val = my_namespace::namespaced_enum::a;
        val |= my_namespace::namespaced_enum::b;
        BOOST_TEST(val == ((my_namespace::namespaced_enum)(64u | 128u)));
    }
    {
        my_namespace::namespaced_enum val = my_namespace::namespaced_enum::a;
        val &= my_namespace::namespaced_enum::b;
        BOOST_TEST(val == ((my_namespace::namespaced_enum)(64u & 128u)));
    }
    {
        my_namespace::namespaced_enum val = my_namespace::namespaced_enum::a;
        val ^= my_namespace::namespaced_enum::b;
        BOOST_TEST(val == ((my_namespace::namespaced_enum)(64u ^ 128u)));
    }
    {
        my_namespace::namespaced_enum val = my_namespace::namespaced_enum::a;
        val |= my_namespace::namespaced_enum::a;
        BOOST_TEST(val == ((my_namespace::namespaced_enum)(64u | 64u)));
    }
    {
        my_namespace::namespaced_enum val = my_namespace::namespaced_enum::a;
        val &= my_namespace::namespaced_enum::a;
        BOOST_TEST(val == ((my_namespace::namespaced_enum)(64u & 64u)));
    }
    {
        my_namespace::namespaced_enum val = my_namespace::namespaced_enum::a;
        val ^= my_namespace::namespaced_enum::a;
        BOOST_TEST(val == ((my_namespace::namespaced_enum)(64u ^ 64u)));
    }
    BOOST_TEST(!my_namespace::namespaced_enum::empty);
    BOOST_TEST(!!my_namespace::namespaced_enum::a);
}

int main()
{
    test_unscoped_enum();
    test_scoped_enum();
    test_namespaced_enum();

    return boost::report_errors();
}
