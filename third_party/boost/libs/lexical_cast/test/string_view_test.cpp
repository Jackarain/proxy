//  Unit test for boost::lexical_cast.
//
//  See http://www.boost.org for most recent version, including documentation.
//
//  Copyright Antony Polukhin, 2012-2024.
//
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt).


#include <boost/lexical_cast.hpp>

#include <boost/utility/string_view.hpp>

#ifndef BOOST_NO_CXX17_HDR_STRING_VIEW
#include <string_view>
#endif

#include <boost/core/lightweight_test.hpp>

using namespace boost;


int main() {
    boost::string_view bsw = "1";
    BOOST_TEST_EQ(lexical_cast<std::string>(bsw), "1");
    BOOST_TEST_EQ(lexical_cast<int>(bsw), 1);

#ifndef BOOST_NO_CXX17_HDR_STRING_VIEW
    std::string_view ssw = "42";
    BOOST_TEST_EQ(lexical_cast<std::string>(ssw), "42");
    BOOST_TEST_EQ(lexical_cast<int>(ssw), 42);
#endif

    return boost::report_errors();
}
