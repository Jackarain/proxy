// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <array>

#if defined(__GNUC__) && __GNUC__ >= 8
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

using namespace boost::decimal;

template <typename ResultsType, typename StringsType>
void test_to_chars_scientific(const ResultsType& decimals, const StringsType& strings)
{
    for (std::size_t i {}; i < decimals.size(); ++i)
    {
        for (std::size_t j {}; j < decimals.size(); ++j)
        {
            BOOST_TEST_EQ(decimals[i], decimals[j]);
        }
    }

    for (std::size_t i {}; i < decimals.size(); ++i)
    {
        char buffer[64] {};
        const auto r {to_chars(buffer, buffer + sizeof(buffer), decimals[i], chars_format::cohort_preserving_scientific)};
        BOOST_TEST(r);
        *r.ptr = '\0';

        BOOST_TEST_CSTR_EQ(buffer, strings[i]);
    }
}

// The cohorts will compare equal regardless so here we check bit-wise equality to be a successful roundtrip
template <typename T, std::size_t N, typename StringsType>
void test_roundtrip(const std::array<T, N>& decimals, const std::array<StringsType, N>& strings)
{
    using bit_type = std::conditional_t<std::is_same<T, decimal32_t>::value, std::uint32_t,
                        std::conditional_t<std::is_same<T, decimal64_t>::value, std::uint64_t, boost::int128::uint128_t>>;

    for (std::size_t i {}; i < decimals.size(); ++i)
    {
        bit_type initial_bits;
        std::memcpy(&initial_bits, &decimals[i], sizeof(initial_bits));

        char buffer[64] {};
        const auto r {to_chars(buffer, buffer + sizeof(buffer), decimals[i], chars_format::cohort_preserving_scientific)};
        BOOST_TEST(r);
        *r.ptr = '\0';
        BOOST_TEST_CSTR_EQ(buffer, strings[i]);

        T return_val;
        const auto return_r {from_chars(buffer, buffer + sizeof(buffer), return_val, chars_format::cohort_preserving_scientific)};
        BOOST_TEST(return_r);

        bit_type return_bits;
        std::memcpy(&return_bits, &return_val, sizeof(return_bits));

        BOOST_TEST_EQ(initial_bits, return_bits);
    }
}

template <typename TargetDecimalType, typename T, std::size_t N>
void test_invalid_values(const std::array<T, N>& strings)
{
    for (std::size_t i {}; i < strings.size(); ++i)
    {
        TargetDecimalType val;
        const auto r {from_chars(strings[i], strings[i] + sizeof(strings[i]), val, chars_format::cohort_preserving_scientific)};
        BOOST_TEST(!r);
    }
}

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4127)
#endif

template <typename T, std::size_t N>
void test_invalid_to_chars(const std::array<T, N>& decimals)
{
    BOOST_DECIMAL_IF_CONSTEXPR (detail::is_fast_type_v<T>)
    {
        for (std::size_t i {}; i < decimals.size(); ++i)
        {
            char buffer[64] {};
            const auto r {to_chars(buffer, buffer + sizeof(buffer), decimals[i], chars_format::cohort_preserving_scientific)};
            BOOST_TEST(!r);
        }
    }

    for (std::size_t i {}; i < decimals.size(); ++i)
    {
        char buffer[64] {};
        const auto r {to_chars(buffer, buffer + sizeof(buffer), decimals[i], chars_format::cohort_preserving_scientific, 5)};
        BOOST_TEST(!r);
    }
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

template <typename T>
const std::array<T, 7> decimals = {
    T{3, 2},
    T{30, 1},
    T{300, 0},
    T{3000, -1},
    T{30000, -2},
    T{300000, -3},
    T{3000000, -4},
};

constexpr std::array<const char*, 7> strings = {
    "3e+02",
    "3.0e+02",
    "3.00e+02",
    "3.000e+02",
    "3.0000e+02",
    "3.00000e+02",
    "3.000000e+02",
};

template <typename T>
const std::array<T, 6> decimals_with_exp = {
    T {42, 50},
    T {420, 49},
    T {4200, 48},
    T {42000, 47},
    T {420000, 46},
    T {4200000, 45}
};

constexpr std::array<const char*, 6> decimals_with_exp_strings = {
    "4.2e+51",
    "4.20e+51",
    "4.200e+51",
    "4.2000e+51",
    "4.20000e+51",
    "4.200000e+51",
};

template <typename T>
const std::array<T, 5> negative_values = {
    T {-321, -49},
    T {-3210, -50},
    T {-32100, -51},
    T {-321000, -52},
    T {-3210000, -53}
};

constexpr std::array<const char*, 5> negative_values_strings = {
    "-3.21e-47",
    "-3.210e-47",
    "-3.2100e-47",
    "-3.21000e-47",
    "-3.210000e-47"
};

constexpr std::array<const char*, 3> invalid_decimal32_strings = {
    "+3.2e+20",
    "3.421",
    "9.999999999999999e+05",
};

int main()
{
    test_to_chars_scientific(decimals<decimal32_t>, strings);
    test_to_chars_scientific(decimals<decimal64_t>, strings);
    test_to_chars_scientific(decimals<decimal128_t>, strings);

    test_to_chars_scientific(decimals_with_exp<decimal32_t>, decimals_with_exp_strings);
    test_to_chars_scientific(decimals_with_exp<decimal64_t>, decimals_with_exp_strings);
    test_to_chars_scientific(decimals_with_exp<decimal128_t>, decimals_with_exp_strings);

    test_to_chars_scientific(negative_values<decimal32_t>, negative_values_strings);
    test_to_chars_scientific(negative_values<decimal64_t>, negative_values_strings);
    test_to_chars_scientific(negative_values<decimal128_t>, negative_values_strings);

    test_roundtrip(decimals<decimal32_t>, strings);
    test_roundtrip(decimals<decimal64_t>, strings);
    test_roundtrip(decimals<decimal128_t>, strings);

    test_roundtrip(decimals_with_exp<decimal32_t>, decimals_with_exp_strings);
    test_roundtrip(decimals_with_exp<decimal64_t>, decimals_with_exp_strings);
    test_roundtrip(decimals_with_exp<decimal128_t>, decimals_with_exp_strings);

    test_roundtrip(negative_values<decimal32_t>, negative_values_strings);
    test_roundtrip(negative_values<decimal64_t>, negative_values_strings);
    test_roundtrip(negative_values<decimal128_t>, negative_values_strings);

    test_invalid_values<decimal32_t>(invalid_decimal32_strings);

    // Every value for fast types are invalid
    test_invalid_values<decimal_fast32_t>(strings);
    test_invalid_values<decimal_fast64_t>(decimals_with_exp_strings);
    test_invalid_values<decimal_fast128_t>(negative_values_strings);
    test_invalid_to_chars(decimals<decimal_fast32_t>);
    test_invalid_to_chars(decimals<decimal_fast64_t>);
    test_invalid_to_chars(decimals<decimal_fast128_t>);

    // Specified precision is not allowed
    test_invalid_to_chars(decimals<decimal32_t>);
    test_invalid_to_chars(decimals<decimal64_t>);
    test_invalid_to_chars(decimals<decimal128_t>);

    return boost::report_errors();
}
