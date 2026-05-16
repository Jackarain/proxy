// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <iostream>

#ifdef BOOST_DECIMAL_BENCHMARK_U256

#include <boost/decimal/detail/u256.hpp>
#include <boost/decimal/detail/integer_search_trees.hpp>
#include <chrono>
#include <random>
#include <vector>
#include <type_traits>
#include <iomanip>
#include <string>
#include <cmath>
#include <cstring>
#include <functional>

constexpr unsigned N = 20'000'000;
constexpr unsigned K = 5;

using namespace boost::decimal;
using namespace std::chrono_literals;

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wold-style-cast"
#  define BOOST_DECIMAL_NO_INLINE __attribute__ ((__noinline__))
#elif defined(_MSC_VER)
#  define BOOST_DECIMAL_NO_INLINE __declspec(noinline)
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  define BOOST_DECIMAL_NO_INLINE __attribute__ ((__noinline__))
#  if __GNUC__ >= 11
#    pragma GCC diagnostic ignored "-Wstringop-overread"
#  endif
#endif

// 4 = 4 words
// 5 = 4 / 2

template <int words>
std::vector<detail::u256> generate_random_vector_new(std::size_t size = N, unsigned seed = 42U)
{
    if (seed == 0)
    {
        std::random_device rd;
        seed = rd();
    }

    std::mt19937_64 gen(seed);
    std::uniform_int_distribution<std::uint64_t> dist(UINT64_C(0), UINT64_MAX);
    std::uniform_int_distribution<int> size_dist(0, 1);

    std::vector<detail::u256> result(size);
    for (std::size_t i = 0; i < size; ++i)
    {
        auto current_words = words;
        BOOST_INT128_IF_CONSTEXPR (words == 5)
        {
            // Alternating 4/2
            current_words = i % 2 == 0 ? 4 : 2;
        }

        switch (current_words)
        {
            case 4:
                result[i].bytes[0] = dist(gen);
                result[i].bytes[1] = dist(gen);
                result[i].bytes[2] = dist(gen);
                result[i].bytes[3] = dist(gen);
                break;
            case 3:
                result[i].bytes[0] = dist(gen);
                result[i].bytes[1] = dist(gen);
                result[i].bytes[2] = dist(gen);
                break;
            case 2:
                result[i].bytes[0] = dist(gen);
                result[i].bytes[1] = dist(gen);
                break;
            case 1:
                result[i].bytes[0] = dist(gen);
                break;
            default:
                BOOST_DECIMAL_UNREACHABLE;
        }
    }

    return result;
}

template <typename T>
BOOST_DECIMAL_NO_INLINE void test_comparisons(const std::vector<T>& data_vec, const char* label)
{
    const auto t1 = std::chrono::steady_clock::now();
    std::size_t s = 0; // discard variable

    for (std::size_t k {}; k < K; ++k)
    {
        for (std::size_t i {}; i < data_vec.size() - 1U; ++i)
        {
            const auto val1 = data_vec[i];
            const auto val2 = data_vec[i + 1];
            s += static_cast<std::size_t>(val1 > val2);
            s += static_cast<std::size_t>(val1 >= val2);
            s += static_cast<std::size_t>(val1 < val2);
            s += static_cast<std::size_t>(val1 <= val2);
            s += static_cast<std::size_t>(val1 == val2);
            s += static_cast<std::size_t>(val1 != val2);
        }
    }

    const auto t2 = std::chrono::steady_clock::now();

    std::cout << "comp<" << std::left << std::setw(11) << label << ">: " << std::setw( 10 ) << ( t2 - t1 ) / 1us << " us (s=" << s << ")\n";
}

template <typename T>
BOOST_DECIMAL_NO_INLINE void test_bitwise_ops(const std::vector<T>& data_vec, const char* label)
{
    const auto t1 = std::chrono::steady_clock::now();
    std::size_t s = 0; // discard variable

    for (std::size_t k {}; k < K; ++k)
    {
        for (std::size_t i {}; i < data_vec.size() - 1U; ++i)
        {
            const auto val1 = data_vec[i];
            const auto val2 = data_vec[i + 1];
            s += static_cast<std::size_t>(val1 | val2);
            s += static_cast<std::size_t>(val1 & val2);
        }
    }

    const auto t2 = std::chrono::steady_clock::now();

    std::cout << "bits<" << std::left << std::setw(11) << label << ">: " << std::setw( 10 ) << ( t2 - t1 ) / 1us << " us (s=" << s << ")\n";
}

template <typename T, typename Func>
BOOST_DECIMAL_NO_INLINE void test_two_element_operation(const std::vector<T>& data_vec, Func op, const char* operation, const char* type)
{
    const auto t1 = std::chrono::steady_clock::now();
    std::size_t s = 0; // discard variable

    for (std::size_t k {}; k < K; ++k)
    {
        for (std::size_t i {}; i < data_vec.size() - 1U; ++i)
        {
            const auto val1 = data_vec[i];
            const auto val2 = data_vec[i + 1];
            s += static_cast<std::size_t>(op(val1, val2));
        }
    }

    const auto t2 = std::chrono::steady_clock::now();

    std::cout << operation << " <" << std::left << std::setw(11) << type << ">: " << std::setw( 10 ) << ( t2 - t1 ) / 1us << " us (s=" << s << ")\n";
}

template <typename T>
BOOST_DECIMAL_NO_INLINE void test_digit_counting(const std::vector<T>& data_vec, const char* label)
{
    const auto t1 = std::chrono::steady_clock::now();
    std::size_t s = 0; // discard variable

    for (std::size_t k {}; k < K; ++k)
    {
        for (std::size_t i {}; i < data_vec.size(); ++i)
        {
            const auto val1 = data_vec[i];
            s += static_cast<std::size_t>(boost::decimal::detail::num_digits(val1));
        }
    }

    const auto t2 = std::chrono::steady_clock::now();

    std::cout << "digits<" << std::left << std::setw(11) << label << ">: " << std::setw( 10 ) << ( t2 - t1 ) / 1us << " us (s=" << s << ")\n";
}

template <typename T>
BOOST_DECIMAL_NO_INLINE void test_umul256(const std::vector<T>& data_vec, const char* label)
{
    const auto t1 = std::chrono::steady_clock::now();
    std::size_t s = 0; // discard variable

    for (std::size_t k {}; k < K; ++k)
    {
        for (std::size_t i {}; i < data_vec.size() - 1U; ++i)
        {
            const auto val1 = static_cast<boost::int128::uint128_t>(data_vec[i]);
            const auto val2 = static_cast<boost::int128::uint128_t>(data_vec[i + 1]);
            s += static_cast<std::size_t>(boost::decimal::detail::umul256(val1, val2));
        }
    }

    const auto t2 = std::chrono::steady_clock::now();

    std::cout << "umul<" << std::left << std::setw(11) << label << ">: " << std::setw( 10 ) << ( t2 - t1 ) / 1us << " us (s=" << s << ")\n";
}

template <typename T>
BOOST_DECIMAL_NO_INLINE void test_umul256_new(const std::vector<T>& data_vec, const char* label)
{
    const auto t1 = std::chrono::steady_clock::now();
    std::size_t s = 0; // discard variable

    for (std::size_t k {}; k < K; ++k)
    {
        for (std::size_t i {}; i < data_vec.size() - 1U; ++i)
        {
            const auto val1 = static_cast<boost::int128::uint128_t>(data_vec[i]);
            const auto val2 = static_cast<boost::int128::uint128_t>(data_vec[i + 1]);
            s += static_cast<std::size_t>(boost::decimal::detail::umul256_new(val1, val2));
        }
    }

    const auto t2 = std::chrono::steady_clock::now();

    std::cout << "umul<" << std::left << std::setw(11) << label << ">: " << std::setw( 10 ) << ( t2 - t1 ) / 1us << " us (s=" << s << ")\n";
}

int main()
{
    using namespace boost::decimal::detail;

    // Four word operations
    {
        std::cout << "\n---------------------------\n";
        std::cout << "Four Word Operations\n";
        std::cout << "---------------------------\n\n";

        const auto new_vector = generate_random_vector_new<4>();

        test_comparisons(new_vector, "new");

        std::cout << std::endl;

        test_two_element_operation(new_vector, std::plus<>(), "add", "New");

        std::cout << std::endl;

        test_two_element_operation(new_vector, std::divides<>(), "div", "New");

        std::cout << std::endl;

        test_digit_counting(new_vector, "new");
    }
    // Two word operations
    {
        std::cout << "\n---------------------------\n";
        std::cout << "Two Word Operations\n";
        std::cout << "---------------------------\n\n";

        const auto new_vector = generate_random_vector_new<2>();

        test_comparisons(new_vector, "new");

        std::cout << std::endl;

        test_two_element_operation(new_vector, std::plus<>(), "add", "New");

        std::cout << std::endl;

        test_two_element_operation(new_vector, std::divides<>(), "div", "New");

        std::cout << std::endl;

        test_digit_counting(new_vector, "new");

        std::cout << std::endl;

        test_umul256_new(new_vector, "new");
    }

    // 4 x 2 word operations
    {
        std::cout << "\n---------------------------\n";
        std::cout << "Four Word x Two Word Operations\n";
        std::cout << "---------------------------\n\n";

        const auto new_vector = generate_random_vector_new<5>();

        test_comparisons(new_vector, "new");

        std::cout << std::endl;

        test_two_element_operation(new_vector, std::plus<>(), "add", "New");

        std::cout << std::endl;

        test_two_element_operation(new_vector, std::divides<>(), "div", "New");

        std::cout << std::endl;
    }

    return 1;
}

#else

int main()
{
    std::cerr << "Benchmarks not run" << std::endl;
    return 1;
}

#endif // BOOST_DECIMAL_BENCHMARK_U128
