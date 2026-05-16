// Copyright 2023 - 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#ifndef BOOST_DECIMAL_DETAIL_ADD_IMPL_HPP
#define BOOST_DECIMAL_DETAIL_ADD_IMPL_HPP

#include <boost/decimal/detail/attributes.hpp>
#include <boost/decimal/detail/apply_sign.hpp>
#include <boost/decimal/detail/fenv_rounding.hpp>
#include <boost/decimal/detail/components.hpp>
#include <boost/decimal/detail/power_tables.hpp>
#include <boost/decimal/detail/promotion.hpp>
#include <boost/decimal/detail/is_power_of_10.hpp>
#include <boost/decimal/detail/i256.hpp>
#include "int128.hpp"

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cstdint>
#endif

namespace boost {
namespace decimal {
namespace detail {

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4127) // Conditional expression is constant
#endif

template <typename ReturnType, typename T>
constexpr auto add_impl(const T& lhs, const T& rhs) noexcept -> ReturnType
{
    // Each of the significands is maximally 23 bits.
    // Rather than doing division to get proper alignment we will promote to 64 bits
    // And do a single mul followed by an add
    using add_type = std::conditional_t<decimal_val_v<T> < 64, std::int_fast64_t, int128::int128_t>;
    using promoted_sig_type = std::conditional_t<decimal_val_v<T> < 64, std::uint_fast64_t, int128::uint128_t>;

    promoted_sig_type big_lhs {lhs.full_significand()};
    promoted_sig_type big_rhs {rhs.full_significand()};
    auto lhs_exp {lhs.biased_exponent()};
    auto rhs_exp {rhs.biased_exponent()};

    // Align to larger exponent
    if (lhs_exp != rhs_exp)
    {
        constexpr auto max_shift {detail::make_positive_unsigned(std::numeric_limits<promoted_sig_type>::digits10 - detail::precision_v<ReturnType> - 1)};
        const auto shift {detail::make_positive_unsigned(lhs_exp - rhs_exp)};

        if (shift > max_shift)
        {
            auto round {_boost_decimal_global_rounding_mode};

            #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION

            if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
            {
                round = fegetround();
            }

            #endif

            if (BOOST_DECIMAL_LIKELY(round != rounding_mode::fe_dec_downward && round != rounding_mode::fe_dec_upward))
            {
                return big_lhs != 0U && (lhs_exp > rhs_exp) ?
                                    ReturnType{lhs.full_significand(), lhs.biased_exponent(), lhs.isneg()} :
                                    ReturnType{rhs.full_significand(), rhs.biased_exponent(), rhs.isneg()};
            }
            else if (round == rounding_mode::fe_dec_downward)
            {
                // If we are subtracting even disparate numbers we need to round down
                // E.g. "5e+95"_DF - "4e-100"_DF == "4.999999e+95"_DF
                const auto use_lhs {big_lhs != 0U && (lhs_exp > rhs_exp)};

                // Need to check for the case where we have 1e+95 - anything = 9.99999... without losing a nine
                if (use_lhs)
                {
                    if (big_rhs != 0U && (lhs.isneg() != rhs.isneg()))
                    {
                        if (is_power_of_10(big_lhs))
                        {
                            --big_lhs;
                            big_lhs *= 10U;
                            big_lhs += 9U;
                            --lhs_exp;
                        }
                        else
                        {
                            --big_lhs;
                        }
                    }

                    return ReturnType{big_lhs, lhs_exp, lhs.isneg()};
                }
                else
                {
                    if (big_lhs != 0U && (lhs.isneg() != rhs.isneg()))
                    {
                        if (is_power_of_10(big_rhs))
                        {
                            --big_rhs;
                            big_rhs *= 10U;
                            big_rhs += 9U;
                            --rhs_exp;
                        }
                        else
                        {
                            --big_rhs;
                        }
                    }

                    return ReturnType{big_rhs, rhs_exp, rhs.isneg()};
                }
            }
            else
            {
                // rounding mode == fe_dec_upward
                // Unconditionally round up. Could be 5e+95 + 4e-100 -> 5.000001e+95
                const bool use_lhs {big_lhs != 0U && (lhs_exp > rhs_exp)};

                if (use_lhs)
                {
                    if (big_rhs != 0U)
                    {
                        if (lhs.isneg() != rhs.isneg())
                        {
                            if (is_power_of_10(big_lhs))
                            {
                                --big_lhs;
                                big_lhs *= 10U;
                                big_lhs += 9U;
                                --lhs_exp;
                            }
                            else
                            {
                                --big_lhs;
                            }
                        }
                        else
                        {
                            ++big_lhs;
                        }
                    }

                    return ReturnType{big_lhs, lhs_exp, lhs.isneg()} ;
                }
                else
                {
                    if (big_lhs != 0U)
                    {
                        if (rhs.isneg() != lhs.isneg())
                        {
                            --big_rhs;
                            big_rhs *= 10U;
                            big_rhs += 9U;
                            --rhs_exp;
                        }
                        else
                        {
                            ++big_rhs;
                        }
                    }

                    return ReturnType{big_rhs, rhs_exp, rhs.isneg()};
                }
            }
        }

        if (lhs_exp < rhs_exp)
        {
            big_rhs *= detail::pow10<promoted_sig_type>(shift);
            lhs_exp = rhs_exp - static_cast<decltype(lhs_exp)>(shift);
        }
        else
        {
            big_lhs *= detail::pow10<promoted_sig_type>(shift);
            lhs_exp -= static_cast<decltype(lhs_exp)>(shift);
        }
    }

    // Perform signed addition with overflow protection
    const auto signed_lhs {detail::make_signed_value<add_type>(static_cast<add_type>(big_lhs), lhs.isneg())};
    const auto signed_rhs {detail::make_signed_value<add_type>(static_cast<add_type>(big_rhs), rhs.isneg())};

    const auto new_sig {signed_lhs + signed_rhs};
    const auto return_sig {detail::make_positive_unsigned(new_sig)};

    return ReturnType{return_sig, lhs_exp, new_sig < 0};
}

template <typename ReturnType, typename T>
constexpr auto d128_add_impl_new(const T& lhs, const T& rhs) noexcept -> ReturnType
{
    using promoted_sig_type = u256;

    auto big_lhs {lhs.full_significand()};
    auto big_rhs {rhs.full_significand()};
    auto lhs_exp {lhs.biased_exponent()};
    auto rhs_exp {rhs.biased_exponent()};
    promoted_sig_type promoted_lhs {big_lhs};
    promoted_sig_type promoted_rhs {big_rhs};

    // Align to larger exponent
    if (lhs_exp != rhs_exp)
    {
        constexpr auto max_shift {detail::make_positive_unsigned(std::numeric_limits<promoted_sig_type>::digits10 - detail::precision_v<ReturnType> - 1)};
        const auto shift {detail::make_positive_unsigned(lhs_exp - rhs_exp)};

        if (shift > max_shift)
        {
            auto round {_boost_decimal_global_rounding_mode};

            #ifndef BOOST_DECIMAL_NO_CONSTEVAL_DETECTION

            if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
            {
                round = fegetround();
            }

            #endif

            if (BOOST_DECIMAL_LIKELY(round != rounding_mode::fe_dec_downward && round != rounding_mode::fe_dec_upward))
            {
                return big_lhs != 0U && (lhs_exp > rhs_exp) ?
                                    ReturnType{lhs.full_significand(), lhs.biased_exponent(), lhs.isneg()} :
                                    ReturnType{rhs.full_significand(), rhs.biased_exponent(), rhs.isneg()};
            }
            else if (round == rounding_mode::fe_dec_downward)
            {
                // If we are subtracting even disparate numbers we need to round down
                // E.g. "5e+95"_DF - "4e-100"_DF == "4.999999e+95"_DF
                const auto use_lhs {big_lhs != 0U && (lhs_exp > rhs_exp)};

                // Need to check for the case where we have 1e+95 - anything = 9.99999... without losing a nine
                if (use_lhs)
                {
                    if (big_rhs != 0U && (lhs.isneg() != rhs.isneg()))
                    {
                        if (is_power_of_10(big_lhs))
                        {
                            --big_lhs;
                            big_lhs *= 10U;
                            big_lhs += 9U;
                            --lhs_exp;
                        }
                        else
                        {
                            --big_lhs;
                        }
                    }

                    return ReturnType{big_lhs, lhs_exp, lhs.isneg()};
                }
                else
                {
                    if (big_lhs != 0U && (lhs.isneg() != rhs.isneg()))
                    {
                        if (is_power_of_10(big_rhs))
                        {
                            --big_rhs;
                            big_rhs *= 10U;
                            big_rhs += 9U;
                            --rhs_exp;
                        }
                        else
                        {
                            --big_rhs;
                        }
                    }

                    return ReturnType{big_rhs, rhs_exp, rhs.isneg()};
                }
            }
            else
            {
                // rounding mode == fe_dec_upward
                // Unconditionally round up. Could be 5e+95 + 4e-100 -> 5.000001e+95
                const bool use_lhs {big_lhs != 0U && (lhs_exp > rhs_exp)};

                if (use_lhs)
                {
                    if (big_rhs != 0U)
                    {
                        if (lhs.isneg() != rhs.isneg())
                        {
                            if (is_power_of_10(big_lhs))
                            {
                                --big_lhs;
                                big_lhs *= 10U;
                                big_lhs += 9U;
                                --lhs_exp;
                            }
                            else
                            {
                                --big_lhs;
                            }
                        }
                        else
                        {
                            ++big_lhs;
                        }
                    }

                    return ReturnType{big_lhs, lhs_exp, lhs.isneg()} ;
                }
                else
                {
                    if (big_lhs != 0U)
                    {
                        if (rhs.isneg() != lhs.isneg())
                        {
                            --big_rhs;
                            big_rhs *= 10U;
                            big_rhs += 9U;
                            --rhs_exp;
                        }
                        else
                        {
                            ++big_rhs;
                        }
                    }

                    return ReturnType{big_rhs, rhs_exp, rhs.isneg()};
                }
            }
        }

        const auto shift_pow10 {detail::pow10_256(shift)};

        if (lhs_exp < rhs_exp)
        {
            promoted_rhs *= shift_pow10;
            lhs_exp = rhs_exp - static_cast<decltype(lhs_exp)>(shift);
        }
        else
        {
            promoted_lhs *= shift_pow10;
            lhs_exp -= static_cast<decltype(lhs_exp)>(shift);
        }
    }
    
    u256 return_sig {};
    bool return_sign {};
    const auto lhs_sign {lhs.isneg()};
    const auto rhs_sign {rhs.isneg()};

    if (lhs_sign && !rhs_sign)
    {
        // -lhs + rhs = rhs - lhs
        return_sign = i256_sub(promoted_rhs, promoted_lhs, return_sig);
    }
    else if (!lhs_sign && rhs_sign)
    {
        // lhs - rhs
        return_sign = i256_sub(promoted_lhs, promoted_rhs, return_sig);
    }
    else
    {
        // lhs + rhs or -lhs + -rhs
        return_sig = promoted_lhs + promoted_rhs;
        return_sign = lhs_sign && rhs_sign;
    }

    BOOST_DECIMAL_IF_CONSTEXPR (detail::decimal_val_v<ReturnType> == 128)
    {
        // In the regular 128-bit case there's a chance the high words are empty,
        // and we can just convert to 128-bit arithmetic now
        if (return_sig[2] == 0U && return_sig[3] == 0U)
        {
            return ReturnType{static_cast<int128::uint128_t>(return_sig), lhs_exp, return_sign};
        }
    }

    return ReturnType{return_sig, lhs_exp, return_sign};
}

#ifdef _MSC_VER
#  pragma warning(pop)
#endif

} // namespace detail
} // namespace decimal
} // namespace boost

#endif //BOOST_DECIMAL_DETAIL_ADD_IMPL_HPP
