// Copyright 2024 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/hash2/detail/has_tag_invoke.hpp>
#include <boost/hash2/hash_append_fwd.hpp>
#include <boost/core/lightweight_test_trait.hpp>

struct X1
{
};

struct X2
{
    int a;
    int b;
};

template<class Provider, class Hash, class Flavor>
void tag_invoke( boost::hash2::hash_append_tag const&, Provider const&, Hash& h, Flavor const& f, X2 const* x )
{
    boost::hash2::hash_append( h, f, x->a );
    boost::hash2::hash_append( h, f, x->b );
}

class X3
{
private:

    int a;
    int b;

private:

    template<class Provider, class Hash, class Flavor>
    inline friend void tag_invoke( boost::hash2::hash_append_tag const&, Provider const&, Hash& h, Flavor const& f, X3 const* x )
    {
        boost::hash2::hash_append( h, f, x->a );
        boost::hash2::hash_append( h, f, x->b );
    }
};

struct another_tag
{
};

struct X4
{
};

template<class... T> void tag_invoke( another_tag const&, T const&... )
{
}

int main()
{
    using boost::hash2::detail::has_tag_invoke;

    BOOST_TEST_TRAIT_FALSE(( has_tag_invoke<X1> ));
    BOOST_TEST_TRAIT_FALSE(( has_tag_invoke<X1 const> ));

    BOOST_TEST_TRAIT_TRUE(( has_tag_invoke<X2> ));
    BOOST_TEST_TRAIT_TRUE(( has_tag_invoke<X2 const> ));

    BOOST_TEST_TRAIT_TRUE(( has_tag_invoke<X3> ));
    BOOST_TEST_TRAIT_TRUE(( has_tag_invoke<X3 const> ));

    BOOST_TEST_TRAIT_FALSE(( has_tag_invoke<X4> ));
    BOOST_TEST_TRAIT_FALSE(( has_tag_invoke<X4 const> ));

    return boost::report_errors();
}
