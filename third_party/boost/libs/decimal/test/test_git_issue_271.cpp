// Copyright 2023 Matt Borland
// Copyright 2023 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <iomanip>
#include <iostream>
#include <random>
#include <boost/core/lightweight_test.hpp>
#include <boost/decimal.hpp>

void simple_test()
{
    // a_flt  : -9.000000020492800e+00

    using decimal_type = boost::decimal::decimal64_t;
    using float_type   = double;

    const auto a_flt0 = static_cast<float_type>(9.000000020492800e+00L);
    const auto a_flt1 = static_cast<float_type>(9.000002004928000e+00L);

    const auto a_dec0 = static_cast<decimal_type>(a_flt0);
    const auto a_dec1 = static_cast<decimal_type>(a_flt1);

    const auto floor_check0 = !(floor(a_dec0) == a_dec0); // Why is this truncated to exactly 9?
    const auto floor_check1 = !(floor(a_dec1) == a_dec1);

    const auto flg = std::cout.flags();

    // I believe that both of these should be false.
    // Is floor or ceil truncating at 32-bits?
    if (!BOOST_TEST(floor_check0) || !BOOST_TEST(floor_check1))
    {
        // LCOV_EXCL_START
        std::cout << "floor_check0: " << std::boolalpha << floor_check0 << std::endl;
        std::cout << "floor_check1: " << std::boolalpha << floor_check1 << std::endl;

        // Note: The problem is not at construction.
        std::cout << "a_dec0: " << std::setprecision(std::numeric_limits<decimal_type>::digits10) << a_dec0
                  << "\nfloor(a_dec0):" << floor(a_dec0) << std::endl;
        std::cout << "a_dec1: " << std::setprecision(std::numeric_limits<decimal_type>::digits10) << a_dec1
                  << "\nfloor(a_dec1):" << floor(a_dec1) << std::endl;

        std::cout.flags(flg);
        // LCOV_EXCL_STOP
    }
}

void random_test()
{
    using decimal_type = boost::decimal::decimal64_t;
    using float_type   = double;
    
    std::mt19937_64 gen(42);

    auto dist_a =
            std::uniform_real_distribution<float_type>
                    {
                            static_cast<float_type>(9.000000020492800e+00L),
                            static_cast<float_type>(9.000002004928000e+00L)
                    };

    const auto flg = std::cout.flags();

    for(auto index = 0U; index < 16U; ++index)
    {
        static_cast<void>(index);

        const auto a_flt = dist_a(gen);
        const auto a_dec = static_cast<decimal_type>(a_flt);

        const auto floor_check = !(floor(a_dec) == a_dec); // Is this truncated?

        // Is floor or ceil truncating at 32-bits?
        if (!BOOST_TEST(floor_check))
        {
            // LCOV_EXCL_START
            std::cout << "a_dec0: " << std::setprecision(std::numeric_limits<decimal_type>::digits10) << a_dec
                      << ", floor_check: " << std::boolalpha << floor_check << std::endl;

            std::cout.flags(flg);
            // LCOV_EXCL_STOP
        }
    }
}

int main()
{
    simple_test();
    random_test();

    return boost::report_errors();
}
