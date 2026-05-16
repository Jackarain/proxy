// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// Propogates up from boost.math
#define _SILENCE_CXX23_DENORM_DEPRECATION_WARNING

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
#include <iostream>
#include <random>
#include <cmath>

// Needed to reduce CI runtime
#ifdef _MSC_VER
#  ifndef BOOST_DECIMAL_REDUCE_TEST_DEPTH
#    define BOOST_DECIMAL_REDUCE_TEST_DEPTH
#  endif
#endif

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH) && !defined(_MSC_VER)
static constexpr auto N = static_cast<std::size_t>(64U); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(8); // Number of trials
#endif

static std::mt19937_64 rng(42);

using namespace boost::decimal;

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable : 4305)
#endif

template <typename T>
void print_value(T value, const char* str)
{
    int ptr;
    const auto sig_val = frexp10(value, &ptr);
    std::cerr << std::setprecision(std::numeric_limits<T>::digits10) << str << ": " << value
              << "\nSig: " << sig_val.high << " " << sig_val.low
              << "\nExp: " << ptr << "\n" << std::endl;
}

template <typename T>
void test_erf()
{
    std::uniform_int_distribution<int> dist(1, 1);

    BOOST_TEST(isnan(erf(std::numeric_limits<T>::quiet_NaN()) * dist(rng)));
    BOOST_TEST_EQ(erf(T(0 * dist(rng))), T{0});
    BOOST_TEST_EQ(erf(std::numeric_limits<T>::infinity() * dist(rng)), T{1});
    BOOST_TEST_EQ(erf(-std::numeric_limits<T>::infinity() * dist(rng)), T{-1});

    // Small values
    using float_type = std::conditional_t<std::is_same<T, decimal32_t>::value, float, double>;

    std::uniform_real_distribution<float_type> smallest_vals(static_cast<float_type>(1e-15), static_cast<float_type>(1e-10));
    for (std::size_t i {}; i < N / 6; ++i)
    {
        const auto val {smallest_vals(rng)};
        const T dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> second_smallest(static_cast<float_type>(1e-10), static_cast<float_type>(0.5));
    for (std::size_t i {}; i < N / 6; ++i)
    {
        const auto val {second_smallest(rng)};
        const T dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_smallest(static_cast<float_type>(0.5), static_cast<float_type>(1.5));
    for (std::size_t i {}; i < N / 6; ++i)
    {
        const auto val {erfc_smallest(rng)};
        const T dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_middle(static_cast<float_type>(1.5), static_cast<float_type>(2.5));
    for (std::size_t i {}; i < N / 6; ++i)
    {
        const auto val {erfc_middle(rng)};
        const T dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_large(static_cast<float_type>(2.5), static_cast<float_type>(4.5));
    for (std::size_t i {}; i < N / 6; ++i)
    {
        const auto val {erfc_large(rng)};
        const T dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> catch_all(static_cast<float_type>(4.5), static_cast<float_type>(110.0));
    for (std::size_t i {}; i < N / 6; ++i)
    {
        const auto val {catch_all(rng)};
        const T dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Negative values
    std::uniform_real_distribution<float_type> negative(static_cast<float_type>(-2), static_cast<float_type>(0.0));
    for (std::size_t i {}; i < N / 8; ++i)
    {
        const auto val {negative(rng)};
        const T dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "  Val: " << val
                      << "\nFloat: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Underflow case
    BOOST_TEST_EQ(erf(T{120}), T{1} * dist(rng));
}

#if !defined(BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE)
template <>
void test_erf<decimal128_t>()
{
    std::uniform_int_distribution<int> dist(1, 1);

    BOOST_TEST(isnan(erf(std::numeric_limits<decimal128_t>::quiet_NaN()) * dist(rng)));
    BOOST_TEST_EQ(erf(decimal128_t(0 * dist(rng))), decimal128_t{0});
    BOOST_TEST_EQ(erf(std::numeric_limits<decimal128_t>::infinity() * dist(rng)), decimal128_t{1});
    BOOST_TEST_EQ(erf(-std::numeric_limits<decimal128_t>::infinity() * dist(rng)), decimal128_t{-1});

    // Small values
    using float_type = long double;

    std::uniform_real_distribution<float_type> smallest_vals(1e-25, 1e-20);
    for (std::size_t i {}; i < 5; ++i)
    {
        const auto val {smallest_vals(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> second_smallest(1e-20, 0.5);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {second_smallest(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_smallest(0.5, 1.0);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_smallest(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_two(1.0, 1.5);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_two(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_three(1.5, 2.25);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_three(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_four(2.25, 3.0);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_four(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_five(3.0, 3.5);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_five(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_six(3.5, 5.5);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_six(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_seven(5.5, 7.5);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_seven(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_eight(7.5, 11.5);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_eight(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_large(11.5, 15.0);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_large(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Underflow case
    BOOST_TEST_EQ(erf(decimal128_t{120}), decimal128_t{1} * dist(rng));
}

#endif

template <typename T>
void test_erfc()
{
    std::uniform_int_distribution<int> dist(1, 1);

    BOOST_TEST(isnan(erfc(std::numeric_limits<T>::quiet_NaN() * dist(rng))));
    BOOST_TEST_EQ(erfc(std::numeric_limits<T>::infinity() * dist(rng)), T{0});
    BOOST_TEST_EQ(erfc(-std::numeric_limits<T>::infinity() * dist(rng)), T{2});

    // Small values
    using float_type = std::conditional_t<std::is_same<T, decimal32_t>::value, float, double>;

    std::uniform_real_distribution<float_type> smallest_vals(static_cast<float_type>(1e-15), static_cast<float_type>(1e-10));
    for (std::size_t i {}; i < N / 6; ++i)
    {
        const auto val {smallest_vals(rng)};
        const T dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> second_smallest(static_cast<float_type>(1e-10), static_cast<float_type>(0.5));
    for (std::size_t i {}; i < N / 6; ++i)
    {
        const auto val {second_smallest(rng)};
        const T dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_smallest(static_cast<float_type>(0.5), static_cast<float_type>(1.5));
    for (std::size_t i {}; i < N / 6; ++i)
    {
        const auto val {erfc_smallest(rng)};
        const T dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 50))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_middle(static_cast<float_type>(1.5), static_cast<float_type>(2.5));
    for (std::size_t i {}; i < N / 6; ++i)
    {
        const auto val {erfc_middle(rng)};
        const T dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 50))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_large(static_cast<float_type>(2.5), static_cast<float_type>(4.5));
    for (std::size_t i {}; i < N / 6; ++i)
    {
        const auto val {erfc_large(rng)};
        const T dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 150))
        {
            // LCOV_EXCL_START
            std::cerr << "  Val: " << val
                      << "\nFloat: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> catch_all(static_cast<float_type>(4.5), static_cast<float_type>(10.0));
    for (std::size_t i {}; i < N / 6; ++i)
    {
        const auto val {catch_all(rng)};
        const T dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 1e15))
        {
            // LCOV_EXCL_START
            std::cerr << "  Val: " << val
                      << "\nFloat: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Negative values
    std::uniform_real_distribution<float_type> negative_first_path(static_cast<float_type>(-2), static_cast<float_type>(-0.5));
    for (std::size_t i {}; i < N / 8; ++i)
    {
        const auto val {negative_first_path(rng)};
        const T dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "  Val: " << val
                      << "\nFloat: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> negative_second_path(static_cast<float_type>(-0.5), static_cast<float_type>(0.0));
    for (std::size_t i {}; i < N / 8; ++i)
    {
        const auto val {negative_first_path(rng)};
        const T dec_val {val};

        const auto float_res {std::erf(val)};
        const auto dec_res {static_cast<float_type>(erf(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "  Val: " << val
                      << "\nFloat: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Underflow case
    BOOST_TEST_EQ(erfc(T{120}), T{0} * dist(rng));
}

#if !defined(BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE)

template <>
void test_erfc<decimal128_t>()
{
    std::uniform_int_distribution<int> dist(1, 1);

    BOOST_TEST(isnan(erfc(std::numeric_limits<decimal128_t>::quiet_NaN() * dist(rng))));
    BOOST_TEST_EQ(erfc(std::numeric_limits<decimal128_t>::infinity() * dist(rng)), decimal128_t{0});
    BOOST_TEST_EQ(erfc(-std::numeric_limits<decimal128_t>::infinity() * dist(rng)), decimal128_t{2});

    // Small values
    using float_type = long double;

    std::uniform_real_distribution<float_type> smallest_vals(1e-25, 1e-20);
    for (std::size_t i {}; i < 5; ++i)
    {
        const auto val {smallest_vals(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> second_smallest(1e-20, 0.5);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {second_smallest(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 30))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_smallest(0.5, 1.0);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_smallest(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 20000))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_two(1.0, 1.5);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_two(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 20000))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_three(1.5, 2.25);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_three(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 20000))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_four(2.25, 3.0);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_four(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 25000))
        {
            // LCOV_EXCL_START
            std::cerr << "  Val: " << val
                      << "\nFloat: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_five(3.0, 3.5);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_five(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 20000))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_six(3.5, 5.5);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_six(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 20000))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_seven(5.5, 7.5);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_seven(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 20000))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_eight(7.5, 11.5);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_eight(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 2000))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    std::uniform_real_distribution<float_type> erfc_large(11.5, 15.0);
    for (std::size_t i {}; i < 2; ++i)
    {
        const auto val {erfc_large(rng)};
        const decimal128_t dec_val {val};

        const auto float_res {std::erfc(val)};
        const auto dec_res {static_cast<float_type>(erfc(dec_val))};
        const auto distance {boost::math::float_distance(float_res, dec_res)};

        if (!BOOST_TEST(distance < 1e16))
        {
            // LCOV_EXCL_START
            std::cerr << "Float: " << float_res
                      << "\n  Dec: " << dec_res
                      << "\n Dist: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Underflow case
    BOOST_TEST_EQ(erfc(decimal128_t{120}), decimal128_t{0} * dist(rng));
}

#endif

int main()
{
    #ifdef BOOST_DECIMAL_GENERATE_CONSTANT_SIGS
    // For ERF
    print_value("0.003379167095512573896158903121545171688"_DL, "z < 1e-20");

    print_value("0.0442269454158250738961589031215451778"_DL, "P1-0");
    print_value("-0.35549265736002144875335323556961233"_DL, "P1-1");
    print_value("-0.0582179564566667896225454670863270393"_DL, "P1-2");
    print_value("-0.0112694696904802304229950538453123925"_DL, "P1-3");
    print_value("-0.000805730648981801146251825329609079099"_DL, "P1-4");
    print_value("-0.566304966591936566229702842075966273e-4"_DL, "P1-5");
    print_value("-0.169655010425186987820201021510002265e-5"_DL, "P1-6");
    print_value("-0.344448249920445916714548295433198544e-7"_DL, "P1-7");

    print_value("1"_DL, "Q1-0");
    print_value("0.466542092785657604666906909196052522"_DL, "Q1-1");
    print_value("0.100005087012526447295176964142107611"_DL, "Q1-2");
    print_value("0.0128341535890117646540050072234142603"_DL, "Q1-3");
    print_value("0.00107150448466867929159660677016658186"_DL, "Q1-4");
    print_value("0.586168368028999183607733369248338474e-4"_DL, "Q1-5");
    print_value("0.196230608502104324965623171516808796e-5"_DL, "Q1-6");
    print_value("0.313388521582925207734229967907890146e-7"_DL, "Q1-7");

    // For ERFC
    std::cerr << "---------- z < 1 --------\n";
    print_value("0.371877193450927734375"_DL, "Y");
    print_value("-0.0640320213544647969396032886581290455"_DL, "P0");
    print_value("0.200769874440155895637857443946706731"_DL, "P1");
    print_value("0.378447199873537170666487408805779826"_DL, "P2");
    print_value("0.30521399466465939450398642044975127"_DL, "P3");
    print_value("0.146890026406815277906781824723458196"_DL, "P4");
    print_value("0.0464837937749539978247589252732769567"_DL, "P5");
    print_value("0.00987895759019540115099100165904822903"_DL, "P6");
    print_value("0.00137507575429025512038051025154301132"_DL, "P7");
    print_value("0.0001144764551085935580772512359680516"_DL, "P8");
    print_value("0.436544865032836914773944382339900079e-5"_DL, "P9");

    print_value("1"_DL, "Q0");
    print_value("2.47651182872457465043733800302427977"_DL, "Q1");
    print_value("2.78706486002517996428836400245547955"_DL, "Q2");
    print_value("1.87295924621659627926365005293130693"_DL, "Q3");
    print_value("0.829375825174365625428280908787261065"_DL, "Q4");
    print_value("0.251334771307848291593780143950311514"_DL, "Q5");
    print_value("0.0522110268876176186719436765734722473"_DL, "Q6");
    print_value("0.00718332151250963182233267040106902368"_DL, "Q7");
    print_value("0.000595279058621482041084986219276392459"_DL, "Q8");
    print_value("0.226988669466501655990637599399326874e-4"_DL, "Q9");
    print_value("0.270666232259029102353426738909226413e-10"_DL, "Q10");

    std::cerr << "---------- z < 1.5 --------\n";
    print_value("0.45658016204833984375"_DL, "Y");

    print_value("-0.0289965858925328393392496555094848345"_DL, "P0");
    print_value("0.0868181194868601184627743162571779226"_DL, "P1");
    print_value("0.169373435121178901746317404936356745"_DL, "P2");
    print_value("0.13350446515949251201104889028133486"_DL, "P3");
    print_value("0.0617447837290183627136837688446313313"_DL, "P4");
    print_value("0.0185618495228251406703152962489700468"_DL, "P5");
    print_value("0.00371949406491883508764162050169531013"_DL, "P6");
    print_value("0.000485121708792921297742105775823900772"_DL, "P7");
    print_value("0.376494706741453489892108068231400061e-4"_DL, "P8");
    print_value("0.133166058052466262415271732172490045e-5"_DL, "P9");

    print_value("1"_DL, "Q0");
    print_value("2.32970330146503867261275580968135126"_DL, "Q1");
    print_value("2.46325715420422771961250513514928746"_DL, "Q2");
    print_value("1.55307882560757679068505047390857842"_DL, "Q3");
    print_value("0.644274289865972449441174485441409076"_DL, "Q4");
    print_value("0.182609091063258208068606847453955649"_DL, "Q5");
    print_value("0.0354171651271241474946129665801606795"_DL, "Q6");
    print_value("0.00454060370165285246451879969534083997"_DL, "Q7");
    print_value("0.000349871943711566546821198612518656486"_DL, "Q8");
    print_value("0.123749319840299552925421880481085392e-4"_DL, "Q9");

    std::cerr << "---------- z < 2.25 --------\n";
    print_value("0.50250148773193359375"_DL, "Y");

    print_value("-0.0201233630504573402185161184151016606"_DL, "P0");
    print_value("0.0331864357574860196516686996302305002"_DL, "P1");
    print_value("0.0716562720864787193337475444413405461"_DL, "P2");
    print_value("0.0545835322082103985114927569724880658"_DL, "P3");
    print_value("0.0236692635189696678976549720784989593"_DL, "P4");
    print_value("0.00656970902163248872837262539337601845"_DL, "P5");
    print_value("0.00120282643299089441390490459256235021"_DL, "P6");
    print_value("0.000142123229065182650020762792081622986"_DL, "P7");
    print_value("0.991531438367015135346716277792989347e-5"_DL, "P8");
    print_value("0.312857043762117596999398067153076051e-6"_DL, "P9");

    print_value("1"_DL, "Q0");
    print_value("2.13506082409097783827103424943508554"_DL, "Q1");
    print_value("2.06399257267556230937723190496806215"_DL, "Q2");
    print_value("1.18678481279932541314830499880691109"_DL, "Q3");
    print_value("0.447733186643051752513538142316799562"_DL, "Q4");
    print_value("0.11505680005657879437196953047542148"_DL, "Q5");
    print_value("0.020163993632192726170219663831914034"_DL, "Q6");
    print_value("0.00232708971840141388847728782209730585"_DL, "Q7");
    print_value("0.000160733201627963528519726484608224112"_DL, "Q8");
    print_value("0.507158721790721802724402992033269266e-5"_DL, "Q9");
    print_value("0.18647774409821470950544212696270639e-12"_DL, "Q10");

    std::cerr << "---------- z < 3 --------\n";
    print_value("0.52896785736083984375"_DL, "Y");

    print_value("-0.00902152521745813634562524098263360074"_DL, "P0");
    print_value("0.0145207142776691539346923710537580927"_DL, "P1");
    print_value("0.0301681239582193983824211995978678571"_DL, "P2");
    print_value("0.0215548540823305814379020678660434461"_DL, "P3");
    print_value("0.00864683476267958365678294164340749949"_DL, "P4");
    print_value("0.00219693096885585491739823283511049902"_DL, "P5");
    print_value("0.000364961639163319762492184502159894371"_DL, "P6");
    print_value("0.388174251026723752769264051548703059e-4"_DL, "P7");
    print_value("0.241918026931789436000532513553594321e-5"_DL, "P8");
    print_value("0.676586625472423508158937481943649258e-7"_DL, "P9");

    print_value("1"_DL, "Q0");
    print_value("1.93669171363907292305550231764920001"_DL, "Q1");
    print_value("1.69468476144051356810672506101377494"_DL, "Q2");
    print_value("0.880023580986436640372794392579985511"_DL, "Q3");
    print_value("0.299099106711315090710836273697708402"_DL, "Q4");
    print_value("0.0690593962363545715997445583603382337"_DL, "Q5");
    print_value("0.0108427016361318921960863149875360222"_DL, "Q6");
    print_value("0.00111747247208044534520499324234317695"_DL, "Q7");
    print_value("0.686843205749767250666787987163701209e-4"_DL, "Q8");
    print_value("0.192093541425429248675532015101904262e-5"_DL, "Q9");

    std::cerr << "---------- z < 3.5 --------\n";
    print_value("0.54037380218505859375"_DL, "Y");

    print_value("-0.0033703486408887424921155540591370375"_DL, "P0");
    print_value("0.0104948043110005245215286678898115811"_DL, "P1");
    print_value("0.0148530118504000311502310457390417795"_DL, "P2");
    print_value("0.00816693029245443090102738825536188916"_DL, "P3");
    print_value("0.00249716579989140882491939681805594585"_DL, "P4");
    print_value("0.0004655591010047353023978045800916647"_DL, "P5");
    print_value("0.531129557920045295895085236636025323e-4"_DL, "P6");
    print_value("0.343526765122727069515775194111741049e-5"_DL, "P7");
    print_value("0.971120407556888763695313774578711839e-7"_DL, "P8");

    print_value("1"_DL, "Q0");
    print_value("1.59911256167540354915906501335919317"_DL, "Q1");
    print_value("1.136006830764025173864831382946934"_DL, "Q2");
    print_value("0.468565867990030871678574840738423023"_DL, "Q3");
    print_value("0.122821824954470343413956476900662236"_DL, "Q4");
    print_value("0.0209670914950115943338996513330141633"_DL, "Q5");
    print_value("0.00227845718243186165620199012883547257"_DL, "Q6");
    print_value("0.000144243326443913171313947613547085553"_DL, "Q7");
    print_value("0.407763415954267700941230249989140046e-5"_DL, "Q8");

    std::cerr << "---------- z < 5.5 --------\n";
    print_value("0.55000019073486328125"_DL, "Y");

    print_value("0.00118142849742309772151454518093813615"_DL, "P0");
    print_value("0.0072201822885703318172366893469382745"_DL, "P1");
    print_value("0.0078782276276860110721875733778481505"_DL, "P2");
    print_value("0.00418229166204362376187593976656261146"_DL, "P3");
    print_value("0.00134198400587769200074194304298642705"_DL, "P4");
    print_value("0.000283210387078004063264777611497435572"_DL, "P5");
    print_value("0.405687064094911866569295610914844928e-4"_DL, "P6");
    print_value("0.39348283801568113807887364414008292e-5"_DL, "P7");
    print_value("0.248798540917787001526976889284624449e-6"_DL, "P8");
    print_value("0.929502490223452372919607105387474751e-8"_DL, "P9");
    print_value("0.156161469668275442569286723236274457e-9"_DL, "P10");

    print_value("1"_DL, "Q0");
    print_value("1.52955245103668419479878456656709381"_DL, "Q1");
    print_value("1.06263944820093830054635017117417064"_DL, "Q2");
    print_value("0.441684612681607364321013134378316463"_DL, "Q3");
    print_value("0.121665258426166960049773715928906382"_DL, "Q4");
    print_value("0.0232134512374747691424978642874321434"_DL, "Q5");
    print_value("0.00310778180686296328582860464875562636"_DL, "Q6");
    print_value("0.000288361770756174705123674838640161693"_DL, "Q7");
    print_value("0.177529187194133944622193191942300132e-4"_DL, "Q8");
    print_value("0.655068544833064069223029299070876623e-6"_DL, "Q9");
    print_value("0.11005507545746069573608988651927452e-7"_DL, "Q10");

    std::cerr << "---------- z < 7.5 --------\n";
    print_value("0.5574436187744140625"_DL, "Y");
    print_value("0.000293236907400849056269309713064107674"_DL, "P0");
    print_value("0.00225110719535060642692275221961480162"_DL, "P1");
    print_value("0.00190984458121502831421717207849429799"_DL, "P2");
    print_value("0.000747757733460111743833929141001680706"_DL, "P3");
    print_value("0.000170663175280949889583158597373928096"_DL, "P4");
    print_value("0.246441188958013822253071608197514058e-4"_DL, "P5");
    print_value("0.229818000860544644974205957895688106e-5"_DL, "P6");
    print_value("0.134886977703388748488480980637704864e-6"_DL, "P7");
    print_value("0.454764611880548962757125070106650958e-8"_DL, "P8");
    print_value("0.673002744115866600294723141176820155e-10"_DL, "P9");

    print_value("1"_DL, "Q0");
    print_value("1.12843690320861239631195353379313367"_DL, "Q1");
    print_value("0.569900657061622955362493442186537259"_DL, "Q2");
    print_value("0.169094404206844928112348730277514273"_DL, "Q3");
    print_value("0.0324887449084220415058158657252147063"_DL, "Q4");
    print_value("0.00419252877436825753042680842608219552"_DL, "Q5");
    print_value("0.00036344133176118603523976748563178578"_DL, "Q6");
    print_value("0.204123895931375107397698245752850347e-4"_DL, "Q7");
    print_value("0.674128352521481412232785122943508729e-6"_DL, "Q8");
    print_value("0.997637501418963696542159244436245077e-8"_DL, "Q9");

    std::cerr << "---------- z < 11.5 --------\n";
    print_value("0.56083202362060546875"_DL, "Y");
    print_value("0.000282420728751494363613829834891390121"_DL, "P0");
    print_value("0.00175387065018002823433704079355125161"_DL, "P1");
    print_value("0.0021344978564889819420775336322920375"_DL, "P2");
    print_value("0.00124151356560137532655039683963075661"_DL, "P3");
    print_value("0.000423600733566948018555157026862139644"_DL, "P4");
    print_value("0.914030340865175237133613697319509698e-4"_DL, "P5");
    print_value("0.126999927156823363353809747017945494e-4"_DL, "P6");
    print_value("0.110610959842869849776179749369376402e-5"_DL, "P7");
    print_value("0.55075079477173482096725348704634529e-7"_DL, "P8");
    print_value("0.119735694018906705225870691331543806e-8"_DL, "P9");

    print_value("1"_DL, "Q0");
    print_value("1.69889613396167354566098060039549882"_DL, "Q1");
    print_value("1.28824647372749624464956031163282674"_DL, "Q2");
    print_value("0.572297795434934493541628008224078717"_DL, "Q3");
    print_value("0.164157697425571712377043857240773164"_DL, "Q4");
    print_value("0.0315311145224594430281219516531649562"_DL, "Q5");
    print_value("0.00405588922155632380812945849777127458"_DL, "Q6");
    print_value("0.000336929033691445666232029762868642417"_DL, "Q7");
    print_value("0.164033049810404773469413526427932109e-4"_DL, "Q8");
    print_value("0.356615210500531410114914617294694857e-6"_DL, "Q9");

    std::cerr << "---------- z >= 11.5 --------\n";
    print_value("0.5632686614990234375"_DL, "Y");
    print_value("0.000920922048732849448079451574171836943"_DL, "P0");
    print_value("0.00321439044532288750501700028748922439"_DL, "P1");
    print_value("0.250455263029390118657884864261823431"_DL, "P2");
    print_value("0.906807635364090342031792404764598142"_DL, "P3");
    print_value("8.92233572835991735876688745989985565"_DL, "P4");
    print_value("21.7797433494422564811782116907878495"_DL, "P5");
    print_value("91.1451915251976354349734589601171659"_DL, "P6");
    print_value("144.1279109655993927069052125017673"_DL, "P7");
    print_value("313.845076581796338665519022313775589"_DL, "P8");
    print_value("273.11378811923343424081101235736475"_DL, "P9");
    print_value("271.651566205951067025696102600443452"_DL, "P10");
    print_value("60.0530577077238079968843307523245547"_DL, "P11");

    print_value("1"_DL, "Q0");
    print_value("3.49040448075464744191022350947892036"_DL, "Q1");
    print_value("34.3563592467165971295915749548313227"_DL, "Q2");
    print_value("84.4993232033879023178285731843850461"_DL, "Q3");
    print_value("376.005865281206894120659401340373818"_DL, "Q4");
    print_value("629.95369438888946233003926191755125"_DL, "Q5");
    print_value("1568.35771983533158591604513304269098"_DL, "Q6");
    print_value("1646.02452040831961063640827116581021"_DL, "Q7");
    print_value("2299.96860633240298708910425594484895"_DL, "Q8");
    print_value("1222.73204392037452750381340219906374"_DL, "Q9");
    print_value("799.359797306084372350264298361110448"_DL, "Q10");
    print_value("72.7415265778588087243442792401576737"_DL, "Q11");

    return 1;
    #endif

    test_erf<decimal32_t>();
    test_erf<decimal64_t>();

    test_erfc<decimal32_t>();
    test_erfc<decimal64_t>();

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH) && BOOST_DECIMAL_LDBL_BITS != 128 && !defined(BOOST_DECIMAL_UNSUPPORTED_LONG_DOUBLE) && !defined(__i386__) && !defined(_WIN32)
    test_erf<decimal128_t>();
    test_erfc<decimal128_t>();
    #endif

    return boost::report_errors();
}

#ifdef _MSC_VER
# pragma warning(pop)
#endif
