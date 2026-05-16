// Copyright 2024 Matt Borland
// Copyright 2024 Christopher Kormanyos
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// Propogates up from boost.math
#define _SILENCE_CXX23_DENORM_DEPRECATION_WARNING

#include "testing_config.hpp"
#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>

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
#include <iostream>
#include <random>
#include <cmath>

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
static constexpr auto N = static_cast<std::size_t>(128); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(128 >> 4U); // Number of trials
#endif

static std::mt19937_64 rng(42);

using namespace boost::decimal;

template <typename Dec>
void test_sin()
{
    std::uniform_real_distribution<float> dist(-3.14F * 2, 3.14F * 2);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        Dec d1 {val1};

        auto ret_val {std::sin(val1)};
        auto ret_dec {static_cast<float>(sin(d1))};

        if (!BOOST_TEST(std::fabs(ret_val - ret_dec) < 40*std::numeric_limits<float>::epsilon()))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << std::fabs(ret_val - ret_dec) / std::numeric_limits<float>::epsilon() << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST(isinf(sin(std::numeric_limits<Dec>::infinity() * Dec(dist(rng)))));
    BOOST_TEST(isnan(sin(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)))));
    BOOST_TEST_EQ(abs(sin(Dec(0) * Dec(dist(rng)))), Dec(0));

    // Check the phases of large positive/negative arguments.
    using std::atan;

    for(auto x = 0.1F; x < 20.0F; x += 2.0F * atan(1.0F))
    {
        using std::sin;

        BOOST_TEST_EQ((sin(boost::decimal::decimal32_t { x }) < 0), (sin(x) < 0));
    }

    for(auto x = 0.1F; x < 20.0F; x += 2.0F * atan(1.0F))
    {
        using std::sin;

        BOOST_TEST_EQ((sin(boost::decimal::decimal32_t { -x }) < 0), (sin(-x) < 0));
    }
}

template <typename Dec>
void test_cos()
{
    std::uniform_real_distribution<float> dist(-3.14F * 2, 3.14F * 2);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value ? N / 4 : N};
    for (std::size_t n {}; n < max_iter; ++n)
    {
        const auto val1 {dist(rng)};
        Dec d1 {val1};

        auto ret_val {std::cos(val1)};
        auto ret_dec {static_cast<float>(cos(d1))};

        if (!BOOST_TEST(std::fabs(ret_val - ret_dec) < 35*std::numeric_limits<float>::epsilon()))
        {
            // LCOV_EXCL_START
            std::cerr << "Val 1: " << val1
                      << "\nDec 1: " << d1
                      << "\nRet val: " << ret_val
                      << "\nRet dec: " << ret_dec
                      << "\nEps: " << std::fabs(ret_val - ret_dec) / std::numeric_limits<float>::epsilon() << std::endl;
            // LCOV_EXCL_STOP
        }
    }

    BOOST_TEST(isinf(cos(std::numeric_limits<Dec>::infinity() * Dec(dist(rng)))));
    BOOST_TEST(isnan(cos(std::numeric_limits<Dec>::quiet_NaN() * Dec(dist(rng)))));
    BOOST_TEST_EQ(cos(Dec(0) * Dec(dist(rng))), Dec(1));

    // Check the phases of large positive/negative arguments.
    using std::atan;

    for(auto x = 0.1F; x < 20.0F; x += 2.0F * atan(1.0F))
    {
        using std::cos;

        BOOST_TEST_EQ((cos(boost::decimal::decimal32_t { x }) < 0), (cos(x) < 0));
    }

    for(auto x = 0.1F; x < 20.0F; x += 2.0F * atan(1.0F))
    {
        using std::cos;

        BOOST_TEST_EQ((cos(boost::decimal::decimal32_t { -x }) < 0), (cos(-x) < 0));
    }
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

namespace local
{
  template<typename NumericType>
  auto is_close_fraction(const NumericType& a,
                         const NumericType& b,
                         const NumericType& tol) noexcept -> bool
  {
    using std::fabs;

    auto result_is_ok = bool { };

    NumericType delta { };

    if(b == static_cast<NumericType>(0))
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

  template <typename T>
  auto test_sin_128(const int tol_factor) -> bool
  {
    using decimal_type = T;

    using str_ctrl_array_type = std::array<const char*, 41U>;

    const str_ctrl_array_type ctrl_strings =
    {{
       // Table[N[Sin[n + n/10], 36], {n, -20, 20, 1}]
       "0.00885130929040387592169025681577233246",
       "-0.887157528692350427205640661441011342",
       "-0.813673737507104955433222744609065147",
       "0.148999025814198104343982890664237216",
       "0.948844497918124441518161248410867044",
       "0.711785342369123065842340834512896188",
       "-0.303118356745702602523931087729992333",
       "-0.986771964274613470590033455846296362",
       "-0.592073514707223565308069810796062123",
       "0.449647464534601151267544078200296711",
       "0.999990206550703457051564899025522107",
       "0.457535893775321044413818107505363926",
       "-0.584917192891762253530931311812375128",
       "-0.988168233877000368552393618723663021",
       "-0.311541363513378174354985105592593697",
       "0.705540325570391906231919175522070079",
       "0.951602073889515954035392333380387684",
       "0.157745694143248382011654277602482371",
       "-0.808496403819590184304036910416119065",
       "-0.891207360061435339951802577871703538",
       "0",
       "0.891207360061435339951802577871703538",
       "0.808496403819590184304036910416119065",
       "-0.157745694143248382011654277602482371",
       "-0.951602073889515954035392333380387684",
       "-0.705540325570391906231919175522070079",
       "0.311541363513378174354985105592593697",
       "0.988168233877000368552393618723663021",
       "0.584917192891762253530931311812375128",
       "-0.457535893775321044413818107505363926",
       "-0.999990206550703457051564899025522107",
       "-0.449647464534601151267544078200296711",
       "0.592073514707223565308069810796062123",
       "0.986771964274613470590033455846296362",
       "0.303118356745702602523931087729992333",
       "-0.711785342369123065842340834512896188",
       "-0.948844497918124441518161248410867044",
       "-0.148999025814198104343982890664237216",
       "0.813673737507104955433222744609065147",
       "0.887157528692350427205640661441011342",
       "-0.00885130929040387592169025681577233246",
    }};

    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> sin_values  { };
    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> ctrl_values { };

    int nx { -20 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<str_ctrl_array_type>::value; ++i)
    {
      const decimal_type x_arg =
        decimal_type
        {
            decimal_type { nx }
          + decimal_type { nx, -1 }
        };

        ++nx;

        sin_values[i] = sin(x_arg);

        static_cast<void>
        (
          from_chars(ctrl_strings[i], ctrl_strings[i] + std::strlen(ctrl_strings[i]), ctrl_values[i])
        );

      const decimal_type local_tol = ((ctrl_values[i] < decimal_type { 1, -1 }) ? my_tol * 16 : my_tol);

      const auto result_sin_is_ok = is_close_fraction(sin_values[i], ctrl_values[i], local_tol);

      result_is_ok = (result_sin_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

  template <typename T>
  auto test_cos_128(const int tol_factor) -> bool
  {
    using decimal_type = T;

    using str_ctrl_array_type = std::array<const char*, 41U>;

    const str_ctrl_array_type ctrl_strings =
    {{
       // Table[N[Cos[n + n/10], 36], {n, -20, 20, 1}]
       "-0.999960826394637126454174739212693774",
       "-0.461466704415910626922141930570155132",
       "0.581321811814436275127478838749985834",
       "0.988837342694145995574183803962615751",
       "0.315743754919241977341902454154186407",
       "-0.702397057502713532361560769391904267",
       "-0.952952916887180197669329573420619689",
       "-0.162114436499717558295988827296285793",
       "0.805883957640450316780870877627822774",
       "0.893206111509322690144989864397000805",
       "0.00442569798805078574835502472394157323",
       "-0.889191152625361054634438698689106779",
       "-0.811093014061655562889085504219324484",
       "0.153373862037864525977384239572053515",
       "0.950232591958529466219737721668197376",
       "0.708669774291260000027421181325843735",
       "-0.307332869978419683119139742217712371",
       "-0.987479769908864883936591051102853311",
       "-0.588501117255345708524142612654928416",
       "0.453596121425577387771370051784716122",
       "1",
       "0.453596121425577387771370051784716122",
       "-0.588501117255345708524142612654928416",
       "-0.987479769908864883936591051102853311",
       "-0.307332869978419683119139742217712371",
       "0.708669774291260000027421181325843735",
       "0.950232591958529466219737721668197376",
       "0.153373862037864525977384239572053515",
       "-0.811093014061655562889085504219324484",
       "-0.889191152625361054634438698689106779",
       "0.00442569798805078574835502472394157323",
       "0.893206111509322690144989864397000805",
       "0.805883957640450316780870877627822774",
       "-0.162114436499717558295988827296285793",
       "-0.952952916887180197669329573420619689",
       "-0.702397057502713532361560769391904267",
       "0.315743754919241977341902454154186407",
       "0.988837342694145995574183803962615751",
       "0.581321811814436275127478838749985834",
       "-0.461466704415910626922141930570155132",
       "-0.999960826394637126454174739212693774",
    }};

    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> cos_values  { };
    std::array<decimal_type, std::tuple_size<str_ctrl_array_type>::value> ctrl_values { };

    int nx { -20 };

    bool result_is_ok { true };

    const decimal_type my_tol { std::numeric_limits<decimal_type>::epsilon() * static_cast<decimal_type>(tol_factor) };

    for(auto i = static_cast<std::size_t>(UINT8_C(0)); i < std::tuple_size<str_ctrl_array_type>::value; ++i)
    {
      const decimal_type x_arg =
        decimal_type
        {
            decimal_type { nx }
          + decimal_type { nx, -1 }
        };

        ++nx;

        cos_values[i] = cos(x_arg);

        static_cast<void>
        (
          from_chars(ctrl_strings[i], ctrl_strings[i] + std::strlen(ctrl_strings[i]), ctrl_values[i])
        );

      const decimal_type local_tol = ((ctrl_values[i] < decimal_type { 1, -1 }) ? my_tol * 16 : my_tol);

      const auto result_cos_is_ok = is_close_fraction(cos_values[i], ctrl_values[i], local_tol);

      result_is_ok = (result_cos_is_ok && result_is_ok);
    }

    return result_is_ok;
  }

} // namespace local

int main()
{
    #ifdef BOOST_DECIMAL_GENERATE_CONSTANT_SIGS
    std::cerr << "----- Sin Coeffs -----" << '\n';
    print_value("1.5699342435209476025651717041741222e-19"_DL, "a0");
    print_value("-8.8132732956573375066199565461908719e-18"_DL, "a1");
    print_value("1.3863524195221541686408628558203953e-18"_DL, "a2");
    print_value("2.8092138674699095038603966669197672e-15"_DL, "a3");
    print_value("2.6602926311836597886374265715483009e-18"_DL, "a4");
    print_value("-7.647187644614729441998912554111624e-13"_DL, "a5");
    print_value("1.6631026334876884745954474452210885e-18"_DL, "a6");
    print_value("1.6059043746245914318033425521094091e-10"_DL, "a7");
    print_value("3.8855818741866909397363716405229141e-19"_DL, "a8");
    print_value("-2.5052108385573133011847226381434009e-08"_DL, "a9");
    print_value("3.4914886265734843190231514748823632e-20"_DL, "a10");
    print_value("2.7557319223985818369477273553859578e-06"_DL, "a11");
    print_value("1.1508316403034518496534319639927136e-21"_DL, "a12");
    print_value("-0.00019841269841269841283663285131581154"_DL, "a13");
    print_value("1.2185770421264807218078195121820301e-23"_DL, "a14");
    print_value("0.0083333333333333333333325747157858247"_DL, "a15");
    print_value("3.151985000765337776035625655498396e-26"_DL, "a16");
    print_value("-0.166666666666666666666666667468363"_DL, "a17");
    print_value("1.0798913896208528750785230421502658e-29"_DL, "a18");
    print_value("0.9999999999999999999999999999994222"_DL, "a19");
    print_value("5.142496035903513218983541015724909e-35"_DL, "a20");

    std::cerr << "\n----- Cos Coeffs -----" << '\n';
    print_value("3.7901566851452528911995533679050154e-19"_DL, "a0");
    print_value("1.6306031360068815018052629353024035e-19"_DL, "a1");
    print_value("-1.5662432214968104749920975944134976e-16"_DL, "a2");
    print_value("7.4986543056686147173668341796175043e-19"_DL, "a3");
    print_value("4.7793843527366502077270638230621663e-14"_DL, "a4");
    print_value("8.6228514128460838465953643567304485e-19"_DL, "a5");
    print_value("-1.1470746211515679895911787383100785e-11"_DL, "a6");
    print_value("3.4035901048127981461895025346148961e-19"_DL, "a7");
    print_value("2.0876756986386685705585269979282106e-09"_DL, "a8");
    print_value("5.0707181385354000943766543756900133e-20"_DL, "a9");
    print_value("-2.7557319223987251623517450696484143e-07"_DL, "a10");
    print_value("2.842434434652202240144372150674076e-21"_DL, "a11");
    print_value("2.4801587301587301131235298728224135-05"_DL, "a12");
    print_value("5.5157104597335436534512866304911642e-23"_DL, "a13");
    print_value("-0.0013888888888888888888937824839517112"_DL, "a14");
    print_value("3.0643338914793183455023135450473855e-25"_DL, "a15");
    print_value("0.0416666666666666666666666538646346421"_DL, "a16");
    print_value("3.2731904841623917801666953504570181e-28"_DL, "a17");
    print_value("-0.50000000000000000000000000000443128"_DL, "a18");
    print_value("2.3830720519892726434235205129382137e-32"_DL, "a19");
    print_value("1"_DL, "a20");

    throw;
    #endif

    test_sin<decimal32_t>();
    test_cos<decimal32_t>();
    test_sin<decimal_fast32_t>();
    test_cos<decimal_fast32_t>();
    test_sin<decimal64_t>();
    test_cos<decimal64_t>();
    test_sin<decimal_fast64_t>();
    test_cos<decimal_fast64_t>();

    {
        const auto result_sin128_is_ok = local::test_sin_128<decimal128_t>(0x800);
        const auto result_cos128_is_ok = local::test_cos_128<decimal128_t>(0x800);

        BOOST_TEST(result_sin128_is_ok);
        BOOST_TEST(result_cos128_is_ok);
    }

    {
        const auto result_sin128_is_ok = local::test_sin_128<decimal_fast128_t>(0x800);
        const auto result_cos128_is_ok = local::test_cos_128<decimal_fast128_t>(0x800);

        BOOST_TEST(result_sin128_is_ok);
        BOOST_TEST(result_cos128_is_ok);
    }

    return boost::report_errors();
}
