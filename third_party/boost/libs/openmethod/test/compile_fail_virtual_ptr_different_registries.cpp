// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>

using namespace boost::openmethod;

struct Cat {
    virtual ~Cat() {
    }
};

struct other_registry : default_registry::without<policies::type_hash>::with<
                            policies::runtime_checks> {};

BOOST_OPENMETHOD(poke, (virtual_ptr<Cat>), void, other_registry);

int main() {
    Cat felix;
    poke(felix);
    return 0;
}
