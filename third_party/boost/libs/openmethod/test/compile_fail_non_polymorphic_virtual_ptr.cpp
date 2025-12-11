// Copyright (c) 2018-2025 Jean-Louis Leroy
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/openmethod.hpp>

struct Cat {};

int main() {
    Cat felix;
    boost::openmethod::virtual_ptr<Cat> p(felix);
    (void)p;

    return 0;
}
