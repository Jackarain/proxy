// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_INT128_BIT_HPP
#define BOOST_DECIMAL_DETAIL_INT128_BIT_HPP

#include "int128.hpp"
#include "detail/config.hpp"
#include "detail/clz.hpp"
#include "detail/ctz.hpp"

namespace boost {
namespace int128 {

constexpr bool has_single_bit(const uint128_t x) noexcept
{
    return x && !(x & (x - 1U));
}

constexpr int countl_zero(const uint128_t x) noexcept
{
    return x.high == 0 ? 64 + detail::countl_zero(x.low) : detail::countl_zero(x.high);
}

constexpr int countl_one(const uint128_t x) noexcept
{
    return countl_zero(~x);
}

constexpr int bit_width(const uint128_t x) noexcept
{
    return x ? 128 - countl_zero(x) : 0;
}

constexpr uint128_t bit_ceil(const uint128_t x) noexcept
{
    return x <= 1U ? static_cast<uint128_t>(1) : static_cast<uint128_t>(1) << bit_width(x - 1U);
}

constexpr uint128_t bit_floor(const uint128_t x) noexcept
{
    return x > 0U ? static_cast<uint128_t>(1) << (bit_width(x) - 1U) : static_cast<uint128_t>(0);
}

constexpr int countr_zero(const uint128_t x) noexcept
{
    return x.low == 0 ? 64 + detail::countr_zero(x.high) : detail::countr_zero(x.low);
}

constexpr int countr_one(const uint128_t x) noexcept
{
    return countr_zero(~x);
}

constexpr uint128_t rotl(const uint128_t x, const int s) noexcept
{
    constexpr auto mask {127U};
    return x << (static_cast<unsigned>(s) & mask) | x >> (static_cast<unsigned>(-s) & mask);
}

constexpr uint128_t rotr(const uint128_t x, const int s) noexcept
{
    constexpr auto mask {127U};
    return x >> (static_cast<unsigned>(s) & mask) | x << (static_cast<unsigned>(-s) & mask);
}

#if BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(__builtin_popcountll)

constexpr int popcount(const uint128_t x) noexcept
{
    return __builtin_popcountll(x.high) + __builtin_popcountll(x.low);
}

#endif

namespace impl {

constexpr int popcount_impl(std::uint64_t x) noexcept
{
    x = x - ((x >> 1U) & UINT64_C(0x5555555555555555));
    x = (x & UINT64_C(0x3333333333333333)) + ((x >> 2U) & UINT64_C(0x3333333333333333));
    x = (x + (x >> 4U)) & UINT64_C(0x0F0F0F0F0F0F0F0F);

    return static_cast<int>((x * UINT64_C(0x0101010101010101)) >> 56U);
}

} // namespace impl

#if defined(_M_AMD64) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(__builtin_popcountll)

constexpr int popcount(const uint128_t x) noexcept
{
    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x))
    {
        return impl::popcount_impl(x.high) + impl::popcount_impl(x.low); // LCOV_EXCL_LINE
    }
    else
    {
        #ifdef __AVX__

        return static_cast<int>(_mm_popcnt_u64(x.high) +  _mm_popcnt_u64(x.low));

        #else

        return static_cast<int>(__popcnt64(x.high) + __popcnt64(x.low));

        #endif
    }
}

#elif defined(_M_IX86) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(__builtin_popcountll)

constexpr int popcount(const uint128_t x) noexcept
{
    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x))
    {
        return impl::popcount_impl(x.high) + impl::popcount_impl(x.low); // LCOV_EXCL_LINE
    }
    else
    {
        #ifdef __AVX__

        return static_cast<int>(
               _mm_popcnt_u32(static_cast<unsigned>(x.high)) +
               _mm_popcnt_u32(static_cast<unsigned>(x.high >> 32U)) +
               _mm_popcnt_u32(static_cast<unsigned>(x.low)) +
               _mm_popcnt_u32(static_cast<unsigned>(x.low >> 32U)));

        #else

        return static_cast<int>(
               __popcnt(static_cast<unsigned>(x.high)) +
               __popcnt(static_cast<unsigned>(x.high >> 32U)) +
               __popcnt(static_cast<unsigned>(x.low)) +
               __popcnt(static_cast<unsigned>(x.low >> 32U)));

        #endif
    }
}

#elif !BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(__builtin_popcountll)

constexpr int popcount(const uint128_t x) noexcept
{
    return impl::popcount_impl(x.high) + impl::popcount_impl(x.low);
}

#endif

#if BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(__builtin_bswap64)

constexpr uint128_t byteswap(const uint128_t x) noexcept
{
    return {__builtin_bswap64(x.low), __builtin_bswap64(x.high)};
}

#endif

namespace impl {

constexpr std::uint64_t byteswap_impl(const std::uint64_t x) noexcept
{
    const auto step32 {x << 32U | x >> 32U};
    const auto step16 {(step32 & UINT64_C(0x0000FFFF0000FFFF)) << 16U | (step32 & UINT64_C(0xFFFF0000FFFF0000)) >> 16U};
    return (step16 & UINT64_C(0x00FF00FF00FF00FF)) << 8U | (step16 & UINT64_C(0xFF00FF00FF00FF00)) >> 8U;
}

constexpr uint128_t byteswap_impl(const uint128_t x) noexcept
{
    return {byteswap_impl(x.low), byteswap_impl(x.high)};
}

} // namespace impl

#if defined(_MSC_VER) && !defined(BOOST_DECIMAL_DETAIL_INT128_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(__builtin_bswap64)

constexpr uint128_t byteswap(const uint128_t x) noexcept
{
    if (BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x))
    {
        return impl::byteswap_impl(x); // LCOV_EXCL_LINE
    }
    else
    {
        return {_byteswap_uint64(x.low), _byteswap_uint64(x.high)};
    }
}

#elif !BOOST_DECIMAL_DETAIL_INT128_HAS_BUILTIN(__builtin_bswap64)

constexpr uint128_t byteswap(const uint128_t x) noexcept
{
    return impl::byteswap_impl(x);
}

#endif

} // namespace int128
} // namespace boost

#endif // BOOST_DECIMAL_DETAIL_INT128_BIT_HPP
