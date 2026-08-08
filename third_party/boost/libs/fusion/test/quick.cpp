// Copyright 2026 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt)

#include <boost/fusion/sequence.hpp>
#include <boost/fusion/include/sequence.hpp>
#include <string>

int main()
{
    using namespace boost::fusion;

    vector<int, char, std::string> stuff( 1, 'x', "howdy" );

    int i = at_c<0>( stuff );
    char ch = at_c<1>( stuff );
    std::string s = at_c<2>( stuff );

    (void)i;
    (void)ch;
    (void)s;
}
