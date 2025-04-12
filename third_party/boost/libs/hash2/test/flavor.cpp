// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/flavor.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <cstdint>

int main()
{
    using namespace boost::hash2;

    BOOST_TEST_TRAIT_SAME( default_flavor::size_type, std::uint32_t );
    BOOST_TEST( default_flavor::byte_order == endian::native );

    BOOST_TEST_TRAIT_SAME( little_endian_flavor::size_type, std::uint32_t );
    BOOST_TEST( little_endian_flavor::byte_order == endian::little );

    BOOST_TEST_TRAIT_SAME( big_endian_flavor::size_type, std::uint32_t );
    BOOST_TEST( big_endian_flavor::byte_order == endian::big );

    return boost::report_errors();
}
