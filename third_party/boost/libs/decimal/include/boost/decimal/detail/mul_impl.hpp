// Copyright 2023 - 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_MUL_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_MUL_IMPL_HPP

#include <boost/decimal/detail/attributes.hpp>
#include <boost/decimal/detail/apply_sign.hpp>
#include <boost/decimal/detail/fenv_rounding.hpp>
#include "int128.hpp"
#include <boost/decimal/detail/u256.hpp>
#include <boost/decimal/detail/power_tables.hpp>
#include <boost/decimal/detail/components.hpp>
#include "int128.hpp"

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cstdint>
#endif

namespace boost {
namespace decimal {
namespace detail {

// Each type has two different multiplication impls
// 1) Returns a decimal type and lets the constructor handle with shrinking the significand
// 2) Returns a struct of the constituent components (used with FMAs)

template <typename ReturnType, typename T>
BOOST_DECIMAL_FORCE_INLINE constexpr auto mul_impl(const T& lhs, const T& rhs) noexcept -> ReturnType
{
    using mul_type = std::conditional_t<decimal_val_v<T> < 64, std::uint_fast64_t, int128::uint128_t>;

    // The constructor needs to calculate the number of digits in the significand which for uint128 is slow
    // Since we know the value of res_sig is constrained to [1'000'000^2, 9'999'999^2] which equates to
    // either 13 or 14 decimal digits we can use a single division to make binary search occur with
    // uint32_t instead. 14 - 5 = 9 or 13 - 5 = 8 which are both still greater than or equal to
    // digits10 + 1 for rounding which is 8 decimal digits

    auto res_sig {(static_cast<mul_type>(lhs.full_significand()) * static_cast<mul_type>(rhs.full_significand()))};
    auto res_exp {lhs.biased_exponent() + rhs.biased_exponent()};

    return {res_sig, res_exp, lhs.isneg() != rhs.isneg()};
}

template <typename ReturnType, typename T, typename U>
BOOST_DECIMAL_FORCE_INLINE constexpr auto mul_impl(T lhs_sig, U lhs_exp, bool lhs_sign,
                                                   T rhs_sig, U rhs_exp, bool rhs_sign) noexcept -> ReturnType
{
    using mul_type = std::uint_fast64_t;

    // The constructor needs to calculate the number of digits in the significand which for uint128 is slow
    // Since we know the value of res_sig is constrained to [1'000'000^2, 9'999'999^2] which equates to
    // either 13 or 14 decimal digits we can use a single division to make binary search occur with
    // uint32_t instead. 14 - 5 = 9 or 13 - 5 = 8 which are both still greater than or equal to
    // digits10 + 1 for rounding which is 8 decimal digits

    constexpr auto ten_pow_five {pow10(static_cast<mul_type>(5))};
    auto res_sig {(static_cast<mul_type>(lhs_sig) * static_cast<mul_type>(rhs_sig)) / ten_pow_five};
    auto res_exp {lhs_exp + rhs_exp + static_cast<U>(5)};

    return {static_cast<std::uint32_t>(res_sig), res_exp, lhs_sign != rhs_sign};
}

// In the fast case we are better served doing our 128-bit division here since we are at a know starting point
template <typename ReturnType, typename T>
constexpr auto d64_mul_impl(const T& lhs, const T& rhs) noexcept -> ReturnType
{
    using unsigned_int128_type = boost::int128::uint128_t;

    // Once we have the normalized significands and exponents all we have to do is
    // multiply the significands and add the exponents
    //
    // The constructor needs to calculate the number of digits in the significand which for uint128 is slow
    // Since we know the value of res_sig is constrained to [(10^16)^2, (10^17 - 1)^2] which equates to
    // either 31 or 32 decimal digits we can use a single division to make binary search occur with
    // uint_fast64_t instead. 32 - 13 = 19 or 31 - 13 = 18 which are both still greater than
    // digits10 + 1 for rounding which is 17 decimal digits

    constexpr auto ten_pow_13 {pow10(static_cast<unsigned_int128_type>(13))};
    const auto res_sig {static_cast<std::uint64_t>((static_cast<unsigned_int128_type>(lhs.full_significand()) * static_cast<unsigned_int128_type>(rhs.full_significand())) / ten_pow_13)};
    const auto res_exp {lhs.biased_exponent() + rhs.biased_exponent() + 13};

    return ReturnType{res_sig, res_exp, lhs.isneg() != rhs.isneg()};
}

template <typename ReturnType, BOOST_DECIMAL_INTEGRAL T, BOOST_DECIMAL_INTEGRAL U>
BOOST_DECIMAL_FORCE_INLINE constexpr auto d64_mul_impl(T lhs_sig, U lhs_exp, bool lhs_sign,
                                                       T rhs_sig, U rhs_exp, bool rhs_sign) noexcept
                                                       -> std::enable_if_t<detail::is_decimal_floating_point_v<ReturnType>, ReturnType>
{
    using unsigned_int128_type = boost::int128::uint128_t;

    // Once we have the normalized significands and exponents all we have to do is
    // multiply the significands and add the exponents
    //
    // The constructor needs to calculate the number of digits in the significand which for uint128 is slow
    // Since we know the value of res_sig is constrained to [(10^16)^2, (10^17 - 1)^2] which equates to
    // either 31 or 32 decimal digits we can use a single division to make binary search occur with
    // uint_fast64_t instead. 32 - 13 = 19 or 31 - 13 = 18 which are both still greater than
    // digits10 + 1 for rounding which is 17 decimal digits

    constexpr auto ten_pow_13 {pow10(static_cast<unsigned_int128_type>(13))};
    auto res_sig {(static_cast<unsigned_int128_type>(lhs_sig) * static_cast<unsigned_int128_type>(rhs_sig)) / ten_pow_13};
    auto res_exp {lhs_exp + rhs_exp + static_cast<U>(13)};

    return {static_cast<std::uint64_t>(res_sig), res_exp, lhs_sign != rhs_sign};
}

template <typename ReturnType, BOOST_DECIMAL_INTEGRAL T, BOOST_DECIMAL_INTEGRAL U>
BOOST_DECIMAL_FORCE_INLINE constexpr auto d64_mul_impl(T lhs_sig, U lhs_exp, bool lhs_sign,
                                                       T rhs_sig, U rhs_exp, bool rhs_sign) noexcept
                                                       -> std::enable_if_t<!detail::is_decimal_floating_point_v<ReturnType>, ReturnType>
{
    using unsigned_int128_type = boost::int128::uint128_t;
    constexpr auto comp_value {detail::pow10(static_cast<unsigned_int128_type>(31))};

    #ifdef BOOST_DECIMAL_DEBUG
    std::cerr << "sig lhs: " << sig_lhs
              << "\nexp lhs: " << exp_lhs
              << "\nsig rhs: " << sig_rhs
              << "\nexp rhs: " << exp_rhs;
    #endif

    bool sign {lhs_sign != rhs_sign};

    // Once we have the normalized significands and exponents all we have to do is
    // multiply the significands and add the exponents

    auto res_sig {static_cast<unsigned_int128_type>(lhs_sig) * static_cast<unsigned_int128_type>(rhs_sig)};
    auto res_exp {static_cast<typename ReturnType::biased_exponent_type>(lhs_exp + rhs_exp)};

    const auto sig_dig {res_sig >= comp_value ? 32 : 31};
    constexpr auto max_dig {std::numeric_limits<typename ReturnType::significand_type>::digits10};
    res_sig /= detail::pow10(static_cast<unsigned_int128_type>(sig_dig - max_dig));
    res_exp += sig_dig - max_dig;

    const auto res_sig_64 {static_cast<typename ReturnType::significand_type>(res_sig)};

    #ifdef BOOST_DECIMAL_DEBUG
    std::cerr << "\nres sig: " << res_sig_64
              << "\nres exp: " << res_exp << std::endl;
    #endif

    return {res_sig_64, res_exp, sign};
}

template <typename ReturnType, BOOST_DECIMAL_INTEGRAL T1, BOOST_DECIMAL_INTEGRAL U1,
                               BOOST_DECIMAL_INTEGRAL T2, BOOST_DECIMAL_INTEGRAL U2>
BOOST_DECIMAL_FORCE_INLINE
constexpr auto d128_mul_impl(const T1& lhs_sig, const U1 lhs_exp, const bool lhs_sign,
                             const T2& rhs_sig, const U2 rhs_exp, const bool rhs_sign) noexcept -> ReturnType
{
    using sig_type = T1;
    static_assert(std::is_same<sig_type, T2>::value, "Should have a common type by this point");

    const bool sign {lhs_sign != rhs_sign};

    auto res_sig {detail::umul256(lhs_sig, rhs_sig)};
    auto res_exp {lhs_exp + rhs_exp};
    const auto sig_dig {detail::num_digits(res_sig)};

    // 34 is the number of digits in the d128 significand
    // this way we can skip rounding in the constructor a second time
    const auto digit_delta {sig_dig - std::numeric_limits<sig_type>::digits10};
    if (BOOST_DECIMAL_LIKELY(digit_delta > 0))
    {
        auto biased_exp {res_exp + detail::bias_v<ReturnType>};
        detail::coefficient_rounding<ReturnType>(res_sig, res_exp, biased_exp, sign, sig_dig);
    }

    BOOST_DECIMAL_ASSERT((res_sig[3] | res_sig[2]) == 0U);
    return {int128::uint128_t{res_sig[1], res_sig[0]}, res_exp, sign};
}

template <typename ReturnType, BOOST_DECIMAL_INTEGRAL T1, BOOST_DECIMAL_INTEGRAL U1,
                               BOOST_DECIMAL_INTEGRAL T2, BOOST_DECIMAL_INTEGRAL U2>
BOOST_DECIMAL_FORCE_INLINE
constexpr auto d128_fast_mul_impl(const T1& lhs_sig, const U1 lhs_exp, const bool lhs_sign,
                                  const T2& rhs_sig, const U2 rhs_exp, const bool rhs_sign) noexcept -> ReturnType
{
    const bool sign {lhs_sign != rhs_sign};

    // Once we have the normalized significands and exponents all we have to do is
    // multiply the significands and add the exponents
    auto res_sig {detail::umul256(lhs_sig, rhs_sig)};
    const auto res_exp {lhs_exp + rhs_exp + 30};

    constexpr auto ten_pow_30 {detail::pow10(static_cast<int128::uint128_t>(30))};
    res_sig /= ten_pow_30;

    BOOST_DECIMAL_ASSERT((res_sig[3] | res_sig[2]) == 0U); // LCOV_EXCL_LINE
    return {int128::uint128_t{res_sig[1], res_sig[0]}, res_exp, sign};
}

template <typename ReturnType>
BOOST_DECIMAL_FORCE_INLINE auto mul_impl(const decimal128_t_components& lhs, const decimal128_t_components& rhs) noexcept -> ReturnType
{
    return d128_mul_impl<ReturnType>(lhs.sig, lhs.exp, lhs.sign,
                                     rhs.sig, rhs.exp, rhs.sign);
}

} // namespace detail
} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_MUL_IMPL_HPP
