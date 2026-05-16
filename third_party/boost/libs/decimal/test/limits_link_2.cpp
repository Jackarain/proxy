// Copyright 2023 Peter Dimov
// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <limits>

void test_odr_use( int const* );
void test_odr_use( std::size_t const* );

template<typename T> void test()
{
    test_odr_use( &boost::decimal::formatting_limits<T>::scientific_format_max_chars );
    test_odr_use( &boost::decimal::formatting_limits<T>::fixed_format_max_chars );
    test_odr_use( &boost::decimal::formatting_limits<T>::hex_format_max_chars );
    test_odr_use( &boost::decimal::formatting_limits<T>::cohort_preserving_scientific_max_chars );
    test_odr_use( &boost::decimal::formatting_limits<T>::general_format_max_chars );
    test_odr_use( &std::numeric_limits<T>::digits10 );
}

template <typename T> void test_numbers()
{
    test_odr_use( &std::numeric_limits<T>::digits10 );
}

void f2()
{
    test<boost::decimal::decimal32_t>();
    test<boost::decimal::decimal64_t>();
    test<boost::decimal::decimal128_t>();

    test<boost::decimal::decimal_fast32_t>();
    test<boost::decimal::decimal_fast64_t>();
    test<boost::decimal::decimal_fast128_t>();

    test_numbers<boost::decimal::uint128_t>();
    test_numbers<boost::decimal::detail::u256>();
}
