// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>

using boost::openmethod::virtual_;

struct Cat {
    virtual ~Cat() {
    }
};

BOOST_OPENMETHOD(poke, (virtual_<Cat>), void);

int main() {
    Cat felix;
    poke(felix);
    return 0;
}
