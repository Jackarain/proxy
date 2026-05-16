// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <random>
#include <string>

using namespace boost::decimal;

static std::mt19937_64 rng {42};
static constexpr std::size_t N {1024};

template <typename T>
T recover_value(const std::string& str, std::size_t* ptr);

template <>
decimal32_t recover_value<decimal32_t>(const std::string& str, std::size_t* ptr)
{
    return stod32(str, ptr);
}

template <>
decimal_fast32_t recover_value<decimal_fast32_t>(const std::string& str, std::size_t* ptr)
{
    return stod32f(str, ptr);
}

template <>
decimal64_t recover_value<decimal64_t>(const std::string& str, std::size_t* ptr)
{
    return stod64(str, ptr);
}

template <>
decimal_fast64_t recover_value<decimal_fast64_t>(const std::string& str, std::size_t* ptr)
{
    return stod64f(str, ptr);
}

template <>
decimal128_t recover_value<decimal128_t>(const std::string& str, std::size_t* ptr)
{
    return stod128(str, ptr);
}

template <>
decimal_fast128_t recover_value<decimal_fast128_t>(const std::string& str, std::size_t* ptr)
{
    return stod128f(str, ptr);
}

template <typename T>
void test()
{
    std::uniform_int_distribution<std::int32_t> sig_dist {-9'999'999, 9'999'999};
    std::uniform_int_distribution<std::int32_t> exp_dist {-50, 50};

    for (std::size_t i {}; i < N; ++i)
    {
        const T val {sig_dist(rng), exp_dist(rng)};

        char buffer[64];
        const auto r {to_chars(buffer, buffer + sizeof(buffer), val)};
        BOOST_TEST(r);
        const auto dist {static_cast<std::size_t>(r.ptr - buffer)};

        *r.ptr = '\0';
        const std::string str {buffer};
        std::size_t idx {};
        const T return_value {recover_value<T>(str, &idx)};

        BOOST_TEST_EQ(return_value, val);
        BOOST_TEST_EQ(idx, dist);
    }
}

inline void test_overflow_path()
{
    const std::string str {"INF"};

    #ifndef BOOST_DECIMAL_DISABLE_EXCEPTIONS

    BOOST_TEST_THROWS(recover_value<decimal32_t>(str, nullptr), std::out_of_range);

    #else

    BOOST_TEST(isnan(recover_value<decimal32_t>(str, nullptr)));

    #endif
}

inline void test_invalid_path()
{
    const std::string str {"JUNK"};

    #ifndef BOOST_DECIMAL_DISABLE_EXCEPTIONS

    BOOST_TEST_THROWS(recover_value<decimal32_t>(str, nullptr), std::invalid_argument);

    #else

    BOOST_TEST(isnan(recover_value<decimal32_t>(str, nullptr)));

    #endif
}

int main()
{
    test<decimal32_t>();
    test<decimal_fast32_t>();
    test<decimal64_t>();
    test<decimal_fast64_t>();
    test<decimal128_t>();
    test<decimal_fast128_t>();

    test_overflow_path();
    test_invalid_path();

    return boost::report_errors();
}

