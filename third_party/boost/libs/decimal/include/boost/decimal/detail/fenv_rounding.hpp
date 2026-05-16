// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_FENV_ROUNDING_HPP
#define BOOST_DECIMAL_DETAIL_FENV_ROUNDING_HPP

#include <boost/decimal/cfenv.hpp>
#include <boost/decimal/detail/attributes.hpp>
#include <boost/decimal/detail/type_traits.hpp>
#include <boost/decimal/detail/config.hpp>
#include <boost/decimal/detail/power_tables.hpp>
#include <boost/decimal/detail/integer_search_trees.hpp>
#include "int128/cstdlib.hpp"

namespace boost {
namespace decimal {
namespace detail {

namespace impl {

// These structs are for internal use only and change based on the type of T
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wpadded"
#endif

template <typename T>
struct divmod_result
{
    T quotient;
    T remainder;
};

template <typename T>
struct divmod10_result
{
    T quotient;
    std::uint64_t remainder;
};

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

template <typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
constexpr auto divmod(T dividend, T divisor) noexcept -> divmod_result<T>
{
    // Compilers usually can lump these together
    const auto q {static_cast<T>(dividend / divisor)};
    const auto r {static_cast<T>(dividend % divisor)};
    return {q, r};
}

template <typename T, std::enable_if_t<!std::is_integral<T>::value, bool> = true>
constexpr auto divmod(T dividend, T divisor) noexcept -> divmod_result<T>
{
    T q {dividend / divisor};
    T r {dividend - q * divisor};
    return {q, r};
}

#ifdef BOOST_DECIMAL_DETAIL_INT128_HAS_INT128

constexpr auto divmod(const int128::uint128_t dividend, const int128::uint128_t divisor) -> divmod_result<int128::uint128_t>
{
    const auto builtin_num {static_cast<int128::detail::builtin_u128>(dividend)};
    const auto builtin_denom {static_cast<int128::detail::builtin_u128>(divisor)};
    return {builtin_num / builtin_denom, builtin_num % builtin_denom};
}

#endif

constexpr auto divmod(const u256& lhs, const u256& rhs) noexcept
{
    return div_mod(lhs, rhs);
}

template <typename T>
constexpr auto divmod10(const T dividend) noexcept -> divmod10_result<T>
{
    const auto q {static_cast<T>(dividend / 10U)};
    const auto r {static_cast<T>(dividend - q * 10U)};
    return {q, static_cast<std::uint64_t>(r)};
}

constexpr auto divmod10(const u256& lhs) noexcept
{
    constexpr int128::uint128_t ten {10U};
    return div_mod(lhs, ten);
}

constexpr auto divmod10(const int128::uint128_t lhs) noexcept -> divmod10_result<int128::uint128_t>
{
    constexpr std::uint32_t ten {10U};
    int128::uint128_t q {};
    int128::uint128_t r {};
    int128::detail::half_word_div(lhs, ten, q, r);
    return {q, r.low};
}

template <typename TargetType, typename T>
constexpr auto fenv_round_impl(T& val, const bool is_neg, const bool sticky, const rounding_mode round = _boost_decimal_global_rounding_mode) noexcept -> int
{
    using significand_type = std::conditional_t<decimal_val_v<TargetType> >= 128, int128::uint128_t, std::int64_t>;

    int exp {1};

    auto div_res {divmod10(val)};
    val = div_res.quotient;
    const auto trailing_num {div_res.remainder};

    // Default rounding mode
    switch (round)
    {
        case rounding_mode::fe_dec_to_nearest_from_zero:
            if (trailing_num >= 5U)
            {
                ++val;
            }
            break;
        case rounding_mode::fe_dec_downward:
            if (is_neg && (trailing_num != 0U || sticky))
            {
                ++val;
            }
            break;
        case rounding_mode::fe_dec_to_nearest:
            // Round to even or nearest
            if (trailing_num > 5U || (trailing_num == 5U && (sticky || (static_cast<std::uint64_t>(val) & 1U) == 1U)))
            {
                ++val;
            }
            break;
        case rounding_mode::fe_dec_toward_zero:
            // Do nothing
            break;
        case rounding_mode::fe_dec_upward:
            if (!is_neg && (trailing_num != 0U || sticky))
            {
                ++val;
            }
            break;
            // LCOV_EXCL_START
        default:
            BOOST_DECIMAL_UNREACHABLE;
            // LCOV_EXCL_STOP
    }

    // If the significand was e.g. 99'999'999 rounding up
    // would put it out of range again
    if (BOOST_DECIMAL_UNLIKELY(static_cast<significand_type>(val) > max_significand_v<TargetType>()))
    {
        val /= 10U;
        ++exp;
    }

    return exp;
}

}

#ifdef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION

template <typename TargetType, typename T, std::enable_if_t<is_integral_v<T>, bool> = true>
constexpr auto fenv_round(T& val, bool is_neg = false, bool sticky = false) noexcept -> int
{
    return impl::fenv_round_impl<TargetType>(val, is_neg, sticky);
}

#else

template <typename TargetType, typename T, std::enable_if_t<is_integral_v<T>, bool> = true>
constexpr auto fenv_round(T& val, bool is_neg = false, bool sticky = false) noexcept -> int // NOLINT(readability-function-cognitive-complexity)
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(coeff))
    {
        return impl::fenv_round_impl<TargetType>(val, is_neg, sticky);
    }
    else
    {
        const auto round {fegetround()};
        return impl::fenv_round_impl<TargetType>(val, is_neg, sticky, round);
    }
}

#endif

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4127)
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

template <typename TargetDecimalType, typename T1, typename T2, typename T3>
constexpr auto coefficient_rounding(T1& coeff, T2& exp, T3& biased_exp, const bool sign, int coeff_digits) noexcept
{
    // T1 will be a 128-bit or 256-bit
    using sig_type = typename TargetDecimalType::significand_type;
    using demoted_integer_type = std::conditional_t<std::numeric_limits<T1>::digits10 < std::numeric_limits<sig_type>::digits10, T1, sig_type>;

    // How many digits need to be shifted?
    const int shift_for_large_coeff {(coeff_digits - detail::precision_v<TargetDecimalType>) - 1};
    int shift {};
    BOOST_DECIMAL_IF_CONSTEXPR (is_fast_type_v<TargetDecimalType>)
    {
        // For fast types we never want to reduce past precision digits
        // otherwise we could potentially end up incorrectly normalized
        shift = shift_for_large_coeff;
    }
    else
    {
        const auto shift_for_small_exp {static_cast<int>((-biased_exp) - 1)};
        shift = std::max(shift_for_small_exp, shift_for_large_coeff);
    }

    if (BOOST_DECIMAL_UNLIKELY(shift > std::numeric_limits<T1>::digits10))
    {
        // Bounds check for our tables in pow10
        coeff = 0;
        return 1;
    }

    // Do shifting
    BOOST_DECIMAL_ASSERT(shift >= 0);
    const auto shift_pow_ten {detail::pow10(static_cast<T1>(shift))};

    // In the synthetic integer cases it's inexpensive to see if we can demote the type
    // relative to the cost of the division and modulo operation
    demoted_integer_type shifted_coeff {};
    bool sticky {};
    BOOST_DECIMAL_IF_CONSTEXPR (sizeof(T1) < sizeof(int128::uint128_t))
    {
        const auto div_res {impl::divmod(coeff, shift_pow_ten)};
        shifted_coeff = static_cast<demoted_integer_type>(div_res.quotient);
        const auto trailing_digits {div_res.remainder};
        sticky = trailing_digits != 0U;
    }
    else
    {
        if (coeff < std::numeric_limits<demoted_integer_type>::max())
        {
            const auto smaller_coeff {static_cast<demoted_integer_type>(coeff)};
            const auto div_res {impl::divmod(smaller_coeff, static_cast<demoted_integer_type>(shift_pow_ten))};
            shifted_coeff = static_cast<demoted_integer_type>(div_res.quotient);
            const auto trailing_digits {div_res.remainder};
            sticky = trailing_digits != 0U;
        }
        else
        {
            const auto div_res {impl::divmod(coeff, shift_pow_ten)};
            shifted_coeff = static_cast<demoted_integer_type>(div_res.quotient);
            const auto trailing_digits {div_res.remainder};
            sticky = trailing_digits != 0U;
        }
    }

    // Do rounding
    const auto removed_digits {detail::fenv_round<TargetDecimalType>(shifted_coeff, sign, sticky)};
    coeff = static_cast<T1>(shifted_coeff);

    const auto offset {removed_digits + shift};
    exp += offset;
    biased_exp += offset;
    coeff_digits -= offset;

    return coeff_digits;
}

#ifdef _MSC_VER
#  pragma warning(pop)
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

} // namespace detail
} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_FENV_ROUNDING_HPP
