/*
 *             Copyright Andrey Semashev 2025.
 * Distributed under the Boost Software License, Version 1.0.
 *    (See accompanying file LICENSE_1_0.txt or copy at
 *          http://www.boost.org/LICENSE_1_0.txt)
 */

// The test verifies that the compiler provides a <type_traits> header that is
// sufficient for Boost.Scope needs.

#include <type_traits>

int main(int, char*[])
{
    using std::integral_constant;
    using std::true_type;
    using std::false_type;
    using std::conditional;
    using std::enable_if;

    using std::decay;

    using std::is_class;
    using std::is_reference;

    using std::is_constructible;
    using std::is_nothrow_constructible;
    using std::is_move_constructible;
    using std::is_nothrow_move_constructible;
    using std::is_default_constructible;
    using std::is_nothrow_default_constructible;
    using std::is_assignable;
    using std::is_nothrow_assignable;
    using std::is_move_assignable;
    using std::is_nothrow_move_assignable;

    return 0;
}
