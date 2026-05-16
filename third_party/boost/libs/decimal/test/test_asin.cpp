// Copyright 2024 - 2026 Matt Borland
// Copyright 2024 - 2026 Christopher Kormanyos
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
#  if (__clang_major__ >= 20)
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

#include <chrono>
#include <cmath>
#include <random>

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
static constexpr auto N = static_cast<std::size_t>(128); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(128 >> 4U); // Number of trials
#endif

static std::mt19937_64 rng(42);

using namespace boost::decimal;

template<typename T> auto my_zero() -> T;
template<typename T> auto my_one () -> T;

namespace local
{
  template<typename IntegralTimePointType,
           typename ClockType = std::chrono::high_resolution_clock>
  auto time_point() noexcept -> IntegralTimePointType
  {
    using local_integral_time_point_type = IntegralTimePointType;
    using local_clock_type               = ClockType;

    const auto current_now =
      static_cast<std::uintmax_t>
      (
        std::chrono::duration_cast<std::chrono::nanoseconds>
        (
          local_clock_type::now().time_since_epoch()
        ).count()
      );

    return static_cast<local_integral_time_point_type>(current_now);
  }
} // namespace local

template <typename Dec>
void test_asin()
{
    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? 2 : N};
    constexpr auto tol {std::is_same<Dec, decimal128_t>::value || std::is_same<Dec, decimal_fast128_t>::value ? 25000 : 50};

    for (std::size_t n {}; n < max_iter; ++n)
    {
        std::uniform_real_distribution<float> small_vals(0.0F, 0.5F);
        const auto val1 {small_vals(rng)};
        Dec d1 {val1};

        auto ret_val {std::asin(val1)};
        auto ret_dec {static_cast<float>(asin(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < tol))
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
        std::uniform_real_distribution<float> big_vals(0.5F, 0.9999F);
        const auto val1 {big_vals(rng)};
        Dec d1 {val1};

        auto ret_val {std::asin(val1)};
        auto ret_dec {static_cast<float>(asin(d1))};

        if (std::isinf(ret_dec))
        {
            std::cerr << "INF: " << d1 << " iter: n = " << n << std::endl; // LCOV_EXCL_LINE
        }

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < tol))
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
        std::uniform_real_distribution<float> neg_vals(-0.9999F, 0.0F);
        const auto val1 {neg_vals(rng)};
        Dec d1 {val1};

        auto ret_val {std::asin(val1)};
        auto ret_dec {static_cast<float>(asin(d1))};

        const auto distance {std::fabs(boost::math::float_distance(ret_val, ret_dec))};
        if (!BOOST_TEST(distance < tol))
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
    BOOST_TEST(isnan(asin(std::numeric_limits<Dec>::infinity() * Dec(one(rng)))));
    BOOST_TEST(isnan(asin(-std::numeric_limits<Dec>::infinity() * Dec(one(rng)))));
    BOOST_TEST(isnan(asin(std::numeric_limits<Dec>::quiet_NaN() * Dec(one(rng)))));
    BOOST_TEST_EQ(asin(std::numeric_limits<Dec>::epsilon() * Dec(one(rng))), std::numeric_limits<Dec>::epsilon() * Dec(one(rng)));
}

template <typename T>
void print_value(T value, const char* str)
{
    int ptr;
    const auto sig_val = frexp10(value, &ptr);
    std::cerr << std::setprecision(std::numeric_limits<T>::digits10) << str << ": " << value
              << "\nSig: " << sig_val.high << " " << sig_val.low
              << "\nExp: " << ptr << "\n" << std::endl;
}

template<typename T>
auto test_asin_edge() -> void
{
    using nl = std::numeric_limits<T>;

    const T tiny0 { nl::epsilon() * 999 / 1000 };
    const T tiny1 { nl::epsilon() };
    const T tiny2 { nl::epsilon() * 1000 / 999 };

    const T asin_tiny0 { asin(tiny0) };
    const T asin_tiny1 { asin(tiny1) };
    const T asin_tiny2 { asin(tiny2) };

    // tiny1: 1
    // tiny2: 1.001001
    // tiny0: 0.999
    // tiny1: 1
    // tiny2: 1.001001001001001
    // tiny0: 0.999
    // tiny1: 1
    // tiny2: 1.001001001001001001001001001001001

    constexpr T ctrl_tiny2
    {
          std::numeric_limits<T>::digits10 < 10 ? T("1.001001")
        : std::numeric_limits<T>::digits10 < 20 ? T("1.001001001001001")
        :                                         T("1.001001001001001001001001001001001")
    };

    BOOST_TEST_EQ(asin_tiny0 / nl::epsilon(), T(999, -3));
    BOOST_TEST_EQ(asin_tiny1 / nl::epsilon(), T(1));
    BOOST_TEST_EQ(asin_tiny2 / nl::epsilon(), ctrl_tiny2);

    constexpr T half_pi { numbers::pi_v<T> / 2 };

    std::random_device rd;
    std::mt19937_64 gen(rd());

    gen.seed(local::time_point<typename std::mt19937_64::result_type>());

    auto dis =
      std::uniform_real_distribution<float>
      {
        static_cast<float>(1.01F),
        static_cast<float>(1.04F)
      };

    BOOST_TEST_EQ(asin((my_zero<T>() * static_cast<T>(dis(gen))) + my_one<T>()), half_pi);
    BOOST_TEST_EQ(asin((my_zero<T>() * static_cast<T>(dis(gen))) - my_one<T>()), -half_pi);

    for(auto i = static_cast<unsigned>(UINT8_C(0)); i < static_cast<unsigned>(UINT8_C(4)); ++i)
    {
        static_cast<void>(i);

        const T near_one { my_one<T>() * static_cast<T>(dis(gen)) };

        const T arg_one { T { static_cast<int>(near_one) } };

        const auto val_one_pos = asin(+arg_one);
        const auto val_one_neg = asin(-arg_one);

        BOOST_TEST_EQ(val_one_pos, +half_pi);
        BOOST_TEST_EQ(val_one_neg, -half_pi);
    }

}

template<typename T>
void test_asin_1137()
{
    using nl = std::numeric_limits<T>;

    const T tiny0 { nl::epsilon() * 999/1000 };
    const T tiny1 { nl::epsilon() };
    const T tiny2 { nl::epsilon() * 1000/999 };

    BOOST_TEST(tiny0 != tiny1);
    BOOST_TEST(tiny1 != tiny2);

    std::stringstream strm { };

    BOOST_TEST_EQ(tiny0, asin(tiny0));
    BOOST_TEST_EQ(tiny1, asin(tiny1));
    BOOST_TEST_EQ(tiny2, asin(tiny2));

    const T sqrt_tiny0 { sqrt(nl::epsilon() * 999/1000) };
    const T sqrt_tiny1 { sqrt(nl::epsilon()) };
    const T sqrt_tiny2 { sqrt(nl::epsilon() * 1000/999) };

    BOOST_TEST_EQ(sqrt_tiny0, asin(sqrt_tiny0));
    BOOST_TEST_EQ(sqrt_tiny1, asin(sqrt_tiny1));
    BOOST_TEST_EQ(sqrt_tiny2, asin(sqrt_tiny2));

    const T cbrt_tiny0 { cbrt(nl::epsilon() * 999/1000) };
    const T cbrt_tiny1 { cbrt(nl::epsilon()) };
    const T cbrt_tiny2 { cbrt(nl::epsilon() * 1000/999) };
    const T cbrt_tiny3 { cbrt(nl::epsilon() * 1004/999) };

    auto mini_series
    {
        [](const T eps)
        {
            return eps * (1 + (eps / 6) * eps);
        }
    };

    auto is_close
    {
        [](const T a, const T b)
        {
            const T delta { fabs(a - b) };

            return (delta < (std::numeric_limits<T>::epsilon() * 4));
        }
    };

    BOOST_TEST(is_close(asin(cbrt_tiny0), mini_series(cbrt_tiny0)));
    BOOST_TEST(is_close(asin(cbrt_tiny1), mini_series(cbrt_tiny1)));
    BOOST_TEST(is_close(asin(cbrt_tiny2), mini_series(cbrt_tiny2)));
    BOOST_TEST(is_close(asin(cbrt_tiny3), mini_series(cbrt_tiny3)));
}

int main()
{
    #ifdef BOOST_DECIMAL_GENERATE_CONSTANT_SIGS
    print_value("436.021684388252698008009005087997361"_DL, "a0");
    print_value("-4039.21638981374780301605091138334041"_DL, "a1");
    print_value("18097.000997294144904446653101142636"_DL, "a2");
    print_value("-52177.282596450846709087608773379376"_DL, "a3");
    print_value("108708.48432550268314425281408153419"_DL, "a4");
    print_value("-174262.5954622716602957172670677698"_DL, "a5");
    print_value("223493.57422415636689935147124438032"_DL, "a6");
    print_value("-235454.321064437777710838740822821291"_DL, "a7");
    print_value("207630.98777755980752169002184822046"_DL, "a8");
    print_value("-155396.69461689412343100720698901834"_DL, "a9");
    print_value("99746.039943987449311614022410223385"_DL, "a10");
    print_value("-55350.309707359479385689369358768157"_DL, "a11");
    print_value("26715.867014556930852695472313352703"_DL, "a12");
    print_value("-11268.338244572170249967527464615569"_DL, "a13");
    print_value("4167.6576373496175687445580059590862"_DL, "a14");
    print_value("-1354.9661562313129205074831860545212"_DL, "a15");
    print_value("387.85110009446107984667231846782873"_DL, "a16");
    print_value("-97.817489091457389284370953518072941"_DL, "a17");
    print_value("21.743307363075844418345038236692007"_DL, "a18");
    print_value("-4.2480133538896834744681916927887348"_DL, "a19");
    print_value("0.73292313819882720927775694986695822"_DL, "a20");
    print_value("-0.1010376444122805424367275705575875"_DL, "a21");
    print_value("0.014666852097615399958677037388739653"_DL, "a22");
    print_value("0.0098574983213099696795218405193307645"_DL, "a23");
    print_value("0.00017008786793851718237248325423866098"_DL, "a24");
    print_value("0.013950081361102626885069793269505888"_DL, "a25");
    print_value("1.1009122133007665306092568908271278e-06"_DL, "a26");
    print_value("0.017352694401278245203946951656646601"_DL, "a27");
    print_value("3.7646439494486735639855121728483381e-09"_DL, "a28");
    print_value("0.022372158921646811423908478031651474"_DL, "a29");
    print_value("6.2818862516685652814845387053772962e-12"_DL, "a30");
    print_value("0.030381944444255037245064480562068303"_DL, "a31");
    print_value("4.5487997852735016804398755617727032e-15"_DL, "a32");
    print_value("0.044642857142857057989046867608414157"_DL, "a33");
    print_value("1.190951954325925523311588515718388e-18"_DL, "a34");
    print_value("0.074999999999999999987961873655488348"_DL, "a35");
    print_value("8.2501737347587131207284128263397492e-23"_DL, "a36");
    print_value("0.16666666666666666666666631637239263"_DL, "a37");
    print_value("7.9432557432242856979871267315136865e-28"_DL, "a38");
    print_value("0.99999999999999999999999999999928086"_DL, "a39");
    print_value("1.0845024738180057189197205194839397e-34"_DL, "a40");

    throw;
    #endif

    test_asin<decimal32_t>();
    test_asin<decimal64_t>();
    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    test_asin<decimal128_t>();
    #endif

    test_asin<decimal_fast32_t>();

    test_asin_edge<decimal32_t>();
    test_asin_edge<decimal64_t>();
    test_asin_edge<decimal128_t>();

    test_asin_1137<decimal32_t>();
    test_asin_1137<decimal64_t>();
    test_asin_1137<decimal128_t>();

    return boost::report_errors();
}

template<typename T> auto my_zero() -> T { T zero { 0 }; return zero; }
template<typename T> auto my_one () -> T { T one  { 1 }; return one; }
