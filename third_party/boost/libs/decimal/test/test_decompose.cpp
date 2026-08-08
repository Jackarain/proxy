// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <random>

static constexpr std::size_t N = 1024;
static std::mt19937_64 rng {42};

template <typename DecimalType>
void test_zero()
{
    const DecimalType pos_zero {0, 0};
    const auto pos_components {boost::decimal::decompose(pos_zero)};
    BOOST_TEST_EQ(pos_components.sig, 0U);
    BOOST_TEST_EQ(pos_components.sign, false);

    const DecimalType neg_zero {-DecimalType{0, 0}};
    const auto neg_components {boost::decimal::decompose(neg_zero)};
    BOOST_TEST_EQ(neg_components.sig, 0U);
    BOOST_TEST_EQ(neg_components.sign, true);
}

template <typename DecimalType>
void test_sign()
{
    const DecimalType pos {123, 0};
    const auto pos_components {boost::decimal::decompose(pos)};
    BOOST_TEST_EQ(pos_components.sign, false);

    const DecimalType neg {-123, 0};
    const auto neg_components {boost::decimal::decompose(neg)};
    BOOST_TEST_EQ(neg_components.sign, true);

    // Magnitudes are equal regardless of sign
    BOOST_TEST_EQ(pos_components.sig, neg_components.sig);
    BOOST_TEST_EQ(pos_components.exp, neg_components.exp);
}

template <typename DecimalType>
void test_roundtrip()
{
    std::uniform_int_distribution<std::int64_t> sig_dist {1, 9'999'999};
    std::uniform_int_distribution<int> exp_dist {-50, 50};
    std::uniform_int_distribution<int> sign_dist {0, 1};

    for (std::size_t i {}; i < N; ++i)
    {
        const auto sig {sig_dist(rng)};
        const auto exp {exp_dist(rng)};
        const bool neg {sign_dist(rng) == 1};

        const DecimalType val {neg ? -sig : sig, exp};
        const auto components {boost::decimal::decompose(val)};

        const DecimalType reconstructed {components.sig,
                                         components.exp,
                                         components.sign};

        BOOST_TEST_EQ(val, reconstructed);
    }
}

template <typename DecimalType>
void test_ieee_spot()
{
    // For IEEE types the encoded cohort is preserved, so the components carry
    // the exact significand and exponent that were used at construction.
    const DecimalType v1 {1234567, 0};
    const auto c1 {boost::decimal::decompose(v1)};
    BOOST_TEST_EQ(c1.sig, 1234567U);
    BOOST_TEST_EQ(c1.exp, 0);
    BOOST_TEST_EQ(c1.sign, false);

    const DecimalType v2 {1234567, -3};
    const auto c2 {boost::decimal::decompose(v2)};
    BOOST_TEST_EQ(c2.sig, 1234567U);
    BOOST_TEST_EQ(c2.exp, -3);
    BOOST_TEST_EQ(c2.sign, false);

    const DecimalType v3 {-9999999, 5};
    const auto c3 {boost::decimal::decompose(v3)};
    BOOST_TEST_EQ(c3.sig, 9999999U);
    BOOST_TEST_EQ(c3.exp, 5);
    BOOST_TEST_EQ(c3.sign, true);

    // Different cohort representations of the same value yield different
    // components, unlike normalize / frexp10.
    const DecimalType a {1, 0};
    const DecimalType b {10, -1};
    BOOST_TEST_EQ(a, b);
    const auto ca {boost::decimal::decompose(a)};
    const auto cb {boost::decimal::decompose(b)};
    BOOST_TEST_EQ(ca.sig, 1U);
    BOOST_TEST_EQ(ca.exp, 0);
    BOOST_TEST_EQ(cb.sig, 10U);
    BOOST_TEST_EQ(cb.exp, -1);
}

template <typename DecimalType>
void test_fast_spot()
{
    // Fast types normalize their significand in the constructor, so the
    // components always reflect the fully-normalized form regardless of the
    // cohort used at construction.
    const DecimalType a {1, 0};
    const DecimalType b {10, -1};
    BOOST_TEST_EQ(a, b);

    const auto ca {boost::decimal::decompose(a)};
    const auto cb {boost::decimal::decompose(b)};
    BOOST_TEST_EQ(ca.sig, cb.sig);
    BOOST_TEST_EQ(ca.exp, cb.exp);
    BOOST_TEST_EQ(ca.sign, false);
    BOOST_TEST_EQ(cb.sign, false);
}

int main()
{
    test_zero<boost::decimal::decimal32_t>();
    test_zero<boost::decimal::decimal64_t>();
    test_zero<boost::decimal::decimal128_t>();
    test_zero<boost::decimal::decimal_fast32_t>();
    test_zero<boost::decimal::decimal_fast64_t>();
    test_zero<boost::decimal::decimal_fast128_t>();

    test_sign<boost::decimal::decimal32_t>();
    test_sign<boost::decimal::decimal64_t>();
    test_sign<boost::decimal::decimal128_t>();
    test_sign<boost::decimal::decimal_fast32_t>();
    test_sign<boost::decimal::decimal_fast64_t>();
    test_sign<boost::decimal::decimal_fast128_t>();

    test_roundtrip<boost::decimal::decimal32_t>();
    test_roundtrip<boost::decimal::decimal64_t>();
    test_roundtrip<boost::decimal::decimal128_t>();
    test_roundtrip<boost::decimal::decimal_fast32_t>();
    test_roundtrip<boost::decimal::decimal_fast64_t>();
    test_roundtrip<boost::decimal::decimal_fast128_t>();

    test_ieee_spot<boost::decimal::decimal32_t>();
    test_ieee_spot<boost::decimal::decimal64_t>();
    test_ieee_spot<boost::decimal::decimal128_t>();

    test_fast_spot<boost::decimal::decimal_fast32_t>();
    test_fast_spot<boost::decimal::decimal_fast64_t>();
    test_fast_spot<boost::decimal::decimal_fast128_t>();

    return boost::report_errors();
}
