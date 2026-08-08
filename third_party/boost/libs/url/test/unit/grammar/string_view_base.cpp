//
// Copyright (c) 2022 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

// Test that header file is self-contained.
#include <boost/url/grammar/string_view_base.hpp>

#include "test_suite.hpp"

namespace boost {
namespace urls {
namespace grammar {

// Minimal derived class for testing
struct test_string_view
    : string_view_base
{
    explicit
    test_string_view(
        core::string_view s) noexcept
        : string_view_base(s)
    {
    }
};

struct string_view_base_test
{
    void
    run()
    {
        // operator std::string() allocates
        static_assert(
            !noexcept(std::string(
                std::declval<test_string_view const&>())),
            "operator std::string() must not be noexcept");

        // Verify the conversion still works correctly
        {
            test_string_view sv("hello");
            std::string s = static_cast<std::string>(sv);
            BOOST_TEST(s == "hello");
        }
        {
            test_string_view sv("");
            std::string s = static_cast<std::string>(sv);
            BOOST_TEST(s.empty());
        }
    }
};

TEST_SUITE(
    string_view_base_test,
    "boost.url.grammar.string_view_base");

} // grammar
} // urls
} // boost
