// Copyright 2023 - 2024 Matt Borland
// Copyright 2023 - 2024 Christopher Kormanyos
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

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
static constexpr auto N = static_cast<std::size_t>(128); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(128 >> 4U); // Number of trials
#endif

namespace local {

template<typename NumericType>
auto is_close_fraction(const NumericType& a,
                        const NumericType& b,
                        const NumericType& tol) noexcept -> bool
{
    using std::fabs;

    auto result_is_ok = bool { };

    NumericType delta { };

    if (b == static_cast<NumericType>(0))
    {
        delta = fabs(a - b); // LCOV_EXCL_LINE

        result_is_ok = (delta < tol); // LCOV_EXCL_LINE
    }
    else
    {
        delta = fabs(1 - (a / b));

        result_is_ok = (delta < tol);
    }

    // LCOV_EXCL_START
    if (!result_is_ok)
    {
        std::cerr << std::setprecision(std::numeric_limits<NumericType>::digits10) << "a: " << a
                  << "\nb: " << b
                  << "\ndelta: " << delta
                  << "\ntol: " << tol << std::endl;
    }
    // LCOV_EXCL_STOP

    return result_is_ok;
}

} // namespace local

static std::mt19937_64 rng(42);

using namespace boost::decimal;

template <typename Dec = decimal32_t>
void spot_test(float val)
{
    const auto ret_val {std::atan(val)};
    const auto ret_dec {static_cast<float>(atan(Dec{val}))};

    const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
    if (!BOOST_TEST(distance < 100))
    {
        // LCOV_EXCL_START
        std::cerr << "Val 1: " << val
                  << "\nDec 1: " << Dec{val}
                  << "\nRet val: " << ret_val
                  << "\nRet dec: " << ret_dec
                  << "\nEps: " << distance << std::endl;
        // LCOV_EXCL_STOP
    }
}

template <typename Dec>
void test_atan()
{
    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value ? N / 4 : N};

    for (std::size_t n {}; n < max_iter; ++n)
    {
        std::uniform_real_distribution<float> small_vals(0.0F, 0.4375F);
        const auto val1 {small_vals(rng)};
        Dec d1 {val1};

        auto ret_val {std::atan(val1)};
        auto ret_dec {static_cast<float>(atan(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 100))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t n {}; n < max_iter; ++n)
    {
        std::uniform_real_distribution<float> half_vals(0.4375F, 0.6875F);
        const auto val1 {half_vals(rng)};
        Dec d1 {val1};

        auto ret_val {std::atan(val1)};
        auto ret_dec {static_cast<float>(atan(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 100))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t n {}; n < max_iter; ++n)
    {
        std::uniform_real_distribution<float> one_vals(0.6875F, 1.1875F);
        const auto val1 {one_vals(rng)};
        Dec d1 {val1};

        auto ret_val {std::atan(val1)};
        auto ret_dec {static_cast<float>(atan(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 100))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t n {}; n < max_iter; ++n)
    {
        std::uniform_real_distribution<float> three_halves(1.1875F, 2.4375F);
        const auto val1 {three_halves(rng)};
        Dec d1 {val1};

        auto ret_val {std::atan(val1)};
        auto ret_dec {static_cast<float>(atan(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 100))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t n {}; n < max_iter; ++n)
    {
        std::uniform_real_distribution<float> less_than_6(2.4375F, 6.0F);
        const auto val1 {less_than_6(rng)};
        Dec d1 {val1};

        auto ret_val {std::atan(val1)};
        auto ret_dec {static_cast<float>(atan(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 1000))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t n {}; n < max_iter; ++n)
    {
        std::uniform_real_distribution<float> less_than_12(6.0F, 12.0F);
        const auto val1 {less_than_12(rng)};
        Dec d1 {val1};

        auto ret_val {std::atan(val1)};
        auto ret_dec {static_cast<float>(atan(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 1000))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t n {}; n < max_iter; ++n)
    {
        std::uniform_real_distribution<float> less_than_24(12.0F, 24.0F);
        const auto val1 {less_than_24(rng)};
        Dec d1 {val1};

        auto ret_val {std::atan(val1)};
        auto ret_dec {static_cast<float>(atan(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 1000))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    for (std::size_t n {}; n < max_iter; ++n)
    {
        std::uniform_real_distribution<float> big_vals(2.4375F, 100.0F);
        const auto val1 {big_vals(rng)};
        Dec d1 {val1};

        auto ret_val {std::atan(val1)};
        auto ret_dec {static_cast<float>(atan(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < 1e6))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << distance << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    // Edge cases
    std::uniform_int_distribution<int> one(1,1);
    BOOST_TEST_EQ(atan(std::numeric_limits<Dec>::infinity() * Dec(one(rng))), numbers::pi_v<Dec>/2);
    BOOST_TEST_EQ(atan(-std::numeric_limits<Dec>::infinity() * Dec(one(rng))), -numbers::pi_v<Dec>/2);
    BOOST_TEST(isnan(atan(std::numeric_limits<Dec>::quiet_NaN() * Dec(one(rng)))));
    BOOST_TEST_EQ(atan(Dec(0) * Dec(one(rng))), Dec(0));
    BOOST_TEST_EQ(atan(std::numeric_limits<Dec>::epsilon() * Dec(one(rng))), std::numeric_limits<Dec>::epsilon() * Dec(one(rng)));
}

namespace local {

auto test_atan_128(const int tol_factor) -> bool
{
    using decimal_type = boost::decimal::decimal128_t;

    using str_ctrl_array_type = std::array<const char*, 31U>;

    const str_ctrl_array_type ctrl_strings =
    {{
        // Table[N[ArcTan[n + (n + 1)/10], 36], {n, 0, 30, 1}]
        "0.0996686524911620273784461198780205902",
        "0.876058050598193423114047521128341339",
        "1.16066898625340562678011092078453218",
        "1.28474488507757839521660045035576124",
        "1.35212738092095465718914794138981285",
        "1.39408747072486000451142034998493574",
        "1.42263630606306524074609878711808965",
        "1.44328676857965836015625241442945425",
        "1.45890606062322050438578419322289952",
        "1.47112767430373459185287557176173085",
        "1.48094878710026889004729148794586818",
        "1.48901194617690061950062694387225355",
        "1.49574956319845608444166302563671922",
        "1.50146319310668803425604443731368245",
        "1.50636948736934306863178215633740183",
        "1.51062807563988690252010732385580363",
        "1.51435914848319329323768224134174846",
        "1.51765491794996116222569383300002088",
        "1.52058730451178540948599035411720525",
        "1.52321322351791322342928897562326592",
        "1.52557830188603652983022768173087618",
        "1.52771954287113490153496773234777134",
        "1.52966727041734455455350773831183303",
        "1.53144657040629129910455280928278240",
        "1.53307837432803240434373777942318448",
        "1.53458028472248503068348160776186418",
        "1.53596721141036644725997548254677890",
        "1.53725186723321821298607774053472251",
        "1.53844515819879435219259100906944202",
        "1.53955649336462834297760994674726047",
        "1.54059403307910435064686494555939664",
    }};

    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> atan_values { };
    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> ctrl_values { };

    int nx { 0 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<str_ctrl_array_type>::value; ++i)
    {
        const decimal_type
            x_arg
            {
                decimal_type { nx }
              + decimal_type { nx + 1, -1 }
            };

        ++nx;

        atan_values[i] = atan(x_arg);

        static_cast<void>
        (
            from_chars(ctrl_strings[i], ctrl_strings[i] + std::strlen(ctrl_strings[i]), ctrl_values[i])
        );

        const auto result_atan_is_ok = is_close_fraction(atan_values[i], ctrl_values[i], my_tol);

        result_is_ok = (result_atan_is_ok && result_is_ok);
    }

    return result_is_ok;
}

} // namespace local

int main()
{
    test_atan<decimal32_t>();
    test_atan<decimal64_t>();
    test_atan<decimal_fast32_t>();

    spot_test(0.344559F);
    spot_test(0.181179F);

    {
        const auto result_pos128_is_ok = local::test_atan_128(4096);

        BOOST_TEST(result_pos128_is_ok);
    }

    return boost::report_errors();
}
