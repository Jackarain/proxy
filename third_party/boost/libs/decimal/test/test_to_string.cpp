// Copyright 2023 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/decimal/decimal32_t.hpp>
#include <boost/decimal/decimal64_t.hpp>
#include <boost/decimal/decimal128_t.hpp>
#include <boost/decimal/string.hpp>
#include <boost/decimal/iostream.hpp>
#include <boost/core/lightweight_test.hpp>

using namespace boost::decimal;

#if !defined(BOOST_DECIMAL_DISABLE_CLIB)

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4127)
#endif

template <BOOST_DECIMAL_DECIMAL_FLOATING_TYPE T>
void test()
{
    BOOST_TEST_EQ(to_string(T{1}), "1");
    BOOST_TEST_EQ(to_string(T{10}), "10");
    BOOST_TEST_EQ(to_string(T{100}), "100");
    BOOST_TEST_EQ(to_string(T{1000}), "1000");
    BOOST_TEST_EQ(to_string(T{10000}), "10000");
    BOOST_TEST_EQ(to_string(T{210000}), "210000");
    BOOST_TEST_EQ(to_string(T{2100000}), "2100000");

    BOOST_DECIMAL_IF_CONSTEXPR (detail::decimal_val_v<T> > 32)
    {
        BOOST_TEST_EQ(to_string(T{21U, 6, true}), "-21000000");
        BOOST_TEST_EQ(to_string(T{211U, 6, true}), "-211000000");
        BOOST_TEST_EQ(to_string(T{2111U, 6, true}), "-2111000000");
    }
    else
    {
        BOOST_TEST_EQ(to_string(T{21U, 6, true}), "-2.1e+07");
        BOOST_TEST_EQ(to_string(T{211U, 6, true}), "-2.11e+08");
        BOOST_TEST_EQ(to_string(T{2111U, 6, true}), "-2.111e+09");
    }

    BOOST_TEST_EQ(to_string(std::numeric_limits<T>::infinity()), "inf");
    BOOST_TEST_EQ(to_string(-std::numeric_limits<T>::infinity()), "-inf");
    BOOST_TEST_EQ(to_string(std::numeric_limits<T>::quiet_NaN()), "nan");
    BOOST_TEST_EQ(to_string(-std::numeric_limits<T>::quiet_NaN()), "-nan(ind)");
    BOOST_TEST_EQ(to_string(std::numeric_limits<T>::signaling_NaN()), "nan(snan)");
    BOOST_TEST_EQ(to_string(-std::numeric_limits<T>::signaling_NaN()), "-nan(snan)");
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

int main()
{
    test<decimal32_t>();
    test<decimal64_t>();
    test<decimal128_t>();

    return boost::report_errors();
}

#else

int main()
{
    return 0;
}

#endif
