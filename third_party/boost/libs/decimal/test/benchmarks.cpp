// Copyright 2023 Peter Dimov
// Copyright 2024 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_CONVERSION
#define BOOST_DECIMAL_DETAIL_INT128_ALLOW_SIGN_COMPARE

#include <boost/decimal.hpp>
#include <chrono>
#include <random>
#include <vector>
#include <type_traits>
#include <iostream>
#include <iomanip>
#include <string>
#include <cmath>
#include <cstring>

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wold-style-cast"
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wsign-conversion"
#  pragma clang diagnostic ignored "-Wfloat-equal"

#  if (__clang_major__ >= 10 && !defined(__APPLE__)) || __clang_major__ >= 13
#    pragma clang diagnostic ignored "-Wdeprecated-copy"
#  endif

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

#if __cplusplus >= 201703L || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#if __has_include(<charconv>)
#  include <charconv>
#    if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
#      define BOOST_DECIMAL_ALLOW_BENCHMARK_CHARCONV
#    endif
#endif
#endif

#ifdef BOOST_DECIMAL_BENCHMARK_CHARCONV
#  ifndef BOOST_DECIMAL_ALLOW_BENCHMARK_CHARCONV
#    undef BOOST_DECIMAL_BENCHMARK_CHARCONV
#  endif
#endif

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
#  pragma GCC diagnostic ignored "-Wstringop-overread"
#  define BOOST_DECIMAL_NO_INLINE __attribute__ ((__noinline__))
#endif

constexpr unsigned N = 20'000'000U;
constexpr int K = 5;

template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
std::vector<T> generate_random_vector(std::size_t size = N, unsigned seed = 42U)
{
    if (seed == 0)
    {
        std::random_device rd;
        seed = rd();
    }
    std::vector<T> v(size);

    std::mt19937_64 gen(seed);

    std::uniform_real_distribution<T> dis(0, std::numeric_limits<T>::max());
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        v[i] = dis(gen);
    }
    return v;
}

template <typename T, std::enable_if_t<!std::is_floating_point<T>::value, bool> = true>
std::vector<T> generate_random_vector(std::size_t size = N, unsigned seed = 42U)
{
    if (seed == 0)
    {
        std::random_device rd;
        seed = rd();
    }
    std::vector<T> v(size);

    std::mt19937_64 gen(seed);

    boost::random::uniform_int_distribution<typename T::significand_type> sig_dis(0U, std::numeric_limits<typename T::significand_type>::max() / 1000U);
    std::uniform_int_distribution<int> exp_dis(std::numeric_limits<T>::min_exponent, std::numeric_limits<T>::max_exponent);

    for (std::size_t i = 0; i < v.size(); ++i)
    {
        v[i] = T{sig_dis(gen), exp_dis(gen)};
    }
    return v;
}

template <typename ResultType, typename T>
std::vector<ResultType> convert_copy_vector(const std::vector<T>& v)
{
    std::vector<ResultType> r(v.size());
    for (std::size_t i {}; i < v.size(); ++i)
    {
        r[i] = static_cast<ResultType>(v[i]);
    }

    return r;
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

    std::cerr << "comparisons<" << std::left << std::setw(13) << label << ">: " << std::setw( 10 ) << ( t2 - t1 ) / 1us << " us (s=" << s << ")\n";
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

    std::cerr << operation << "<" << std::left << std::setw(13) << type << ">: " << std::setw( 10 ) << ( t2 - t1 ) / 1us << " us (s=" << s << ")\n";
}

template <typename T, typename Func>
BOOST_DECIMAL_NO_INLINE void test_one_element_operation(const std::vector<T>& data_vec, Func op, const char* operation, const char* type, std::size_t max_element = N)
{
    const auto t1 = std::chrono::steady_clock::now();
    std::size_t s = 0; // discard variable

    for (std::size_t k {}; k < K; ++k)
    {
        for (std::size_t i {}; i < max_element; ++i)
        {
            s += static_cast<std::size_t>(op(data_vec[i]));
        }
    }

    const auto t2 = std::chrono::steady_clock::now();

    std::cerr << operation << "<" << std::left << std::setw(13) << type << ">: " << std::setw( 10 ) << ( t2 - t1 ) / 1us << " us (s=" << s << ")\n";
}

template <typename T>
static BOOST_DECIMAL_NO_INLINE void init_input_data( std::vector<T>& data )
{
    using std::isfinite;
    data.reserve( N );

    std::mt19937_64 rng(42);

    for( unsigned i = 0; i < N; ++i )
    {
        std::uint64_t tmp = rng();

        T x;
        std::memcpy( &x, &tmp, sizeof(x) );

        if( !isfinite(x) ) continue;

        data.push_back( x );
    }
}

#ifdef BOOST_DECIMAL_BENCHMARK_CHARCONV

template <typename T, std::enable_if_t<!std::is_floating_point<T>::value, bool> = true>
static BOOST_DECIMAL_NO_INLINE void test_boost_to_chars( std::vector<T> const& data, bool general, char const* label, int precision, const char* type )
{
    auto t1 = std::chrono::steady_clock::now();

    std::size_t s = 0;

    for( int i = 0; i < K; ++i )
    {
        char buffer[ 256 ];

        for( auto x: data )
        {
            boost::decimal::chars_format fmt = general? boost::decimal::chars_format::general: boost::decimal::chars_format::scientific;

            auto r = precision == 0?
                     boost::decimal::to_chars( buffer, buffer + sizeof( buffer ), x, fmt ):
                     boost::decimal::to_chars( buffer, buffer + sizeof( buffer ), x, fmt, precision );

            s += static_cast<std::size_t>( r.ptr - buffer );
            s += static_cast<unsigned char>( buffer[0] );
        }
    }

    auto t2 = std::chrono::steady_clock::now();

    std::cerr << "boost::decimal::to_chars<" << std::left << std::setw(13) << type << ">, " << label << ", " << precision << ": " << std::setw( 10 ) << ( t2 - t1 ) / 1us << " us (s=" << s << ")\n";
}

template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
static BOOST_DECIMAL_NO_INLINE void test_boost_to_chars( std::vector<T> const& data, bool general, char const* label, int precision, const char* type )
{
    auto t1 = std::chrono::steady_clock::now();

    std::size_t s = 0;

    for( int i = 0; i < K; ++i )
    {
        char buffer[ 256 ];

        for( auto x: data )
        {
            std::chars_format fmt = general ? std::chars_format::general : std::chars_format::scientific;

            auto r = precision == 0?
                     std::to_chars( buffer, buffer + sizeof( buffer ), x, fmt ):
                     std::to_chars( buffer, buffer + sizeof( buffer ), x, fmt, precision );

            s += static_cast<std::size_t>( r.ptr - buffer );
            s += static_cast<unsigned char>( buffer[0] );
        }
    }

    auto t2 = std::chrono::steady_clock::now();

    std::cerr << "           std::to_chars<" << std::left << std::setw(10) << type << "   >, " << label << ", " << precision << ": " << std::setw( 10 ) << ( t2 - t1 ) / 1us << " us (s=" << s << ")\n";
}


template <typename T>
void test_to_chars(const char* type)
{
    std::vector<T> data;
    init_input_data(data);
    test_boost_to_chars(data, false, "scientific", 0, type);
    test_boost_to_chars(data, false, "scientific", 6, type);
    test_boost_to_chars(data, true, "general   ", 0, type);
    test_boost_to_chars(data, true, "general   ", 6, type);
}

template<class T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
BOOST_DECIMAL_NO_INLINE void init_from_chars_input_data( std::vector<std::string>& data, bool general )
{
    using std::isfinite;

    data.reserve( N );

    std::mt19937_64 rng;

    for( unsigned i = 0; i < N; ++i )
    {
        std::uint64_t tmp = rng();

        T x;
        std::memcpy( &x, &tmp, sizeof(x) );

        if( !isfinite(x) ) continue;

        char buffer[ 64 ];
        auto r = std::to_chars( buffer, buffer + sizeof( buffer ), x, general? std::chars_format::general: std::chars_format::scientific );

        std::string y( buffer, r.ptr );
        data.push_back( y );
    }
}

template<class T, std::enable_if_t<!std::is_floating_point<T>::value, bool> = true>
BOOST_DECIMAL_NO_INLINE void init_from_chars_input_data( std::vector<std::string>& data, bool general )
{
    using std::isfinite;

    data.reserve( N );

    std::mt19937_64 rng;

    for( unsigned i = 0; i < N; ++i )
    {
        std::uint64_t tmp = rng();

        T x;
        std::memcpy( &x, &tmp, sizeof(x) );

        if( !isfinite(x) ) continue;

        char buffer[ 64 ];
        auto r = boost::decimal::to_chars( buffer, buffer + sizeof( buffer ), x, general? boost::decimal::chars_format::general: boost::decimal::chars_format::scientific );

        std::string y( buffer, r.ptr );
        data.push_back( y );
    }
}

template <typename T, std::enable_if_t<std::is_floating_point<T>::value, bool> = true>
BOOST_DECIMAL_NO_INLINE void test_boost_from_chars( std::vector<std::string> const& data, bool general, char const* label, char const* type )
{
    auto t1 = std::chrono::steady_clock::now();

    std::size_t s = 0;

    for( int i = 0; i < K; ++i )
    {
        for( auto const& x: data )
        {
            T y;
            auto r = std::from_chars( x.data(), x.data() + x.size(), y, general ? std::chars_format::general : std::chars_format::scientific );

            s = static_cast<std::size_t>(r.ec);
        }
    }

    auto t2 = std::chrono::steady_clock::now();

    std::cerr << "           std::from_chars<" << std::left << std::setw(13) << type << ">, " << label << ": " << std::setw( 10 ) << ( t2 - t1 ) / 1us << " us (s=" << s << ")\n";
}

template <typename T, std::enable_if_t<!std::is_floating_point<T>::value, bool> = true>
BOOST_DECIMAL_NO_INLINE void test_boost_from_chars( std::vector<std::string> const& data, bool general, char const* label, char const* type )
{
    auto t1 = std::chrono::steady_clock::now();
    std::size_t s = 0;

    for( int i = 0; i < K; ++i )
    {
        for( auto const& x: data )
        {
            T y;
            auto r = boost::decimal::from_chars( x.data(), x.data() + x.size(), y, general? boost::decimal::chars_format::general: boost::decimal::chars_format::scientific );

            s = static_cast<std::size_t>(r.ec);
        }
    }

    auto t2 = std::chrono::steady_clock::now();

    std::cerr << "boost::decimal::from_chars<" << std::left << std::setw(13) << type << ">, " << label << ": " << std::setw( 10 ) << ( t2 - t1 ) / 1us << " us (s=" << s << ")\n";
}

template <typename T>
void test_from_chars(bool general, const char* type)
{
    std::vector<std::string> data;
    init_from_chars_input_data<T>(data, general);

    char const* label = general? "general   ": "scientific";
    test_boost_from_chars<T>( data, general, label, type );
}

#endif

int main()
{
    const auto float_vector = generate_random_vector<float>();
    const auto double_vector = generate_random_vector<double>();
    const auto dec32_vector = generate_random_vector<decimal32_t>();
    const auto dec64_vector = generate_random_vector<decimal64_t>();
    const auto dec128_vector = generate_random_vector<decimal128_t>();

    // We use identical values to ensure fair comparison of IEEE vs fast types
    const auto dec32_fast_vector = convert_copy_vector<decimal_fast32_t>(dec32_vector);
    const auto dec64_fast_vector = convert_copy_vector<decimal_fast64_t>(dec64_vector);
    const auto dec128_fast_vector = convert_copy_vector<decimal_fast128_t>(dec128_vector);

    std::cerr << "===== Comparisons =====\n";

    test_comparisons(float_vector, "float");
    test_comparisons(double_vector, "double");
    test_comparisons(dec32_vector, "decimal32_t");
    test_comparisons(dec64_vector, "decimal64_t");
    test_comparisons(dec128_vector, "decimal128_t");
    test_comparisons(dec32_fast_vector, "dec32_fast");
    test_comparisons(dec64_fast_vector, "dec64_fast");
    test_comparisons(dec128_fast_vector, "dec128_fast");

    std::cerr << "\n===== Addition =====\n";

    test_two_element_operation(float_vector, std::plus<>(), "Addition", "float");
    test_two_element_operation(double_vector, std::plus<>(), "Addition", "double");
    test_two_element_operation(dec32_vector, std::plus<>(), "Addition", "decimal32_t");
    test_two_element_operation(dec64_vector, std::plus<>(), "Addition", "decimal64_t");
    test_two_element_operation(dec128_vector, std::plus<>(), "Addition", "decimal128_t");
    test_two_element_operation(dec32_fast_vector, std::plus<>(), "Addition", "dec32_fast");
    test_two_element_operation(dec64_fast_vector, std::plus<>(), "Addition", "dec64_fast");
    test_two_element_operation(dec128_fast_vector, std::plus<>(), "Addition", "dec128_fast");

    std::cerr << "\n===== Subtraction =====\n";

    test_two_element_operation(float_vector, std::minus<>(), "Subtraction", "float");
    test_two_element_operation(double_vector, std::minus<>(), "Subtraction", "double");
    test_two_element_operation(dec32_vector, std::minus<>(), "Subtraction", "decimal32_t");
    test_two_element_operation(dec64_vector, std::minus<>(), "Subtraction", "decimal64_t");
    test_two_element_operation(dec128_vector, std::minus<>(), "Subtraction", "decimal128_t");
    test_two_element_operation(dec32_fast_vector, std::minus<>(), "Subtraction", "dec32_fast");
    test_two_element_operation(dec64_fast_vector, std::minus<>(), "Subtraction", "dec64_fast");
    test_two_element_operation(dec128_fast_vector, std::minus<>(), "Subtraction", "dec128_fast");

    std::cerr << "\n===== Multiplication =====\n";

    test_two_element_operation(float_vector, std::multiplies<>(), "Multiplication", "float");
    test_two_element_operation(double_vector, std::multiplies<>(), "Multiplication", "double");
    test_two_element_operation(dec32_vector, std::multiplies<>(), "Multiplication", "decimal32_t");
    test_two_element_operation(dec64_vector, std::multiplies<>(), "Multiplication", "decimal64_t");
    test_two_element_operation(dec128_vector, std::multiplies<>(), "Multiplication", "decimal128_t");
    test_two_element_operation(dec32_fast_vector, std::multiplies<>(), "Multiplication", "dec32_fast");
    test_two_element_operation(dec64_fast_vector, std::multiplies<>(), "Multiplication", "dec64_fast");
    test_two_element_operation(dec128_fast_vector, std::multiplies<>(), "Multiplication", "dec128_fast");

    std::cerr << "\n===== Division =====\n";

    test_two_element_operation(float_vector, std::divides<>(), "Division", "float");
    test_two_element_operation(double_vector, std::divides<>(), "Division", "double");
    test_two_element_operation(dec32_vector, std::divides<>(), "Division", "decimal32_t");
    test_two_element_operation(dec64_vector, std::divides<>(), "Division", "decimal64_t");
    test_two_element_operation(dec128_vector, std::divides<>(), "Division", "decimal128_t");
    test_two_element_operation(dec32_fast_vector, std::divides<>(), "Division", "dec32_fast");
    test_two_element_operation(dec64_fast_vector, std::divides<>(), "Division", "dec64_fast");
    test_two_element_operation(dec128_fast_vector, std::divides<>(), "Division", "dec128_fast");

#if 0
    std::cerr << "\n===== sqrt =====\n";

    test_one_element_operation(float_vector, (float(*)(float))std::sqrt, "sqrt", "float");
    test_one_element_operation(double_vector, (double(*)(double))std::sqrt, "sqrt", "double");
    test_one_element_operation(dec32_vector, (decimal32_t(*)(decimal32_t))sqrt, "sqrt", "decimal32_t");
    test_one_element_operation(dec64_vector, (decimal64_t(*)(decimal64_t))sqrt, "sqrt", "decimal64_t");
    test_one_element_operation(dec128_vector, (decimal128_t(*)(decimal128_t))sqrt, "sqrt", "decimal128_t");
#endif
#ifdef BOOST_DECIMAL_BENCHMARK_CHARCONV
    std::cerr << "\n===== <charconv> to_chars =====\n";
    test_to_chars<float>("float");
    test_to_chars<double>("double");
    test_to_chars<decimal32_t>("decimal32_t");
    test_to_chars<decimal64_t>("decimal64_t");
    test_to_chars<decimal128_t>("decimal128_t");
    test_to_chars<decimal_fast32_t>("dec32_fast");
    test_to_chars<decimal_fast64_t>("dec64_fast");
    test_to_chars<decimal_fast128_t>("dec128_fast");

    std::cerr << "\n===== <charconv> from_chars =====\n";
    test_from_chars<float>(false, "float");
    test_from_chars<float>(true, "float");
    test_from_chars<double>(false, "double");
    test_from_chars<double>(true, "double");
    test_from_chars<decimal32_t>(false, "decimal32_t");
    test_from_chars<decimal32_t>(true, "decimal32_t");
    test_from_chars<decimal64_t>(false, "decimal64_t");
    test_from_chars<decimal64_t>(true, "decimal64_t");
    test_from_chars<decimal128_t>(false, "decimal128_t");
    test_from_chars<decimal128_t>(true, "decimal128_t");
    test_from_chars<decimal_fast32_t>(false, "dec32_fast");
    test_from_chars<decimal_fast32_t>(true, "dec32_fast");
    test_from_chars<decimal_fast64_t>(false, "dec64_fast");
    test_from_chars<decimal_fast64_t>(true, "dec64_fast");
    test_from_chars<decimal_fast128_t>(false, "dec128_fast");
    test_from_chars<decimal_fast128_t>(true, "dec128_fast");
#endif
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
