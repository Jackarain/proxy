// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/config/pragma_message.hpp>

#if !defined(__cpp_constexpr_dynamic_alloc) || __cpp_constexpr_dynamic_alloc < 201907L

BOOST_PRAGMA_MESSAGE("Skipping test because __cpp_constexpr_dynamic_alloc < 201907L")
int main() {}

#else

using namespace boost::variant2;

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

struct X
{
    constexpr ~X() {}
};

int main()
{
    constexpr variant<X, int> v( 5 );

    STATIC_ASSERT( v.index() == 1 );
    STATIC_ASSERT( get<1>(v) == 5 );
}

#endif
