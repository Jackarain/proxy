// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This example demonstrates how to perform statistics using boost.math

// Needed for operations with boost math
#define BOOST_DECIMAL_ALLOW_IMPLICIT_INTEGER_CONVERSIONS

#include "where_file.hpp"
#include <boost/decimal/decimal64_t.hpp>    // For type decimal64_t
#include <boost/decimal/charconv.hpp>       // For from_chars
#include <boost/decimal/iostream.hpp>       // Decimal support to <iostream> and <iomanip>
#include <boost/decimal/cmath.hpp>          // For sqrt of decimal types
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

// Warning suppression for boost.math
// Boost.decimal is tested with -Werror -Wall -Wextra and a few other additional flags
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic ignored "-Wstring-conversion"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include <boost/math/statistics/univariate_statistics.hpp>

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

using boost::decimal::decimal64_t;

// This struct holds all the information that is provided
// for a single trading day
struct daily_data
{
    std::string date;
    decimal64_t open;
    decimal64_t high;
    decimal64_t low;
    decimal64_t close;
    decimal64_t volume;
};

auto parse_csv_line(const std::string& line) -> daily_data
{
    std::stringstream ss(line);
    std::string token;
    daily_data data;

    // Parse each column
    std::getline(ss, data.date, ',');
    std::getline(ss, token, ',');
    from_chars(token, data.open);

    std::getline(ss, token, ',');
    from_chars(token, data.high);

    std::getline(ss, token, ',');
    from_chars(token, data.low);

    std::getline(ss, token, ',');
    from_chars(token, data.close);

    std::getline(ss, token, ',');
    from_chars(token, data.volume);

    return data;
}

int main()
{
    // The first few lines of this file are similar to the previous example
    // in that we parse a single year of AAPL stock data before we can do anything useful with

    std::vector<daily_data> stock_data;

    // Open and read the CSV file
    std::ifstream file(boost::decimal::where_file("AAPL.csv"));
    std::string line;

    // Skip header line
    std::getline(file, line);

    // Read data
    while (std::getline(file, line))
    {
        stock_data.push_back(parse_csv_line(line));
    }

    // Get the closing prices for the entire year
    std::vector<decimal64_t> closing_prices;
    for (const auto& day : stock_data)
    {
        closing_prices.emplace_back(day.close);
    }

    // Here we use Boost.Math's statistics facilities
    // As shown at the top of the file you will need to define BOOST_DECIMAL_ALLOW_IMPLICIT_INTEGER_CONVERSIONS,
    // and suppress a few warnings to make this build cleanly
    const decimal64_t mean_closing_price = boost::math::statistics::mean(closing_prices);
    const decimal64_t median_closing_price = boost::math::statistics::median(closing_prices);
    const decimal64_t variance_closing_price = boost::math::statistics::variance(closing_prices);
    const decimal64_t std_dev_closing_price = boost::decimal::sqrt(variance_closing_price);

    // 2-Sigma Bollinger Bands
    // These are of a single point in time rather than making a plot over time for simplicity
    const decimal64_t upper_band = mean_closing_price + 2 * std_dev_closing_price;
    const decimal64_t lower_band = mean_closing_price - 2 * std_dev_closing_price;

    std::cout << std::fixed << std::setprecision(2)
              << "  Mean Closing Price: $" << mean_closing_price << '\n'
              << "Median Closing Price: $" << median_closing_price << '\n'
              << "  Standard Deviation: $" << std_dev_closing_price << '\n'
              << "Upper Bollinger Band: $" << upper_band << '\n'
              << "Lower Bollinger Band: $" << lower_band << std::endl;
}
