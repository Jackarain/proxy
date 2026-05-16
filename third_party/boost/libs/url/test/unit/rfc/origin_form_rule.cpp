//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

// Test that header file is self-contained.
#include <boost/url/rfc/origin_form_rule.hpp>

#include "test_rule.hpp"

namespace boost {
namespace urls {

struct origin_form_rule_test
{
    void
    run()
    {
        // javadoc
        {
            system::result< url_view > rv = grammar::parse( "/index.htm?layout=mobile", origin_form_rule );
            (void)rv;
        }

        ok(origin_form_rule, "/");
        ok(origin_form_rule, "/path/to/resource");
        ok(origin_form_rule, "/path?query");
        ok(origin_form_rule, "/path?a=1&b=2");

        bad(origin_form_rule, "");
        bad(origin_form_rule, "relative");
        bad(origin_form_rule, "/foo%");
        bad(origin_form_rule, "/foo%G0");
    }
};

TEST_SUITE(
    origin_form_rule_test,
    "boost.url.origin_form_rule");

} // urls
} // boost
