// Copyright 2024 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/siphash.hpp>
#include <boost/hash2/md5.hpp>
#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/get_integral_result.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/core/type_name.hpp>
#include <type_traits>
#include <string>
#include <iostream>

template<class T, class H1, class H2 = H1> class hash
{
public:

    using hash_type = typename std::conditional<
        sizeof(typename H1::result_type) == sizeof(std::size_t), H1, H2
    >::type;

private:

    hash_type h_;

public:

    hash(): h_()
    {
    }

    explicit hash( std::uint64_t seed ): h_( seed )
    {
    }

    hash( unsigned char const* p, std::size_t n ): h_( p, n )
    {
    }

    std::size_t operator()( T const& v ) const
    {
        hash_type h( h_ );
        boost::hash2::hash_append( h, {}, v );
        return boost::hash2::get_integral_result<std::size_t>( h );
    }
};

int main()
{
    {
        using hasher = hash<std::string, boost::hash2::siphash_32, boost::hash2::siphash_64>;

        std::cout << boost::core::type_name<hasher>() << " uses "
            << boost::core::type_name<hasher::hash_type>() << std::endl;

        boost::unordered_flat_map<std::string, int, hasher> map;

        map[ "foo" ] = 1;
        map[ "bar" ] = 2;
    }

    {
        using hasher = hash<std::string, boost::hash2::md5_128>;

        std::cout << boost::core::type_name<hasher>() << " uses "
            << boost::core::type_name<hasher::hash_type>() << std::endl;

        boost::unordered_flat_map<std::string, int, hasher> map;

        map[ "foo" ] = 1;
        map[ "bar" ] = 2;
    }
}
