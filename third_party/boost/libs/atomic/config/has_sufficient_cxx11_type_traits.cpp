/*
 *             Copyright Andrey Semashev 2025.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */

// The test verifies that the compiler provides a <type_traits> header that is
// sufficient for Boost.Atomic needs.

#include <type_traits>
#include <boost/config.hpp>

int main(int, char*[])
{
    using std::integral_constant;
    using std::true_type;
    using std::false_type;
    using std::conditional;
    using std::enable_if;

    using std::alignment_of;
    using std::is_signed;
    using std::is_unsigned;
    using std::make_signed;
    using std::make_unsigned;
    using std::is_integral;
    using std::is_floating_point;

    using std::remove_cv;
    using std::remove_all_extents;

    using std::is_enum;
    using std::is_function;

    using std::is_constructible;
    using std::is_nothrow_default_constructible;

    using std::is_trivially_destructible;

#if !defined(BOOST_LIBSTDCXX_VERSION) || (BOOST_LIBSTDCXX_VERSION >= 50100)
    using std::is_trivially_default_constructible;
    using std::is_trivially_copyable;
#else
    using std::has_trivial_default_constructor;
    using std::has_trivial_copy_constructor;
    using std::has_trivial_copy_assign;
#endif

    return 0;
}
