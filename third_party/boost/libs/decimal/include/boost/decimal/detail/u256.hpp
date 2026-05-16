// Copyright 2023 - 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// This is not a fully featured implementation of a 256-bit integer like int128::uint128_t is
// u256 only contains the minimum amount that we need to perform operations like decimal128_t mul

#ifndef BOOST_DECIMAL_DETAIL_U256_HPP
#define BOOST_DECIMAL_DETAIL_U256_HPP

#include <boost/decimal/detail/config.hpp>
#include "int128.hpp"

#ifndef BOOST_DECIMAL_BUILD_MODULE
#include <cstdint>
#include <cmath>
#endif

namespace boost {
namespace decimal {
namespace detail {

struct alignas(sizeof(std::uint64_t) * 4)
u256
{
    std::uint64_t bytes[4] {};

    // Constructors
    constexpr u256() noexcept = default;
    constexpr u256(const u256& other) noexcept = default;
    constexpr u256(u256&& other) noexcept = default;
    constexpr u256& operator=(const u256& other) noexcept = default;
    constexpr u256& operator=(u256&& other) noexcept = default;

    constexpr u256(std::uint64_t byte3, std::uint64_t byte2, std::uint64_t byte1, std::uint64_t byte0) noexcept;
    constexpr u256(const int128::uint128_t x) noexcept { bytes[0] = x.low; bytes[1] = x.high; }
    constexpr u256(const std::uint64_t x) noexcept { bytes[0] = x; }

    explicit constexpr operator std::uint64_t() const noexcept { return bytes[0]; }

    template<typename T = std::size_t>
    explicit constexpr operator std::enable_if_t<
        !std::is_same<T, std::uint64_t>::value, T>() const noexcept
    {
        return static_cast<std::size_t>(bytes[0]);
    }

    // Conversion to/from int128::uint128_t
    constexpr u256(const int128::uint128_t& high_, const int128::uint128_t& low_) noexcept;
    explicit constexpr operator int128::uint128_t() const noexcept;

    constexpr std::uint64_t operator[](std::size_t i) const noexcept;
    constexpr std::uint64_t& operator[](std::size_t i) noexcept;

    // Compound operators
    constexpr u256& operator<<=(int amount) noexcept;
    constexpr u256& operator>>=(int amount) noexcept;
    constexpr u256& operator|=(const u256& rhs) noexcept;

    constexpr u256& operator*=(const u256& rhs) noexcept;

    constexpr u256& operator/=(const u256& rhs) noexcept;
    constexpr u256& operator/=(const int128::uint128_t& rhs) noexcept;
    constexpr u256& operator/=(std::uint64_t rhs) noexcept;

    constexpr u256& operator%=(const u256& rhs) noexcept;
    constexpr u256& operator%=(std::uint64_t rhs) noexcept;

    constexpr u256& operator++() noexcept;
    constexpr u256& operator++(int) noexcept;
};

} // namespace detail
} // namespace decimal
} // namespace boost

namespace boost {
namespace decimal {
namespace detail {

constexpr u256::u256(const std::uint64_t byte3, const std::uint64_t byte2, const std::uint64_t byte1, const std::uint64_t byte0) noexcept
{
    bytes[0] = byte0;
    bytes[1] = byte1;
    bytes[2] = byte2;
    bytes[3] = byte3;
}

constexpr u256::u256(const int128::uint128_t& high_, const int128::uint128_t& low_) noexcept
{
    bytes[0] = low_.low;
    bytes[1] = low_.high;
    bytes[2] = high_.low;
    bytes[3] = high_.high;
}

constexpr u256::operator int128::uint128_t() const noexcept
{
    return int128::uint128_t {bytes[1], bytes[0]};
}

constexpr std::uint64_t u256::operator[](const std::size_t i) const noexcept
{
    BOOST_DECIMAL_ASSERT(i < 4);
    return bytes[i];
}

constexpr std::uint64_t& u256::operator[](const std::size_t i) noexcept
{
    BOOST_DECIMAL_ASSERT(i < 4);
    return bytes[i];
}

//=====================================
// Equality Operators
//=====================================

namespace impl {

BOOST_DECIMAL_FORCE_INLINE constexpr bool basic_equality_impl(const u256& lhs, const u256& rhs) noexcept
{
    return lhs[0] == rhs[0] && lhs[1] == rhs[1] && lhs[2] == rhs[2] && lhs[3] == rhs[3];
}

BOOST_DECIMAL_FORCE_INLINE constexpr bool basic_inequality_impl(const u256& lhs, const u256& rhs) noexcept
{
    return lhs[0] != rhs[0] || lhs[1] != rhs[1] || lhs[2] != rhs[2] || lhs[3] != rhs[3];
}

} // namespace impl

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__AVX2__)

constexpr bool operator==(const u256& lhs, const u256& rhs) noexcept
{
    // Start comp from low word since they will most likely be filled
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_equality_impl(lhs, rhs);
    }
    else
    {
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.bytes));
        __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.bytes));

        __m256i cmp = _mm256_cmpeq_epi64(a, b);

        const int mask = _mm256_movemask_epi8(cmp);

        return mask == -1;
    }
}

#else

constexpr bool operator==(const u256& lhs, const u256& rhs) noexcept
{
    return impl::basic_equality_impl(lhs, rhs);
}

#endif

//=====================================
// Inequality Operators
//=====================================

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__AVX2__)

constexpr bool operator!=(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_inequality_impl(lhs, rhs);
    }
    else
    {
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(lhs.bytes));
        __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(rhs.bytes));

        __m256i cmp = _mm256_cmpeq_epi64(a, b);

        const int mask = _mm256_movemask_epi8(cmp);

        return mask != -1;
    }
}

#else

constexpr bool operator!=(const u256& lhs, const u256& rhs) noexcept
{
    return impl::basic_inequality_impl(lhs, rhs);
}

#endif // !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__AVX2__)

//=====================================
// Less Than Operator
//=====================================

namespace impl {

BOOST_DECIMAL_FORCE_INLINE constexpr bool basic_lt_impl(const u256& lhs, const u256& rhs) noexcept
{
    if (lhs[3] != rhs[3])
    {
        return lhs[3] < rhs[3];
    }
    if (lhs[2] != rhs[2])
    {
        return lhs[2] < rhs[2];
    }
    if (lhs[1] != rhs[1])
    {
        return lhs[1] < rhs[1];
    }

    return lhs[0] < rhs[0];
}

} // namespace impl

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__AVX2__)

constexpr bool operator<(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_lt_impl(lhs, rhs);
    }
    else
    {
        __m256i lhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&lhs));
        __m256i rhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&rhs));

        __m256i eq_mask = _mm256_cmpeq_epi64(lhs_vec, rhs_vec);
        uint32_t eq_bits = _mm256_movemask_pd(_mm256_castsi256_pd(eq_mask));

        if ((eq_bits & 0x8) == 0)
        {
            return lhs[3] < rhs[3];
        }
        else if ((eq_bits & 0x4) == 0)
        {
            return lhs[2] < rhs[2];
        }
        else if ((eq_bits & 0x2) == 0)
        {
            return lhs[1] < rhs[1];
        }
        else if ((eq_bits & 0x1) == 0)
        {
            return lhs[0] < rhs[0];
        }

        return false;
    }
}

#else

constexpr bool operator<(const u256& lhs, const u256& rhs) noexcept
{
    return impl::basic_lt_impl(lhs, rhs);
}

#endif

constexpr bool operator<(const u256& lhs, const int128::uint128_t& rhs) noexcept
{
    return lhs[3] == 0U && lhs[2] == 0U && int128::uint128_t{lhs[1], lhs[0]} < rhs;
}

constexpr bool operator<(const int128::uint128_t& lhs, const u256& rhs) noexcept
{
    return rhs[3] == 0U && rhs[2] == 0U && lhs < int128::uint128_t{rhs[1], rhs[0]};
}

constexpr bool operator<(const u256& lhs, const std::uint64_t rhs) noexcept
{
    return lhs[3] == 0 && lhs[2] == 0 && lhs[1] == 0 && lhs[0] < rhs;
}

constexpr bool operator<(const std::uint64_t lhs, const u256& rhs) noexcept
{
    return rhs[3] == 0 && rhs[2] == 0 && rhs[1] == 0 && lhs < rhs[0];
}

//=====================================
// Less Equal Operator
//=====================================

namespace impl {

BOOST_DECIMAL_FORCE_INLINE constexpr bool basic_le_impl(const u256& lhs, const u256& rhs) noexcept
{
    return !(rhs < lhs);
}

} // namespace impl

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__AVX2__)

constexpr bool operator<=(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_le_impl(lhs, rhs);
    }
    else
    {
        __m256i lhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&lhs));
        __m256i rhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&rhs));

        // Compare all elements for equality
        __m256i eq_mask = _mm256_cmpeq_epi64(lhs_vec, rhs_vec);
        uint32_t eq_bits = _mm256_movemask_pd(_mm256_castsi256_pd(eq_mask));

        // Check each position from most significant to least significant
        if ((eq_bits & 0x8) == 0)
        {
            return (rhs[3] > lhs[3]);
        }
        else if ((eq_bits & 0x4) == 0)
        {
            return (rhs[2] > lhs[2]);
        }
        else if ((eq_bits & 0x2) == 0)
        {
            return (rhs[1] > lhs[1]);
        }
        else if ((eq_bits & 0x1) == 0)
        {
            return (rhs[0] > lhs[0]);
        }
        else
        {
            return true;
        }
    }
}

#else

constexpr bool operator<=(const u256& lhs, const u256& rhs) noexcept
{
    return impl::basic_le_impl(lhs, rhs);
}

#endif

constexpr bool operator<=(const u256& lhs, const std::uint64_t rhs) noexcept
{
    return lhs[3] == 0 && lhs[2] == 0 && lhs[1] == 0 && lhs[0] <= rhs;
}

constexpr bool operator<=(const std::uint64_t lhs, const u256& rhs) noexcept
{
    return rhs[3] == 0 && rhs[2] == 0 && rhs[1] == 0 && lhs <= rhs[0];
}

//=====================================
// Greater Than Operator
//=====================================

constexpr bool operator>(const u256& lhs, const u256& rhs) noexcept
{
    return rhs < lhs;
}

constexpr bool operator>(const u256& lhs, const int128::uint128_t& rhs) noexcept
{
    return lhs[3] > 0U || lhs[2] > 0U || int128::uint128_t{lhs[1], lhs[0]} > rhs;
}

constexpr bool operator>(const u256& lhs, const std::uint64_t rhs) noexcept
{
    return lhs[3] != 0U || lhs[2] != 0U || lhs[1] != 0U || lhs[0] > rhs;
}

//=====================================
// Greater Equal Operator
//=====================================

constexpr bool operator>=(const u256& lhs, const u256& rhs) noexcept
{
    return !(lhs < rhs);
}

//=====================================
// Left Shift Operators
//=====================================

constexpr u256 operator<<(const u256& lhs, const int shift) noexcept
{
    u256 result {};

    if (shift >= 256 || shift < 0)
    {
        return result;
    }

    const auto word_shift {static_cast<std::size_t>(shift / 64)};
    const auto bit_shift {static_cast<std::size_t>(shift % 64)};

    // Only moving whole words
    if (bit_shift == 0)
    {
        for (std::size_t i {word_shift}; i < 4; ++i)
        {
            result[i] = lhs[i - word_shift];
        }

        return result;
    }

    if (word_shift < 4U)
    {
        result[word_shift] = lhs[0U] << bit_shift;
    }

    for (std::size_t i {word_shift + 1U}; i < 4U; ++i)
    {
        result[i] = (lhs[i - word_shift] << bit_shift) |
                    (lhs[i - word_shift - 1] >> (64 - bit_shift));
    }

    return result;
}

constexpr u256& u256::operator<<=(const int amount) noexcept
{
    *this = *this << amount;
    return *this;
}

//=====================================
// Right Shift Operators
//=====================================

constexpr u256 operator>>(const u256& lhs, const int shift) noexcept
{
    u256 result {};

    if (shift >= 256)
    {
        return result;
    }

    const auto word_shift {static_cast<std::size_t>(shift / 64)};
    const auto bit_shift {static_cast<std::size_t>(shift % 64)};

    // Only moving whole words
    if (bit_shift == 0)
    {
        for (std::size_t i {}; i < 4 - word_shift; ++i)
        {
            result[i] = lhs[i + word_shift];
        }

        return result;
    }

    // Handle partial shifts across word boundaries
    for (std::size_t i {}; i < 4 - word_shift - 1; ++i)
    {
        result[i] = (lhs[i + word_shift] >> bit_shift) |
                    (lhs[i + word_shift + 1] << (64 - bit_shift));
    }

    // Handle the last word that has a partial shift
    if (word_shift < 4)
    {
        result[3 - word_shift] = lhs[3] >> bit_shift;
    }

    return result;
}

constexpr u256& u256::operator>>=(const int amount) noexcept
{
    *this = *this >> amount;
    return *this;
}

//=====================================
// Or Operators
//=====================================

namespace impl {

BOOST_DECIMAL_FORCE_INLINE constexpr u256 basic_or_impl(const u256& lhs, const u256& rhs) noexcept
{
    u256 result;

    result[3] = lhs[3] | rhs[3];
    result[2] = lhs[2] | rhs[2];
    result[1] = lhs[1] | rhs[1];
    result[0] = lhs[0] | rhs[0];

    return result;
}

} // namespace impl

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__AVX2__)

constexpr u256 operator|(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_or_impl(lhs, rhs);
    }
    else
    {
        u256 result;

        // Load 256 bits from each operand into AVX2 registers
        __m256i lhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&lhs));
        __m256i rhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&rhs));

        // Perform the bitwise OR operation
        __m256i result_vec = _mm256_or_si256(lhs_vec, rhs_vec);

        // Store the result back to memory
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&result), result_vec);

        return result;
    }
}

#elif !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(BOOST_DECIMAL_HAS_ARM_INTRINSICS)

constexpr u256 operator|(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_or_impl(lhs, rhs);
    }
    else
    {
        u256 result;

        uint64x2_t lhs_low = vld1q_u64(&lhs.bytes[0]);
        uint64x2_t lhs_high = vld1q_u64(&lhs.bytes[2]);

        uint64x2_t rhs_low = vld1q_u64(&rhs.bytes[0]);
        uint64x2_t rhs_high = vld1q_u64(&rhs.bytes[2]);

        // Perform bitwise OR in parallel
        uint64x2_t result_low = vorrq_u64(lhs_low, rhs_low);
        uint64x2_t result_high = vorrq_u64(lhs_high, rhs_high);

        // Store results back
        vst1q_u64(&result[0], result_low);
        vst1q_u64(&result[2], result_high);

        return result;
    }
}

#else

constexpr u256 operator|(const u256& lhs, const u256& rhs) noexcept
{
    return impl::basic_or_impl(lhs, rhs);
}

#endif

constexpr u256& u256::operator|=(const u256& rhs) noexcept
{
    *this = *this | rhs;
    return *this;
}

//=====================================
// And Operators
//=====================================

namespace impl {

BOOST_DECIMAL_FORCE_INLINE constexpr u256 basic_and_impl(const u256& lhs, const u256& rhs) noexcept
{
    u256 result;

    result[3] = lhs[3] & rhs[3];
    result[2] = lhs[2] & rhs[2];
    result[1] = lhs[1] & rhs[1];
    result[0] = lhs[0] & rhs[0];

    return result;
}

} // namespace impl

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__AVX2__)

constexpr u256 operator&(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_and_impl(lhs, rhs);
    }
    else
    {
        u256 result;

        // Load 256 bits from each operand into AVX2 registers
        __m256i lhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&lhs));
        __m256i rhs_vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&rhs));

        // Perform the bitwise AND operation
        __m256i result_vec = _mm256_and_si256(lhs_vec, rhs_vec);

        // Store the result back to memory
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(&result), result_vec);

        return result;
    }
}

#elif !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(BOOST_DECIMAL_HAS_ARM_INTRINSICS)

constexpr u256 operator&(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_and_impl(lhs, rhs);
    }
    else
    {
        u256 result;

        uint64x2_t lhs_low = vld1q_u64(&lhs.bytes[0]);
        uint64x2_t lhs_high = vld1q_u64(&lhs.bytes[2]);

        uint64x2_t rhs_low = vld1q_u64(&rhs.bytes[0]);
        uint64x2_t rhs_high = vld1q_u64(&rhs.bytes[2]);

        // Perform bitwise AND in parallel
        uint64x2_t result_low = vandq_u64(lhs_low, rhs_low);
        uint64x2_t result_high = vandq_u64(lhs_high, rhs_high);

        // Store results back
        vst1q_u64(&result[0], result_low);
        vst1q_u64(&result[2], result_high);

        return result;
    }
}

#else

constexpr u256 operator&(const u256& lhs, const u256& rhs) noexcept
{
    return impl::basic_and_impl(lhs, rhs);
}

#endif

//=====================================
// Addition Operators
//=====================================

namespace impl {

constexpr u256 basic_add_impl(const u256& lhs, const u256& rhs) noexcept
{
    u256 result;
    std::uint64_t carry {};

    auto sum {lhs[0] + rhs[0]};
    result[0] = sum;
    carry = (sum < lhs[0]) ? 1 : 0;

    sum = lhs[1] + rhs[1] + carry;
    result[1] = sum;
    carry = (sum < lhs[1] || (sum == lhs[1] && carry)) ? 1 : 0;

    sum = lhs[2] + rhs[2] + carry;
    result[2] = sum;
    carry = (sum < lhs[2] || (sum == lhs[2] && carry)) ? 1 : 0;

    result[3] = lhs[3] + rhs[3] + carry;

    return result;
}

} // namespace impl

#if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(BOOST_DECIMAL_ADD_CARRY)

constexpr u256 operator+(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_add_impl(lhs, rhs);
    }
    else
    {
        unsigned long long result[4] {};
        unsigned char carry {};
        carry = BOOST_DECIMAL_ADD_CARRY(carry, lhs[0], rhs[0], &result[0]);
        carry = BOOST_DECIMAL_ADD_CARRY(carry, lhs[1], rhs[1], &result[1]);
        carry = BOOST_DECIMAL_ADD_CARRY(carry, lhs[2], rhs[2], &result[2]);
        carry = BOOST_DECIMAL_ADD_CARRY(carry, lhs[3], rhs[3], &result[3]);

        return {result[3], result[2], result[1], result[0]};
    }
}

#elif !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && defined(__GNUC__) && !defined(BOOST_DECIMAL_ADD_CARRY)

namespace impl {

inline bool add_carry_u64(const bool carry_in, const std::uint64_t a, const std::uint64_t b, unsigned long long* sum) noexcept
{
    unsigned long long res;
    auto c = __builtin_uaddll_overflow(a, b, &res);
    c |= __builtin_uaddll_overflow(res, static_cast<unsigned long long>(carry_in), &res);
    *sum = res;
    return c;
}

} // namespace impl

constexpr u256 operator+(const u256& lhs, const u256& rhs) noexcept
{
    if (BOOST_DECIMAL_IS_CONSTANT_EVALUATED(lhs))
    {
        return impl::basic_add_impl(lhs, rhs);
    }
    else
    {
        unsigned long long result[4] {};
        bool carry {};
        carry = impl::add_carry_u64(carry, lhs[0], rhs[0], &result[0]);
        carry = impl::add_carry_u64(carry, lhs[1], rhs[1], &result[1]);
        carry = impl::add_carry_u64(carry, lhs[2], rhs[2], &result[2]);
        carry = impl::add_carry_u64(carry, lhs[3], rhs[3], &result[3]);

        return {result[3], result[2], result[1], result[0]};
    }
}

#else

constexpr u256 operator+(const u256& lhs, const u256& rhs) noexcept
{
    return impl::basic_add_impl(lhs, rhs);
}

#endif

constexpr u256& u256::operator++() noexcept
{
    *this = *this + static_cast<std::uint64_t>(1);
    return *this;
}

constexpr u256& u256::operator++(int) noexcept
{
    *this = *this + static_cast<std::uint64_t>(1);
    return *this;
}


//=====================================
// Multiplication Operators
//=====================================

namespace impl {

#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

template <std::size_t word_size>
BOOST_DECIMAL_FORCE_INLINE constexpr u256 from_words(const std::uint32_t (&words)[word_size]) noexcept
{
    static_assert(word_size >= 8, "Not enough words to convert to u256");

    u256 result {};

    #if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_ENDIAN_BIG_BYTE
    if (!BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(words))
    {
        std::memcpy(&result, words, sizeof(result));
    }
    else
    #endif
    {
        result[0] = static_cast<std::uint64_t>(words[0]) | (static_cast<std::uint64_t>(words[1]) << 32);
        result[1] = static_cast<std::uint64_t>(words[2]) | (static_cast<std::uint64_t>(words[3]) << 32);
        result[2] = static_cast<std::uint64_t>(words[4]) | (static_cast<std::uint64_t>(words[5]) << 32);
        result[3] = static_cast<std::uint64_t>(words[6]) | (static_cast<std::uint64_t>(words[7]) << 32);
    }

    return result;
}

#if defined(__GNUC__) && __GNUC__ >= 7
#pragma GCC diagnostic pop
#endif

template <std::size_t u_size, std::size_t v_size>
constexpr u256 knuth_mulitply(const std::uint32_t (&u)[u_size],
                              const std::uint32_t (&v)[v_size]) noexcept
{
    std::uint32_t w[u_size + v_size] {};

    // M.1
    for (std::size_t j {}; j < v_size; ++j)
    {
        // M.2
        if (v[j] == 0)
        {
            w[j + u_size] = 0u;
            continue;
        }

        // M.3
        std::uint64_t t {};
        for (std::size_t i {}; i < u_size; ++i)
        {
            // M.4
            t += static_cast<std::uint64_t>(u[i]) * v[j] + w[i + j];
            w[i + j] = static_cast<std::uint32_t>(t);
            t >>= 32u;
        }

        // M.5
        w[j + u_size] = static_cast<std::uint32_t>(t);
    }

    return from_words(w);
}

constexpr void to_words(const u256& x, std::uint32_t (&words)[8]) noexcept
{
    #if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_ENDIAN_BIG_BYTE
    if (!BOOST_DECIMAL_DETAIL_INT128_IS_CONSTANT_EVALUATED(x))
    {
        std::memcpy(words, &x, sizeof(x));
    }
    else
    #endif
    {

        words[0] = static_cast<std::uint32_t>(x[0] & UINT32_MAX);
        words[1] = static_cast<std::uint32_t>(x[0] >> 32U);
        words[2] = static_cast<std::uint32_t>(x[1] & UINT32_MAX);
        words[3] = static_cast<std::uint32_t>(x[1] >> 32U);
        words[4] = static_cast<std::uint32_t>(x[2] & UINT32_MAX);
        words[5] = static_cast<std::uint32_t>(x[2] >> 32U);
        words[6] = static_cast<std::uint32_t>(x[3] & UINT32_MAX);
        words[7] = static_cast<std::uint32_t>(x[3] >> 32U);
    }
}

template <typename UnsignedInteger>
BOOST_DECIMAL_FORCE_INLINE constexpr u256 default_mul(const u256& lhs, const UnsignedInteger& rhs) noexcept
{
    using boost::decimal::detail::impl::to_words;
    using boost::int128::detail::to_words;

    constexpr std::size_t rhs_words_needed {sizeof(UnsignedInteger) / sizeof(std::uint32_t)};

    std::uint32_t lhs_words[8] {};
    std::uint32_t rhs_words[rhs_words_needed] {};

    to_words(lhs, lhs_words);
    to_words(rhs, rhs_words);

    return knuth_mulitply(lhs_words, rhs_words);
}

} // namespace impl

constexpr u256 operator*(const u256& lhs, const u256& rhs) noexcept
{
    return impl::default_mul(lhs, rhs);
}

template <typename UnsignedInteger>
constexpr u256 operator*(const u256& lhs, const UnsignedInteger rhs) noexcept
{
    return impl::default_mul(lhs, rhs);
}

template <typename UnsignedInteger>
constexpr u256 operator*(const UnsignedInteger lhs, const u256& rhs) noexcept
{
    return impl::default_mul(rhs, lhs);
}

constexpr u256 umul256(const int128::uint128_t& a, const int128::uint128_t& b) noexcept
{
    u256 result{};

    const int128::uint128_t a_low {a.low};
    const int128::uint128_t a_high {a.high};
    const auto b_low {b.low};
    const auto b_high {b.high};

    const auto p0 = a_low * b_low;
    const auto p1 = a_low * b_high;
    const auto p2 = a_high * b_low;
    const auto p3 = a_high * b_high;

    // Combine results
    const auto middle = p1 + p2 + p0.high;

    result.bytes[0] = p0.low;
    result.bytes[1] = middle.low;

    const auto high_sum = middle.high + p3;
    result.bytes[2] = high_sum.low;
    result.bytes[3] = high_sum.high;

    return result;
}

// 128×64→256 multiplication (SoftFloat-style lightweight primitive)
// Used when rhs is 64-bit (e.g. r_scaled from approx_recip_sqrt64)
// Explicit uint128_t cast ensures 64×64→128 widening (a.low*b otherwise returns uint64_t on some platforms)
constexpr u256 mul128By64(const int128::uint128_t& a, const std::uint64_t b) noexcept
{
    const int128::uint128_t p0 = int128::uint128_t{a.low} * b;   // 64×64→128
    const int128::uint128_t p1 = int128::uint128_t{a.high} * b; // 64×64→128
    const auto mid = p1.low + p0.high;
    const std::uint64_t carry1 = (mid < p0.high) ? 1U : 0U;
    const auto hi = p1.high + carry1;
    const std::uint64_t carry2 = (hi < carry1) ? 1U : 0U;

    u256 result{};
    result.bytes[0] = p0.low;
    result.bytes[1] = mid;
    result.bytes[2] = hi;
    result.bytes[3] = carry2;
    return result;
}

constexpr u256& u256::operator*=(const u256& rhs) noexcept
{
    *this = *this * rhs;
    return *this;
}

//=====================================
// Division Operators
//=====================================

namespace impl {

constexpr std::size_t div_to_words(const u256& x, std::uint32_t (&words)[8]) noexcept
{
    #if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_ENDIAN_BIG_BYTE
    if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(x))
    {
        std::memcpy(words, &x, sizeof(x));
    }
    else
    #endif
    {
        words[0] = static_cast<std::uint32_t>(x[0] & UINT32_MAX);
        words[1] = static_cast<std::uint32_t>(x[0] >> 32U);
        words[2] = static_cast<std::uint32_t>(x[1] & UINT32_MAX);
        words[3] = static_cast<std::uint32_t>(x[1] >> 32U);
        words[4] = static_cast<std::uint32_t>(x[2] & UINT32_MAX);
        words[5] = static_cast<std::uint32_t>(x[2] >> 32U);
        words[6] = static_cast<std::uint32_t>(x[3] & UINT32_MAX);
        words[7] = static_cast<std::uint32_t>(x[3] >> 32U);
    }

    std::size_t word_count {8};
    while (words[word_count - 1U] == 0U)
    {
        --word_count;
    }

    return word_count;
}

constexpr std::size_t div_to_words(const boost::int128::uint128_t& x, std::uint32_t (&words)[8]) noexcept
{
    #if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_ENDIAN_BIG_BYTE
    if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(x))
    {
        std::memcpy(words, &x, sizeof(boost::int128::uint128_t));
    }
    else
    #endif
    {
        words[0] = static_cast<std::uint32_t>(x.low & UINT32_MAX);                              // LCOV_EXCL_LINE
        words[1] = static_cast<std::uint32_t>(x.low >> 32);                                     // LCOV_EXCL_LINE
        words[2] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(x.high) & UINT32_MAX); // LCOV_EXCL_LINE
        words[3] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(x.high) >> 32);        // LCOV_EXCL_LINE
    }

    BOOST_DECIMAL_DETAIL_INT128_ASSERT_MSG(x != 0U, "Division by 0"); // LCOV_EXCL_LINE : False Negative

    std::size_t word_count {4};
    while (words[word_count - 1U] == 0U)
    {
        word_count--;
    }

    return word_count;
}

constexpr std::size_t div_to_words(const std::uint64_t x, std::uint32_t (&words)[2]) noexcept
{
    #if !defined(BOOST_DECIMAL_NO_CONSTEVAL_DETECTION) && !BOOST_DECIMAL_ENDIAN_BIG_BYTE
    if (!BOOST_DECIMAL_IS_CONSTANT_EVALUATED(x))
    {
        std::memcpy(words, &x, sizeof(std::uint64_t));
    }
    else
    #endif
    {
        words[0] = static_cast<std::uint32_t>(x & UINT32_MAX);
        words[1] = static_cast<std::uint32_t>(x >> 32);
    }

    BOOST_DECIMAL_DETAIL_INT128_ASSERT_MSG(x != 0U, "Division by 0");

    std::size_t word_count {2};
    while (words[word_count - 1U] == 0U)
    {
        word_count--;
    }

    return word_count;
}

BOOST_DECIMAL_FORCE_INLINE constexpr u256 default_div(const u256& lhs, const std::uint64_t rhs) noexcept
{
    u256 quotient;

    int128::uint128_t current {lhs[3]};
    quotient[3] = static_cast<std::uint64_t>(current / rhs);
    auto remainder = static_cast<std::uint64_t>(current % rhs);

    current = static_cast<int128::uint128_t>(remainder) << 64U | lhs[2];
    quotient[2] = static_cast<std::uint64_t>(current / rhs);
    remainder = static_cast<std::uint64_t>(current % rhs);

    current = static_cast<int128::uint128_t>(remainder) << 64U | lhs[1];
    quotient[1] = static_cast<std::uint64_t>(current / rhs);
    remainder = static_cast<std::uint64_t>(current % rhs);

    current = static_cast<int128::uint128_t>(remainder) << 64U | lhs[0];
    quotient[0] = static_cast<std::uint64_t>(current / rhs);

    return quotient;
}

template <typename UnsignedInteger>
BOOST_DECIMAL_FORCE_INLINE constexpr u256 default_div(const u256& lhs, const UnsignedInteger& rhs) noexcept
{
    if (rhs <= UINT64_MAX)
    {
        return default_div(lhs, static_cast<std::uint64_t>(rhs));
    }
    else if (lhs < rhs)
    {
        return u256{};
    }

    std::uint32_t u[8] {};
    std::uint32_t v[8] {};
    std::uint32_t q[8] {};

    const auto m {div_to_words(lhs, u)};
    const auto n {div_to_words(rhs, v)};

    BOOST_DECIMAL_DETAIL_INT128_ASSERT(m >= n);

    int128::detail::impl::knuth_divide<false>(u, m, v, n, q);

    return from_words(q);
}

struct u256_divmod_result
{
    u256 quotient;
    u256 remainder;
};

template <typename UnsignedInteger>
BOOST_DECIMAL_FORCE_INLINE constexpr auto div_mod(const u256& lhs, const UnsignedInteger& rhs) noexcept -> u256_divmod_result
{
    std::uint32_t u[8] {};
    std::uint32_t v[8] {};
    std::uint32_t q[8] {};

    const auto m {div_to_words(lhs, u)};
    const auto n {div_to_words(rhs, v)};

    BOOST_DECIMAL_DETAIL_INT128_ASSERT(m >= n);

    // Simplify handling of single word division
    // We run into this case with dividing by powers of 10 while rounding u256
    if (n == 1U)
    {
        std::uint64_t remainder {};

        for (std::size_t j = m; j-- > 0;)
        {
            const auto dividend {(remainder << 32) | u[j]};
            q[j] = static_cast<std::uint32_t>(dividend / v[0]);
            remainder = dividend % v[0];
        }

        u[0] = static_cast<std::uint32_t>(remainder);
    }
    else
    {
        int128::detail::impl::knuth_divide<true>(u, m, v, n, q);
    }

     return {from_words(q), from_words(u)};
}

} // namespace impl

constexpr u256 operator/(const u256& lhs, const u256& rhs) noexcept
{
    return impl::default_div(lhs, rhs);
}

template <typename UnsignedInteger>
constexpr u256 operator/(const u256& lhs, const UnsignedInteger rhs) noexcept
{
    return impl::default_div(lhs, rhs);
}

constexpr u256& u256::operator/=(const u256& rhs) noexcept
{
    *this = *this / rhs;
    return *this;
}

constexpr u256& u256::operator/=(const int128::uint128_t& rhs) noexcept
{
    *this = *this / rhs;
    return *this;
}

constexpr u256& u256::operator/=(const std::uint64_t rhs) noexcept
{
    *this = impl::default_div(*this, rhs);
    return *this;
}

//=====================================
// Modulo Operators
//=====================================

namespace impl {

BOOST_DECIMAL_FORCE_INLINE constexpr u256 default_mod(const u256& lhs, const std::uint64_t rhs) noexcept
{
    u256 quotient;

    int128::uint128_t current {lhs[3]};
    quotient[3] = static_cast<std::uint64_t>(current / rhs);
    auto remainder = static_cast<std::uint64_t>(current % rhs);

    current = static_cast<int128::uint128_t>(remainder) << 64U | lhs[2];
    quotient[2] = static_cast<std::uint64_t>(current / rhs);
    remainder = static_cast<std::uint64_t>(current % rhs);

    current = static_cast<int128::uint128_t>(remainder) << 64U | lhs[1];
    quotient[1] = static_cast<std::uint64_t>(current / rhs);
    remainder = static_cast<std::uint64_t>(current % rhs);

    current = static_cast<int128::uint128_t>(remainder) << 64U | lhs[0];
    quotient[0] = static_cast<std::uint64_t>(current / rhs);
    remainder = static_cast<std::uint64_t>(current % rhs);

    return remainder;
}

template <typename UnsignedInteger>
BOOST_DECIMAL_FORCE_INLINE constexpr u256 default_mod(const u256& lhs, const UnsignedInteger& rhs) noexcept
{
    if (rhs <= UINT64_MAX)
    {
        return default_mod(lhs, static_cast<std::uint64_t>(rhs));
    }

    std::uint32_t u[8] {};
    std::uint32_t v[8] {};
    std::uint32_t q[8] {};

    const auto m {div_to_words(lhs, u)};
    const auto n {div_to_words(rhs, v)};

    int128::detail::impl::knuth_divide<true>(u, m, v, n, q);

    return from_words(u);
}

} // namespace impl

constexpr u256 operator%(const u256& lhs, const u256& rhs) noexcept
{
    return impl::default_mod(lhs, rhs);
}

template <typename UnsignedInteger>
constexpr u256 operator%(const u256& lhs, const UnsignedInteger rhs) noexcept
{
    return impl::default_mod(lhs, rhs);
}

constexpr u256& u256::operator%=(const u256& rhs) noexcept
{
    *this = *this % rhs;
    return *this;
}

constexpr u256& u256::operator%=(const std::uint64_t rhs) noexcept
{
    *this = impl::default_mod(*this, rhs);
    return *this;
}

namespace impl {

inline auto u256_to_buffer(char (&buffer)[128], u256 v) noexcept
{
    constexpr u256 zero {0, 0, 0, 0};

    char* p = buffer + 128;
    *--p = '\0';

    do
    {
        const auto index {static_cast<std::size_t>(v % UINT64_C(10))};
        BOOST_DECIMAL_ASSERT(index <= 10);
        *--p = "0123456789"[index];
        v /= UINT64_C(10);
    }
    while ( v != zero );

    return p;
}

} // namespace impl

#if !defined(BOOST_DECIMAL_DISABLE_IOSTREAM)
template <typename charT, typename traits>
auto operator<<(std::basic_ostream<charT, traits>& os, const u256& val) -> std::basic_ostream<charT, traits>&
{
    char buffer[128];

    os << impl::u256_to_buffer(buffer, val);

    return os;
}
#endif

template <bool>
class numeric_limits_impl256
{
public:

    // Member constants
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;

    // These members were deprecated in C++23
    #if ((!defined(_MSC_VER) && (__cplusplus <= 202002L)) || (defined(_MSC_VER) && (_MSVC_LANG <= 202002L)))
    static constexpr std::float_denorm_style has_denorm = std::denorm_absent;
    static constexpr bool has_denorm_loss = false;
    #endif

    static constexpr std::float_round_style round_style = std::round_toward_zero;
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = true;
    static constexpr int digits = 256;
    static constexpr int digits10 = 78;
    static constexpr int max_digits10 = 0;
    static constexpr int radix = 2;
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool traps = std::numeric_limits<std::uint64_t>::traps;
    static constexpr bool tinyness_before = false;

    // Member functions
    static constexpr u256 (min)() { return {0, 0, 0, 0}; }
    static constexpr u256 lowest() { return {0, 0, 0, 0}; }
    static constexpr u256 (max)() { return {UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX}; }
    static constexpr u256 epsilon() { return {0, 0, 0, 0}; }
    static constexpr u256 round_error() { return {0, 0, 0, 0}; }
    static constexpr u256 infinity() { return {0, 0, 0, 0}; }
    static constexpr u256 quiet_NaN() { return {0, 0, 0, 0}; }
    static constexpr u256 signaling_NaN() { return {0, 0, 0, 0}; }
    static constexpr u256 denorm_min() { return {0, 0, 0, 0}; }
};

#if !defined(__cpp_inline_variables) || __cpp_inline_variables < 201606L

template <bool b> constexpr bool numeric_limits_impl256<b>::is_specialized;
template <bool b> constexpr bool numeric_limits_impl256<b>::is_signed;
template <bool b> constexpr bool numeric_limits_impl256<b>::is_integer;
template <bool b> constexpr bool numeric_limits_impl256<b>::is_exact;
template <bool b> constexpr bool numeric_limits_impl256<b>::has_infinity;
template <bool b> constexpr bool numeric_limits_impl256<b>::has_quiet_NaN;
template <bool b> constexpr bool numeric_limits_impl256<b>::has_signaling_NaN;

// These members were deprecated in C++23
#if ((!defined(_MSC_VER) && (__cplusplus <= 202002L)) || (defined(_MSC_VER) && (_MSVC_LANG <= 202002L)))
template <bool b> constexpr std::float_denorm_style numeric_limits_impl256<b>::has_denorm;
template <bool b> constexpr bool numeric_limits_impl256<b>::has_denorm_loss;
#endif

template <bool b> constexpr std::float_round_style numeric_limits_impl256<b>::round_style;
template <bool b> constexpr bool numeric_limits_impl256<b>::is_iec559;
template <bool b> constexpr bool numeric_limits_impl256<b>::is_bounded;
template <bool b> constexpr bool numeric_limits_impl256<b>::is_modulo;
template <bool b> constexpr int numeric_limits_impl256<b>::digits;
template <bool b> constexpr int numeric_limits_impl256<b>::digits10;
template <bool b> constexpr int numeric_limits_impl256<b>::max_digits10;
template <bool b> constexpr int numeric_limits_impl256<b>::radix;
template <bool b> constexpr int numeric_limits_impl256<b>::min_exponent;
template <bool b> constexpr int numeric_limits_impl256<b>::min_exponent10;
template <bool b> constexpr int numeric_limits_impl256<b>::max_exponent;
template <bool b> constexpr int numeric_limits_impl256<b>::max_exponent10;
template <bool b> constexpr bool numeric_limits_impl256<b>::traps;
template <bool b> constexpr bool numeric_limits_impl256<b>::tinyness_before;

#endif // !defined(__cpp_inline_variables) || __cpp_inline_variables < 201606L

} // namespace detail
} // namespace decimal
} // namespace boost

namespace std {

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wmismatched-tags"
#endif

template <>
class numeric_limits<boost::decimal::detail::u256> :
    public boost::decimal::detail::numeric_limits_impl256<true> {};

#ifdef __clang__
#  pragma clang diagnostic pop
#endif

} // namespace std

#endif // BOOST_DECIMAL_DETAIL_U256_HPP
