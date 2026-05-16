/*
 *  (C) Copyright Nick Thompson 2018.
 *  (C) Copyright Matt Borland 2020 - 2024.
 *  Use, modification and distribution are subject to the
 *  Boost Software License, Version 1.0. (See accompanying file
 *  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

// Propogates up from boost.math
#define _SILENCE_CXX23_DENORM_DEPRECATION_WARNING

// Needed for operations with boost math
#define BOOST_DECIMAL_ALLOW_IMPLICIT_INTEGER_CONVERSIONS

#include <boost/decimal.hpp>

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
#  if __clang_major__ >= 20
#    pragma clang diagnostic ignored "-Wfortify-source"
#  endif
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wold-style-cast"
#  pragma GCC diagnostic ignored "-Wundef"
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wsign-conversion"
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wuseless-cast"
#endif

#include <vector>
#include <array>
#include <list>
#include <forward_list>
#include <algorithm>
#include <random>
#include <tuple>
#include <type_traits>
#include <cstdint>
#include <boost/core/lightweight_test.hpp>
#include <boost/math/statistics/univariate_statistics.hpp>
#include <boost/random.hpp>

using std::abs;
using namespace boost::decimal;

template <typename T>
std::vector<T> generate_random_vector(std::size_t size, unsigned seed)
{
    if (seed == 0)
    {
        std::random_device rd;
        seed = rd();
    }
    std::vector<T> v(size);

    std::mt19937 gen(seed);

    boost::random::normal_distribution<double> dis(0, 1);
    for(std::size_t i = 0; i < v.size(); ++i)
    {
        v[i] = T{dis(gen)};
    }
    return v;
}

template <typename RandomAccessContainer, typename Real = typename std::iterator_traits<typename RandomAccessContainer::iterator>::value_type>
Real naive_mean(RandomAccessContainer const & v)
{
    typename RandomAccessContainer::value_type sum = 0;

    for (auto & x : v)
    {
        sum += x;
    }

    return sum/v.size();
}

template <typename Real>
void test_mean()
{
    constexpr Real tol = 10*std::numeric_limits<Real>::epsilon();

    std::vector<Real> v{1,2,3,4,5};
    Real mu = boost::math::statistics::mean(v.begin(), v.end());
    BOOST_TEST(abs(mu - 3) < tol);

    // Does range call work?
    mu = boost::math::statistics::mean(v);
    BOOST_TEST(abs(mu - 3) < tol);

    // Can we successfully average only part of the vector?
    mu = boost::math::statistics::mean(v.begin(), v.begin() + 3);
    BOOST_TEST(abs(mu - 2) < tol);

    // Does it work when we const qualify?
    mu = boost::math::statistics::mean(v.cbegin(), v.cend());
    BOOST_TEST(abs(mu - 3) < tol);

    // Does it work for std::array?
    std::array<Real, 7> u{1,2,3,4,5,6,7};
    mu = boost::math::statistics::mean(u.begin(), u.end());
    BOOST_TEST(abs(mu - 4) < tol);

    // Does it work for a forward iterator?
    std::forward_list<Real> l{1,2,3,4,5,6,7};
    mu = boost::math::statistics::mean(l.begin(), l.end());
    BOOST_TEST(abs(mu - 4) < tol);

    // Stress test:
    for (std::size_t i = 1; i < 30; ++i)
    {
        v = generate_random_vector<Real>(i, 12803U);
        auto naive_ = naive_mean(v);
        auto higham_ = boost::math::statistics::mean(v);
        if (abs(higham_ - naive_) >= 100*tol*abs(naive_))
        {
            std::cout << std::hexfloat << std::setprecision(std::numeric_limits<Real>::digits10);
            std::cout << "Terms = " << v.size() << "\n";
            std::cout << "higham = " << higham_ << "\n";
            std::cout << "naive_ = " << naive_ << "\n";
        }
        BOOST_TEST(abs(higham_ - naive_) < 100*tol*abs(naive_));
    }
}

template <typename Real>
void test_variance()
{
    Real tol = std::numeric_limits<Real>::epsilon();
    std::vector<Real> v{1,1,1,1,1,1};
    Real sigma_sq = boost::math::statistics::variance(v.begin(), v.end());
    BOOST_TEST(abs(sigma_sq) < tol);

    sigma_sq = boost::math::statistics::variance(v);
    BOOST_TEST(abs(sigma_sq) < tol);

    Real s_sq = boost::math::statistics::sample_variance(v);
    BOOST_TEST(abs(s_sq) < tol);

    std::vector<Real> u{1};
    sigma_sq = boost::math::statistics::variance(u.cbegin(), u.cend());
    BOOST_TEST(abs(sigma_sq) < tol);

    std::array<Real, 8> w{0,1,0,1,0,1,0,1};
    sigma_sq = boost::math::statistics::variance(w.begin(), w.end());
    BOOST_TEST(abs(sigma_sq - Real{25, -2}) < tol);

    sigma_sq = boost::math::statistics::variance(w);
    BOOST_TEST(abs(sigma_sq - Real{25, -2}) < tol);

    std::forward_list<Real> l{0,1,0,1,0,1,0,1};
    sigma_sq = boost::math::statistics::variance(l.begin(), l.end());
    BOOST_TEST(abs(sigma_sq - Real{25, -2}) < tol);
}

template <typename Real>
void test_skewness()
{
    constexpr Real tol = 15*std::numeric_limits<Real>::epsilon();
    std::vector<Real> v{1,1,1};
    Real skew = boost::math::statistics::skewness(v);
    BOOST_TEST(abs(skew) < tol);

    // Dataset is symmetric about the mean:
    v = {1,2,3,4,5};
    skew = boost::math::statistics::skewness(v);
    BOOST_TEST(abs(skew) < tol);

    v = {0,0,0,0,5};
    // mu = 1, sigma^2 = 4, sigma = 2, skew = 3/2
    skew = boost::math::statistics::skewness(v);
    BOOST_TEST(abs(skew - Real{15, -1}) < tol);

    std::forward_list<Real> w2{0,0,0,0,5};
    skew = boost::math::statistics::skewness(w2);
    BOOST_TEST(abs(skew - Real{15, -1}) < tol);
}

template <typename Real>
void test_kurtosis()
{
    Real tol = 15*std::numeric_limits<Real>::epsilon();
    std::vector<Real> v{1,1,1};
    Real kurt = boost::math::statistics::kurtosis(v);
    BOOST_TEST(abs(kurt) < tol);

    v = {1,2,3,4,5};
    // mu =1, sigma^2 = 2, kurtosis = 17/10
    kurt = boost::math::statistics::kurtosis(v);
    BOOST_TEST(abs(kurt - Real{17, -1}) < 10*tol);

    v = {0,0,0,0,5};
    // mu = 1, sigma^2 = 4, sigma = 2, skew = 3/2, kurtosis = 13/4
    kurt = boost::math::statistics::kurtosis(v);
    BOOST_TEST(abs(kurt - Real(13)/Real(4)) < tol);

    std::array<Real, 5> v1{0,0,0,0,5};
    kurt = boost::math::statistics::kurtosis(v1);
    BOOST_TEST(abs(kurt - Real(13)/Real(4)) < tol);

    std::forward_list<Real> v2{0,0,0,0,5};
    kurt = boost::math::statistics::kurtosis(v2);
    BOOST_TEST(abs(kurt - Real(13)/Real(4)) < tol);
}

template <typename Real>
void test_first_four_moments()
{
    Real tol = 10*std::numeric_limits<Real>::epsilon();
    std::vector<Real> v{1,1,1};
    std::tuple<Real, Real, Real, Real> M = boost::math::statistics::first_four_moments(v);
    BOOST_TEST(abs(std::get<0>(M) - 1) < tol);
    BOOST_TEST(abs(std::get<1>(M)) < tol);
    BOOST_TEST(abs(std::get<2>(M)) < tol);
    BOOST_TEST(abs(std::get<3>(M)) < tol);

    v = {1, 2, 3, 4, 5};
    std::tuple<Real, Real, Real, Real> M2 = boost::math::statistics::first_four_moments(v);
    BOOST_TEST(abs(std::get<0>(M2) - 3) < tol);
    BOOST_TEST(abs(std::get<1>(M2) - 2) < tol);
    BOOST_TEST(abs(std::get<2>(M2)) < tol);
    BOOST_TEST(abs(std::get<3>(M2) - Real(34)/Real(5)) < tol);
}

template <typename Real>
void test_median()
{
    std::mt19937 g(12);
    std::vector<Real> v{1,2,3,4,5,6,7};

    Real m = boost::math::statistics::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 4);

    std::shuffle(v.begin(), v.end(), g);
    // Does range call work?
    m = boost::math::statistics::median(v);
    BOOST_TEST_EQ(m, 4);

    v = {1,2,3,3,4,5};
    m = boost::math::statistics::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 3);
    std::shuffle(v.begin(), v.end(), g);
    m = boost::math::statistics::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 3);

    v = {1};
    m = boost::math::statistics::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 1);

    v = {1,1};
    m = boost::math::statistics::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 1);

    v = {2,4};
    m = boost::math::statistics::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 3);

    v = {1,1,1};
    m = boost::math::statistics::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 1);

    v = {1,2,3};
    m = boost::math::statistics::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 2);
    std::shuffle(v.begin(), v.end(), g);
    m = boost::math::statistics::median(v.begin(), v.end());
    BOOST_TEST_EQ(m, 2);

    // Does it work with std::array?
    std::array<Real, 3> w{1,2,3};
    m = boost::math::statistics::median(w);
    BOOST_TEST_EQ(m, 2);
}

template <typename Real>
void test_median_absolute_deviation()
{
    std::vector<Real> v{-1, 2, -3, 4, -5, 6, -7};

    Real m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 4);

    std::mt19937 g(12);
    std::shuffle(v.begin(), v.end(), g);
    m = boost::math::statistics::median_absolute_deviation(v, 0);
    BOOST_TEST_EQ(m, 4);

    v = {1, -2, -3, 3, -4, -5};
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 3);
    std::shuffle(v.begin(), v.end(), g);
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 3);

    v = {-1};
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 1);

    v = {-1, 1};
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 1);
    m = boost::math::statistics::median_absolute_deviation(v, 0);
    BOOST_TEST_EQ(m, 1);


    v = {2, -4};
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 3);

    v = {1, -1, 1};
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 1);

    v = {1, 2, -3};
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 2);
    std::shuffle(v.begin(), v.end(), g);
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end(), 0);
    BOOST_TEST_EQ(m, 2);

    std::array<Real, 3> w{1, 2, -3};
    m = boost::math::statistics::median_absolute_deviation(w, 0);
    BOOST_TEST_EQ(m, 2);


    v = {-1, 2, -3, 4, -5, 6, -7};
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end());
    BOOST_TEST_EQ(m, 4);

    g = std::mt19937(12);
    std::shuffle(v.begin(), v.end(), g);
    m = boost::math::statistics::median_absolute_deviation(v);
    BOOST_TEST_EQ(m, 4);

    v = {1, -2, -3, 3, -4, -5};
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end());
    BOOST_TEST_EQ(m, 2);
    std::shuffle(v.begin(), v.end(), g);
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end());
    BOOST_TEST_EQ(m, 2);

    v = {-1};
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end());
    BOOST_TEST_EQ(m, 0);

    v = {-1, 1};
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end());
    BOOST_TEST_EQ(m, 1);

    m = boost::math::statistics::median_absolute_deviation(v);
    BOOST_TEST_EQ(m, 1);


    v = {2, -4};
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end());
    BOOST_TEST_EQ(m, 3);

    v = {1, -1, 1};
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end());
    BOOST_TEST_EQ(m, 0);

    v = {1, 2, -3};
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end());
    BOOST_TEST_EQ(m, 1);
    std::shuffle(v.begin(), v.end(), g);
    m = boost::math::statistics::median_absolute_deviation(v.begin(), v.end());
    BOOST_TEST_EQ(m, 1);

    w = {1, 2, -3};
    m = boost::math::statistics::median_absolute_deviation(w);
    BOOST_TEST_EQ(m, 1);
}

template <typename Real>
void test_sample_gini_coefficient()
{
    constexpr Real tol = 10*std::numeric_limits<Real>::epsilon();

    std::vector<Real> v{1,0,0};
    Real gini = boost::math::statistics::sample_gini_coefficient(v.begin(), v.end());
    BOOST_TEST(abs(gini - 1) < tol);

    gini = boost::math::statistics::sample_gini_coefficient(v);
    BOOST_TEST(abs(gini - 1) < tol);

    v[0] = 1;
    v[1] = 1;
    v[2] = 1;
    gini = boost::math::statistics::sample_gini_coefficient(v.begin(), v.end());
    BOOST_TEST(abs(gini) < tol);

    v[0] = 0;
    v[1] = 0;
    v[2] = 0;
    gini = boost::math::statistics::sample_gini_coefficient(v.begin(), v.end());
    BOOST_TEST(abs(gini) < tol);

    std::array<Real, 3> w{0,0,0};
    gini = boost::math::statistics::sample_gini_coefficient(w);
    BOOST_TEST(abs(gini) < tol);
}

template <typename Real>
void test_gini_coefficient()
{
    constexpr Real tol = 10*std::numeric_limits<Real>::epsilon();

    std::vector<Real> v{1,0,0};
    Real gini = boost::math::statistics::gini_coefficient(v.begin(), v.end());
    Real expected = Real(2)/Real(3);
    BOOST_TEST(abs(gini - expected) < tol);

    gini = boost::math::statistics::gini_coefficient(v);
    BOOST_TEST(abs(gini - expected) < tol);

    v[0] = 1;
    v[1] = 1;
    v[2] = 1;
    gini = boost::math::statistics::gini_coefficient(v.begin(), v.end());
    BOOST_TEST(abs(gini) < tol);

    v[0] = 0;
    v[1] = 0;
    v[2] = 0;
    gini = boost::math::statistics::gini_coefficient(v.begin(), v.end());
    BOOST_TEST(abs(gini) < tol);

    std::array<Real, 3> w{0,0,0};
    gini = boost::math::statistics::gini_coefficient(w);
    BOOST_TEST(abs(gini) < tol);
}

template <typename Real>
void test_interquartile_range()
{
    std::mt19937 gen(486);
    Real iqr;
    // Taken from Wikipedia's example:
    std::vector<Real> v{7, 7, 31, 31, 47, 75, 87, 115, 116, 119, 119, 155, 177};

    // Q1 = 31, Q3 = 119, Q3 - Q1 = 88.
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 88);

    std::shuffle(v.begin(), v.end(), gen);
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 88);

    std::shuffle(v.begin(), v.end(), gen);
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 88);

    std::fill(v.begin(), v.end(), 1);
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 0);

    v = {1,2,3};
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 2);
    std::shuffle(v.begin(), v.end(), gen);
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 2);

    v = {0, 3, 5};
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 5);
    std::shuffle(v.begin(), v.end(), gen);
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 5);

    v = {1,2,3,4};
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 2);
    std::shuffle(v.begin(), v.end(), gen);
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 2);

    v = {1,2,3,4,5};
    // Q1 = 1.5, Q3 = 4.5
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 3);
    std::shuffle(v.begin(), v.end(), gen);
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 3);

    v = {1,2,3,4,5,6};
    // Q1 = 2, Q3 = 5
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 3);
    std::shuffle(v.begin(), v.end(), gen);
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 3);

    v = {1,2,3, 4, 5,6,7};
    // Q1 = 2, Q3 = 6
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 4);
    std::shuffle(v.begin(), v.end(), gen);
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 4);

    v = {1,2,3,4,5,6,7,8};
    // Q1 = 2.5, Q3 = 6.5
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 4);
    std::shuffle(v.begin(), v.end(), gen);
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 4);

    v = {1,2,3,4,5,6,7,8,9};
    // Q1 = 2.5, Q3 = 7.5
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 5);
    std::shuffle(v.begin(), v.end(), gen);
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 5);

    v = {1,2,3,4,5,6,7,8,9,10};
    // Q1 = 3, Q3 = 8
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 5);
    std::shuffle(v.begin(), v.end(), gen);
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 5);

    v = {1,2,3,4,5,6,7,8,9,10,11};
    // Q1 = 3, Q3 = 9
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 6);
    std::shuffle(v.begin(), v.end(), gen);
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 6);

    v = {1,2,3,4,5,6,7,8,9,10,11,12};
    // Q1 = 3.5, Q3 = 9.5
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 6);
    std::shuffle(v.begin(), v.end(), gen);
    iqr = boost::math::statistics::interquartile_range(v);
    BOOST_TEST_EQ(iqr, 6);
}

template <typename Z>
void test_mode()
{
    std::vector<Z> modes;
    std::vector<Z> v {1, 2, 2, 3, 4, 5};
    const Z ref = 2;

    // Does iterator call work?
    boost::math::statistics::mode(v.begin(), v.end(), std::back_inserter(modes));
    BOOST_TEST_EQ(ref, modes[0]);

    // Does container call work?
    modes.clear();
    boost::math::statistics::mode(v, std::back_inserter(modes));
    BOOST_TEST_EQ(ref, modes[0]);

    // Does it work with part of a vector?
    modes.clear();
    boost::math::statistics::mode(v.begin(), v.begin() + 3, std::back_inserter(modes));
    BOOST_TEST_EQ(ref, modes[0]);

    // Does it work with const qualification? Only if pre-sorted
    modes.clear();
    boost::math::statistics::mode(v.cbegin(), v.cend(), std::back_inserter(modes));
    BOOST_TEST_EQ(ref, modes[0]);

    // Does it work with std::array?
    modes.clear();
    std::array<Z, 6> u {1, 2, 2, 3, 4, 5};
    boost::math::statistics::mode(u, std::back_inserter(modes));
    BOOST_TEST_EQ(ref, modes[0]);

    // Does it work with a bi-modal distribuition?
    modes.clear();
    std::vector<Z> w {1, 2, 2, 3, 3, 4, 5};
    boost::math::statistics::mode(w.begin(), w.end(), std::back_inserter(modes));
    BOOST_TEST_EQ(modes.size(), 2);

    // Does it work with an empty vector?
    modes.clear();
    std::vector<Z> x {};
    boost::math::statistics::mode(x, std::back_inserter(modes));
    BOOST_TEST_EQ(modes.size(), 0);

    // Does it work with a one item vector
    modes.clear();
    x.push_back(2);
    boost::math::statistics::mode(x, std::back_inserter(modes));
    BOOST_TEST_EQ(ref, modes[0]);

    // Does it work with a doubly linked list
    modes.clear();
    std::list<Z> dl {1, 2, 2, 3, 4, 5};
    boost::math::statistics::mode(dl, std::back_inserter(modes));
    BOOST_TEST_EQ(ref, modes[0]);

    // Does it work with a singly linked list
    modes.clear();
    std::forward_list<Z> fl {1, 2, 2, 3, 4, 5};
    boost::math::statistics::mode(fl, std::back_inserter(modes));
    BOOST_TEST_EQ(ref, modes[0]);

    // Does the returning a list work?
    auto return_modes = boost::math::statistics::mode(v);
    BOOST_TEST_EQ(ref, return_modes.front());

    auto return_modes_2 = boost::math::statistics::mode(v.begin(), v.end());
    BOOST_TEST_EQ(ref, return_modes_2.front());
}

int main()
{
    test_mean<decimal32_t>();
    test_mean<decimal64_t>();

    test_variance<decimal32_t>();
    test_variance<decimal64_t>();

    test_skewness<decimal32_t>();
    test_skewness<decimal64_t>();

    test_kurtosis<decimal32_t>();
    test_kurtosis<decimal64_t>();

    test_first_four_moments<decimal32_t>();
    test_first_four_moments<decimal64_t>();

    test_median<decimal32_t>();
    test_median<decimal64_t>();

    test_median_absolute_deviation<decimal32_t>();
    test_median_absolute_deviation<decimal64_t>();

    test_sample_gini_coefficient<decimal32_t>();
    test_sample_gini_coefficient<decimal64_t>();

    test_gini_coefficient<decimal32_t>();
    test_gini_coefficient<decimal64_t>();

    test_interquartile_range<decimal32_t>();
    test_interquartile_range<decimal64_t>();

    test_mode<decimal32_t>();
    test_mode<decimal64_t>();

    return boost::report_errors();
}
