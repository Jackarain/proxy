// Copyright 2026 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// Benchmark for the small-exponent-difference alignment kernel in add_impl.
// The companion benchmarks.cpp uses random exponents across the full range,
// which makes the dominant-operand short-circuit fire in ~94% of operations
// and leaves the alignment kernel under-exercised. This benchmark generates
// same-magnitude operands so the kernel actually has to align and add.

#define BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
#define BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

#include <boost/decimal.hpp>
#include <chrono>
#include <random>
#include <vector>
#include <type_traits>
#include <iostream>
#include <iomanip>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wold-style-cast"
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wundef"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

#include <boost/random/uniform_int_distribution.hpp>

#ifdef BOOST_DECIMAL_RUN_BENCHMARKS

using namespace boost::decimal;
using namespace std::chrono_literals;

#ifdef __clang__
#  define BOOST_DECIMAL_NO_INLINE __attribute__ ((__noinline__))
#elif defined(_MSC_VER)
#  define BOOST_DECIMAL_NO_INLINE __declspec(noinline)
#elif defined(__GNUC__)
#  define BOOST_DECIMAL_NO_INLINE __attribute__ ((__noinline__))
#endif

constexpr unsigned N = 20'000'000U;
constexpr int K = 5;

// Same-exponent workload: every operand has biased_exponent == fixed_exp.
// Exercises the same-exp pass-through inside add_impl.
template <typename T>
std::vector<T> generate_same_exp_vector(int fixed_exp, unsigned seed = 42U)
{
    std::vector<T> v(N);
    std::mt19937_64 gen(seed);
    boost::random::uniform_int_distribution<typename T::significand_type>
        sig_dis(0U, std::numeric_limits<typename T::significand_type>::max() / 1000U);
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        v[i] = T{sig_dis(gen), fixed_exp};
    }
    return v;
}

// Small-diff workload: significands random, exponents drawn from a narrow window
// so the difference stays inside max_shift but is rarely zero. Forces the
// alignment-multiply-then-add path inside add_impl.
template <typename T>
std::vector<T> generate_small_diff_vector(int base_exp, int half_window, unsigned seed = 42U)
{
    std::vector<T> v(N);
    std::mt19937_64 gen(seed);
    boost::random::uniform_int_distribution<typename T::significand_type>
        sig_dis(0U, std::numeric_limits<typename T::significand_type>::max() / 1000U);
    std::uniform_int_distribution<int> exp_dis(base_exp - half_window, base_exp + half_window);
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        v[i] = T{sig_dis(gen), exp_dis(gen)};
    }
    return v;
}

template <typename T, typename Func>
BOOST_DECIMAL_NO_INLINE void test_two_element(const std::vector<T>& data, Func op,
                                              const char* label, const char* type)
{
    const auto t1 = std::chrono::steady_clock::now();
    std::size_t s = 0;
    for (std::size_t k = 0; k < K; ++k)
    {
        for (std::size_t i = 0; i < data.size() - 1U; ++i)
        {
            s += static_cast<std::size_t>(op(data[i], data[i + 1]));
        }
    }
    const auto t2 = std::chrono::steady_clock::now();
    std::cerr << std::left << std::setw(11) << label << "<" << std::setw(13) << type
              << ">: " << std::setw(10) << (t2 - t1) / 1us << " us (s=" << s << ")\n";
}

template <typename T>
void run_same_exp(int fixed_exp, const char* type)
{
    const auto v = generate_same_exp_vector<T>(fixed_exp);
    test_two_element(v, std::plus<>(),  "AddSameExp", type);
    test_two_element(v, std::minus<>(), "SubSameExp", type);
}

template <typename T>
void run_small_diff(int base_exp, int half_window, const char* type)
{
    const auto v = generate_small_diff_vector<T>(base_exp, half_window);
    test_two_element(v, std::plus<>(),  "AddSmallDiff", type);
    test_two_element(v, std::minus<>(), "SubSmallDiff", type);
}

int main()
{
    std::cerr << "\n===== Same-exponent (alignment kernel pass-through) =====\n";
    run_same_exp<decimal32_t>(0, "decimal32_t");
    run_same_exp<decimal64_t>(0, "decimal64_t");
    run_same_exp<decimal128_t>(0, "decimal128_t");
    run_same_exp<decimal_fast32_t>(0, "dec32_fast");
    run_same_exp<decimal_fast64_t>(0, "dec64_fast");
    run_same_exp<decimal_fast128_t>(0, "dec128_fast");

    // For d64 max_shift = 21; window of 5 keeps diffs in [0, 10] so the small-diff
    // alignment kernel fires every time. For d128 max_shift = 43 in u256 mode but
    // 3 in the planned uint128 fast path; window of 2 keeps diffs in [0, 4] so we
    // exercise both paths (until Phase 1 lands, after which we'd want 0-3).
    std::cerr << "\n===== Small exp diff (alignment kernel multiply+add) =====\n";
    run_small_diff<decimal32_t>(0, 5, "decimal32_t");
    run_small_diff<decimal64_t>(0, 5, "decimal64_t");
    run_small_diff<decimal128_t>(0, 2, "decimal128_t");
    run_small_diff<decimal_fast32_t>(0, 5, "dec32_fast");
    run_small_diff<decimal_fast64_t>(0, 5, "dec64_fast");
    run_small_diff<decimal_fast128_t>(0, 2, "dec128_fast");

    std::cerr << std::endl;
    return 1;
}

#else

int main()
{
    std::cerr << "Benchmarks not run" << std::endl;
    return 1;
}

#endif
