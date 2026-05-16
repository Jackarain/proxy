// Copyright 2025 Matt Borland
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#define BOOST_TEST_MODULE github_issue_475
#include <optional>
#include <boost/test/unit_test.hpp>

BOOST_TEST_DONT_PRINT_LOG_VALUE(std::optional<int>)

BOOST_AUTO_TEST_CASE(test1)
{
    std::optional<int> a,b;
    BOOST_TEST(a==b);
}
