//  Copyright 2026 Peter Dimov
//  Distributed under the Boost Software License, Version 1.0.
//  http://www.boost.org/LICENSE_1_0.txt

#include <boost/convert.hpp>
#include <boost/convert/stream.hpp>
#include <string>

struct boost::cnv::by_default: boost::cnv::cstream {};

int main()
{
    int x = boost::convert<int>( "14" ).value_or( -1 );
    return x == 14? 0: 1;
}
