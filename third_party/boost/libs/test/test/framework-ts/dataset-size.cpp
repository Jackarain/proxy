//  (C) Copyright Alexander Grund 2025.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE dataset size class
#include <boost/test/unit_test.hpp>

#include <boost/test/data/size.hpp>
#include <limits>

namespace data = boost::unit_test::data;

BOOST_AUTO_TEST_CASE(test_constructor)
{
    data::size_t sz0;
    BOOST_TEST(!sz0.is_inf());
    BOOST_TEST(sz0.value() == 0u);
    BOOST_TEST(!sz0);

    data::size_t sz2 = sz0;
    BOOST_TEST(!sz2.is_inf());
    BOOST_TEST(sz2.value() == 0u);
    BOOST_TEST(!sz2);

    data::size_t sz1(42);
    BOOST_TEST(!sz1.is_inf());
    BOOST_TEST(sz1.value() == 42u);
    BOOST_TEST(!!sz1);

    data::size_t sz3 = sz1;
    BOOST_TEST(!sz3.is_inf());
    BOOST_TEST(sz3.value() == 42u);
    BOOST_TEST(!!sz3);

    data::size_t sz4(true);
    BOOST_TEST(sz4.is_inf());
    BOOST_TEST(sz4.value() == 0u);
    BOOST_TEST(!!sz4);

    data::size_t sz5(false);
    BOOST_TEST(sz5.is_inf());
    BOOST_TEST(sz5.value() == 0u);
    BOOST_TEST(!!sz5);

    sz2 = sz5;
    BOOST_TEST(sz2.is_inf());
    BOOST_TEST(sz2.value() == 0u);
    BOOST_TEST(!!sz2);

    sz2 = sz1;
    BOOST_TEST(!sz2.is_inf());
    BOOST_TEST(sz2.value() == 42u);
    BOOST_TEST(!!sz2);
}

BOOST_AUTO_TEST_CASE(test_unary_ops)
{
    data::size_t sz(100);

    data::size_t sz2 = ++sz;
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 101u);
    BOOST_TEST(!sz2.is_inf());
    BOOST_TEST(sz2.value() == 101u);

    sz2 = sz++;
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 102u);
    BOOST_TEST(!sz2.is_inf());
    BOOST_TEST(sz2.value() == 101u);

    sz2 = --sz;
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 101u);
    BOOST_TEST(!sz2.is_inf());
    BOOST_TEST(sz2.value() == 101u);

    sz2 = sz--;
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 100u);
    BOOST_TEST(!sz2.is_inf());
    BOOST_TEST(sz2.value() == 101u);

    // Over- and underflow
    BOOST_CONSTEXPR_OR_CONST std::size_t maxVal = (std::numeric_limits<size_t>::max)();
    sz = maxVal;
    sz2 = ++sz;
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 0u);
    BOOST_TEST(!sz2.is_inf());
    BOOST_TEST(sz2.value() == 0u);

    sz = maxVal;
    sz2 = sz++;
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 0);
    BOOST_TEST(!sz2.is_inf());
    BOOST_TEST(sz2.value() == maxVal);

    sz = 0;
    sz2 = --sz;
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == maxVal);
    BOOST_TEST(!sz2.is_inf());
    BOOST_TEST(sz2.value() == maxVal);

    sz = 0;
    sz2 = sz--;
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == maxVal);
    BOOST_TEST(!sz2.is_inf());
    BOOST_TEST(sz2.value() == 0u);

    //____________________________________________________________________________//
    sz = data::BOOST_TEST_DS_INFINITE_SIZE;
    sz2 = ++sz;
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);
    BOOST_TEST(sz2.is_inf());
    BOOST_TEST(sz2.value() == 0u);

    sz2 = sz++;
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);
    BOOST_TEST(sz2.is_inf());
    BOOST_TEST(sz2.value() == 0u);

    sz2 = --sz;
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);
    BOOST_TEST(sz2.is_inf());
    BOOST_TEST(sz2.value() == 0u);

    sz2 = sz--;
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);
    BOOST_TEST(sz2.is_inf());
    BOOST_TEST(sz2.value() == 0u);
}

BOOST_AUTO_TEST_CASE(test_binary_inc)
{
    data::size_t sz(100);

    sz += 5;
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 105u);

    sz += data::size_t(5);
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 110u);

    sz += data::BOOST_TEST_DS_INFINITE_SIZE;
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);

    //____________________________________________________________________________//
    sz = data::BOOST_TEST_DS_INFINITE_SIZE;

    sz += 5;
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);

    sz += data::size_t(5);
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);

    sz += data::BOOST_TEST_DS_INFINITE_SIZE;
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);

    //____________________________________________________________________________//
    data::size_t sz2(100);

    sz = sz2 + 5;
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 105u);

    sz = sz2 + data::size_t(5);
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 105u);

    sz = sz2 + data::BOOST_TEST_DS_INFINITE_SIZE;
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);

    //____________________________________________________________________________//
    sz2 = data::BOOST_TEST_DS_INFINITE_SIZE;

    sz = sz2 + 5;
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);

    sz = sz2 + data::size_t(5);
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);

    sz = sz2 + data::BOOST_TEST_DS_INFINITE_SIZE;
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);
}

BOOST_AUTO_TEST_CASE(test_binary_dec)
{
    data::size_t sz(100);

    sz -= 5;
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 95u);

    sz -= data::size_t(5);
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 90u);

    sz -= data::BOOST_TEST_DS_INFINITE_SIZE;
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 90u);

    //____________________________________________________________________________//
    sz = data::BOOST_TEST_DS_INFINITE_SIZE;

    sz -= 5;
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);

    sz -= data::size_t(5);
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);

    sz -= data::BOOST_TEST_DS_INFINITE_SIZE;
    BOOST_TEST(sz.is_inf());
    BOOST_TEST(sz.value() == 0u);

    BOOST_CONSTEXPR_OR_CONST std::size_t maxVal = (std::numeric_limits<size_t>::max)();
    // Underflow is avoided for data::size_t values
    sz = 1;
    sz -= 5;
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == maxVal - 5u + 2u);

    sz = 1;
    sz -= data::size_t(5);
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 0u);

    sz = 1;
    sz -= data::BOOST_TEST_DS_INFINITE_SIZE;
    BOOST_TEST(!sz.is_inf());
    BOOST_TEST(sz.value() == 1u);
}