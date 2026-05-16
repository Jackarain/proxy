// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_DETAIL_INT128_HPP
#define BOOST_DECIMAL_DETAIL_INT128_DETAIL_INT128_HPP

#include "fwd.hpp"
#include "config.hpp"
#include "traits.hpp"
#include "constants.hpp"
#include "clz.hpp"
#include "common_mul.hpp"
#include "common_div.hpp"
#include <cstdint>
#include <cstring>

namespace boost {
namespace int128 {

struct
    #if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) || defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)
    alignas(alignof(detail::builtin_i128))
    #else
    alignas(16)
    #endif
int128_t
{
    #if BOOST_DECIMAL_DETAIL_INT128_ENDIAN_LITTLE_BYTE
    std::uint64_t low {};
    std::int64_t high {};
    #else

    #ifdef __GNUC__
    #  pragma GCC diagnostic push
    #  pragma GCC diagnostic ignored "-Wreorder"
    #endif

    std::int64_t high {};
    std::uint64_t low {};

    #ifdef __GNUC__
    #  pragma GCC diagnostic pop
    #endif

    #endif

    // Defaulted basic construction
    constexpr int128_t() noexcept = default;
    constexpr int128_t(const int128_t&) noexcept = default;
    constexpr int128_t(int128_t&&) noexcept = default;
    constexpr int128_t& operator=(const int128_t&) noexcept = default;
    constexpr int128_t& operator=(int128_t&&) noexcept = default;

    // Requires conversion file to be implemented
    constexpr int128_t(const uint128_t& v) noexcept;

    // Construct from integral types
    #if BOOST_DECIMAL_DETAIL_INT128_ENDIAN_LITTLE_BYTE

    constexpr int128_t(const std::int64_t hi, const std::uint64_t lo) noexcept : low{lo}, high{hi} {}

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
    constexpr int128_t(const SignedInteger v) noexcept : low {static_cast<std::uint64_t>(v)}, high {v < 0 ? -1 : 0} {}

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
    constexpr int128_t(const UnsignedInteger v) noexcept : low {static_cast<std::uint64_t>(v)}, high {} {}

    #if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) || defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)

    BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR int128_t(const detail::builtin_i128 v) noexcept : low {static_cast<std::uint64_t>(v & static_cast<detail::builtin_i128>(detail::low_word_mask))}, high {static_cast<std::int64_t>(v >> static_cast<detail::builtin_i128>(64U))} {}
    BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR int128_t(const detail::builtin_u128 v) noexcept : low {static_cast<std::uint64_t>(v & static_cast<detail::builtin_u128>(detail::low_word_mask))}, high {static_cast<std::int64_t>(v >> static_cast<detail::builtin_u128>(64U))} {}

    #endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

    #else // Big endian

    constexpr int128_t(const std::int64_t hi, const std::uint64_t lo) noexcept : high{hi}, low{lo} {}

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
    constexpr int128_t(const SignedInteger v) noexcept : high{v < 0 ? -1 : 0}, low{static_cast<std::uint64_t>(v)} {}

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
    constexpr int128_t(const UnsignedInteger v) noexcept : high {}, low {static_cast<std::uint64_t>(v)} {}

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

    constexpr int128_t(const detail::builtin_i128 v) noexcept : high {static_cast<std::int64_t>(v >> 64U)}, low {static_cast<std::uint64_t>(v & detail::low_word_mask)} {}
    constexpr int128_t(const detail::builtin_u128 v) noexcept : high {static_cast<std::int64_t>(v >> 64U)}, low {static_cast<std::uint64_t>(v & detail::low_word_mask)} {}

    #endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

    #endif // BOOST_DECIMAL_DETAIL_INT128_ENDIAN_LITTLE_BYTE

    // Integer Conversion operators
    constexpr operator bool() const noexcept { return low || high; }

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
    explicit constexpr operator SignedInteger() const noexcept { return static_cast<SignedInteger>(low); }

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
    explicit constexpr operator UnsignedInteger() const noexcept { return static_cast<UnsignedInteger>(low); }

    #if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) || defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)

    explicit BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR operator detail::builtin_i128() const noexcept { return static_cast<detail::builtin_i128>(static_cast<detail::builtin_u128>(high) << static_cast<detail::builtin_u128>(64)) | static_cast<detail::builtin_i128>(low); }

    explicit BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR operator detail::builtin_u128() const noexcept { return (static_cast<detail::builtin_u128>(high) << static_cast<detail::builtin_u128>(64)) | static_cast<detail::builtin_u128>(low); }

    #endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

    // Conversion to float
    // This is basically the same as ldexp(static_cast<T>(high), 64) + static_cast<T>(low),
    // but can be constexpr at C++11 instead of C++26
    explicit constexpr operator float() const noexcept;
    explicit constexpr operator double() const noexcept;
    explicit constexpr operator long double() const noexcept;

    // Compound Or
    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT>
    constexpr int128_t& operator|=(Integer rhs) noexcept;

    constexpr int128_t& operator|=(int128_t rhs) noexcept;

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_128BIT_INTEGER_CONCEPT>
    inline int128_t& operator|=(Integer rhs) noexcept;

    #endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    // Compound And
    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT>
    constexpr int128_t& operator&=(Integer rhs) noexcept;

    constexpr int128_t& operator&=(int128_t rhs) noexcept;

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_128BIT_INTEGER_CONCEPT>
    inline int128_t& operator&=(Integer rhs) noexcept;

    #endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    // Compound XOR
    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT>
    constexpr int128_t& operator^=(Integer rhs) noexcept;

    constexpr int128_t& operator^=(int128_t rhs) noexcept;

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_128BIT_INTEGER_CONCEPT>
    inline int128_t& operator^=(Integer rhs) noexcept;

    #endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    // Compound Left Shift
    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT>
    constexpr int128_t& operator<<=(Integer rhs) noexcept;

    constexpr int128_t& operator<<=(int128_t rhs) noexcept;

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_128BIT_INTEGER_CONCEPT>
    inline int128_t& operator<<=(Integer rhs) noexcept;

    #endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    // Compound Right Shift
    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT>
    constexpr int128_t& operator>>=(Integer rhs) noexcept;

    constexpr int128_t& operator>>=(int128_t rhs) noexcept;

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_128BIT_INTEGER_CONCEPT>
    inline int128_t& operator>>=(Integer rhs) noexcept;

    #endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    // Prefix and postfix increment
    constexpr int128_t& operator++() noexcept;
    constexpr int128_t& operator++(int) noexcept;

    // Prefix and postfix decrment
    constexpr int128_t& operator--() noexcept;
    constexpr int128_t& operator--(int) noexcept;

    // Compound Addition
    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT>
    constexpr int128_t& operator+=(Integer rhs) noexcept;

    constexpr int128_t& operator+=(int128_t rhs) noexcept;

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_128BIT_INTEGER_CONCEPT>
    inline int128_t& operator+=(Integer rhs) noexcept;

    #endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    // Compound Subtraction
    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT>
    constexpr int128_t& operator-=(Integer rhs) noexcept;

    constexpr int128_t& operator-=(int128_t rhs) noexcept;

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_128BIT_INTEGER_CONCEPT>
    inline int128_t& operator-=(Integer rhs) noexcept;

    #endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    // Compound Multiplication
    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT>
    constexpr int128_t& operator*=(Integer rhs) noexcept;

    constexpr int128_t& operator*=(int128_t rhs) noexcept;

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_128BIT_INTEGER_CONCEPT>
    inline int128_t& operator*=(Integer rhs) noexcept;

    #endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    // Compound Division
    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT>
    constexpr int128_t& operator/=(Integer rhs) noexcept;

    constexpr int128_t& operator/=(int128_t rhs) noexcept;

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_128BIT_INTEGER_CONCEPT>
    inline int128_t& operator/=(Integer rhs) noexcept;

    #endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    // Compound Modulo
    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT>
    constexpr int128_t& operator%=(Integer rhs) noexcept;

    constexpr int128_t& operator%=(int128_t rhs) noexcept;

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

    template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_128BIT_INTEGER_CONCEPT>
    inline int128_t& operator%=(Integer rhs) noexcept;

    #endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128
};

//=====================================
// Absolute Value function
//=====================================

constexpr int128_t abs(int128_t value) noexcept
{
    if (value.high < 0)
    {
        value.low = ~value.low + 1U;
        value.high = static_cast<std::int64_t>(~static_cast<std::uint64_t>(value.high) + static_cast<std::uint64_t>(value.low == 0 ? 1 : 0));
    }
    
    return value;
}

//=====================================
// Float Conversion Operators
//=====================================

// The most correct way to do this would be std::ldexp(static_cast<T>(high), 64) + static_cast<T>(low);
// Since std::ldexp is not constexpr until C++23 we can work around this by multiplying the high word
// by 0xFFFFFFFF in order to generally replicate what ldexp is doing in the constexpr context.
// We also avoid pulling in <quadmath.h> for the __float128 case where we would need ldexpq

constexpr int128_t::operator float() const noexcept
{
    return static_cast<float>(high) * detail::offset_value_v<float> + static_cast<float>(low);
}

constexpr int128_t::operator double() const noexcept
{
    return static_cast<double>(high) * detail::offset_value_v<double> + static_cast<double>(low);
}

constexpr int128_t::operator long double() const noexcept
{
    return static_cast<long double>(high) * detail::offset_value_v<long double> + static_cast<long double>(low);
}

//=====================================
// Unary Operators
//=====================================

constexpr int128_t operator+(const int128_t value) noexcept
{
    return value;
}

constexpr int128_t operator-(const int128_t value) noexcept
{
    return (value.low == 0) ? int128_t{-value.high, 0} :
                              int128_t{~value.high, ~value.low + 1};
}

//=====================================
// Equality Operators
//=====================================

constexpr bool operator==(const int128_t lhs, const bool rhs) noexcept
{
    return lhs.high == 0 && lhs.low == static_cast<std::uint64_t>(rhs);
}

constexpr bool operator==(const bool lhs, const int128_t rhs) noexcept
{
    return rhs.high == 0 && rhs.low == static_cast<std::uint64_t>(lhs);
}

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wsign-compare"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#  pragma GCC diagnostic ignored "-Wsign-compare"
#endif

constexpr bool operator==(const int128_t lhs, const int128_t rhs) noexcept
{
    // x64 and ARM64 like the values in opposite directions

    #if defined(__aarch64__) || defined(_M_ARM64) || defined(__x86_64__) || defined(_M_X64) || defined(_M_IX86)

    return lhs.low == rhs.low && lhs.high == rhs.high;

    #else

    return lhs.high == rhs.high && lhs.low == rhs.low;

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr bool operator==(const int128_t lhs, const SignedInteger rhs) noexcept
{
    return lhs.high == (rhs < 0 ? -1 : 0) && lhs.low == static_cast<std::uint64_t>(rhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr bool operator==(const SignedInteger lhs, const int128_t rhs) noexcept
{
    return rhs.high == (lhs < 0 ? -1 : 0) && rhs.low == static_cast<std::uint64_t>(lhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr bool operator==(const int128_t lhs, const UnsignedInteger rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    return lhs.high == 0 && lhs.low == static_cast<std::uint64_t>(rhs);

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Compare Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr bool operator==(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    return rhs.high == 0 && rhs.low == static_cast<std::uint64_t>(lhs);

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Compare Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #endif
}

#if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) || defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator==(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return lhs == static_cast<int128_t>(rhs);
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator==(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) == rhs;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator==(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return lhs.high < 0 ? false : lhs == static_cast<int128_t>(rhs);
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator==(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return rhs.high < 0 ? false : static_cast<int128_t>(lhs) == rhs;
}

#else

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator==(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return true;
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator==(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return true;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

//=====================================
// Inequality Operators
//=====================================

constexpr bool operator!=(const int128_t lhs, const int128_t rhs) noexcept
{
    // x64 and ARM64 like the values in opposite directions

    #if defined(__aarch64__) || defined(_M_ARM64) || defined(_M_X64) || defined(_M_IX86)

    return lhs.low != rhs.low || lhs.high != rhs.high;

    #elif defined(__x86_64__) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION) && defined(__GNUC__) && !defined(__clang__) && defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)

    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs))
    {
        return lhs.high != rhs.high || lhs.low != rhs.low;
    }
    else
    {
        detail::builtin_i128 builtin_lhs {};
        detail::builtin_i128 builtin_rhs {};

        std::memcpy(&builtin_lhs, &lhs, sizeof(builtin_lhs));
        std::memcpy(&builtin_rhs, &rhs, sizeof(builtin_rhs));

        return builtin_lhs != builtin_rhs;
    }

    #else

    return lhs.high != rhs.high || lhs.low != rhs.low;

    #endif
}

constexpr bool operator!=(const int128_t lhs, const bool rhs) noexcept
{
    return lhs.high != 0 || lhs.low != static_cast<std::uint64_t>(rhs);
}

constexpr bool operator!=(const bool lhs, const int128_t rhs) noexcept
{
    return rhs.high != 0 || rhs.low != static_cast<std::uint64_t>(lhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr bool operator!=(const int128_t lhs, const SignedInteger rhs) noexcept
{
    return lhs.high != (rhs < 0 ? -1 : 0) || lhs.low != static_cast<std::uint64_t>(rhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr bool operator!=(const SignedInteger lhs, const int128_t rhs) noexcept
{
    return rhs.high != (lhs < 0 ? -1 : 0) || rhs.low != static_cast<std::uint64_t>(lhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr bool operator!=(const int128_t lhs, const UnsignedInteger rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    return lhs.high != 0 || lhs.low != static_cast<std::uint64_t>(rhs);

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Compare Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr bool operator!=(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    return rhs.high != 0 || rhs.low != static_cast<std::uint64_t>(lhs);

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Compare Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #endif
}

#if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) || defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator!=(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return lhs != static_cast<int128_t>(rhs);
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator!=(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) != rhs;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator!=(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return lhs.high < 0 ? true : lhs != static_cast<int128_t>(rhs);
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator!=(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return rhs.high < 0 ? true : static_cast<int128_t>(lhs) != rhs;
}

#else

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator!=(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return true;
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator!=(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return true;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

//=====================================
// Less than Operators
//=====================================

constexpr bool operator<(const int128_t lhs, const int128_t rhs) noexcept
{
    // On ARM macs only with the clang compiler is casting to __int128 uniformly better (and seemingly cost free)
    #if defined(__aarch64__) && defined(__APPLE__) && defined(__clang__) && defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)

    return static_cast<detail::builtin_i128>(lhs) < static_cast<detail::builtin_i128>(rhs);

    #elif defined(__x86_64__) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION) && defined(__GNUC__) && !defined(__clang__) && defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)

    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs))
    {
        return lhs.high == rhs.high ? lhs.low < rhs.low : lhs.high < rhs.high;
    }
    else
    {
        detail::builtin_i128 builtin_lhs {};
        detail::builtin_i128 builtin_rhs {};

        std::memcpy(&builtin_lhs, &lhs, sizeof(builtin_lhs));
        std::memcpy(&builtin_rhs, &rhs, sizeof(builtin_rhs));

        return builtin_lhs < builtin_rhs;
    }

    #else

    return lhs.high == rhs.high ? lhs.low < rhs.low : lhs.high < rhs.high;

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr bool operator<(const int128_t lhs, const UnsignedInteger rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    return lhs.high < 0 ? true : lhs.low < static_cast<std::uint64_t>(rhs);

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Compare Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr bool operator<(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    return rhs.high < 0 ? false : static_cast<std::uint64_t>(lhs) < rhs.low;

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Compare Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr bool operator<(const int128_t lhs, const SignedInteger rhs) noexcept
{
    if (lhs.high < 0)
    {
        return rhs >= 0 ? true : lhs < static_cast<int128_t>(rhs);
    }

    if (lhs.high > 0 || rhs < 0)
    {
        return false;
    }

    return lhs.low < static_cast<std::uint64_t>(rhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr bool operator<(const SignedInteger lhs, const int128_t rhs) noexcept
{
    if (rhs.high < 0)
    {
        return lhs >= 0 ? false : static_cast<int128_t>(lhs) < rhs;
    }

    // rhs is positive
    if (rhs.high > 0 || lhs < 0)
    {
        return true;
    }

    return static_cast<std::uint64_t>(lhs) < rhs.low;
}

#if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) || defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator<(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return lhs < static_cast<int128_t>(rhs);
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator<(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) < rhs;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator<(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return lhs.high < 0 ? false : lhs < static_cast<int128_t>(rhs);
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator<(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return rhs.high < 0 ? true : static_cast<int128_t>(lhs) < rhs;
}

#else // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator<(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return true;
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator<(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return true;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

//=====================================
// Greater than Operators
//=====================================

constexpr bool operator>(const int128_t lhs, const int128_t rhs) noexcept
{
    // On ARM macs only with the clang compiler is casting to __int128 uniformly better (and seemingly cost free)
    #if defined(__aarch64__) && defined(__APPLE__) && defined(__clang__) && defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)

    return static_cast<detail::builtin_i128>(lhs) > static_cast<detail::builtin_i128>(rhs);

    #elif defined(__x86_64__) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION) && defined(__GNUC__) && !defined(__clang__) && defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)

    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs))
    {
        return lhs.high == rhs.high ? lhs.low > rhs.low : lhs.high > rhs.high;
    }
    else
    {
        detail::builtin_i128 builtin_lhs {};
        detail::builtin_i128 builtin_rhs {};

        std::memcpy(&builtin_lhs, &lhs, sizeof(builtin_lhs));
        std::memcpy(&builtin_rhs, &rhs, sizeof(builtin_rhs));

        return builtin_lhs > builtin_rhs;
    }

    #else

    return lhs.high == rhs.high ? lhs.low > rhs.low : lhs.high > rhs.high;

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr bool operator>(const int128_t lhs, const SignedInteger rhs) noexcept
{
    return !(lhs < rhs) && !(lhs == rhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr bool operator>(const SignedInteger lhs, const int128_t rhs) noexcept
{
    return !(lhs < rhs) && !(lhs == rhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr bool operator>(const int128_t lhs, const UnsignedInteger rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    return lhs.high > 0 ? true : lhs.low > static_cast<std::uint64_t>(rhs);

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Compare Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr bool operator>(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    return rhs.high < 0 ? true : static_cast<std::uint64_t>(lhs) > rhs.low;

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Compare Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #endif
}

#if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) || defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator>(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return lhs > static_cast<int128_t>(rhs);
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator>(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) > rhs;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator>(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return lhs.high < 0 ? false : lhs > static_cast<int128_t>(rhs);
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator>(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return rhs.high < 0 ? true : static_cast<int128_t>(lhs) > rhs;
}

#else // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator>(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return true;
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator>(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return true;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

//=====================================
// Less Equal Operators
//=====================================

constexpr bool operator<=(const int128_t lhs, const int128_t rhs) noexcept
{
    // On ARM macs only with the clang compiler is casting to __int128 uniformly better (and seemingly cost free)
    #if defined(__aarch64__) && defined(__APPLE__) && defined(__clang__) && defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)

    return static_cast<detail::builtin_i128>(lhs) <= static_cast<detail::builtin_i128>(rhs);

    #elif defined(__x86_64__) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION) && defined(__GNUC__) && !defined(__clang__) && defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)

    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs))
    {
        return lhs.high == rhs.high ? lhs.low <= rhs.low : lhs.high <= rhs.high;
    }
    else
    {
        detail::builtin_i128 builtin_lhs {};
        detail::builtin_i128 builtin_rhs {};

        std::memcpy(&builtin_lhs, &lhs, sizeof(builtin_lhs));
        std::memcpy(&builtin_rhs, &rhs, sizeof(builtin_rhs));

        return builtin_lhs <= builtin_rhs;
    }

    #else

    return lhs.high == rhs.high ? lhs.low <= rhs.low : lhs.high <= rhs.high;

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr bool operator<=(const int128_t lhs, const SignedInteger rhs) noexcept
{
    return !(lhs > rhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr bool operator<=(const SignedInteger lhs, const int128_t rhs) noexcept
{
    return !(lhs > rhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr bool operator<=(const int128_t lhs, const UnsignedInteger rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    return lhs.high < 0 ? true : lhs.low <= static_cast<std::uint64_t>(rhs);

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Compare Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr bool operator<=(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    return rhs.high < 0 ? false : static_cast<std::uint64_t>(lhs) <= rhs.low;

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Compare Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #endif
}

#if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) || defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator<=(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return lhs <= static_cast<int128_t>(rhs);
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator<=(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) <= rhs;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator<=(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return lhs.high < 0 ? true : lhs <= static_cast<int128_t>(rhs);
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator<=(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return rhs.high < 0 ? false : static_cast<int128_t>(lhs) <= rhs;
}

#else // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator<=(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return true;
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator<=(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return true;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

//=====================================
// Greater Equal Operators
//=====================================

constexpr bool operator>=(const int128_t lhs, const int128_t rhs) noexcept
{
    // On ARM macs only with the clang compiler is casting to __int128 uniformly better (and seemingly cost free)
    #if defined(__aarch64__) && defined(__APPLE__) && defined(__clang__) && defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)

    return static_cast<detail::builtin_i128>(lhs) >= static_cast<detail::builtin_i128>(rhs);

    #elif defined(__x86_64__) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION) && defined(__GNUC__) && !defined(__clang__) && defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)

    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs))
    {
        return lhs.high == rhs.high ? lhs.low >= rhs.low : lhs.high >= rhs.high;
    }
    else
    {
        detail::builtin_i128 builtin_lhs {};
        detail::builtin_i128 builtin_rhs {};

        std::memcpy(&builtin_lhs, &lhs, sizeof(builtin_lhs));
        std::memcpy(&builtin_rhs, &rhs, sizeof(builtin_rhs));

        return builtin_lhs >= builtin_rhs;
    }

    #else

    return lhs.high == rhs.high ? lhs.low >= rhs.low : lhs.high >= rhs.high;

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr bool operator>=(const int128_t lhs, const SignedInteger rhs) noexcept
{
    return !(lhs < rhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr bool operator>=(const SignedInteger lhs, const int128_t rhs) noexcept
{
    return !(lhs < rhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr bool operator>=(const int128_t lhs, const UnsignedInteger rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    return lhs.high < 0 ? false : lhs.low >= static_cast<std::uint64_t>(rhs);

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Compare Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr bool operator>=(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

    return rhs.high < 0 ? true : static_cast<std::uint64_t>(lhs) >= rhs.low;

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Compare Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #endif
}

#if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) || defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator>=(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return lhs >= static_cast<int128_t>(rhs);
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator>=(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) >= rhs;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator>=(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return lhs.high < 0 ? false : lhs >= static_cast<int128_t>(rhs);
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator>=(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return rhs.high < 0 ? true : static_cast<int128_t>(lhs) >= rhs;
}

#else // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator>=(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return true;
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR bool operator>=(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return true;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

//=====================================
// Not Operator
//=====================================

constexpr int128_t operator~(const int128_t rhs) noexcept
{
    return {~rhs.high, ~rhs.low};
}

//=====================================
// Or Operator
//=====================================

constexpr int128_t operator|(const int128_t lhs, const int128_t rhs) noexcept
{
    return {lhs.high | rhs.high, lhs.low | rhs.low};
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator|(const int128_t lhs, const SignedInteger rhs) noexcept
{
    return {lhs.high | (rhs < 0 ? -1 : 0), lhs.low | static_cast<std::uint64_t>(rhs)};
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator|(const SignedInteger lhs, const int128_t rhs) noexcept
{
    return {rhs.high | (lhs < 0 ? -1 : 0), static_cast<std::uint64_t>(lhs) | rhs.low};
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator|(const int128_t lhs, const UnsignedInteger rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    return {lhs.high, lhs.low | static_cast<std::uint64_t>(rhs)};

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return {0, 0};

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator|(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    return {rhs.high, static_cast<std::uint64_t>(lhs) | rhs.low};

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return {0, 0};

    #endif
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

constexpr int128_t operator|(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return lhs | static_cast<int128_t>(rhs);
}

constexpr int128_t operator|(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) | rhs;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

constexpr int128_t operator|(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return lhs | static_cast<int128_t>(rhs);
}

constexpr int128_t operator|(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) | rhs;
}

#else // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
constexpr int128_t operator|(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
constexpr int128_t operator|(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

//=====================================
// Compound OR Operator
//=====================================

template <BOOST_DECIMAL_DETAIL_INT128_INTEGER_CONCEPT>
constexpr int128_t& int128_t::operator|=(const Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
    static_assert(detail::is_signed_integer_v<Integer>, "Sign Conversion Error");
    #endif

    *this = *this | rhs;
    return *this;
}

constexpr int128_t& int128_t::operator|=(const int128_t rhs) noexcept
{
    *this = *this | rhs;
    return *this;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

template <BOOST_DECIMAL_DETAIL_INT128_128BIT_INTEGER_CONCEPT>
inline int128_t& int128_t::operator|=(const Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
    static_assert(std::numeric_limits<Integer>::is_signed, "Sign Conversion Error");
    #endif

    *this = *this | rhs;
    return *this;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

//=====================================
// And Operator
//=====================================

constexpr int128_t operator&(const int128_t lhs, const int128_t rhs) noexcept
{
    return {lhs.high & rhs.high, lhs.low & rhs.low};
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator&(const int128_t lhs, const SignedInteger rhs) noexcept
{
    return {lhs.high & (rhs < 0 ? -1 : 0), lhs.low & static_cast<std::uint64_t>(rhs)};
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator&(const SignedInteger lhs, const int128_t rhs) noexcept
{
    return {rhs.high & (lhs < 0 ? -1 : 0), static_cast<std::uint64_t>(lhs) & rhs.low};
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator&(const int128_t lhs, const UnsignedInteger rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    return {lhs.high, lhs.low & static_cast<std::uint64_t>(rhs)};

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return {0, 0};

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator&(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    return {rhs.high, static_cast<std::uint64_t>(lhs) & rhs.low};

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return {0, 0};

    #endif
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

constexpr int128_t operator&(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return lhs & static_cast<int128_t>(rhs);
}

constexpr int128_t operator&(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) & rhs;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

constexpr int128_t operator&(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return lhs & static_cast<int128_t>(rhs);
}

constexpr int128_t operator&(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) & rhs;
}

#else // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
constexpr int128_t operator&(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
constexpr int128_t operator&(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

template <BOOST_DECIMAL_DETAIL_INT128_128BIT_INTEGER_CONCEPT>
inline int128_t& int128_t::operator&=(const Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
    static_assert(std::numeric_limits<Integer>::is_signed, "Sign Conversion Error");
    #endif

    *this = *this & rhs;
    return *this;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

//=====================================
// Compound And Operator
//=====================================

template <BOOST_DECIMAL_DETAIL_INT128_INTEGER_CONCEPT>
constexpr int128_t& int128_t::operator&=(const Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
    static_assert(detail::is_signed_integer_v<Integer>, "Sign Conversion Error");
    #endif

    *this = *this & rhs;
    return *this;
}

constexpr int128_t& int128_t::operator&=(const int128_t rhs) noexcept
{
    *this = *this & rhs;
    return *this;
}

//=====================================
// XOR Operator
//=====================================

constexpr int128_t operator^(const int128_t lhs, const int128_t rhs) noexcept
{
    return {lhs.high ^ rhs.high, lhs.low ^ rhs.low};
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator^(const int128_t lhs, const SignedInteger rhs) noexcept
{
    return {lhs.high ^ (rhs < 0 ? -1 : 0), lhs.low ^ static_cast<std::uint64_t>(rhs)};
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator^(const SignedInteger lhs, const int128_t rhs) noexcept
{
    return {rhs.high ^ (lhs < 0 ? -1 : 0), static_cast<std::uint64_t>(lhs) ^ rhs.low};
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator^(const int128_t lhs, const UnsignedInteger rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    return {lhs.high, lhs.low ^ static_cast<std::uint64_t>(rhs)};

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator^(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    return {rhs.high, static_cast<std::uint64_t>(lhs) ^ rhs.low};

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return true;

    #endif
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

constexpr int128_t operator^(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return lhs ^ static_cast<int128_t>(rhs);
}

constexpr int128_t operator^(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) ^ rhs;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

constexpr int128_t operator^(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return lhs ^ static_cast<int128_t>(rhs);
}

constexpr int128_t operator^(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) ^ rhs;
}

#else // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
constexpr int128_t operator^(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
constexpr int128_t operator^(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

//=====================================
// Compound XOR Operator
//=====================================

template <BOOST_DECIMAL_DETAIL_INT128_INTEGER_CONCEPT>
constexpr int128_t& int128_t::operator^=(Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
    static_assert(detail::is_signed_integer_v<Integer>, "Sign Conversion Error");
    #endif

    *this = *this ^ rhs;
    return *this;
}

constexpr int128_t& int128_t::operator^=(int128_t rhs) noexcept
{
    *this = *this ^ rhs;
    return *this;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

template <BOOST_DECIMAL_DETAIL_INT128_128BIT_INTEGER_CONCEPT>
inline int128_t& int128_t::operator^=(const Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
    static_assert(std::numeric_limits<Integer>::is_signed, "Sign Conversion Error");
    #endif

    *this = *this ^ rhs;
    return *this;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

//=====================================
// Left Shift Operator
//=====================================

namespace detail {

template <typename Integer>
constexpr int128_t default_ls_impl(const int128_t lhs, const Integer rhs) noexcept
{
    if (rhs < 0 || rhs >= 128)
    {
        return {0, 0};
    }

    if (rhs == 0)
    {
        return lhs;
    }

    if (rhs == 64)
    {
        return {static_cast<std::int64_t>(lhs.low), 0};
    }

    if (rhs > 64)
    {
        return {static_cast<std::int64_t>(lhs.low << (rhs - 64)), 0};
    }

    // For shifts < 64
    std::uint64_t high_part = (static_cast<std::uint64_t>(lhs.high) << rhs) |
                              (lhs.low >> (64 - rhs));

    return {
        static_cast<std::int64_t>(high_part),
        lhs.low << rhs
    };
}

template <typename Integer>
int128_t intrinsic_ls_impl(const int128_t lhs, const Integer rhs) noexcept
{
    if (BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(rhs >= 128 || rhs < 0))
    {
        return {0, 0};
    }

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

    #  if defined(__aarch64__)

    #if defined(__GNUC__) && __GNUC__ >= 8
    #  pragma GCC diagnostic push
    #  pragma GCC diagnostic ignored "-Wclass-memaccess"
    #endif

    builtin_i128 value;
    std::memcpy(&value, &lhs, sizeof(builtin_i128));
    const auto res {value << rhs};

    int128_t return_value;
    std::memcpy(&return_value, &res, sizeof(int128_t));
    return return_value;

    #if defined(__GNUC__) && __GNUC__ >= 8
    #  pragma GCC diagnostic pop
    #endif

    #  else

    return static_cast<builtin_i128>(lhs) << rhs;

    #  endif

    #elif defined(_M_AMD64)

    if (rhs >= 64)
    {
        return {static_cast<std::int64_t>(lhs.low << (rhs - 64)), 0};
    }
    else
    {
        int128_t res;
        res.high = static_cast<std::int64_t>(__shiftleft128(lhs.low, static_cast<std::uint64_t>(lhs.high), static_cast<unsigned char>(rhs)));
        res.low = lhs.low << rhs;

        return res;
    }

    #else

    if (BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(rhs == 0))
    {
        return lhs;
    }
    if (rhs == 64)
    {
        return {static_cast<std::int64_t>(lhs.low), 0};
    }

    if (rhs > 64)
    {
        return {static_cast<std::int64_t>(lhs.low << (rhs - 64)), 0};
    }

    // For shifts < 64
    const auto high_part = (static_cast<std::uint64_t>(lhs.high) << rhs) |
                           (lhs.low >> (64 - rhs));

    return {
        static_cast<std::int64_t>(high_part),
        lhs.low << rhs
    };

    #endif
}

} // namespace detail

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT>
constexpr int128_t operator<<(const int128_t lhs, const Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION

    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs))
    {
        return detail::default_ls_impl(lhs, rhs); // LCOV_EXCL_LINE
    }
    else
    {
        return detail::intrinsic_ls_impl(lhs, rhs);
    }

    #else

    return detail::default_ls_impl(lhs, rhs);

    #endif
}

template <typename Integer, std::enable_if_t<detail::is_any_integer_v<Integer> && (sizeof(Integer) * 8 > 16), bool> = true>
constexpr Integer operator<<(const Integer lhs, const int128_t rhs) noexcept
{
    constexpr auto bit_width {sizeof(Integer) * 8};

    if (rhs.high != 0 || rhs.low >= bit_width)
    {
        return 0;
    }

    return lhs << rhs.low;
}

template <typename SignedInteger, std::enable_if_t<detail::is_signed_integer_v<SignedInteger> && (sizeof(SignedInteger) * 8 <= 16), bool> = true>
constexpr int operator<<(const SignedInteger lhs, const int128_t rhs) noexcept
{
    constexpr auto bit_width {sizeof(SignedInteger) * 8};

    if (rhs.high != 0 || rhs.low >= bit_width)
    {
        return 0;
    }

    return static_cast<int>(lhs) << rhs.low;
}

template <typename UnsignedInteger, std::enable_if_t<detail::is_unsigned_integer_v<UnsignedInteger> && (sizeof(UnsignedInteger) * 8 <= 16), bool> = true>
constexpr unsigned operator<<(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    constexpr auto bit_width {sizeof(UnsignedInteger) * 8};

    if (rhs.high != 0 || rhs.low >= bit_width)
    {
        return 0;
    }

    return static_cast<unsigned>(lhs) << rhs.low;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4804) // Unsafe use of type bool in operation
#endif // _MSC_VER

template <BOOST_DECIMAL_DETAIL_INT128_INTEGER_CONCEPT>
constexpr int128_t& int128_t::operator<<=(const Integer rhs) noexcept
{
    *this = *this << rhs;
    return *this;
}

constexpr int128_t& int128_t::operator<<=(const int128_t rhs) noexcept
{
    *this = *this << rhs;
    return *this;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

template <BOOST_DECIMAL_DETAIL_INT128_128BIT_INTEGER_CONCEPT>
inline int128_t& int128_t::operator<<=(const Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
    static_assert(std::numeric_limits<Integer>::is_signed, "Sign Conversion Error");
    #endif

    *this = *this << rhs;
    return *this;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

#ifdef _MSC_VER
#pragma warning(pop)
#endif // _MSC_VER

//=====================================
// Right Shift Operator
//=====================================

namespace detail {

template <typename Integer>
constexpr int128_t default_rs_impl(const int128_t lhs, const Integer rhs) noexcept
{
    if (rhs >= 128 || rhs < 0 )
    {
        return lhs.high < 0 ? int128_t{-1, UINT64_MAX} : int128_t{0, 0};
    }

    if (rhs == 0)
    {
        return lhs;
    }

    if (rhs >= 64)
    {
        return {lhs.high < 0 ? -1 : 0, static_cast<std::uint64_t>(lhs.high >> (rhs - 64))};
    }

    // For shifts < 64
    const auto high_to_low {static_cast<std::uint64_t>(lhs.high) << (64 - rhs)};
    const auto low_shifted {lhs.low >> rhs};
    const auto low_part {high_to_low | low_shifted};

    return {
        lhs.high >> rhs,
        low_part
    };
}

template <typename Integer>
int128_t intrinsic_rs_impl(const int128_t lhs, const Integer rhs) noexcept
{
    if (BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(rhs >= 128 || rhs < 0))
    {
        return {0, 0};
    }

    #ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

    #  if defined(__aarch64__)

    #if defined(__GNUC__) && __GNUC__ >= 8
    #  pragma GCC diagnostic push
    #  pragma GCC diagnostic ignored "-Wclass-memaccess"
    #endif

    builtin_i128 value;
    std::memcpy(&value, &lhs, sizeof(builtin_i128));
    const auto res {value >> rhs};

    int128_t return_value;
    std::memcpy(&return_value, &res, sizeof(int128_t));
    return return_value;

    #if defined(__GNUC__) && __GNUC__ >= 8
    #  pragma GCC diagnostic pop
    #endif

    #  else

    return static_cast<builtin_i128>(lhs) >> rhs;

    #  endif

    #elif defined(_M_AMD64)

    if (rhs >= 64)
    {
        return {lhs.high < 0 ? -1 : 0, static_cast<std::uint64_t>(lhs.high >> (rhs - 64))};
    }
    else
    {
        int128_t res;
        res.low = __shiftright128(lhs.low, static_cast<std::uint64_t>(lhs.high), static_cast<unsigned char>(rhs));
        res.high = lhs.high >> rhs;

        return res;
    }

    #else

    if (BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(rhs == 0))
    {
        return lhs;
    }

    if (rhs >= 64)
    {
        return {lhs.high < 0 ? -1 : 0, static_cast<std::uint64_t>(lhs.high >> (rhs - 64))};
    }

    // For shifts < 64
    const auto high_to_low {static_cast<std::uint64_t>(lhs.high) << (64 - rhs)};
    const auto low_shifted {lhs.low >> rhs};
    const auto low_part {high_to_low | low_shifted};

    return {
        lhs.high >> rhs,
        low_part
    };

    #endif
}

} // namespace detail

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT>
constexpr int128_t operator>>(const int128_t lhs, const Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION

    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs))
    {
        return detail::default_rs_impl(lhs, rhs); // LCOV_EXCL_LINE
    }
    else
    {
        return detail::intrinsic_rs_impl(lhs, rhs);
    }

    #else

    return detail::default_rs_impl(lhs, rhs);

    #endif
}

template <typename Integer, std::enable_if_t<detail::is_any_integer_v<Integer> && (sizeof(Integer) * 8 > 16), bool> = true>
constexpr Integer operator>>(const Integer lhs, const int128_t rhs) noexcept
{
    constexpr auto bit_width {sizeof(Integer) * 8};

    if (rhs.high != 0 || rhs.low >= bit_width)
    {
        return 0;
    }

    return lhs >> rhs.low;
}

template <typename SignedInteger, std::enable_if_t<detail::is_signed_integer_v<SignedInteger> && (sizeof(SignedInteger) * 8 <= 16), bool> = true>
constexpr int operator>>(const SignedInteger lhs, const int128_t rhs) noexcept
{
    constexpr auto bit_width {sizeof(SignedInteger) * 8};

    if (rhs.high != 0 || rhs.low >= bit_width)
    {
        return 0;
    }

    return static_cast<int>(lhs) >> rhs.low;
}

template <typename UnsignedInteger, std::enable_if_t<detail::is_unsigned_integer_v<UnsignedInteger> && (sizeof(UnsignedInteger) * 8 <= 16), bool> = true>
constexpr unsigned operator>>(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    constexpr auto bit_width {sizeof(UnsignedInteger) * 8};

    if (rhs.high != 0 || rhs.low >= bit_width)
    {
        return 0;
    }

    return static_cast<unsigned>(lhs) >> rhs.low;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4804) // Unsafe use of type bool in operation
#endif // _MSC_VER

template <BOOST_DECIMAL_DETAIL_INT128_INTEGER_CONCEPT>
constexpr int128_t& int128_t::operator>>=(const Integer rhs) noexcept
{
    *this = *this >> rhs;
    return *this;
}

constexpr int128_t& int128_t::operator>>=(const int128_t rhs) noexcept
{
    *this = *this >> rhs;
    return *this;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

template <BOOST_DECIMAL_DETAIL_INT128_128BIT_INTEGER_CONCEPT>
inline int128_t& int128_t::operator>>=(const Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
    static_assert(std::numeric_limits<Integer>::is_signed, "Sign Conversion Error");
    #endif

    *this = *this >> rhs;
    return *this;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

#ifdef _MSC_VER
#pragma warning(pop)
#endif // _MSC_VER

//=====================================
// Increment Operators
//=====================================

constexpr int128_t& int128_t::operator++() noexcept
{
    if (++low == UINT64_C(0))
    {
        ++high;
    }

    return *this;
}

constexpr int128_t& int128_t::operator++(int) noexcept
{
    if (++low == UINT64_C(0))
    {
        ++high;
    }

    return *this;
}

//=====================================
// Decrement Operators
//=====================================

constexpr int128_t& int128_t::operator--() noexcept
{
    if (low-- == UINT64_C(0))
    {
        --high;
    }

    return *this;
}

constexpr int128_t& int128_t::operator--(int) noexcept
{
    if (low-- == UINT64_C(0))
    {
        --high;
    }

    return *this;
}

//=====================================
// Addition Operators
//=====================================

namespace detail {

BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr int128_t library_add(const int128_t lhs, const int128_t rhs) noexcept
{
    const auto new_low {lhs.low + rhs.low};
    const auto new_high {static_cast<std::uint64_t>(lhs.high) +
                                        static_cast<std::uint64_t>(rhs.high) +
                                        static_cast<std::uint64_t>(new_low < lhs.low)};

    return int128_t{static_cast<std::int64_t>(new_high), new_low};
}

BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr int128_t default_add(const int128_t lhs, const int128_t rhs) noexcept
{
    #if (defined(__x86_64__) || (defined(__aarch64__) && !defined(__APPLE__))) && !defined(_WIN32) && defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)

    return static_cast<int128_t>(static_cast<detail::builtin_i128>(lhs) + static_cast<detail::builtin_i128>(rhs));

    #elif defined(BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN_ADD_OVERFLOW)

    std::uint64_t result_low {};
    std::uint64_t result_high {};

    result_high = static_cast<std::uint64_t>(lhs.high) + static_cast<std::uint64_t>(rhs.high) + __builtin_add_overflow(lhs.low, rhs.low, &result_low);

    return int128_t{static_cast<std::int64_t>(result_high), result_low};

    #elif defined(__aarch64__) && !defined(__APPLE__) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION) && defined(__GNUC__) && defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)

    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs))
    {
        return library_add(lhs, rhs);
    }
    else
    {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wclass-memaccess"

        detail::builtin_i128 builtin_lhs {};
        detail::builtin_i128 builtin_rhs {};

        std::memcpy(&builtin_lhs, &lhs, sizeof(builtin_i128));
        std::memcpy(&builtin_rhs, &rhs, sizeof(builtin_i128));

        auto builtin_res {builtin_lhs + builtin_rhs};

        int128_t result {};
        std::memcpy(&result, &builtin_res, sizeof(int128_t));

        return result;

        #pragma GCC diagnostic pop
    }

    #elif defined(_M_AMD64) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION)

    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs))
    {
        return library_add(lhs, rhs); // LCOV_EXCL_LINE
    }
    else
    {
        int128_t result {};
        const auto carry {BOOST_DECIMAL_DETAIL_INT128_ADD_CARRY(0, lhs.low, rhs.low, &result.low)};
        BOOST_DECIMAL_DETAIL_INT128_ADD_CARRY(carry, static_cast<std::uint64_t>(lhs.high), static_cast<std::uint64_t>(rhs.high), reinterpret_cast<std::uint64_t*>(&result.high));

        return result;
    }

    #else

    return library_add(lhs, rhs);

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT>
BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr int128_t default_add(const int128_t lhs, const Integer rhs) noexcept
{
    const auto new_low {lhs.low + rhs};
    const auto new_high {static_cast<std::uint64_t>(lhs.high) + static_cast<std::uint64_t>(new_low < lhs.low)};

    return int128_t{static_cast<std::int64_t>(new_high), new_low};
}

BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr int128_t library_sub(const int128_t lhs, const int128_t rhs) noexcept
{
    const auto new_low {lhs.low - rhs.low};
    const auto new_high {static_cast<std::uint64_t>(lhs.high) - static_cast<std::uint64_t>(rhs.high) - static_cast<std::uint64_t>(lhs.low < rhs.low)};

    return int128_t{static_cast<std::int64_t>(new_high), new_low};
}

BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr int128_t default_sub(const int128_t lhs, const int128_t rhs) noexcept
{
    #if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN_SUB_OVERFLOW) && (!defined(__aarch64__) || defined(__APPLE__) || !defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128))

    // __builtin_sub_overflow is marked constexpr so we don't need if consteval handling
    std::uint64_t result_low {};
    const auto result_high {static_cast<std::uint64_t>(lhs.high) - static_cast<std::uint64_t>(rhs.high) - static_cast<std::uint64_t>(__builtin_sub_overflow(lhs.low, rhs.low, &result_low))};

    return int128_t{static_cast<std::int64_t>(result_high), result_low};

    #elif defined(__aarch64__) && !defined(__APPLE__) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION)

    return static_cast<int128_t>(static_cast<detail::builtin_i128>(lhs) - static_cast<detail::builtin_i128>(rhs));

    #elif defined(_M_AMD64) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION)

    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs))
    {
        return library_sub(lhs, rhs); // LCOV_EXCL_LINE
    }
    else
    {
        int128_t result {};
        const auto borrow {BOOST_DECIMAL_DETAIL_INT128_SUB_BORROW(0, lhs.low, rhs.low, &result.low)};
        BOOST_DECIMAL_DETAIL_INT128_SUB_BORROW(borrow, static_cast<std::uint64_t>(lhs.high), static_cast<std::uint64_t>(rhs.high), reinterpret_cast<std::uint64_t*>(&result.high));

        return result;
    }

    #else

    return library_sub(lhs, rhs);

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_INTEGER_CONCEPT>
BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr int128_t default_sub(const int128_t lhs, const Integer rhs) noexcept
{
    const auto new_low {lhs.low - rhs};
    const auto new_high {static_cast<std::uint64_t>(lhs.high) - static_cast<std::uint64_t>(new_low > lhs.low)};
    return int128_t{static_cast<std::int64_t>(new_high), new_low};
}

}

// On s390x with multiple different versions of GCC and language standards
// doing addition via subtraction is >10% faster in the benchmarks
#if defined(__s390__) || defined(__s390x__)

constexpr int128_t operator+(const int128_t lhs, const int128_t rhs) noexcept
{
    return detail::default_sub(lhs, -rhs);
}

#else

constexpr int128_t operator+(const int128_t lhs, const int128_t rhs) noexcept
{
    return detail::default_add(lhs, rhs);
}

#endif

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator+(const int128_t lhs, const UnsignedInteger rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    return detail::default_add(lhs, rhs);

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return {0, 0};

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator+(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    return detail::default_add(rhs, lhs);

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return {0, 0};

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator+(const int128_t lhs, const SignedInteger rhs) noexcept
{
    return rhs > 0 ? detail::default_add(lhs, rhs) : detail::default_sub(lhs, -rhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator+(const SignedInteger lhs, const int128_t rhs) noexcept
{
    return lhs > 0 ? detail::default_add(rhs, lhs) : detail::default_sub(rhs, -lhs);
}

#if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) || defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR int128_t operator+(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return detail::default_add(lhs, static_cast<int128_t>(rhs));
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR int128_t operator+(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return detail::default_add(rhs, static_cast<int128_t>(lhs));
}

#else // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR int128_t operator+(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR int128_t operator+(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR int128_t operator+(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return detail::default_add(lhs, static_cast<int128_t>(rhs));
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR int128_t operator+(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return detail::default_add(rhs, static_cast<int128_t>(lhs));
}

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

template <BOOST_DECIMAL_DETAIL_INT128_INTEGER_CONCEPT>
constexpr int128_t& int128_t::operator+=(const Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
    static_assert(detail::is_signed_integer_v<Integer>, "Sign Conversion Error");
    #endif

    *this = *this + rhs;
    return *this;
}

constexpr int128_t& int128_t::operator+=(const int128_t rhs) noexcept
{
    *this = *this + rhs;
    return *this;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

template <BOOST_DECIMAL_DETAIL_INT128_128BIT_INTEGER_CONCEPT>
inline int128_t& int128_t::operator+=(const Integer rhs) noexcept
{
    *this = *this + rhs;
    return *this;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

//=====================================
// Subtraction Operators
//=====================================

constexpr int128_t operator-(const int128_t lhs, const int128_t rhs) noexcept
{
    return detail::default_sub(lhs, rhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator-(const int128_t lhs, const UnsignedInteger rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    return detail::default_sub(lhs, rhs);

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return {0, 0};

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator-(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    return detail::default_add(-rhs, lhs);

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return {0, 0};

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator-(const int128_t lhs, const SignedInteger rhs) noexcept
{
    return detail::default_sub(lhs, static_cast<int128_t>(rhs));
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator-(const SignedInteger lhs, const int128_t rhs) noexcept
{
    return detail::default_sub(static_cast<int128_t>(lhs), rhs);
}

#if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128) || defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR int128_t operator-(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return lhs - static_cast<int128_t>(rhs);
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR int128_t operator-(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) - rhs;
}

#else // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR int128_t operator-(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR int128_t operator-(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR int128_t operator-(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return lhs - static_cast<int128_t>(rhs);
}

BOOST_DECIMAL_DETAIL_INT128_BUILTIN_CONSTEXPR int128_t operator-(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) - rhs;
}

#endif

template <BOOST_DECIMAL_DETAIL_INT128_INTEGER_CONCEPT>
constexpr int128_t& int128_t::operator-=(const Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
    static_assert(detail::is_signed_integer_v<Integer>, "Sign Conversion Error");
    #endif

    *this = *this - rhs;
    return *this;
}

constexpr int128_t& int128_t::operator-=(const int128_t rhs) noexcept
{
    *this = *this - rhs;
    return *this;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

template <BOOST_DECIMAL_DETAIL_INT128_128BIT_INTEGER_CONCEPT>
inline int128_t& int128_t::operator-=(const Integer rhs) noexcept
{
    *this = *this - rhs;
    return *this;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

//=====================================
// Multiplication Operators
//=====================================

namespace detail {

BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr int128_t signed_shift_left_32(const std::uint64_t low) noexcept
{
    return {static_cast<std::int64_t>(low >> 32), low << 32};
}

BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr int128_t library_mul(const int128_t lhs, const int128_t rhs) noexcept
{
    const auto a {lhs.low >> 32U};
    const auto b {lhs.low & UINT32_MAX};
    const auto c {rhs.low >> 32U};
    const auto d {rhs.low & UINT32_MAX};

    int128_t result { static_cast<std::int64_t>(static_cast<std::uint64_t>(lhs.high) * rhs.low + static_cast<std::uint64_t>(lhs.low) * rhs.high + a * c), b * d };
    result += signed_shift_left_32(a * d) + signed_shift_left_32(b * c);

    return result;
}

BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr int128_t default_mul(const int128_t lhs, const std::uint64_t rhs) noexcept
{
    const auto low_res{lhs.low * rhs};

    const auto a_lo{lhs.low & UINT32_MAX};
    const auto a_high{lhs.low >> 32U};
    const auto b_lo{rhs & UINT32_MAX};
    const auto b_high{rhs >> 32U};

    const auto lo_lo{a_lo * b_lo};
    const auto lo_hi{a_lo * b_high};
    const auto hi_lo{a_high * b_lo};
    const auto hi_hi{a_high * b_high};

    const auto mid{(lo_lo >> 32U) + (lo_hi & UINT32_MAX) + (hi_lo & UINT32_MAX)};

    const auto carry{hi_hi + (lo_hi >> 32) + (hi_lo >> 32) + (mid >> 32)};

    const auto high_res{lhs.high * static_cast<std::int64_t>(rhs) + static_cast<std::int64_t>(carry)};

    return {high_res, low_res};
}

BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr int128_t default_mul(const int128_t lhs, const std::uint32_t rhs) noexcept
{
    const auto low_res{lhs.low * rhs};

    const auto a_hi{lhs.low >> 32U};
    const auto hi_lo{a_hi * rhs};

    const auto high_res{lhs.high * static_cast<std::int64_t>(rhs) + static_cast<std::int64_t>(hi_lo)};

    return {high_res, low_res};
}

#if defined(_M_AMD64) && !defined(__GNUC__)

BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE int128_t msvc_amd64_mul(const int128_t lhs, const int128_t rhs) noexcept
{
    int128_t result {};
    result.low = _umul128(lhs.low, rhs.low, reinterpret_cast<std::uint64_t*>(&result.high));
    result.high += lhs.low * rhs.high;
    result.high += lhs.high * rhs.low;

    return result;
}

#endif

BOOST_DECIMAL_DETAIL_INT128_FORCE_INLINE constexpr int128_t default_mul(const int128_t lhs, const int128_t rhs) noexcept
{
    #if ((defined(__aarch64__) && defined(__APPLE__)) || defined(__x86_64__) || defined(__PPC__) || defined(__powerpc__)) && defined(__GNUC__) && !defined(__clang__) && defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)

    #  if !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION)

    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(lhs))
    {
        return library_mul(lhs, rhs);
    }
    else
    {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wclass-memaccess"

        detail::builtin_u128 new_lhs {};
        detail::builtin_u128 new_rhs {};

        std::memcpy(&new_lhs, &lhs, sizeof(detail::builtin_u128));
        std::memcpy(&new_rhs, &rhs, sizeof(detail::builtin_u128));

        const auto res {new_lhs * new_rhs};
        int128_t library_res {};

        std::memcpy(&library_res, &res, sizeof(detail::builtin_u128));

        return library_res;

        #pragma GCC diagnostic pop
    }

    #  elif defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)

    return static_cast<int128_t>(static_cast<detail::builtin_i128>(lhs) * static_cast<detail::builtin_i128>(rhs));

    #  else

    return library_mul(lhs, rhs);

    #  endif

    #elif defined(__aarch64__) && defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)

    return static_cast<int128_t>(static_cast<detail::builtin_i128>(lhs) * static_cast<detail::builtin_i128>(rhs));

    #elif defined(_M_AMD64) && !defined(__GNUC__) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION)

    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(rhs))
    {
        return library_mul(lhs, rhs); // LCOV_EXCL_LINE
    }
    else
    {
        return msvc_amd64_mul(lhs, rhs);
    }

    #elif (defined(_M_IX86) || defined(_M_ARM) || defined(__arm__)) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION)

    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(rhs))
    {
        return library_mul(lhs, rhs); // LCOV_EXCL_LINE
    }
    else
    {
        std::uint32_t lhs_words[4] {};
        std::uint32_t rhs_words[4] {};

        // Since in all likelihood this equates to memcpy we don't need to convert to non-negative integers and back
        to_words(lhs, lhs_words);
        to_words(rhs, rhs_words);

        return knuth_multiply<int128_t>(lhs_words, rhs_words);
    }

    #else

    return library_mul(lhs, rhs);

    #endif
}

} // namespace detail

constexpr int128_t operator*(const int128_t lhs, const int128_t rhs) noexcept
{
    return detail::default_mul(lhs, rhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator*(const int128_t lhs, const UnsignedInteger rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    using local_eval_type = detail::evaluation_type_t<UnsignedInteger>;
    return detail::default_mul(lhs, static_cast<local_eval_type>(rhs));

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return {0, 0};

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator*(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    using local_eval_type = detail::evaluation_type_t<UnsignedInteger>;
    return detail::default_mul(rhs, static_cast<local_eval_type>(lhs));

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return {0, 0};

    #endif
}

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4146) // Unary minus applied to unsigned
#endif

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator*(const int128_t lhs, const SignedInteger rhs) noexcept
{
    return rhs < 0 ? -detail::default_mul(lhs, -static_cast<std::uint64_t>(rhs)) :
                      detail::default_mul(lhs, static_cast<std::uint64_t>(rhs));
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator*(const SignedInteger lhs, const int128_t rhs) noexcept
{
    return lhs < 0 ? -detail::default_mul(rhs, -static_cast<std::uint64_t>(lhs)) :
                      detail::default_mul(rhs, static_cast<std::uint64_t>(lhs));
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

constexpr int128_t operator*(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return static_cast<int128_t>(static_cast<detail::builtin_i128>(lhs) * rhs);
}

constexpr int128_t operator*(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(static_cast<detail::builtin_i128>(rhs) * lhs);
}

#else // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
constexpr int128_t operator*(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
constexpr int128_t operator*(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

constexpr int128_t operator*(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return detail::default_mul(lhs, static_cast<int128_t>(rhs));
}

constexpr int128_t operator*(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return detail::default_mul(rhs, static_cast<int128_t>(lhs));
}

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

template <BOOST_DECIMAL_DETAIL_INT128_INTEGER_CONCEPT>
constexpr int128_t& int128_t::operator*=(const Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
    static_assert(detail::is_signed_integer_v<Integer>, "Sign Conversion Error");
    #endif

    *this = *this * rhs;
    return *this;
}

constexpr int128_t& int128_t::operator*=(const int128_t rhs) noexcept
{
    *this = *this * rhs;
    return *this;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

template <BOOST_DECIMAL_DETAIL_INT128_128BIT_INTEGER_CONCEPT>
inline int128_t& int128_t::operator*=(const Integer rhs) noexcept
{
    *this = *this * rhs;
    return *this;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

//=====================================
// Division Operator
//=====================================

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wassume"
#endif

constexpr int128_t operator/(const int128_t lhs, const int128_t rhs) noexcept
{
    if (BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(rhs == 0))
    {
        return {0, 0};
    }

    const auto abs_lhs {abs(lhs)};
    const auto abs_rhs {abs(rhs)};

    if (abs_lhs < abs_rhs)
    {
        return {0,0};
    }
    #if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)
    else
    {
        return static_cast<int128_t>(static_cast<detail::builtin_i128>(lhs) / static_cast<detail::builtin_i128>(rhs));
    }
    #else

    int128_t quotient {};
    const auto negative_res {(lhs.high < 0) != (rhs.high < 0)};

    if (abs_rhs.high != 0)
    {
        quotient = detail::knuth_div(abs_lhs, abs_rhs);
    }
    else
    {
        if (abs_lhs.high == 0)
        {
            quotient = {0, abs_lhs.low / abs_rhs.low};
        }
        else
        {
            detail::one_word_div(abs_lhs, abs_rhs.low, quotient);
        }
    }

    return negative_res ? -quotient : quotient;
    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator/(const int128_t lhs, const UnsignedInteger rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    using eval_type = detail::evaluation_type_t<UnsignedInteger>;

    if (BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(rhs == 0))
    {
        return {0, 0};
    }

    const auto abs_lhs {abs(lhs)};

    int128_t quotient {};
    detail::one_word_div(abs_lhs, static_cast<eval_type>(rhs), quotient);
    return lhs < 0 ? -quotient : quotient;

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return {0, 0};

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator/(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    if (BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(rhs == 0))
    {
        return {0, 0};
    }

    if (rhs.high != 0 && rhs.high != -1)
    {
        return {0,0};
    }
    else
    {
        auto abs_rhs {abs(rhs)};
        const auto res {static_cast<std::uint64_t>(lhs) / abs_rhs.low};
        return int128_t{rhs.high, res};
    }

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return {0, 0};

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator/(const int128_t lhs, const SignedInteger rhs) noexcept
{
    using eval_type = detail::evaluation_type_t<SignedInteger>;

    if (BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(rhs == 0))
    {
        return {0, 0};
    }

    int128_t quotient {};

    const auto negative_res {static_cast<bool>((lhs.high < 0) ^ (rhs < 0))};
    const auto abs_rhs {rhs < 0 ? -rhs : rhs};
    const auto abs_lhs {abs(lhs)};
    detail::one_word_div(abs_lhs, static_cast<eval_type>(abs_rhs), quotient);

    return negative_res ? -quotient : quotient;
}

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator/(const SignedInteger lhs, const int128_t rhs) noexcept
{
    if (BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(rhs == 0))
    {
        return {0, 0};
    }

    if (rhs.high != 0 && rhs.high != -1)
    {
        return {0,0};
    }
    else
    {
        const auto negative_res {static_cast<bool>((rhs.high < 0) ^ (lhs < 0))};
        const auto abs_rhs {abs(rhs)};
        const auto abs_lhs {lhs < 0 ? -lhs : lhs};
        const int128_t res {0, static_cast<std::uint64_t>(abs_lhs) / abs_rhs.low};

        return negative_res ? -res : res;
    }
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

constexpr int128_t operator/(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return static_cast<int128_t>(static_cast<detail::builtin_i128>(lhs) / rhs);
}

constexpr int128_t operator/(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs / static_cast<detail::builtin_i128>(rhs));
}

#else // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
constexpr int128_t operator/(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
constexpr int128_t operator/(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

constexpr int128_t operator/(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return static_cast<int128_t>(static_cast<detail::builtin_i128>(lhs) / rhs);
}

constexpr int128_t operator/(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs / static_cast<detail::builtin_i128>(rhs));
}

#elif defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

inline int128_t operator/(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return lhs / static_cast<int128_t>(rhs);
}

inline int128_t operator/(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) / rhs;
}

#else // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
inline int128_t operator/(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
inline int128_t operator/(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

inline int128_t operator/(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return lhs / static_cast<int128_t>(rhs);
}

inline int128_t operator/(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) / rhs;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

template <BOOST_DECIMAL_DETAIL_INT128_INTEGER_CONCEPT>
constexpr int128_t& int128_t::operator/=(const Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
    static_assert(detail::is_signed_integer_v<Integer>, "Sign Conversion Error");
    #endif

    *this = *this / rhs;
    return *this;
}

constexpr int128_t& int128_t::operator/=(const int128_t rhs) noexcept
{
    *this = *this / rhs;
    return *this;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

template <BOOST_DECIMAL_DETAIL_INT128_128BIT_INTEGER_CONCEPT>
inline int128_t& int128_t::operator/=(const Integer rhs) noexcept
{
    *this = *this / rhs;
    return *this;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

//=====================================
// Modulo Operator
//=====================================

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator%(int128_t lhs, UnsignedInteger rhs) noexcept;

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator%(UnsignedInteger lhs, int128_t rhs) noexcept;

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator%(int128_t lhs, SignedInteger rhs) noexcept;

template <BOOST_DECIMAL_DETAIL_INT128_DEFAULTED_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator%(SignedInteger lhs, int128_t rhs) noexcept;

constexpr int128_t operator%(int128_t lhs, int128_t rhs) noexcept;

template <BOOST_DECIMAL_DETAIL_INT128_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator%(const int128_t lhs, const UnsignedInteger rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    using eval_type = detail::evaluation_type_t<UnsignedInteger>;

    if (BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(rhs == 0))
    {
        return {0, 0};
    }

    int128_t quotient {};
    int128_t remainder {};

    const auto abs_lhs {abs(lhs)};

    detail::one_word_div(abs_lhs, static_cast<eval_type>(rhs), quotient, remainder);

    return lhs < 0 ? -remainder : remainder;

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return {0, 0};

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_UNSIGNED_INTEGER_CONCEPT>
constexpr int128_t operator%(const UnsignedInteger lhs, const int128_t rhs) noexcept
{
    #ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

    using eval_type = detail::evaluation_type_t<UnsignedInteger>;

    if (BOOST_DECIMAL_DETAIL_INT128_UNLIKELY(rhs == 0))
    {
        return {0, 0};
    }

    const auto abs_rhs {abs(rhs)};

    if (abs_rhs > lhs)
    {
        return lhs;
    }

    const int128_t remainder {0, static_cast<eval_type>(lhs) % rhs.low};

    return rhs < 0 ? -remainder : remainder;

    #else

    static_assert(detail::is_signed_integer_v<UnsignedInteger>, "Sign Conversion Error");
    static_cast<void>(lhs);
    static_cast<void>(rhs);
    return {0, 0};

    #endif
}

template <BOOST_DECIMAL_DETAIL_INT128_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator%(const int128_t lhs, const SignedInteger rhs) noexcept
{
    return lhs % static_cast<int128_t>(rhs);
}

template <BOOST_DECIMAL_DETAIL_INT128_SIGNED_INTEGER_CONCEPT>
constexpr int128_t operator%(const SignedInteger lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) % rhs;
}

constexpr int128_t operator%(const int128_t lhs, const int128_t rhs) noexcept
{
    if (rhs == 0)
    {
        return {0, 0};
    }

    const auto abs_lhs {abs(lhs)};
    const auto abs_rhs {abs(rhs)};

    if (abs_rhs > abs_lhs)
    {
        return lhs;
    }
    #if defined(BOOST_DECIMAL_DETAIL_INT128_HAS_INT128)
    else
    {
        return static_cast<int128_t>(static_cast<detail::builtin_i128>(lhs) % static_cast<detail::builtin_i128>(rhs));
    }
    #else

    #if defined(_MSC_VER) && !defined(__GNUC__)
    const auto is_neg{lhs < 0};
    #else
    const auto is_neg {(lhs < 0) != (rhs < 0)};
    #endif
    
    int128_t remainder {};

    if (abs_rhs.high != 0)
    {
        detail::knuth_div(abs_lhs, abs_rhs, remainder);
    }
    else
    {
        if (abs_lhs.high == 0)
        {
            remainder = int128_t{0, abs_lhs.low % abs_rhs.low};
        }
        else
        {
            int128_t quotient {};

            detail::one_word_div(abs_lhs, abs_rhs.low, quotient, remainder);
        }
    }

    return is_neg ? -remainder : remainder;

    #endif
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

constexpr int128_t operator%(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return static_cast<detail::builtin_i128>(lhs) % rhs;
}

constexpr int128_t operator%(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return lhs % static_cast<detail::builtin_i128>(rhs);
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

constexpr int128_t operator%(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return static_cast<int128_t>(static_cast<detail::builtin_u128>(lhs) % rhs);
}

constexpr int128_t operator%(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs % static_cast<detail::builtin_u128>(rhs));
}

#else // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
constexpr int128_t operator%(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
constexpr int128_t operator%(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

#elif defined(BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128)

inline int128_t operator%(const int128_t lhs, const detail::builtin_i128 rhs) noexcept
{
    return lhs % static_cast<int128_t>(rhs);
}

inline int128_t operator%(const detail::builtin_i128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) % rhs;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

inline int128_t operator%(const int128_t lhs, const detail::builtin_u128 rhs) noexcept
{
    return lhs % static_cast<int128_t>(rhs);
}

inline int128_t operator%(const detail::builtin_u128 lhs, const int128_t rhs) noexcept
{
    return static_cast<int128_t>(lhs) % rhs;
}

#else // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
inline int128_t operator%(const int128_t, const T) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

template <typename T, std::enable_if_t<std::is_same<T, detail::builtin_u128>::value, bool> = true>
inline int128_t operator%(const T, const int128_t) noexcept
{
    static_assert(detail::is_signed_integer_v<T>, "Sign Compare Error");
    return {0, 0};
}

#endif // BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

template <BOOST_DECIMAL_DETAIL_INT128_INTEGER_CONCEPT>
constexpr int128_t& int128_t::operator%=(const Integer rhs) noexcept
{
    #ifndef BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
    static_assert(detail::is_signed_integer_v<Integer>, "Sign Conversion Error");
    #endif

    *this = *this % rhs;
    return *this;
}

constexpr int128_t& int128_t::operator%=(const int128_t rhs) noexcept
{
    *this = *this % rhs;
    return *this;
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

template <BOOST_DECIMAL_DETAIL_INT128_128BIT_INTEGER_CONCEPT>
inline int128_t& int128_t::operator%=(const Integer rhs) noexcept
{
    *this = *this % rhs;
    return *this;
}

#endif // BOOST_DECIMAL_DETAIL_INT128_HAS_MSVC_INT128

namespace detail {

template <bool>
class numeric_limits_impl_i128
{
public:

        // Member constants
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;

    // C++23 deprecated the following two members
    #if defined(__GNUC__) && __cplusplus > 202002L
    #  pragma GCC diagnostic push
    #  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    #elif defined(_MSC_VER)
    #  pragma warning(push)
    #  pragma warning(disable:4996)
    #endif

    static constexpr std::float_denorm_style has_denorm = std::denorm_absent;
    static constexpr bool has_denorm_loss = false;

    #if defined(__GNUC__) && __cplusplus > 202002L
    #  pragma GCC diagnostic pop
    #elif defined(_MSC_VER)
    #  pragma warning(pop)
    #endif

    static constexpr std::float_round_style round_style = std::round_toward_zero;
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = true;
    static constexpr int digits = 127;
    static constexpr int digits10 = 38;
    static constexpr int max_digits10 = 0;
    static constexpr int radix = 2;
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool traps = std::numeric_limits<std::uint64_t>::traps;
    static constexpr bool tinyness_before = false;

    // Member functions
    static constexpr auto (min)        () -> boost::int128::int128_t { return {INT64_MIN, 0}; }
    static constexpr auto lowest       () -> boost::int128::int128_t { return {INT64_MIN, 0}; }
    static constexpr auto (max)        () -> boost::int128::int128_t { return {INT64_MAX, UINT64_MAX}; }
    static constexpr auto epsilon      () -> boost::int128::int128_t { return {0, 0}; }
    static constexpr auto round_error  () -> boost::int128::int128_t { return {0, 0}; }
    static constexpr auto infinity     () -> boost::int128::int128_t { return {0, 0}; }
    static constexpr auto quiet_NaN    () -> boost::int128::int128_t { return {0, 0}; }
    static constexpr auto signaling_NaN() -> boost::int128::int128_t { return {0, 0}; }
    static constexpr auto denorm_min   () -> boost::int128::int128_t { return {0, 0}; }
};

#if !defined(__cpp_inline_variables) || __cpp_inline_variables < 201606L

template <bool b> constexpr bool numeric_limits_impl_i128<b>::is_specialized;
template <bool b> constexpr bool numeric_limits_impl_i128<b>::is_signed;
template <bool b> constexpr bool numeric_limits_impl_i128<b>::is_integer;
template <bool b> constexpr bool numeric_limits_impl_i128<b>::is_exact;
template <bool b> constexpr bool numeric_limits_impl_i128<b>::has_infinity;
template <bool b> constexpr bool numeric_limits_impl_i128<b>::has_quiet_NaN;
template <bool b> constexpr bool numeric_limits_impl_i128<b>::has_signaling_NaN;

// These members were deprecated in C++23
#if ((!defined(_MSC_VER) && (__cplusplus <= 202002L)) || (defined(_MSC_VER) && (_MSVC_LANG <= 202002L)))
template <bool b> constexpr std::float_denorm_style numeric_limits_impl_i128<b>::has_denorm;
template <bool b> constexpr bool numeric_limits_impl_i128<b>::has_denorm_loss;
#endif

template <bool b> constexpr std::float_round_style numeric_limits_impl_i128<b>::round_style;
template <bool b> constexpr bool numeric_limits_impl_i128<b>::is_iec559;
template <bool b> constexpr bool numeric_limits_impl_i128<b>::is_bounded;
template <bool b> constexpr bool numeric_limits_impl_i128<b>::is_modulo;
template <bool b> constexpr int numeric_limits_impl_i128<b>::digits;
template <bool b> constexpr int numeric_limits_impl_i128<b>::digits10;
template <bool b> constexpr int numeric_limits_impl_i128<b>::max_digits10;
template <bool b> constexpr int numeric_limits_impl_i128<b>::radix;
template <bool b> constexpr int numeric_limits_impl_i128<b>::min_exponent;
template <bool b> constexpr int numeric_limits_impl_i128<b>::min_exponent10;
template <bool b> constexpr int numeric_limits_impl_i128<b>::max_exponent;
template <bool b> constexpr int numeric_limits_impl_i128<b>::max_exponent10;
template <bool b> constexpr bool numeric_limits_impl_i128<b>::traps;
template <bool b> constexpr bool numeric_limits_impl_i128<b>::tinyness_before;

#endif // !defined(__cpp_inline_variables) || __cpp_inline_variables < 201606L

} // namespace detail

} // namespace int128
} // namespace boost

namespace std {

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wmismatched-tags"
#endif

template <>
class numeric_limits<boost::int128::int128_t> :
    public boost::int128::detail::numeric_limits_impl_i128<true> {};

#ifdef __clang__
#  pragma clang diagnostic pop
#endif

} // namespace std

#endif // BOOST_DECIMAL_DETAIL_INT128_DETAIL_INT128_HPP
