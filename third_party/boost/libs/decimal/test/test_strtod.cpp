// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "testing_config.hpp"
#include "mini_to_chars.hpp"
#include <boost/decimal/cstdlib.hpp>
#include <boost/decimal/iostream.hpp>
#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <random>
#include <cwchar>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wold-style-cast"
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wundef"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#include <boost/core/lightweight_test.hpp>

using namespace boost::decimal;

#if !defined(BOOST_DECIMAL_REDUCE_TEST_DEPTH)
static constexpr auto N = static_cast<std::size_t>(1024); // Number of trials
#else
static constexpr auto N = static_cast<std::size_t>(1024 >> 4U); // Number of trials
#endif

template <typename T>
void roundtrip_strtod()
{
    std::mt19937_64 rng(42);

    std::uniform_int_distribution<std::int64_t> sig(1'000'000, 9'999'999);
    std::uniform_int_distribution<std::int64_t> exp(-15, 15);

    constexpr auto max_iter {std::is_same<T, decimal128_t>::value ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        const T val {sig(rng), exp(rng)};
        char buffer[256] {};
        auto r = to_chars(buffer, buffer + sizeof(buffer), val);
        if (!r)
        {
            continue; // LCOV_EXCL_LINE
        }
        else
        {
            *r.ptr = '\0';
        }
        char* endptr {};

        const T return_val {boost::decimal::strtod<T>(buffer, &endptr)};

        if (!BOOST_TEST_EQ(val, return_val))
        {
            // LCOV_EXCL_START
            std::cerr << std::scientific
                      << std::setprecision(std::numeric_limits<T>::digits10)
                      << "Counter: " << i
                      << "\nVal 1: " << val
                      << "\nVal 2: " << return_val << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename T>
void roundtrip_wcstrtod()
{
    std::mt19937_64 rng(42);

    std::uniform_int_distribution<std::int64_t> sig(1'000, 9'999);
    std::uniform_int_distribution<std::int64_t> exp(-15, 15);

    constexpr auto max_iter {std::is_same<T, decimal128_t>::value ? N / 4 : N};
    for (std::size_t i {}; i < max_iter; ++i)
    {
        const T val {sig(rng), exp(rng)};
        std::wstringstream ss;
        ss << std::setprecision(std::numeric_limits<T>::digits10);
        ss << val;
        wchar_t* endptr {};

        const T return_val {boost::decimal::wcstod<T>(ss.str().c_str(), &endptr)};

        if (!BOOST_TEST_EQ(val, return_val))
        {
            // LCOV_EXCL_START
            std::cerr << std::scientific
                      << std::setprecision(std::numeric_limits<T>::digits10)
                      << "Counter: " << i
                      << "\nVal 1: " << val
                      << "\nVal 2: " << return_val << std::endl;
            // LCOV_EXCL_STOP
        }
    }
}

template <typename T>
void test_strtod_edges()
{
    errno = 0;
    BOOST_TEST(isnan(boost::decimal::strtod<T>(nullptr, nullptr))) && BOOST_TEST_EQ(errno, EINVAL);

    errno = 0;
    BOOST_TEST(isnan(boost::decimal::wcstod<T>(nullptr, nullptr))) && BOOST_TEST_EQ(errno, EINVAL);

    errno = 0;
    const char* snan_str = "nan(snan)";
    BOOST_TEST(isnan(boost::decimal::strtod<T>(snan_str, nullptr))) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    const char* qnan_str = "nan";
    BOOST_TEST(isnan(boost::decimal::strtod<T>(qnan_str, nullptr))) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    const char* inf_str = "inf";
    BOOST_TEST(isinf(boost::decimal::strtod<T>(inf_str, nullptr))) && BOOST_TEST_EQ(errno, 0);

    errno = 0;
    const char* junk_str = "junk";
    BOOST_TEST(isnan(boost::decimal::strtod<T>(junk_str, nullptr))) && BOOST_TEST_EQ(errno, EINVAL);
}

#ifndef BOOST_DECIMAL_DISABLE_EXCEPTIONS

template <typename T>
void test_locales()
{
    const char buffer[] = "1,1897e+02";
    const auto valdiation_value = static_cast<T>(1.1897e+02);

    try
    {
        #ifdef BOOST_MSVC
        std::locale::global(std::locale("German"));
        #else
        std::locale::global(std::locale("de_DE.UTF-8"));
        #endif
    }
    // LCOV_EXCL_START
    catch (...)
    {
        std::cerr << "Locale not installed. Skipping test." << std::endl;
        return;
    }
    // LCOV_EXCL_STOP

    char* enddptr;
    const auto val = boost::decimal::strtod<T>(buffer, &enddptr);
    BOOST_TEST_EQ(valdiation_value, val);
}

#endif

template <typename T>
void test_spot(const char* str, T val)
{
    char* endptr;
    auto return_val = boost::decimal::strtod<T>(str, &endptr);
    if (!BOOST_TEST_EQ(val, return_val))
    {
        // LCOV_EXCL_START
        std::cerr << std::scientific
                  << std::setprecision(std::numeric_limits<T>::digits10)
                  << "Val 1: " << val
                  << "\nVal 2: " << return_val << std::endl;
        // LCOV_EXCL_STOP
    }
}

template <typename T>
void test_alloc()
{
    const char buffer[] =
    "1.98264513982765396485719650498261498564729856318926451982754398672495874691824659645"
    "1092348576918246513984659103485721634589126458619584619051982671298642158641958264819"
    "0519826492851648192519856419258612541685159172360917510925761093561879512865908275198"
    "2651982563012895610769517352609182751093560198257610928576510481965234109182954019825"
    "7610935761093571092387540619275610395718290513769285109237856091827569104857109358109"
    "3857910671958109375610935865109834571986201958109246581398246396857109651089639186275"
    "1238961023875609182763509182703618927450916837259013650296857109258607129845760918576"
    "5109384659103865791083659127490287596102935761092375819627109382651098346598163450983"
    "5109384750198273561907351098627531098465109384750983610984675109384750984610987456109"
    "3847561098475260193847609187326501982735601985761904782659013487569102738957109184756"
    "9018743691872634509287561098274510938746509827536019827536091827650918327650918265091"
    "8237560918275961083726510983726510982650198275609182375609187236510982356109827509862"
    "3140985671294567190264790263190756290824609182356907123056918276091837561097812365908"
    "1726509817365109823561097235690719823560987126509812376598712635098712365098475610982"
    "3756098135760981273965019827609182375609182756098123765098123765081273650982173650982"
    "1735091823756109821736509182763905109286510982653109826501982653109835601928375609182"
    "5673098217536098217560918273560982165309821753609817365098271365098217365091827651098"
    "2735610982735610982735610982713509182756310982715630982735610982175631098275610982735"
    "6091827561098273650981253609821753609821765310982756309821763509182756309821756309182"
    "7563098217563109827653091827563098217653091827650918273560982716350918273561098217536"
    "0918276350918276530982176531098275609182756309827156310982735609182753609827153609182"
    "7563098271563908127563091827563109827156309827153609827365109827153098271536098271536"
    "0918275631098271536098271560918275630982716530918275630982715360918275360982715360918"
    "2753609827153609182756309827156309182756310982715630982715360982715360982715360982715"
    "3609827153609827156309827153609821765309182756091827356098271653091827356098271563091"
    "8275630982716350918273561098271536098271536098271536098271563098271536098271536098271"
    "536098271563098271536098271536098271536"
    "198264513982765396485719650498261498564729856318926451982754398672495874691824659645"
    "1092348576918246513984659103485721634589126458619584619051982671298642158641958264819"
    "0519826492851648192519856419258612541685159172360917510925761093561879512865908275198"
    "2651982563012895610769517352609182751093560198257610928576510481965234109182954019825"
    "7610935761093571092387540619275610395718290513769285109237856091827569104857109358109"
    "3857910671958109375610935865109834571986201958109246581398246396857109651089639186275"
    "1238961023875609182763509182703618927450916837259013650296857109258607129845760918576"
    "5109384659103865791083659127490287596102935761092375819627109382651098346598163450983"
    "5109384750198273561907351098627531098465109384750983610984675109384750984610987456109"
    "3847561098475260193847609187326501982735601985761904782659013487569102738957109184756"
    "9018743691872634509287561098274510938746509827536019827536091827650918327650918265091"
    "8237560918275961083726510983726510982650198275609182375609187236510982356109827509862"
    "3140985671294567190264790263190756290824609182356907123056918276091837561097812365908"
    "1726509817365109823561097235690719823560987126509812376598712635098712365098475610982"
    "3756098135760981273965019827609182375609182756098123765098123765081273650982173650982"
    "1735091823756109821736509182763905109286510982653109826501982653109835601928375609182"
    "5673098217536098217560918273560982165309821753609817365098271365098217365091827651098"
    "2735610982735610982735610982713509182756310982715630982735610982175631098275610982735"
    "6091827561098273650981253609821753609821765310982756309821763509182756309821756309182"
    "7563098217563109827653091827563098217653091827650918273560982716350918273561098217536"
    "0918276350918276530982176531098275609182756309827156310982735609182753609827153609182"
    "7563098271563908127563091827563109827156309827153609827365109827153098271536098271536"
    "0918275631098271536098271560918275630982716530918275630982715360918275360982715360918"
    "2753609827153609182756309827156309182756310982715630982715360982715360982715360982715"
    "3609827153609827156309827153609821765309182756091827356098271653091827356098271563091"
    "8275630982716350918273561098271536098271536098271536098271563098271536098271536098271"
    "536098271563098271536098271536098271536";

    const auto validation_value {static_cast<T>(1.982645139827653964857196)};

    char* enddptr;
    auto return_value {boost::decimal::strtod<T>(buffer, &enddptr)};
    BOOST_TEST_EQ(return_value, validation_value);

    wchar_t wc_buffer[5000] {};
    for (std::size_t i {}; i < sizeof(buffer); ++i)
    {
        wc_buffer[i] = static_cast<wchar_t>(buffer[i]);
    }

    wchar_t* wc_endptr;
    auto wc_return_value {boost::decimal::wcstod<T>(wc_buffer, &wc_endptr)};
    BOOST_TEST_EQ(wc_return_value, validation_value);
}

int main()
{
    roundtrip_strtod<decimal32_t>();
    roundtrip_wcstrtod<decimal32_t>();
    test_strtod_edges<decimal32_t>();

    roundtrip_strtod<decimal64_t>();
    roundtrip_wcstrtod<decimal64_t>();
    test_strtod_edges<decimal64_t>();

    roundtrip_strtod<decimal128_t>();
    roundtrip_wcstrtod<decimal128_t>();
    test_strtod_edges<decimal128_t>();

    test_spot("2.9379440e-03", decimal32_t{UINT32_C(29379440), -10});
    test_spot("3.4255120e+06", decimal32_t{3.4255120e+06F});
    test_spot("0.052306360", decimal32_t{ 5.2306360e-02});

    test_alloc<decimal32_t>();
    test_alloc<decimal64_t>();

    // Homebrew GCC does not support locales
    #if !(defined(__GNUC__) && __GNUC__ >= 5 && defined(__APPLE__)) && !defined(BOOST_DECIMAL_QEMU_TEST) && !defined(BOOST_DECIMAL_DISABLE_EXCEPTIONS)
    test_locales<decimal32_t>();
    test_locales<decimal64_t>();
    test_locales<decimal128_t>();
    #endif

    return boost::report_errors();
}
