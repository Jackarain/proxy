// Copyright (c) 2016-2025 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <boost/type_index.hpp>

void do_something(std::ostream& os);

int main() {
    do_something(std::cout);
    std::cout << '\n';
    std::cout << boost::typeindex::type_id_with_cvr<const int>();  // Outputs: const int
}

