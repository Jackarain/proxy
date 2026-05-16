// Copyright (c) 2016-2026 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>

import boost.type_index;

void do_something(std::ostream& os) {
    os << boost::typeindex::type_id_with_cvr<const int>();
}

