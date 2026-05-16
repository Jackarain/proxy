// Copyright 2023 - 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_DIV_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_DIV_IMPL_HPP

#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/u256.hpp>
#include "int128.hpp"

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <limits>
#include <cstdint>
#endif

namespace boost {
namespace decimal {
namespace detail {

template <typename DecimalType, typename T>
BOOST_DECIMAL_FORCE_INLINE constexpr auto generic_div_impl(const T& lhs, const T& rhs) noexcept -> DecimalType
{
    // If rhs is greater than we need to offset the significands to get the correct values
    // e.g. 4/8 is 0 but 40/8 yields 5 in integer maths
    //
    // By expanding the offset to all the way to the value of numeric_limits<std::uint64_t>::digits10
    // we can recover more of what would become the fraction to achieve better rounding

    using div_type = std::uint64_t;

    constexpr auto precision_offset {std::numeric_limits<div_type>::digits10 - precision};
    constexpr auto ten_pow_offset {detail::pow10(static_cast<div_type>(precision_offset))};

    const auto big_sig_lhs {lhs.full_significand() * ten_pow_offset};

    const auto res_sig {big_sig_lhs / rhs.full_significand()};
    const auto res_exp {(lhs.biased_exponent() - precision_offset) - rhs.biased_exponent()};

    // Normalizes sign handling
    bool sign {lhs.isneg() != rhs.isneg()};
    if (BOOST_DECIMAL_UNLIKELY(res_sig == 0U))
    {
        sign = false;
    }

    // Let the constructor handle shrinking it back down and rounding correctly
    return DecimalType{res_sig, res_exp, sign};
}

template <typename DecimalType, typename T>
BOOST_DECIMAL_FORCE_INLINE constexpr auto d64_generic_div_impl(const T& lhs, const T& rhs, const bool sign) noexcept -> DecimalType
{
    using unsigned_int128_type = boost::int128::uint128_t;

    // If rhs is greater than we need to offset the significands to get the correct values
    // e.g. 4/8 is 0 but 40/8 yields 5 in integer maths
    constexpr auto offset {std::numeric_limits<unsigned_int128_type>::digits10 - detail::precision_v<decimal64_t>};
    constexpr auto tens_needed {detail::pow10(static_cast<unsigned_int128_type>(offset))};
    const auto big_sig_lhs {static_cast<unsigned_int128_type>(lhs.sig) * tens_needed};

    const auto res_sig {big_sig_lhs / rhs.sig};
    const auto res_exp {(lhs.exp - offset) - rhs.exp};

    // Let the constructor handle shrinking it back down and rounding correctly
    return DecimalType{res_sig, res_exp, sign};
}

template <typename T>
constexpr auto d128_generic_div_impl(const T& lhs, const T& rhs, T& q) noexcept -> void
{
    bool sign {lhs.sign != rhs.sign};

    constexpr auto ten_pow_precision {pow10(int128::uint128_t(detail::precision_v<decimal128_t>))};
    const auto big_sig_lhs {detail::umul256(lhs.sig, ten_pow_precision)};

    auto res_sig {big_sig_lhs / rhs.sig};
    auto res_exp {lhs.exp - rhs.exp - detail::precision_v<decimal128_t>};

    if (res_sig[3] != 0 || res_sig[2] != 0)
    {
        const auto sig_dig {detail::num_digits(res_sig)};
        const auto digit_delta {sig_dig - std::numeric_limits<int128::uint128_t>::digits10};
        res_sig /= pow10(int128::uint128_t(digit_delta));
        res_exp += digit_delta;
    }
    else if (res_sig[1] == 0 && res_sig[0] == 0)
    {
        sign = false;
    }

    // Let the constructor handle shrinking it back down and rounding correctly
    BOOST_DECIMAL_ASSERT((res_sig[3] | res_sig[2]) == 0U);
    q = T {int128::uint128_t{res_sig[1], res_sig[0]}, res_exp, sign};
}

} // namespace detail
} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_DIV_IMPL_HPP
