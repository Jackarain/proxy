//  Copyright 2026 Peter Dimov
//  Distributed under the Boost Software License, Version 1.0.
//  http://www.boost.org/LICENSE_1_0.txt

#include <boost/proto/core.hpp>

int main()
{
    boost::proto::terminal<int>::type x = {{}};
    (void)x;
}
