/*
 *             Copyright Andrey Semashev 2025.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   filt_wrap_filter.cpp
 * \author Andrey Semashev
 * \date   09.07.2025
 *
 * \brief  This header contains tests for the \c wrap_filter adapter.
 */

#define BOOST_TEST_MODULE filt_wrap_filter

#include <string>
#include <boost/test/unit_test.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/log/attributes/attribute_set.hpp>
#include <boost/log/attributes/attribute_value_set.hpp>
#include <boost/log/expressions.hpp>
#include "char_definitions.hpp"

namespace logging = boost::log;
namespace attrs = logging::attributes;
namespace expr = logging::expressions;

// The test checks that the wrap_filter adapter works
BOOST_AUTO_TEST_CASE(wrap_filter_check)
{
    typedef logging::attribute_set attr_set;
    typedef logging::attribute_value_set attr_values;
    typedef logging::filter filter;
    typedef test_data< char > data;

    attrs::constant< int > attr1(10);
    attrs::constant< double > attr2(5.5);
    attrs::constant< std::string > attr3("Hello, world!");

    attr_set set1, set2, set3;
    attr_values values1(set1, set2, set3);
    values1.freeze();
    set1[data::attr2()] = attr2;
    attr_values values2(set1, set2, set3);
    values2.freeze();
    set1[data::attr3()] = attr3;
    set1[data::attr1()] = attr1;
    attr_values values3(set1, set2, set3);
    values3.freeze();

    int call_count1 = 0, call_count2 = 0;
    filter f = expr::wrap_filter([&](attr_values const& values) { ++call_count1; return values.find(data::attr1()) != values.end(); }) ||
        expr::wrap_filter([&](attr_values const& values) { ++call_count2; return values.find(data::attr2()) != values.end(); });
    BOOST_CHECK(!f(values1));
    BOOST_CHECK(call_count1 == 1);
    BOOST_CHECK(call_count2 == 1);

    call_count1 = 0;
    call_count2 = 0;
    BOOST_CHECK(f(values2));
    BOOST_CHECK(call_count1 == 1);
    BOOST_CHECK(call_count2 == 1);

    call_count1 = 0;
    call_count2 = 0;
    BOOST_CHECK(f(values3));
    BOOST_CHECK(call_count1 == 1);
    BOOST_CHECK(call_count2 == 0);

    f = expr::wrap_filter([&](attr_values const& values) { ++call_count1; return values.find(data::attr1()) != values.end(); }) &&
        expr::wrap_filter([&](attr_values const& values) { ++call_count2; return values.find(data::attr2()) != values.end(); });

    call_count1 = 0;
    call_count2 = 0;
    BOOST_CHECK(!f(values1));
    BOOST_CHECK(call_count1 == 1);
    BOOST_CHECK(call_count2 == 0);

    call_count1 = 0;
    call_count2 = 0;
    BOOST_CHECK(!f(values2));
    BOOST_CHECK(call_count1 == 1);
    BOOST_CHECK(call_count2 == 0);

    call_count1 = 0;
    call_count2 = 0;
    BOOST_CHECK(f(values3));
    BOOST_CHECK(call_count1 == 1);
    BOOST_CHECK(call_count2 == 1);
}
