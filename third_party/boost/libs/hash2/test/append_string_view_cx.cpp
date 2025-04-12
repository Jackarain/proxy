// Copyright 2017, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/utility/string_view.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
#include <string_view>
#endif

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

template<class Hash, class Flavor, class T>
BOOST_CXX14_CONSTEXPR typename Hash::result_type test( T const& v )
{
    Hash h;
    Flavor f{};

    hash_append( h, f, v );

    return h.result();
}

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if defined(BOOST_NO_CXX14_CONSTEXPR) || ( defined(BOOST_GCC) && BOOST_GCC < 60000 )

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2)

#else

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2); STATIC_ASSERT(x1 == x2)

#endif

int main()
{
    using namespace boost::hash2;

    constexpr char const* str = "\x01\x02\x03\x04";

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( boost::string_view( str, 4 ) )), 2227238665 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( boost::string_view( str, 4 ) )), 3245468929 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( boost::core::string_view( str, 4 ) )), 2227238665 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( boost::core::string_view( str, 4 ) )), 3245468929 );

#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)

    TEST_EQ( (test<fnv1a_32, little_endian_flavor>( std::string_view( str ) )), 2227238665 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor>( std::string_view( str ) )), 3245468929 );

#endif

    return boost::report_errors();
}
