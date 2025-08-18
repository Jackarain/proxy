// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/random/issues/133

#include <boost/core/lightweight_test.hpp>
#include <boost/random/beta_distribution.hpp>
#include <random>
#include <cmath>

int main()
{
    constexpr double beta_param = 0.0020368700639848774;
    boost::random::beta_distribution<double> dist(beta_param, beta_param);
    std::mt19937_64 gen(12345);

    for (int i = 0; i < 10000; ++i)
    {
        const double Z = dist(gen);
        BOOST_TEST(!std::isnan(Z));
    }

    return boost::report_errors();
}
