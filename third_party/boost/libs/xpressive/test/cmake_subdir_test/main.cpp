//  Copyright 2026 Peter Dimov
//  Distributed under the Boost Software License, Version 1.0.
//  http://www.boost.org/LICENSE_1_0.txt

#include <boost/xpressive/xpressive.hpp>
#include <string>

using namespace boost::xpressive;

int main()
{
    std::string hello( "hello world!" );

    sregex rex = sregex::compile( "(\\w+) (\\w+)!" );
    smatch what;

    if( regex_match( hello, what, rex ) )
    {
        return what.size() == 3? 0: 1;
    }
    else
    {
        return 2;
    }
}
