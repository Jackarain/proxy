// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/core/lightweight_test.hpp>
#include <random>

using namespace boost::uuids;

int main()
{
    {
        std::mt19937 rng( 0 );
        basic_random_generator<std::mt19937> gen( rng );

        uuid expected = string_generator()( "ac0a7f8c-2faa-4497-b5a6-16b7c0cc21d8" );

        BOOST_TEST_EQ( gen(), expected );
    }

    {
        std::mt19937_64 rng( 0 );
        basic_random_generator<std::mt19937_64> gen( rng );

        uuid expected = string_generator()( "3edc41cb-c537-4828-8bf9-403e7c3afdfd" );

        BOOST_TEST_EQ( gen(), expected );
    }

    return boost::report_errors();
}
