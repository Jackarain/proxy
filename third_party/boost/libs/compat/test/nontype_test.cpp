// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/compat/detail/nontype.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/config/pragma_message.hpp>

#if !( defined(__cpp_nontype_template_parameter_auto) && __cpp_nontype_template_parameter_auto >= 201606L )

BOOST_PRAGMA_MESSAGE("Test skipped, __cpp_nontype_template_parameter_auto is not defined to at least 201606L")
int main() {}

#else

void f() {}

struct X
{
    void f() {}
};

int main()
{
    BOOST_TEST_TRAIT_SAME( boost::compat::nontype_t< 5 > const, decltype( boost::compat::nontype< 5 > ) );
    BOOST_TEST_TRAIT_SAME( boost::compat::nontype_t< ::f > const, decltype( boost::compat::nontype< ::f > ) );
    BOOST_TEST_TRAIT_SAME( boost::compat::nontype_t< &X::f > const, decltype( boost::compat::nontype< &X::f > ) );

    return boost::report_errors();
}

#endif
