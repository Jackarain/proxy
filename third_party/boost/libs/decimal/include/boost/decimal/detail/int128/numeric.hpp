// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_NUMERIC_HPP
#define BOOST_DECIMAL_DETAIL_INT128_NUMERIC_HPP

#include "bit.hpp"
#include "detail/traits.hpp"
#include <limits>
#include <iostream>

namespace boost {
namespace int128 {

namespace detail {

template <typename IntegerType>
struct reduced_integers
{
    static constexpr bool value {std::is_same<IntegerType, signed char>::value ||
                                 std::is_same<IntegerType, unsigned char>::value ||
                                 std::is_same<IntegerType, signed short>::value ||
                                 std::is_same<IntegerType, unsigned short>::value ||
                                 std::is_same<IntegerType, signed int>::value ||
                                 std::is_same<IntegerType, unsigned int>::value ||
                                 std::is_same<IntegerType, signed long>::value ||
                                 std::is_same<IntegerType, unsigned long>::value ||
                                 std::is_same<IntegerType, signed long long>::value ||
                                 std::is_same<IntegerType, unsigned long long>::value ||
                                 std::is_same<IntegerType, int128_t>::value ||
                                 std::is_same<IntegerType, uint128_t>::value};
};

#if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) || defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)

template <typename IntegerType>
BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE bool is_reduced_integer_v {reduced_integers<IntegerType>::value ||
                                              std::is_same<IntegerType, detail::builtin_i128>::value ||
                                              std::is_same<IntegerType, detail::builtin_u128>::value};

#else

template <typename IntegerType>
BOOST_DECIMAL_INLINE_CONSTEXPR_VARIABLE bool is_reduced_integer_v {reduced_integers<IntegerType>::value};

#endif // 128-bit

} // namespace detail

constexpr uint128_t add_sat(const uint128_t x, const uint128_t y) noexcept
{
    const auto z {x + y};

    if (z < x)
    {
        return (std::numeric_limits<uint128_t>::max)();
    }

    return z;
}

constexpr uint128_t sub_sat(const uint128_t x, const uint128_t y) noexcept
{
    const auto z {x - y};

    if (z > x)
    {
        return (std::numeric_limits<uint128_t>::min)();
    }

    return z;
}

constexpr int128_t add_sat(int128_t x, int128_t y) noexcept;
constexpr int128_t sub_sat(int128_t x, int128_t y) noexcept;

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4307) // Addition Overflow
#  pragma warning(disable : 4146) // Unary minus applied to unsigned type
#endif

constexpr int128_t add_sat(const int128_t x, const int128_t y) noexcept
{
    if (x >= 0 && y >= 0)
    {
        constexpr auto max_value {static_cast<uint128_t>((std::numeric_limits<int128_t>::max)())};
        const auto big_x {static_cast<uint128_t>(x)};
        const auto big_y {static_cast<uint128_t>(y)};
        const auto big_res {big_x + big_y};

        return big_res > max_value ? (std::numeric_limits<int128_t>::max)() : static_cast<int128_t>(big_res);
    }
    else if ((x < 0 && y > 0) || (x > 0 && y < 0))
    {
        return x + y;
    }
    else
    {
        // x < 0 and y < 0
        // Nearly the same technique as the positive values case
        constexpr auto max_value {-static_cast<uint128_t>((std::numeric_limits<int128_t>::min)())};
        const auto big_x {static_cast<uint128_t>(abs(x))};
        const auto big_y {static_cast<uint128_t>(abs(y))};
        const auto big_res {big_x + big_y};

        return big_res > max_value ? (std::numeric_limits<int128_t>::min)() : -static_cast<int128_t>(big_res);
    }
}

constexpr int128_t sub_sat(const int128_t x, const int128_t y) noexcept
{
    if (x <= 0 && y >= 0)
    {
        // Underflow case
        const auto res {x - y};
        return res > x ? (std::numeric_limits<int128_t>::min)() : res;
    }
    else if (x > 0 && y < 0)
    {
        // Overflow Case
        constexpr auto max_val {static_cast<uint128_t>((std::numeric_limits<int128_t>::max)())};
        const auto big_x {static_cast<uint128_t>(x)};
        const auto big_y {-static_cast<uint128_t>(y)};
        const auto res {big_x + big_y};

        return (res > max_val || res < big_x) ? (std::numeric_limits<int128_t>::max)() : static_cast<int128_t>(res);
    }
    else
    {
        return x - y;
    }
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

constexpr uint128_t mul_sat(const uint128_t x, const uint128_t y) noexcept
{
    const auto x_bits {bit_width(x)};
    const auto y_bits {bit_width(y)};

    if ((x_bits + y_bits) > std::numeric_limits<uint128_t>::digits)
    {
        return (std::numeric_limits<uint128_t>::max)();
    }

    return x * y;
}

constexpr int128_t mul_sat(const int128_t& x, const int128_t& y) noexcept
{
    const auto x_bits {bit_width(static_cast<uint128_t>(abs(x)))};
    const auto y_bits {bit_width(static_cast<uint128_t>(abs(y)))};

    if ((x_bits + y_bits) > std::numeric_limits<int128_t>::digits)
    {
        if ((x < 0) != (y < 0))
        {
            return (std::numeric_limits<int128_t>::min)();
        }
        else
        {
            return (std::numeric_limits<int128_t>::max)();
        }
    }

    const int128_t res {x * y};
    return res;
}

constexpr uint128_t div_sat(const uint128_t x, const uint128_t y) noexcept
{
    return x / y;
}

constexpr int128_t div_sat(const int128_t x, const int128_t y) noexcept
{
    if (BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(x == (std::numeric_limits<int128_t>::min)() && y == -1))
    {
        // This is the only possible case of overflow
        return (std::numeric_limits<int128_t>::max)();
    }

    return x / y;
}

template <typename TargetType, std::enable_if_t<detail::is_reduced_integer_v<TargetType>, bool> = true>
constexpr TargetType saturate_cast(const uint128_t value) noexcept
{
    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (std::is_same<uint128_t, TargetType>::value)
    {
        return value;
    }
    else
    {
        if (value > static_cast<uint128_t>((std::numeric_limits<TargetType>::max)()))
        {
            return (std::numeric_limits<TargetType>::max)();
        }

        return static_cast<TargetType>(value);
    }
}

template <typename TargetType, std::enable_if_t<detail::is_reduced_integer_v<TargetType>, bool> = true>
constexpr TargetType saturate_cast(const int128_t value) noexcept
{
    BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (std::is_same<int128_t, TargetType>::value)
    {
        return value;
    }
    #if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) || defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)
    else BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (std::is_same<uint128_t, TargetType>::value || std::is_same<detail::builtin_u128, TargetType>::value)
    #else
    else BOOST_DECIMAL_DETAIL_INT128_IF_CONSTEXPR (std::is_same<uint128_t, TargetType>::value)
    #endif
    {
        // We can't possibly have overflow in this case
        return value < 0 ? static_cast<TargetType>(0) : static_cast<TargetType>(value);
    }
    else
    {
        if (value > static_cast<int128_t>((std::numeric_limits<TargetType>::max)()))
        {
            return (std::numeric_limits<TargetType>::max)();
        }
        else if (value < static_cast<int128_t>((std::numeric_limits<TargetType>::min)()))
        {
            return (std::numeric_limits<TargetType>::min)();
        }

        return static_cast<TargetType>(value);
    }
}

} // namespace int128
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_INT128_NUMERIC_HPP
