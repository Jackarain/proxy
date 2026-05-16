// Copyright 2021, 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/describe/enum_to_string.hpp>
#include <boost/describe/enum.hpp>
#include <boost/describe/detail/cx_streq.hpp>
#include <boost/config/pragma_message.hpp>

#if !defined(BOOST_DESCRIBE_CXX14)

BOOST_PRAGMA_MESSAGE("Skipping test because C++14 is not available")

#elif defined(_MSC_VER) && _MSC_VER == 1900

BOOST_PRAGMA_MESSAGE("Skipping test because _MSC_VER == 1900")

#else

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

BOOST_DEFINE_ENUM_CLASS(E, v1, v2, v3)

using boost::describe::enum_to_string;
using boost::describe::detail::cx_streq;

STATIC_ASSERT( cx_streq( enum_to_string( E::v1, "" ), "v1" ) );
STATIC_ASSERT( cx_streq( enum_to_string( E::v2, "" ), "v2" ) );
STATIC_ASSERT( cx_streq( enum_to_string( E::v3, "" ), "v3" ) );
STATIC_ASSERT( cx_streq( enum_to_string( (E)17, "def" ), "def" ) );

#endif // !defined(BOOST_DESCRIBE_CXX14)
