// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <array>
#include <limits>
#include <cstdint>

template <typename T>
void test()
{
    constexpr std::array<std::uint64_t, 20> powers_of_10 =
    {{
             UINT64_C(1), UINT64_C(10), UINT64_C(100), UINT64_C(1000), UINT64_C(10000), UINT64_C(100000), UINT64_C(1000000),
             UINT64_C(10000000), UINT64_C(100000000), UINT64_C(1000000000), UINT64_C(10000000000), UINT64_C(100000000000),
             UINT64_C(1000000000000), UINT64_C(10000000000000), UINT64_C(100000000000000), UINT64_C(1000000000000000),
             UINT64_C(10000000000000000), UINT64_C(100000000000000000), UINT64_C(1000000000000000000), UINT64_C(10000000000000000000)
     }};

    for (const auto& val : powers_of_10)
    {
        if (val < std::numeric_limits<T>::max())
        {
            const auto temp {boost::decimal::detail::remove_trailing_zeros(static_cast<T>(val))};
            if (!BOOST_TEST_EQ(temp.trimmed_number, T(1)))
            {
                // LCOV_EXCL_START
                std::cerr << "Input Number: " << val
                          << "\nOutput Number: " << temp.trimmed_number
                          << "\nZeros removed: " << temp.number_of_removed_zeros << std::endl;
                // LCOV_EXCL_STOP
            }
        }
    }
}

void test_extended()
{
    using namespace boost::decimal;
    const auto powers_of_10 = detail::impl::BOOST_DECIMAL_DETAIL_INT128_pow10;

    for (std::size_t i {}; i < 39; ++i)
    {
        const auto val = powers_of_10[i];
        const auto temp {boost::decimal::detail::remove_trailing_zeros(val)};
        if (!BOOST_TEST_EQ(temp.trimmed_number, boost::int128::uint128_t(1)))
        {
            // LCOV_EXCL_START
            std::cerr << "Input Number: " << val
                      << "\nOutput Number: " << temp.trimmed_number
                      << "\nZeros removed: " << temp.number_of_removed_zeros << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

int main()
{
    test<std::uint32_t>();
    test<std::uint64_t>();
    test<boost::int128::uint128_t>();

    test_extended();

    return boost::report_errors();
}
