// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/911

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstring>

#if defined(__GNUC__) && __GNUC__ >= 8
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

void test_issue()
{
    // -7.50
    constexpr boost::decimal::decimal128_t test_val {-750, -2};
    boost::int128::uint128_t bits {};
    std::memcpy(&bits, &test_val, sizeof(bits));

    BOOST_TEST_EQ(bits.low, UINT64_C(750));
    BOOST_TEST_EQ(bits.high, UINT64_C(12699025049277956096));
}

#ifdef BOOST_DECIMAL_TEST_BSON

#if __has_include(<bson-2.1.0/bson/bson.h>)
#include <bson-2.1.0/bson/bson.h>
#include <random>
#include <string>

void test_random_spots()
{
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<std::int64_t> integer_part_dist(-100, 100);
    std::uniform_int_distribution<std::uint64_t> fractional_part_dist(UINT64_C(0), UINT64_MAX);

    for (std::size_t i {}; i < 1024; ++i)
    {
        const auto integer_part {integer_part_dist(rng)};
        const auto fractional_part {fractional_part_dist(rng)};

        const auto integer_string {std::to_string(integer_part)};
        const auto fractional_string {std::to_string(fractional_part)};

        const auto full_string {integer_string + "." + fractional_string};

        bson_decimal128_t bson_res;
        bson_decimal128_from_string(full_string.c_str(), &bson_res);

        boost::decimal::decimal128_t boost_res;
        const auto r = boost::decimal::from_chars(full_string, boost_res);

        BOOST_TEST(r);

        boost::int128::uint128_t bson_bits;
        std::memcpy(&bson_bits, &bson_res, sizeof(bson_bits));

        boost::int128::uint128_t boost_bits;
        std::memcpy(&boost_bits, &boost_res, sizeof(boost_bits));

        BOOST_TEST_EQ(bson_bits.low, boost_bits.low);
        BOOST_TEST_EQ(bson_bits.high, boost_bits.high);
    }
}

void test_rounded_spots()
{
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<std::int64_t> integer_part_dist(0, 100);
    std::uniform_int_distribution<std::uint64_t> fractional_part_dist(UINT64_C(0), UINT64_MAX);

    for (std::size_t i {}; i < 1024; ++i)
    {
        const auto integer_part {integer_part_dist(rng)};
        const auto fractional_part {fractional_part_dist(rng)};

        const auto integer_string {std::to_string(integer_part)};
        const auto fractional_string {std::to_string(fractional_part)};

        // We are going to force the value to always have 34 significant digits and end in a bunch of 9s
        // This should force the usage of the combination field if we're going to use it
        auto full_string {integer_string + "." + fractional_string + "9999999999"};
        auto current_length {static_cast<int>(full_string.length())};
        while (current_length < std::numeric_limits<boost::decimal::decimal128_t>::digits10 + 1)
        {
            full_string.append("9");
            ++current_length;
        }

        bson_decimal128_t bson_res;
        bson_decimal128_from_string(full_string.c_str(), &bson_res);

        boost::decimal::decimal128_t boost_res;
        const auto r = boost::decimal::from_chars(full_string, boost_res);

        BOOST_TEST(r);

        boost::int128::uint128_t bson_bits;
        std::memcpy(&bson_bits, &bson_res, sizeof(bson_bits));

        boost::int128::uint128_t boost_bits;
        std::memcpy(&boost_bits, &boost_res, sizeof(boost_bits));

        BOOST_TEST_EQ(bson_bits.low, boost_bits.low);
        BOOST_TEST_EQ(bson_bits.high, boost_bits.high);
    }
}

int main()
{
    test_issue();
    test_random_spots();
    test_rounded_spots();

    return boost::report_errors();
}

#else

int main()
{
    test_issue();

    return boost::report_errors();
}

#endif

#else

int main()
{
    test_issue();

    return boost::report_errors();
}

#endif
