//
// Copyright (c) 2025 Alexander Grund
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/static_string
//

#include <boost/static_string.hpp>

int main()
{
    // Smoke test only
    boost::static_string<1> s_empty;
    boost::static_string<4> s_3(3, 'x');
    return (s_empty.empty() && s_3.size() == 3) ? 0 : 1;
}
