/*
 *             Copyright Andrey Semashev 2025.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */
/*!
 * \file   form_wrap_formatter.cpp
 * \author Andrey Semashev
 * \date   11.07.2025
 *
 * \brief  This header contains tests for the \c wrap_formatter utility.
 */

#define BOOST_TEST_MODULE form_wrap_formatter

#include <string>
#include <boost/test/unit_test.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/log/attributes/attribute_set.hpp>
#include <boost/log/utility/formatting_ostream.hpp>
#include <boost/log/utility/value_ref.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/core/record.hpp>
#include "char_definitions.hpp"
#include "make_record.hpp"

namespace logging = boost::log;
namespace attrs = logging::attributes;
namespace expr = logging::expressions;

namespace {

    class my_class
    {
        int m_Data;

    public:
        explicit my_class(int data) : m_Data(data) {}

        int get_data() const { return m_Data; }
    };

    template< typename StreamT >
    inline void format_my_class(StreamT& strm, my_class const& obj)
    {
        strm << "[data: " << obj.get_data() << "]";
    }

} // namespace

// The test checks that wrap_formatter works
BOOST_AUTO_TEST_CASE_TEMPLATE(wrap_formatter_check, CharT, char_types)
{
    typedef logging::record_view record_view;
    typedef logging::attribute_set attr_set;
    typedef std::basic_string< CharT > string;
    typedef logging::basic_formatting_ostream< CharT > osstream;
    typedef logging::basic_formatter< CharT > formatter;
    typedef test_data< CharT > data;

    attrs::constant< int > attr1(10);
    attrs::constant< double > attr2(5.5);
    attrs::constant< my_class > attr3(my_class(77));

    attr_set set1;
    set1[data::attr1()] = attr1;
    set1[data::attr2()] = attr2;
    set1[data::attr3()] = attr3;

    record_view rec = make_record_view(set1);

    // Check for various modes of attribute value type specification
    {
        string str1, str2;
        osstream strm1(str1), strm2(str2);
        int call_count = 0;
        formatter f =
            expr::stream << expr::attr< int >(data::attr1())
                << expr::attr< double >(data::attr2())
                << expr::wrap_formatter< CharT >([&](record_view const& rec, osstream& strm)
                {
                    ++call_count;
                    logging::value_ref< my_class > val = rec[data::attr3()].template extract< my_class >();
                    if (val)
                        format_my_class(strm, val.get());
                });
        f(rec, strm1);
        strm2 << 10 << 5.5;
        format_my_class(strm2, my_class(77));
        BOOST_CHECK(equal_strings(strm1.str(), strm2.str()));
    }
}
