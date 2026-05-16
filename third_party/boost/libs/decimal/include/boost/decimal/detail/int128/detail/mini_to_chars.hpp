// Copyright 2022 Peter Dimov
// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_DETAIL_MINI_TO_CHARS_HPP
#define BOOST_DECIMAL_DETAIL_INT128_DETAIL_MINI_TO_CHARS_HPP

#include "uint128_imp.hpp"
#include "int128_imp.hpp"

namespace boost {
namespace int128 {
namespace detail {

BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE char lower_case_digit_table[] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f'
};

static_assert(sizeof(lower_case_digit_table) == sizeof(char) * 16, "10 numbers, and 6 letters");

BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE char upper_case_digit_table[] = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F'
};

static_assert(sizeof(upper_case_digit_table) == sizeof(char) * 16, "10 numbers, and 6 letters");

constexpr char* mini_to_chars(char (&buffer)[64], uint128_t v, const int base, const bool uppercase) noexcept
{
    char* last {buffer + 64U};
    *--last = '\0';

    if (v == 0U)
    {
        *--last = '0';
        return last;
    }

    const auto digit_table {uppercase ? upper_case_digit_table : lower_case_digit_table};

    switch (base)
    {
        case 8:
            while (v != 0U)
            {
                constexpr unsigned zero {48U};
                *--last = static_cast<char>(zero + (v & 7U));
                v >>= 3U;
            }
            break;

        case 10:
            while (v != 0U)
            {
                *--last = digit_table[static_cast<std::size_t>(v % 10U)];
                v /= 10U;
            }
            break;

        case 16:
            while (v != 0U)
            {
                *--last = digit_table[static_cast<std::size_t>(v & 15U)];
                v >>= 4U;
            }
            break;

        default:                        // LCOV_EXCL_LINE
            BOOST_DECIMAL_DETAIL_INT128_UNREACHABLE;   // LCOV_EXCL_LINE
    }

    return last;
}

constexpr char* mini_to_chars(char (&buffer)[64], const int128_t v, const int base, const bool uppercase) noexcept
{
    char* p {nullptr};

    if (v < 0)
    {
        // We cant negate the min value inside the signed type, but we know what the result will be
        if (v == (std::numeric_limits<int128_t>::min)())
        {
            p = mini_to_chars(buffer, uint128_t{UINT64_C(0x8000000000000000), 0}, base, uppercase);
        }
        else
        {
            const auto neg_v {-v};
            p = mini_to_chars(buffer, static_cast<uint128_t>(neg_v), base, uppercase);
        }

        *--p = '-';
    }
    else
    {
        p = mini_to_chars(buffer, static_cast<uint128_t>(v), base, uppercase);
    }

    return p;
}

} // namespace detail
} // namespace int128
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_INT128_DETAIL_MINI_TO_CHARS_HPP
