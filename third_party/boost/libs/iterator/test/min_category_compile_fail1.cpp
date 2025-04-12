// Copyright Andrey Semashev 2025.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/iterator/min_category.hpp>

using boost::iterators::min_category;

struct A {};
struct B {};

int main(int, char*[])
{
    min_category<A, B>::type cat;

    return 0;
}
