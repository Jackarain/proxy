// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstring>
#include <random>

static constexpr std::size_t N = 1024;
static std::mt19937_64 rng {42};

template <typename DecimalType>
void fast_test()
{
    std::uniform_int_distribution<std::int64_t> dist {INT64_MIN, INT64_MAX};

    for (std::size_t i {}; i < N; ++i)
    {
        const DecimalType val {dist(rng)};
        const DecimalType normalized_val {boost::decimal::normalize(val)};
        BOOST_TEST_EQ(val, normalized_val);
    }
}

#if defined(__GNUC__) && __GNUC__ >= 8
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

template <typename DecimalType>
void ieee_test()
{
    using integer_type = std::conditional_t<std::is_same<DecimalType, boost::decimal::decimal32_t>::value, std::uint32_t,
                         std::conditional_t<std::is_same<DecimalType, boost::decimal::decimal64_t>::value, std::uint64_t, boost::int128::uint128_t>>;

    DecimalType val1 {1, 0};
    val1 = boost::decimal::normalize(val1);

    DecimalType val2 {10, -1};
    val2 = boost::decimal::normalize(val2);

    DecimalType val3 {100, -2};
    val3 = boost::decimal::normalize(val3);

    DecimalType val4 {1000, -3};
    val4 = boost::decimal::normalize(val4);

    DecimalType val5 {10000, -4};
    val5 = boost::decimal::normalize(val5);

    DecimalType val6 {100000, -5};
    val6 = boost::decimal::normalize(val6);

    DecimalType val7 {1000000, -6};
    val7 = boost::decimal::normalize(val7);

    integer_type int1, int2, int3, int4, int5, int6, int7;
    std::memcpy(&int1, &val1, sizeof(integer_type));
    std::memcpy(&int2, &val2, sizeof(integer_type));
    std::memcpy(&int3, &val3, sizeof(integer_type));
    std::memcpy(&int4, &val4, sizeof(integer_type));
    std::memcpy(&int5, &val5, sizeof(integer_type));
    std::memcpy(&int6, &val6, sizeof(integer_type));
    std::memcpy(&int7, &val7, sizeof(integer_type));
    const std::array<integer_type, 7> vals {int1, int2, int3, int4, int5, int6, int7};
    const std::array<DecimalType, 7> dec_vals {val1, val2, val3, val4, val5, val6, val7};

    for (std::size_t i {}; i < vals.size() - 1; ++i)
    {
        for (std::size_t j {i}; j < vals.size(); ++j)
        {
            BOOST_TEST_EQ(vals[i], vals[j]);
            BOOST_TEST(std::memcmp(&dec_vals[i], &dec_vals[j], sizeof(DecimalType)) == 0);
        }
    }

    std::uniform_int_distribution<std::int64_t> dist {INT64_MIN, INT64_MAX};

    for (std::size_t i {}; i < N; ++i)
    {
        const DecimalType val {dist(rng)};
        const DecimalType normalized_val {boost::decimal::normalize(val)};
        BOOST_TEST_EQ(val, normalized_val);
    }
}

#if defined(__GNUC__) && __GNUC__ >= 8
#  pragma GCC diagnostic pop
#endif

int main()
{
    fast_test<boost::decimal::decimal_fast32_t>();
    fast_test<boost::decimal::decimal_fast64_t>();
    fast_test<boost::decimal::decimal_fast128_t>();

    ieee_test<boost::decimal::decimal32_t>();
    ieee_test<boost::decimal::decimal64_t>();
    ieee_test<boost::decimal::decimal128_t>();

    return boost::report_errors();
}
