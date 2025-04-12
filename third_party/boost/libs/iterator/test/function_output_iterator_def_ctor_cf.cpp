// Copyright 2025 (c) Andrey Semashev
// Distributed under the Boost Software License Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/iterator/function_output_iterator.hpp>

int main()
{
    boost::iterators::function_output_iterator< void (*)(int) > it;
    (void)it;

    return 0;
}
