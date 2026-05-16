// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/system/detail/is_aggregate.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/config/pragma_message.hpp>
#include <string>

struct X1
{
};

struct X2
{
    int a, b;
};

struct X3
{
    std::string a, b;
};

struct X4
{
    X1 x1;
    X2 x2;
    X3 x3;
};

struct Y1
{
    Y1() {}
};

#if !defined(BOOST_SYSTEM_HAS_BUILTIN_IS_AGGREGATE) && !( defined(__cpp_lib_is_aggregate) && __cpp_lib_is_aggregate >= 201703L )

BOOST_PRAGMA_MESSAGE("Test skipped, detail::is_aggregate isn't functional")
int main() {}

#else

int main()
{
    using boost::system::detail::is_aggregate;

    BOOST_TEST_TRAIT_TRUE((is_aggregate<X1>));
    BOOST_TEST_TRAIT_TRUE((is_aggregate<X2>));
    BOOST_TEST_TRAIT_TRUE((is_aggregate<X3>));
    BOOST_TEST_TRAIT_TRUE((is_aggregate<X4>));

    BOOST_TEST_TRAIT_FALSE((is_aggregate<Y1>));

    return boost::report_errors();
}

#endif
