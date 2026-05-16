// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//
// See: https://github.com/boostorg/decimal/issues/1299

#include <boost/decimal.hpp>
#include <boost/core/lightweight_test.hpp>
#include <limits>

using namespace boost::decimal;

enum class result_t
{
    lhs,
    rhs,
    qnan
};

enum class result_sign_t
{
    positive,
    negative
};

template <typename T>
void test_fmax_value(const T lhs, const T rhs, const result_t result, const result_sign_t sign = result_sign_t::positive, const int payload_value = 0)
{
    const auto res {fmax(lhs, rhs)};

    switch (result)
    {
        case result_t::lhs:
            BOOST_TEST_EQ(lhs, res);
            break;

        case result_t::rhs:
            BOOST_TEST_EQ(rhs, res);
            break;

        case result_t::qnan:
            BOOST_TEST(isnan(res));
            BOOST_TEST(!issignaling(res));

            if (sign == result_sign_t::negative)
            {
                BOOST_TEST(signbit(res));
            }

            if (payload_value)
            {
                const auto payload {read_payload(res)};
                BOOST_TEST_EQ(static_cast<decltype(payload)>(payload_value), payload);
            }

            break;
    }
}

template <typename T>
void test_fmin_value(const T lhs, const T rhs, const result_t result, const result_sign_t sign = result_sign_t::positive, const int payload_value = 0)
{
    const auto res {fmin(lhs, rhs)};

    switch (result)
    {
        case result_t::lhs:
            BOOST_TEST_EQ(lhs, res);
            break;

        case result_t::rhs:
            BOOST_TEST_EQ(rhs, res);
            break;

        case result_t::qnan:
            BOOST_TEST(isnan(res));
            BOOST_TEST(!issignaling(res));

            if (sign == result_sign_t::negative)
            {
                BOOST_TEST(signbit(res));
            }

            if (payload_value)
            {
                const auto payload {read_payload(res)};
                BOOST_TEST_EQ(static_cast<decltype(payload)>(payload_value), payload);
            }

            break;
    }
}

template <typename T>
void test_driver()
{
    test_fmax_value(T{5}, T{42}, result_t::rhs);
    test_fmax_value(std::numeric_limits<T>::infinity(), std::numeric_limits<T>::quiet_NaN(), result_t::lhs);
    test_fmax_value(-std::numeric_limits<T>::infinity(), std::numeric_limits<T>::quiet_NaN(), result_t::lhs);
    test_fmax_value(T{5}, std::numeric_limits<T>::quiet_NaN(), result_t::lhs);

    // Any operation on SNAN is invalid and returns QNAN with the same payload (as applicable)
    test_fmax_value(std::numeric_limits<T>::signaling_NaN(), T{5}, result_t::qnan);
    test_fmax_value(std::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::signaling_NaN(), result_t::qnan);
    test_fmax_value(std::numeric_limits<T>::signaling_NaN(), std::numeric_limits<T>::quiet_NaN(), result_t::qnan);

    T snan_payload {"-sNaN97"};
    test_fmax_value(snan_payload, std::numeric_limits<T>::quiet_NaN(), result_t::qnan, result_sign_t::negative, 97);
    test_fmax_value(std::numeric_limits<T>::quiet_NaN(), snan_payload, result_t::qnan, result_sign_t::negative, 97);

    snan_payload = -snan_payload;
    T qnan_payload {"NaN100"};
    test_fmax_value(snan_payload, qnan_payload, result_t::qnan, result_sign_t::positive, 97);

    test_fmin_value(T{5}, T{42}, result_t::lhs);
    test_fmin_value(std::numeric_limits<T>::infinity(), std::numeric_limits<T>::quiet_NaN(), result_t::lhs);
    test_fmin_value(-std::numeric_limits<T>::infinity(), std::numeric_limits<T>::quiet_NaN(), result_t::lhs);
    test_fmin_value(T{5}, std::numeric_limits<T>::quiet_NaN(), result_t::lhs);

    // Any operation on SNAN is invalid and returns QNAN with the same payload (as applicable)
    test_fmin_value(std::numeric_limits<T>::signaling_NaN(), T{5}, result_t::qnan);
    test_fmin_value(std::numeric_limits<T>::quiet_NaN(), std::numeric_limits<T>::signaling_NaN(), result_t::qnan);
    test_fmin_value(std::numeric_limits<T>::signaling_NaN(), std::numeric_limits<T>::quiet_NaN(), result_t::qnan);

    snan_payload = T{"-sNaN97"};
    test_fmin_value(snan_payload, std::numeric_limits<T>::quiet_NaN(), result_t::qnan, result_sign_t::negative, 97);
    test_fmin_value(std::numeric_limits<T>::quiet_NaN(), snan_payload, result_t::qnan, result_sign_t::negative, 97);

    snan_payload = -snan_payload;
    qnan_payload = T{"NaN100"};
    test_fmin_value(snan_payload, qnan_payload, result_t::qnan, result_sign_t::positive, 97);
}

int main()
{
    test_driver<decimal32_t>();
    test_driver<decimal64_t>();
    test_driver<decimal128_t>();

    test_driver<decimal_fast32_t>();
    test_driver<decimal_fast64_t>();
    test_driver<decimal_fast128_t>();

    return boost::report_errors();
}
