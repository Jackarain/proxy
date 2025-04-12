//
// Copyright (c) 2022-2025 Alexander Grund
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include "../src/util/numeric_conversion.hpp"
#include "boostLocale/test/tools.hpp"
#include <boost/charconv/limits.hpp>
#include <boost/charconv/to_chars.hpp>
#include <algorithm>
#include <limits>
#ifdef BOOST_LOCALE_WITH_ICU
#    include <random>
#    include <unicode/numfmt.h>
#endif

void test_try_to_int()
{
    using boost::locale::util::try_to_int;

    int v = 1337;
    if TEST(try_to_int("0", v))
        TEST_EQ(v, 0);

    if TEST(try_to_int("42", v))
        TEST_EQ(v, 42);

    if TEST(try_to_int("-1337", v))
        TEST_EQ(v, -1337);

    if TEST(try_to_int("+1337", v))
        TEST_EQ(v, +1337);

    std::ostringstream ss;
    ss.imbue(std::locale::classic());
    empty_stream(ss) << std::numeric_limits<int>::min();
    if TEST(try_to_int(ss.str(), v))
        TEST_EQ(v, std::numeric_limits<int>::min());
    empty_stream(ss) << std::numeric_limits<int>::max();
    if TEST(try_to_int(ss.str(), v))
        TEST_EQ(v, std::numeric_limits<int>::max());

    TEST(!try_to_int("", v));
    TEST(!try_to_int("+", v));
    TEST(!try_to_int("-", v));
    TEST(!try_to_int("++", v));
    TEST(!try_to_int("+-", v));
    TEST(!try_to_int("++1", v));
    TEST(!try_to_int("+-1", v));
    TEST(!try_to_int("a", v));
    TEST(!try_to_int("1.", v));
    TEST(!try_to_int("1a", v));
    TEST(!try_to_int("a1", v));
    static_assert(sizeof(long long) > sizeof(int), "Value below under/overflows!");
    empty_stream(ss) << static_cast<long long>(std::numeric_limits<int>::min()) - 1;
    TEST(!try_to_int(ss.str(), v));
    empty_stream(ss) << static_cast<long long>(std::numeric_limits<int>::max()) + 1;
    TEST(!try_to_int(ss.str(), v));
}

void test_try_scientific_to_int()
{
    using boost::locale::util::try_scientific_to_int;

    for(const auto s : {"255", "255E0", "2.55E2", "255E+0", "2.55E+2"}) {
        TEST_CONTEXT(s);
        uint8_t v;
        if TEST(try_scientific_to_int(s, v))
            TEST_EQ(v, 255);
    }
    for(const auto s : {"0", "0E0", "0.0E1", "0.0E2", "0.00E2"}) {
        TEST_CONTEXT(s);
        uint8_t v;
        if TEST(try_scientific_to_int(s, v))
            TEST_EQ(v, 0);
    }
    for(const auto s : {"3", "3E0", "0.3E1", "0.03E2"}) {
        TEST_CONTEXT(s);
        uint8_t v;
        if TEST(try_scientific_to_int(s, v))
            TEST_EQ(v, 3);
    }
    for(const auto s : {"50", "5E1", "0.5E2"}) {
        TEST_CONTEXT(s);
        uint8_t v;
        if TEST(try_scientific_to_int(s, v))
            TEST_EQ(v, 50);
    }
    for(const auto s : {
          // clang-format off
            "-1.1E1",
            // Too large
            "256", "256E0", "2.56E2", "3.0E2", "3.00E2", "300", "399", "3.99E2", "999", "9.99E2",
            "1000","1000E0", "100E1", "10E2", "1E3","1.0E3",
            // negative
            "-1", "-1.1E1",
            // Fractional
            "1.1", "1.1E0", "1.01E1", "1.001E2", "1E-1", "1.1E-1", "0.1E-1",
            // Possibly fractional: non-normalized E notation
            "1.0", "1.0E0", "1.00E1", "1.000E2",
            "0.0", "0.0E0", "0.00E1", "0.000E2",
            "10E-1", "10.0E-1", "0.0E-1",
            // Possibly fractional: Trailing zeros not covered by exponent
            "1.10E1", "1.010E2", "0.10E1", "0.010E2", "0.0010E3", "0.00010E4",
            // Bogus characters
            "a",     "aE0",   "1.aE1",   "a.1E1",  "1Ea",   "1.1Ea",
            "1a",   "1aE0",  "1.1aE1",  "1a.1E1",  "1E1a",  "1.1E1a",
            "a1",   "a1E0",  "1.a1E1",  "a1.1E1",  "1Ea1",  "1.1Ea1",
            "1a1", "1a1E0", "1.1a1E1", "1a1.1E1",  "1E1a1", "1.1E1a1",
          // clang-format on
        })
    {
        TEST_CONTEXT(s);
        uint8_t v;
        TEST(!try_scientific_to_int(s, v));
    }
}

#ifdef BOOST_LOCALE_WITH_ICU
template<typename T>
void spotcheck_icu_parsing(icu::NumberFormat& parser, const T val)
{
    TEST_CONTEXT(val);
    char buf[boost::charconv::limits<T>::max_chars10];
    const auto r = boost::charconv::to_chars(buf, std::end(buf), val);
    if(!TEST(r))
        return; // LCOV_EXCL_LINE
    UChar u_buf[boost::charconv::limits<T>::max_chars10];
    std::copy(buf, r.ptr, u_buf);
    icu::UnicodeString tmp(false, u_buf, static_cast<int32_t>(r.ptr - buf));

    icu::Formattable parsedByICU;
    UErrorCode err = U_ZERO_ERROR;
    parser.parse(tmp, parsedByICU, err);
    if TEST(U_SUCCESS(err)) {
        T parsedVal{};
        if TEST(boost::locale::util::try_parse_icu(parsedByICU, parsedVal))
            TEST_EQ(parsedVal, val);
    }
}

void test_try_parse_icu()
{
    UErrorCode err = U_ZERO_ERROR;
    std::unique_ptr<icu::NumberFormat> parser(icu::NumberFormat::createInstance(icu::Locale::getEnglish(), err));
    TEST_REQUIRE(U_SUCCESS(err));
    using limits = std::numeric_limits<uint64_t>;
    for(const uint64_t v : {limits::min(), limits::max(), limits::max() - 1, limits::max() / 2})
        spotcheck_icu_parsing(*parser, v);
    for(uint64_t v = 10, c = 1; c < limits::max() / 10; c = v, v *= 10) {
        spotcheck_icu_parsing(*parser, v); // 100[...]0
        const auto v1 = limits::max() / v;
        const auto v2 = v1 * v; // Large with trailing zeros
        spotcheck_icu_parsing(*parser, v1);
        spotcheck_icu_parsing(*parser, v2);
    }
    // Unable to parse to uint64_t
    const char16_t* bigVal1 = u"18446744073709551616";    // UINT64_MAX + 1
    const char16_t* bigVal2 = u"184467440737095516150";   // UINT64_MAX * 10
    const char16_t* signedVal1 = u"-9223372036854775808"; // INT64_MIN
    const char16_t* signedVal2 = u"-9223372036854775809"; // INT64_MIN - 1
    for(const auto* v : {bigVal1, bigVal2, signedVal1, signedVal2}) {
        TEST_CONTEXT(v);
        // ICU < 59 doesn't use char16_t but another 2 byte type
        icu::UnicodeString tmp(true, reinterpret_cast<const UChar*>(v), -1);

        icu::Formattable parsedByICU;
        parser->parse(tmp, parsedByICU, err);
        if TEST(U_SUCCESS(err)) {
            uint64_t parsedVal{};
            TEST(!boost::locale::util::try_parse_icu(parsedByICU, parsedVal));
        }
    }
    {
        icu::Formattable invalidNumber{"Str"};
        uint64_t parsedVal{};
        TEST(!boost::locale::util::try_parse_icu(invalidNumber, parsedVal));
    }
    // Test some numbers randomly to find missed cases
    std::mt19937_64 mt{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> distr;
    for(int i = 0; i < 42; ++i)
        spotcheck_icu_parsing(*parser, distr(mt));
}
#endif

void test_main(int /*argc*/, char** /*argv*/)
{
    test_try_to_int();
    test_try_scientific_to_int();
#ifdef BOOST_LOCALE_WITH_ICU
    test_try_parse_icu();
#endif
}
