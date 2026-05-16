// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

// Propogates up from boost.math
#define _SILENCE_CXX23_DENORM_DEPRECATION_WARNING

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4714) // Marked as forceinline but not inlined
#endif

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
#include <boost/math/special_functions/legendre.hpp>
#include <boost/core/lightweight_test.hpp>

#include <cmath>
#include <iostream>
#include <random>

static constexpr std::size_t N {10};

static std::mt19937_64 rng(42);

using namespace boost::decimal;

template <typename Dec>
void test()
{
    std::uniform_real_distribution<float> dist(-1, 1);

    constexpr auto max_iter {std::is_same<Dec, decimal128_t>::value ? static_cast<std::size_t>(2) : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        for (unsigned n {}; n < 4; ++n)
        {
            for (unsigned m {}; m < 4; ++m)
            {
                const auto val1 {dist(rng)};
                Dec d1 {val1};

                auto ret_val {boost::math::legendre_p(static_cast<int>(n), static_cast<int>(m), val1)};
                auto ret_dec {static_cast<float>(assoc_legendre(n, m, d1))};

                if (!BOOST_TEST(std::fabs(ret_val - ret_dec) < 200*std::numeric_limits<float>::epsilon()))
                {
                    // LCOV_EXCL_START
                    std::cerr << "Val 1: " << val1
                              << "\nDec 1: " << d1
                              << "\nN: " << n
                              << "\nM: " << m
                              << "\nRet val: " << ret_val
                              << "\nRet dec: " << ret_dec
                              << "\nEps: " << std::fabs(ret_val - ret_dec) / std::numeric_limits<float>::epsilon() << std::endl;
                    // LCOV_EXCL_STOP
                }
            }
        }
    }

    BOOST_TEST(isnan(assoc_legendre(1U, 1U, Dec{dist(rng)} * std::numeric_limits<Dec>::signaling_NaN())));
    BOOST_TEST(isnan(assoc_legendre(1U, 1U, Dec{10})));
    BOOST_TEST(isnan(assoc_legendre(1U, 1U, Dec{-10})));
    BOOST_TEST(isnan(assoc_legendre(200U, 1U, Dec{dist(rng)} * Dec{1})));
}

// LCOV_EXCL_START

template <typename T>
void print_value(T value)
{
    const char* leading_type = std::is_same<T, decimal32_t>::value ? "decimal32_t{UINT32_C(" : "decimal64_t{UINT64_C(";

    int ptr;
    const auto sig_val = frexp10(value, &ptr);
    std::cerr << leading_type << sig_val << ")," << ptr << "}," << std::endl;
}

template <>
void print_value<decimal128_t>(decimal128_t value)
{
    int ptr;
    const auto sig_val = frexp10(value, &ptr);
    std::cerr << "decimal128_t{detail::uint128{UINT64_C(" << sig_val.high << "),UINT64_C(" << sig_val.low << ")}," << ptr << "}," << std::endl;
}

template <typename T>
void print_table_constants()
{
    constexpr std::array<long double, 100> p0_values = {{
       1.0L,
       1.0L,
       2.0L,
       3.0L,
       8.0L,
       15.0L,
       48.0L,
       105.0L,
       384.0L,
       945.0L,
       3840.0L,
       10395.0L,
       46080.0L,
       135135.0L,
       645120.0L,
       2027025.0L,
       10321920.0L,
       34459425.0L,
       185794560.0L,
       654729075.0L,
       3715891200.0L,
       13749310575.0L,
       81749606400.0L,
       316234143225.0L,
       1961990553600.0L,
       7905853580625.0L,
       51011754393600.0L,
       213458046676875.0L,
       1428329123020800.0L,
       6190283353629375.0L,
       42849873690624000.0L,
       191898783962510625.0L,
       1371195958099968000.0L,
       6332659870762850625.0L,
       46620662575398912000.0L,
       221643095476699771872.0L,
       1678343852714360832000.0L,
       8200794532637891558912.0L,
       63777066403145711616000.0L,
       319830986772877770817536.0L,
       2551082656125828464640000.0L,
       13113070457687988603191296.0L,
       107145471557284795514880000.0L,
       563862029680583509939322880.0L,
       4714400748520531002654720000.0L,
       25373791335626257946766213120.0L,
       216862434431944426122117120000.0L,
       1.19256819277443412350660195123e+30L,
       1.040939685273333245386162176e+31L,
       5.84358414459472720557405057843e+31L,
       5.20469842636666622693081088e+32L,
       2.98022791374331087492632867871e+33L,
       2.70644318171066643806031665294e+34L,
       1.57952079428395476368562145181e+35L,
       1.46147931812375987646700259967e+36L,
       8.68736436856175120017183879317e+36L,
       8.18428418149305530833050670861e+37L,
       4.95179769008019818388465763375e+38L,
       4.74688482526597207902538470377e+39L,
       2.92156063714731692835175274895e+40L,
       2.84813089515958324729717166019e+41L,
       1.78215198865986332643151780487e+42L,
       1.76584115499894161332046853613e+43L,
       1.12275575285571389566092316071e+44L,
       1.13013833919932263252509986312e+45L,
       7.29791239356214032193140023643e+45L,
       7.45891303871552937491324710447e+46L,
       4.8896013036866340151926724425e+47L,
       5.07206086632655997482216578727e+48L,
       3.37382489954377747064140031035e+49L,
       3.55044260642859198233748653308e+50L,
       2.39541567867608200433286530438e+51L,
       2.55631867662858622731544215918e+52L,
       1.74865344543353986304616499288e+53L,
       1.89167582070515380816150422921e+54L,
       1.31149008407515489733862363199e+55L,
       1.43767362373591689427585075397e+56L,
       1.00984736473786927084838964096e+57L,
       1.12138542651401517749475505702e+58L,
       7.97779418142916724051044878499e+58L,
       8.97108341211212142017582117102e+59L,
       6.46201328695762546434523497896e+60L,
       7.35628839793193956430025895963e+61L,
       5.3634710281748291356643973989e+62L,
       6.17928225426282923425752572327e+63L,
       4.5589503739486047653637994285e+64L,
       5.31418273866603314144006340662e+65L,
       3.96628682533528614565241834889e+66L,
       4.67648081002610916450379333876e+67L,
       3.52999527454840466967815113372e+68L,
       4.20883272902349824801833796559e+69L,
       3.21229569983904824961523536484e+70L,
       3.87212611070161838810204204451e+71L,
       2.98743500085031487212720311254e+72L,
       3.63979854405952128478239618188e+73L,
       2.83806325080779912851126485978e+74L,
       3.49420660229714043358725976384e+75L,
       2.75292135328356515459462709235e+76L,
       3.42432247025119762492728413431e+77L,
       2.72539213975072950297178632517e+78L,
    }};

    for (auto val : p0_values) {
        print_value(T{val});
    }
}

// LCOV_EXCL_STOP

int main()
{
    test<decimal32_t>();
    test<decimal64_t>();

    #if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
    test<decimal128_t>();
    #endif

    #ifdef BOOST_DECIMAL_GENERATE_ASSOC_LEGENDRE_CONSTANTS
    print_table_constants<decimal32_t>();
    print_table_constants<decimal64_t>();
    print_table_constants<decimal128_t>();
    throw;
    #endif

    test<decimal_fast32_t>();

    return boost::report_errors();
}

#ifdef _MSC_VER
# pragma warning(pop)
#endif
