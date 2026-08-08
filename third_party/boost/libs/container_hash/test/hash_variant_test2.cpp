// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/container_hash/hash.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_NO_CXX17_HDR_VARIANT)

BOOST_PRAGMA_MESSAGE( "Skipping test because BOOST_NO_CXX17_HDR_VARIANT is defined" )
int main() {}

#elif defined(BOOST_CLANG_VERSION) && BOOST_CLANG_VERSION < 70100

BOOST_PRAGMA_MESSAGE( "Skipping test because BOOST_CLANG_VERSION < 70100" )
int main() {}

#else

#include <boost/core/lightweight_test.hpp>
#include <variant>
#include <set>

struct X
{
    operator std::set<float>() const { throw 5; }
};

using V = std::variant<std::set<int>, std::set<float>>;

V make_valueless_variant()
{
    V v;

    try
    {
        v.emplace<1>( X() );
    }
    catch( int )
    {
    }

    BOOST_TEST( v.valueless_by_exception() );

    return v;
}

int main()
{
    V v1, v2 = make_valueless_variant();

    BOOST_TEST_NE( (boost::hash<V>()( v1 )), (boost::hash<V>()( v2 )) );

    return boost::report_errors();
}

#endif
