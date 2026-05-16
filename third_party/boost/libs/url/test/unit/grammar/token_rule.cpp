//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

// Test that header file is self-contained.
#include <boost/url/grammar/token_rule.hpp>

#include <boost/url/grammar/alpha_chars.hpp>
#include <boost/url/grammar/parse.hpp>

#include "test_rule.hpp"

namespace boost {
namespace urls {
namespace grammar {

struct token_rule_test
{
    void
    run()
    {
        // constexpr
        constexpr auto r = token_rule(alpha_chars);

        // javadoc
        {
            system::result< core::string_view > rv = parse( "abcdef", token_rule( alpha_chars ) );

            (void)rv;
        }

        ok(r, "a", "a");
        bad(r, "", error::need_more);
        bad(r, "1", error::mismatch);

        // Test default construction with stateless CharSet
        {
            constexpr auto r2 = token_rule<implementation_defined::alpha_chars_t>();
            ok(r2, "abc", "abc");
            ok(r2, "ABC", "ABC");
            bad(r2, "", error::need_more);
            bad(r2, "123", error::mismatch);
        }

        // Test EBO - token_rule_t with empty CharSet should be minimal size
        {
            // alpha_chars_t is empty (no data members)
            static_assert(
                sizeof(implementation_defined::token_rule_t<implementation_defined::alpha_chars_t>) == 1,
                "EBO should reduce size to 1 byte for empty CharSet");
        }

        // Test that default-constructed rules work correctly at runtime
        {
            auto r3 = token_rule<implementation_defined::alpha_chars_t>();
            auto rv = parse("abcdef", r3);
            BOOST_TEST(rv.has_value());
            BOOST_TEST_EQ(rv.value(), "abcdef");
        }
    }
};

TEST_SUITE(
    token_rule_test,
    "boost.url.grammar.token_rule");

} // grammar
} // urls
} // boost
