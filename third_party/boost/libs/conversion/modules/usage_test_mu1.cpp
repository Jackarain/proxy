// Copyright (c) 2025-2026 Antony Polukhin
// Copyright (c) 2025-2026 Fedor Osetrov
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

#include <boost/implicit_cast.hpp>

long make_me_long(int x);

int main() {
    std::cout << make_me_long(42) << '\n';
    std::cout << boost::implicit_cast<long>(42) << '\n';
}
