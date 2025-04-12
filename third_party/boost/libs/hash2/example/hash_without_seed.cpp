// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/fnv1a.hpp>
#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/get_integral_result.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <string>

template<class T, class H> class hash
{
public:

    std::size_t operator()( T const& v ) const
    {
        H h;
        boost::hash2::hash_append( h, {}, v );
        return boost::hash2::get_integral_result<std::size_t>( h );
    }
};

int main()
{
    using hasher = hash<std::string, boost::hash2::fnv1a_64>;

    boost::unordered_flat_map<std::string, int, hasher> map;

    map[ "foo" ] = 1;
    map[ "bar" ] = 2;
}
