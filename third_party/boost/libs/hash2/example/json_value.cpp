// Copyright 2024, 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/md5.hpp>
#include <boost/hash2/hash_append.hpp>
#include <boost/json.hpp>
#include <type_traits>
#include <iostream>

namespace boost
{
namespace json
{

template<class Provider, class Hash, class Flavor>
void tag_invoke( boost::hash2::hash_append_tag const&, Provider const&,
    Hash& h, Flavor const& f, boost::json::value const* v )
{
    boost::hash2::hash_append( h, f, v->kind() ); // TODO: int64 vs uint64
    boost::json::visit( [&](auto const& w){
        boost::hash2::hash_append( h, f, w ); }, *v );
}

} // namespace json
} // namespace boost

template<class T> auto md5( T const& v )
{
    boost::hash2::md5_128 hash;
    boost::hash2::hash_append( hash, {}, v );
    return hash.result();
}

static constexpr char const* json = R"(
{
  "x1": -5,
  "x2": 3.14,
  "x3": "str",
  "x4": [ null, true, false ]
}
)";

int main()
{
    auto jv = boost::json::parse( json );
    std::cout << md5( jv ) << std::endl;
}
