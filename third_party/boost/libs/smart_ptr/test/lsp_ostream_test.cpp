// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/smart_ptr/local_shared_ptr.hpp>
#include <boost/core/lightweight_test.hpp>
#include <sstream>

template<class T> std::string to_string( T const& t )
{
    std::ostringstream os;
    os << t;
    return os.str();
}

template<class T> std::wstring to_wstring( T const& t )
{
    std::wostringstream os;
    os << t;
    return os.str();
}

int main()
{
    boost::local_shared_ptr<int> p1, p2( new int );

    BOOST_TEST_EQ( to_string( p1 ), to_string( p1.get() ) );
    BOOST_TEST_EQ( to_string( p2 ), to_string( p2.get() ) );

    BOOST_TEST( to_wstring( p1 ) == to_wstring( p1.get() ) );
    BOOST_TEST( to_wstring( p2 ) == to_wstring( p2.get() ) );

    return boost::report_errors();
}
