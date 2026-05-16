//  Copyright 2026 Peter Dimov
//  Distributed under the Boost Software License, Version 1.0.
//  http://www.boost.org/LICENSE_1_0.txt

#undef NDEBUG

#include <boost/range.hpp>
#include <string>
#include <cassert>

int main()
{
    std::string s( "foo" );

    assert( boost::begin( s ) == s.begin() );
    assert( boost::end( s ) == s.end() );
    assert( boost::empty( s ) == s.empty() );
    assert( boost::size( s ) == s.size() );
}
