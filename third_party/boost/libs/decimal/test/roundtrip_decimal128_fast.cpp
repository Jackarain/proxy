// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "mini_to_chars.hpp"
#include "testing_config.hpp"
#include <boost/decimal.hpp>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wold-style-cast"
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  if __clang_major__ >= 20
#    pragma clang diagnostic ignored "-Wfortify-source"
#  endif
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wundef"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wuseless-cast"
#endif

#include <boost/math/special_functions/next.hpp>
#include <boost/core/lightweight_test.hpp>

#include <limits>
#include <random>
#include <sstream>
#include <iomanip>
#include <cerrno>

using namespace boost::decimal;

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
static constexpr auto N = static_cast<std::size_t>(1024); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(1024 >> 4U); // Number of trials
#endif

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4146)
#endif

template <typename T>
void test_conversion_to_integer()
{
    errno = 0;
    constexpr decimal_fast128_t one(1, 0);
    constexpr decimal_fast128_t zero(0, 0);
    constexpr decimal_fast128_t half(5, -1);
    BOOST_TEST_EQ(static_cast<T>(one), static_cast<T>(1)) && BOOST_TEST_EQ(errno, 0);
    BOOST_TEST_EQ(static_cast<T>(zero), static_cast<T>(0));

    BOOST_IF_CONSTEXPR (std::is_signed<T>::value)
    {
        BOOST_TEST_EQ(static_cast<T>(-one), static_cast<T>(-1)) && BOOST_TEST_EQ(errno, 0);
    }
    else
    {
        // Bad conversion so we use zero
        BOOST_TEST_EQ(static_cast<T>(-one), std::numeric_limits<T>::max()) && BOOST_TEST_EQ(errno, ERANGE);
    }

    errno = 0;
    BOOST_TEST_EQ(static_cast<T>(std::numeric_limits<decimal_fast128_t>::infinity()), std::numeric_limits<T>::max()) && BOOST_TEST_EQ(errno, ERANGE);

    errno = 0;
    BOOST_TEST_EQ(static_cast<T>(-std::numeric_limits<decimal_fast128_t>::infinity()), std::numeric_limits<T>::max()) && BOOST_TEST_EQ(errno, ERANGE);

    errno = 0;
    BOOST_TEST_EQ(static_cast<T>(std::numeric_limits<decimal_fast128_t>::quiet_NaN()), std::numeric_limits<T>::max()) && BOOST_TEST_EQ(errno, EINVAL);

    errno = 0;
    BOOST_TEST_EQ(static_cast<T>(std::numeric_limits<decimal_fast128_t>::signaling_NaN()), std::numeric_limits<T>::max()) && BOOST_TEST_EQ(errno, EINVAL);

    errno = 0;
    BOOST_TEST_EQ(static_cast<T>(half), static_cast<T>(0)) && BOOST_TEST_EQ(errno, 0);

    constexpr decimal_fast128_t one_e_8(1, 8);
    BOOST_TEST_EQ(static_cast<T>(one_e_8), static_cast<T>(100'000'000)) && BOOST_TEST_EQ(errno, 0);

    constexpr decimal_fast128_t one_e_8_2(1'000'000, 2);
    BOOST_TEST_EQ(static_cast<T>(one_e_8_2), static_cast<T>(100'000'000)) && BOOST_TEST_EQ(errno, 0);

    // Edge case
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> dist(-100, -20);
    errno = 0;
    BOOST_TEST_EQ(static_cast<unsigned>(decimal_fast128_t(dist(rng))), std::numeric_limits<unsigned>::max()) && BOOST_TEST_EQ(errno, ERANGE);

    errno = 0;
    BOOST_TEST_EQ(static_cast<unsigned long>(decimal_fast128_t(dist(rng))), std::numeric_limits<unsigned long>::max()) && BOOST_TEST_EQ(errno, ERANGE);

    errno = 0;
    BOOST_TEST_EQ(static_cast<unsigned long long>(decimal_fast128_t(dist(rng))), std::numeric_limits<unsigned long long>::max()) && BOOST_TEST_EQ(errno, ERANGE);
}

template <typename T>
void test_roundtrip_conversion_integer(T min = T(0), T max = T(detail::max_significand))
{
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<T> dist(min, max);

    for (std::size_t i = 0; i < N; ++i)
    {
        const T val = dist(rng);
        const decimal_fast128_t initial_decimal(val);
        const T return_val (initial_decimal);
        const decimal_fast128_t return_decimal(return_val);

        BOOST_TEST_EQ(val, return_val);
        BOOST_TEST_EQ(initial_decimal, return_decimal);
    }

    // These will have loss of precision for the integer,
    // but should roundtrip for the decimal part
    std::uniform_int_distribution<T> big_dist((std::numeric_limits<T>::min)(), (std::numeric_limits<T>::max)());

    for (std::size_t i = 0; i < N; ++i)
    {
        const T val = dist(rng);
        const decimal_fast128_t initial_decimal(val);
        const T return_val (initial_decimal);
        const decimal_fast128_t return_decimal(return_val);

        BOOST_TEST_EQ(initial_decimal, return_decimal);
    }
}

template <typename T>
void test_conversion_from_float()
{
    errno = 0;

    auto half {static_cast<T>(0.5)};
    decimal_fast128_t dec_half {5, -1};
    BOOST_TEST_EQ(decimal_fast128_t(half), dec_half) && BOOST_TEST_EQ(errno, 0);
    BOOST_TEST_EQ(decimal_fast128_t(-half), -dec_half) && BOOST_TEST_EQ(errno, 0);

    BOOST_TEST(isnan(decimal_fast128_t(std::numeric_limits<T>::quiet_NaN())));
    BOOST_TEST(isnan(decimal_fast128_t(std::numeric_limits<T>::signaling_NaN())));
    BOOST_TEST(isinf(decimal_fast128_t(std::numeric_limits<T>::infinity())));
    BOOST_TEST(isinf(decimal_fast128_t(-std::numeric_limits<T>::infinity())));
}

template <typename T>
void test_conversion_to_float()
{
    errno = 0;

    constexpr decimal_fast128_t half(5, -1);
    BOOST_TEST_EQ(static_cast<T>(half), T(0.5)) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    BOOST_TEST_EQ(static_cast<T>(std::numeric_limits<decimal_fast128_t>::infinity()), std::numeric_limits<T>::infinity()) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    BOOST_TEST_EQ(static_cast<T>(-std::numeric_limits<decimal_fast128_t>::infinity()), std::numeric_limits<T>::infinity()) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    BOOST_TEST(static_cast<T>(std::numeric_limits<decimal_fast128_t>::quiet_NaN()) != std::numeric_limits<T>::quiet_NaN()) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    BOOST_TEST(static_cast<T>(std::numeric_limits<decimal_fast128_t>::signaling_NaN()) != std::numeric_limits<T>::signaling_NaN()) && BOOST_TEST_EQ(errno, 0);
}

template <typename T>
void test_roundtrip_conversion_float()
{
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<T> dist(T(0), (std::numeric_limits<T>::max)());

    for (std::size_t i = 0; i < N; ++i)
    {
        const T val {dist(rng)};
        const decimal_fast128_t initial_decimal(val);
        const T return_val {static_cast<T>(initial_decimal)};
        const decimal_fast128_t return_decimal {return_val};

        if (!BOOST_TEST_EQ(initial_decimal, return_decimal))
        {
            // LCOV_EXCL_START
            std::cerr << std::setprecision(std::numeric_limits<T>::digits10)
                      << "Val: " << val
                      << "\nDec: " << initial_decimal
                      << "\nReturn Val: " << return_val
                      << "\nReturn Dec: " << return_decimal << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

#if !defined(BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE)

template <>
void test_roundtrip_conversion_float<long double>()
{
    std::mt19937_64 rng(42);

    // First test clingers fast path
    std::uniform_real_distribution<long double> dist(0.0L, 1e55L);

    for (std::size_t i = 0; i < N; ++i)
    {
        const long double val {dist(rng)};
        const decimal_fast128_t initial_decimal(val);
        const auto return_val {static_cast<long double>(initial_decimal)};
        const decimal_fast128_t return_decimal {return_val};

        if (!BOOST_TEST(boost::math::float_distance(val, return_val) < 50))
        {
            // LCOV_EXCL_START
            std::cerr << std::setprecision(std::numeric_limits<long double>::digits10 + 1)
                      << "Val: " << val
                      << "\nDec: " << initial_decimal
                      << "\nReturn Val: " << return_val
                      << "\nReturn Dec: " << return_decimal
                      << "\nDist: " << boost::math::float_distance(val, return_val) << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Then the rest of the domain
    std::uniform_real_distribution<long double> dist2(0.0L, LDBL_MAX);
    for (std::size_t i = 0; i < N; ++i)
    {
        const long double val {dist2(rng)};
        const decimal_fast128_t initial_decimal(val);
        const auto return_val {static_cast<long double>(initial_decimal)};
        const decimal_fast128_t return_decimal {return_val};

        if (!BOOST_TEST(boost::math::float_distance(val, return_val) < 50))
        {
            // LCOV_EXCL_START
            std::cerr << std::setprecision(std::numeric_limits<long double>::digits10 + 1)
                      << "Val: " << val
                      << "\nDec: " << initial_decimal
                      << "\nReturn Val: " << return_val
                      << "\nReturn Dec: " << return_decimal
                      << "\nDist: " << boost::math::float_distance(val, return_val) << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST(isnan(decimal_fast128_t(std::numeric_limits<long double>::quiet_NaN() * dist(rng))));
}

#endif

template <typename T>
void test_roundtrip_integer_stream()
{
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<T> dist((std::numeric_limits<T>::min)(), (std::numeric_limits<T>::max)());

    for (std::size_t i {}; i < N; ++i)
    {
        const decimal_fast128_t first_val {dist(rng)};
        const T first_val_int {static_cast<T>(first_val)};
        std::stringstream ss;
        ss << std::setprecision(std::numeric_limits<decimal_fast128_t>::digits10);
        ss << first_val;
        decimal_fast128_t return_val {};
        ss >> return_val;
        const T return_val_int {static_cast<T>(return_val)};

        if (!BOOST_TEST_EQ(first_val, return_val) || !BOOST_TEST_EQ(first_val_int, return_val_int))
        {
            // LCOV_EXCL_START
            std::cerr << std::setprecision(std::numeric_limits<decimal_fast128_t>::digits10)
                      << "    Val: " << first_val
                      << "\nInt Val: " << first_val_int
                      << "\n SS Val: " << ss.str()
                      << "\n    Ret: " << return_val
                      << "\nInt Ret: " << return_val_int << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename T>
void test_roundtrip_float_stream()
{
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<T> dist((std::numeric_limits<T>::min)(), (std::numeric_limits<T>::max)());

    for (std::size_t i {}; i < N; ++i)
    {
        const decimal_fast128_t first_val {dist(rng)};
        const T first_val_flt {static_cast<T>(first_val)};
        std::stringstream ss;
        ss << std::setprecision(std::numeric_limits<decimal_fast128_t>::digits10);
        ss << first_val;
        decimal_fast128_t return_val {};
        ss >> return_val;
        const T return_val_flt {static_cast<T>(return_val)};

        if (!BOOST_TEST_EQ(first_val, return_val) || !BOOST_TEST_EQ(first_val_flt, return_val_flt))
        {
            // LCOV_EXCL_START
            std::cerr << std::setprecision(std::numeric_limits<T>::digits10)
                      << "Val: " << first_val
                      << "\nInt Val: " << first_val_flt
                      << "\nRet: " << return_val
                      << "\nInt Ret: " << return_val_flt << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

#if !defined(BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE)

template <>
void test_roundtrip_float_stream<long double>()
{
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<long double> dist((std::numeric_limits<long double>::min)(),
                                                     (std::numeric_limits<long double>::max)());

    for (std::size_t i {}; i < N; ++i)
    {
        const decimal_fast128_t first_val {dist(rng)};
        const long double first_val_flt {static_cast<long double>(first_val)};
        std::stringstream ss;
        ss << std::setprecision(std::numeric_limits<decimal_fast128_t>::digits10);
        ss << first_val;
        decimal_fast128_t return_val {};
        ss >> return_val;
        const auto return_val_flt {static_cast<long double>(return_val)};

        if (!BOOST_TEST(boost::math::float_distance(first_val_flt, return_val_flt) < 100))
        {
            // LCOV_EXCL_START
            std::cerr << std::setprecision(std::numeric_limits<long double>::digits10)
                      << "    Dec: " << first_val
                      << "\n    Val: " << first_val_flt
                      << "\nRet Dec: " << return_val
                      << "\nRet Val: " << return_val_flt
                      << "\n  Dist :" << boost::math::float_distance(first_val_flt, return_val_flt) << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

#endif

void test_roundtrip_conversion_decimal32_t()
{
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> dist(-9'999'999, 9'999'999);

    for (std::size_t i = 0; i < N; ++i)
    {
        const decimal_fast128_t val {dist(rng)};
        const decimal32_t short_dec(val);
        const decimal_fast128_t return_decimal {short_dec};

        if(!BOOST_TEST_EQ(val, return_decimal))
        {
            // LCOV_EXCL_START
            std::cerr << "       Val: " << val
                      << "\n       Dec: " << short_dec
                      << "\nReturn Dec: " << return_decimal << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

int main()
{
    test_conversion_to_integer<int>();
    test_conversion_to_integer<unsigned>();
    test_conversion_to_integer<long>();
    test_conversion_to_integer<unsigned long>();
    test_conversion_to_integer<long long>();
    test_conversion_to_integer<unsigned long long>();
    test_conversion_to_integer<std::int32_t>();
    test_conversion_to_integer<std::uint32_t>();
    test_conversion_to_integer<std::int64_t>();
    test_conversion_to_integer<std::uint64_t>();

    test_roundtrip_conversion_integer<int>(-9'999'999, 9'999'999);
    test_roundtrip_conversion_integer<unsigned>(0, 9'999'999);
    test_roundtrip_conversion_integer<long>(-9'999'999, 9'999'999);
    test_roundtrip_conversion_integer<unsigned long>(0, 9'999'999);
    test_roundtrip_conversion_integer<long long>(-9'999'999, 9'999'999);
    test_roundtrip_conversion_integer<unsigned long long>(0, 9'999'999);

    test_roundtrip_conversion_integer<std::int32_t>(-9'999'999, 9'999'999);
    test_roundtrip_conversion_integer<std::uint32_t>(0, 9'999'999);
    test_roundtrip_conversion_integer<std::int64_t>(-9'999'999, 9'999'999);
    test_roundtrip_conversion_integer<std::uint64_t>(0, 9'999'999);

    test_conversion_from_float<float>();
    test_conversion_from_float<double>();

    test_conversion_to_float<float>();
    test_conversion_to_float<double>();

    test_roundtrip_conversion_float<float>();
    test_roundtrip_conversion_float<double>();

    test_roundtrip_integer_stream<int>();
    test_roundtrip_integer_stream<unsigned>();
    test_roundtrip_integer_stream<long>();
    test_roundtrip_integer_stream<unsigned long>();
    test_roundtrip_integer_stream<long long>();
    test_roundtrip_integer_stream<unsigned long long>();

    test_roundtrip_float_stream<float>();
    test_roundtrip_float_stream<double>();

    #if BOOST_DECIMAL_LDBL_BITS < 128 && !defined(BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE)
    test_conversion_from_float<long double>();
    test_conversion_to_float<long double>();
    test_roundtrip_conversion_float<long double>();
    test_roundtrip_float_stream<long double>();
    #endif

    #ifdef BOOST_DECIMAL_HAS_FLOAT16
    test_conversion_to_float<std::float16_t>();
    //test_roundtrip_conversion_float<std::float16_t>();
    //test_roundtrip_float_stream<std::float16_t>();
    #endif
    #ifdef BOOST_DECIMAL_HAS_FLOAT32
    test_conversion_to_float<std::float32_t>();
    //test_roundtrip_conversion_float<std::float32_t>();
    //test_roundtrip_float_stream<std::float32_t>();
    #endif
    #ifdef BOOST_DECIMAL_HAS_FLOAT64
    test_conversion_to_float<std::float64_t>();
    //test_roundtrip_conversion_float<std::float64_t>();
    //test_roundtrip_float_stream<std::float64_t>();
    #endif
    #ifdef BOOST_DECIMAL_HAS_BRAINFLOAT16
    test_conversion_to_float<std::bfloat16_t>();
    //test_roundtrip_conversion_float<std::bfloat16_t>();
    //test_roundtrip_float_stream<std::bfloat16_t>();
    #endif

    test_roundtrip_conversion_decimal32_t();

    return boost::report_errors();
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif
