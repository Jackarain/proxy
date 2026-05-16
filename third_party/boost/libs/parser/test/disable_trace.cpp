/**
*   Copyright (C) 2025
 *
 *   Distributed under the Boost Software License, Version 1.0. (See
 *   accompanying file LICENSE_1_0.txt or copy at
 *   http://www.boost.org/LICENSE_1_0.txt)
 */

#define BOOST_PARSER_DISABLE_TRACE

#include <boost/core/lightweight_test.hpp>
#include <boost/parser/parser.hpp>

int main()
{
    namespace bp = boost::parser;
    {
        auto const parser =
            bp::string("FOO") >> -(bp::string("bar") | bp::string("foo"));

        auto result = bp::parse("FOOfoo", parser);
        BOOST_TEST(result);
        BOOST_TEST(bp::get(*result, bp::llong<0>{}) == std::string("FOO"));
        BOOST_TEST(bp::get(*result, bp::llong<1>{}) == std::string("foo"));
    }
    return boost::report_errors();
}