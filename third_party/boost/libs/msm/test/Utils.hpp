// Copyright 2025 Christian Granzin
// Copyright 2010 Christophe Henry
// henry UNDERSCORE christophe AT hotmail DOT com
// This is an extended version of the state machine available in the boost::mpl library
// Distributed under the same license as the original.
// Copyright for the original version:
// Copyright 2005 David Abrahams and Aleksey Gurtovoy. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#define ASSERT_AND_RESET(value, expected)                                      \
    BOOST_REQUIRE(value == expected);                                          \
    value = 0

#define ASSERT_ONE_AND_RESET(value)                                            \
    BOOST_REQUIRE(value == 1);                                                 \
    value = 0

