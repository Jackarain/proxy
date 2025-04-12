// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/siphash.hpp>
#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/get_integral_result.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <string>

template<class T, class H> class hash
{
private:

    std::uint64_t seed_;

public:

    explicit hash( std::uint64_t seed ): seed_( seed )
    {
    }

    std::size_t operator()( T const& v ) const
    {
        H h( seed_ );
        boost::hash2::hash_append( h, {}, v );
        return boost::hash2::get_integral_result<std::size_t>( h );
    }
};

int main()
{
    std::uint64_t seed = 0x0102030405060708ull;

    using hasher = hash<std::string, boost::hash2::siphash_64>;

    boost::unordered_flat_map<std::string, int, hasher> map( 0, hasher( seed ) );

    map[ "foo" ] = 1;
    map[ "bar" ] = 2;
}
