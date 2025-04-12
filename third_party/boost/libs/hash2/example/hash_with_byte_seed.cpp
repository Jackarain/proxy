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

    H h_;

public:

    hash( unsigned char const* p, std::size_t n ): h_( p, n )
    {
    }

    std::size_t operator()( T const& v ) const
    {
        H h( h_ );
        boost::hash2::hash_append( h, {}, v );
        return boost::hash2::get_integral_result<std::size_t>( h );
    }
};

int main()
{
    unsigned char const seed[ 16 ] =
    {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
    };

    using hasher = hash<std::string, boost::hash2::siphash_64>;

    boost::unordered_flat_map<std::string, int, hasher> map( 0, hasher( seed, sizeof(seed) ) );

    map[ "foo" ] = 1;
    map[ "bar" ] = 2;
}
