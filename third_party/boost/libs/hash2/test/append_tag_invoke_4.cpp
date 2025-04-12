// Copyright 2024, 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/fnv1a.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>

// https://github.com/jbcoe/poison_hash_append

struct A
{
};

struct B
{
    B( A const& )
    {
    }

    B()
    {
    }

    A a_;
};

template<class Provider, class Hash, class Flavor>
void tag_invoke( boost::hash2::hash_append_tag const&, Provider const&, Hash& h, Flavor const& f, B const* b )
{
    boost::hash2::hash_append( h, f, b->a_ );
}

int main()
{
    B b;

    boost::hash2::fnv1a_32 h;
    boost::hash2::hash_append( h, {}, b );
}
