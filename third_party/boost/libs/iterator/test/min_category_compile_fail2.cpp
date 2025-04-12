// Copyright Andrey Semashev 2025.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/iterator/min_category.hpp>

using boost::iterators::min_category;

int main(int, char*[])
{
    min_category<>::type cat;

    return 0;
}
