// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This file demonstrates the difference between decompose, frexp10 and
// normalize, all of which inspect the internal representation of a
// decimal floating point value but expose it in different ways.

#include <boost/decimal/decimal32_t.hpp>    // For the type decimal32_t
#include <boost/decimal/decimal_fast32_t.hpp>// For the type decimal_fast32_t
#include <boost/decimal/cmath.hpp>          // For decompose, frexp10, normalize
#include <boost/decimal/iostream.hpp>       // For decimal support for <iostream>
#include <array>
#include <iostream>

template <typename Decimal>
void show(const Decimal value)
{
    // decompose returns the encoded significand, biased exponent and sign.
    // For IEEE types this preserves the cohort that was used at construction.
    // For fast types the value is normalized in the constructor, so the
    // components always reflect the normalized form.
    const auto components {boost::decimal::decompose(value)};

    // frexp10 returns the cohort-agnostic significand together with the
    // matching exponent written through the pointer argument. The significand
    // is normalized to the full precision of the type (for decimal32_t the
    // range is [1'000'000, 9'999'999]).
    int frexp_exp {};
    const auto frexp_sig {boost::decimal::frexp10(value, &frexp_exp)};

    // normalize returns a decimal of the same type whose representation has
    // had the cohort effects removed. For fast types this is a no-op since
    // they are already normalized.
    const auto normalized {boost::decimal::normalize(value)};

    std::cout << "  value      = " << value << '\n'
              << "  decompose  : sig = " << components.sig
              << ", exp = " << components.exp
              << ", sign = " << components.sign << '\n'
              << "  frexp10    : sig = " << frexp_sig
              << ", exp = " << frexp_exp << '\n'
              << "  normalize  = " << normalized << "\n\n";
}

int main()
{
    // All three values compare equal but use different cohorts.
    constexpr std::array<boost::decimal::decimal32_t, 3> ieee_values {
        boost::decimal::decimal32_t{3, 2},
        boost::decimal::decimal32_t{300, 0},
        boost::decimal::decimal32_t{3000000, -4}
    };

    std::cout << "IEEE decimal32_t (cohort is preserved by the encoding):\n\n";
    for (const auto& v : ieee_values)
    {
        show(v);
    }

    // The same set of inputs given to a fast type all encode identically.
    constexpr std::array<boost::decimal::decimal_fast32_t, 3> fast_values {
        boost::decimal::decimal_fast32_t{3, 2},
        boost::decimal::decimal_fast32_t{300, 0},
        boost::decimal::decimal_fast32_t{3000000, -4}
    };

    std::cout << "decimal_fast32_t (always normalized internally):\n\n";
    for (const auto& v : fast_values)
    {
        show(v);
    }

    return 0;
}
