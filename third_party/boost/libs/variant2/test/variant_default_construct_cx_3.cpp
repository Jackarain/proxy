// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/variant2/variant.hpp>
#include <boost/config/pragma_message.hpp>

#if !defined(__cpp_constexpr_dynamic_alloc) || __cpp_constexpr_dynamic_alloc < 201907L

BOOST_PRAGMA_MESSAGE("Skipping constexpr destructor test because __cpp_constexpr_dynamic_alloc < 201907L")
int main() {}

#elif defined(BOOST_CLANG) && BOOST_CLANG_VERSION < 180000

BOOST_PRAGMA_MESSAGE("Skipping constexpr destructor test because BOOST_CLANG_VERSION < 180000")
int main() {}

#else

using namespace boost::variant2;

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

struct X
{
	~X() {}
};

int main()
{
    constexpr variant<int, X> v;

    STATIC_ASSERT( v.index() == 0 );
    STATIC_ASSERT( get<0>(v) == 0 );
}

#endif
