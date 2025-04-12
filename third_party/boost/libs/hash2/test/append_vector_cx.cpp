// Copyright 2017, 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <vector>
#include <cstdint>

#if defined(BOOST_MSVC) && BOOST_MSVC < 1920
# pragma warning(disable: 4307) // integral constant overflow
#endif

template<class Hash, class Flavor, class T>
BOOST_CXX14_CONSTEXPR typename Hash::result_type test()
{
    unsigned char w[] = { 1, 2, 3, 4 };
    std::vector<T> v( w, w + sizeof(w) / sizeof(w[0]) );

    Hash h;
    Flavor f{};

    boost::hash2::hash_append( h, f, v );

    return h.result();
}

#define STATIC_ASSERT(...) static_assert(__VA_ARGS__, #__VA_ARGS__)

#if !( defined(__cpp_lib_constexpr_vector) && __cpp_lib_constexpr_vector >= 201907L )

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2)

#else

# define TEST_EQ(x1, x2) BOOST_TEST_EQ(x1, x2); STATIC_ASSERT(x1 == x2)

#endif

int main()
{
    using namespace boost::hash2;

    TEST_EQ( (test<fnv1a_32, little_endian_flavor, unsigned char>()), 2227238665 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor, unsigned char>()), 3245468929 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor, int>()), 1745310485 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor, int>()), 3959736277 );

    TEST_EQ( (test<fnv1a_32, little_endian_flavor, double>()), 1949716516 );
    TEST_EQ( (test<fnv1a_32, big_endian_flavor, double>()), 2651227990 );

    return boost::report_errors();
}
