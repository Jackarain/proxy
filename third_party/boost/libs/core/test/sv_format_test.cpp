// Copyright 2025 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/core/detail/string_view.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if !( defined(__cpp_lib_format) && __cpp_lib_format >= 201907L )

BOOST_PRAGMA_MESSAGE( "Test skipped because __cpp_lib_format is not defined to at least 201907L" )
int main() {}

#else

#include <format>
#include <string_view>

int main()
{
    BOOST_TEST_EQ( std::format( "{}", boost::core::string_view( "123" ) ), std::format( "{}", std::string_view( "123" ) ) );
    BOOST_TEST_EQ( std::format( "{:s}", boost::core::string_view( "123" ) ), std::format( "{:s}", std::string_view( "123" ) ) );
    BOOST_TEST_EQ( std::format( "{:^7}", boost::core::string_view( "123" ) ), std::format( "{:^7}", std::string_view( "123" ) ) );
    BOOST_TEST_EQ( std::format( "{:-^7}", boost::core::string_view( "123" ) ), std::format( "{:-^7}", std::string_view( "123" ) ) );

    return boost::report_errors();
}

#endif
