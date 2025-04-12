// Copyright 2017, 2023 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>

int main()
{
    boost::hash2::fnv1a_32 hash;

    char const str[ 6 ] = { 'f', 'o', 'o', 'b', 'a', 'r' };

    boost::hash2::hash_append( hash, {}, str );

    return hash.result() == 0xbf9cf968ul? 0: 1;
}
