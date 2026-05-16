//Copyright (c) 2006-2026 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

#if defined( BOOST_NO_EXCEPTIONS )
#   error This program requires exception handling.
#endif

#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/serialization/boost_json_encoder.hpp>
#include <boost/throw_exception.hpp>

#include <boost/json.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <iomanip>
#include <iostream>
#include <exception>
#include <vector>

using output_encoder = boost::exception_serialization::boost_json_encoder;

namespace
boost
    {
    namespace
    exception_serialization
        {
        template <class Handle, class E>
        void
        serialize(Handle & h, E const & e, char const * name)
            {
            h.dispatch(
                [&](boost_json_encoder & enc) { output_at(enc, e, name); } );
            }
        }
    }

typedef boost::error_info<struct my_error1_, int> my_error1;
typedef boost::error_info<struct my_error2_, std::string> my_error2;
typedef boost::error_info<struct my_error4_, std::vector<int>> my_error4;

struct
my_info
    {
    int code;
    char const * message;

    friend
    void
    tag_invoke(boost::json::value_from_tag, boost::json::value & v, my_info const & e)
        {
        v = {
            {"code", e.code},
            {"message", e.message}
        };
        }
    };

typedef boost::error_info<struct my_error3_, my_info> my_error3;

struct
test_exception:
    virtual boost::exception,
    virtual std::exception
    {
    char const *
    what() const noexcept override
        {
        return "test_exception::what";
        }
    };

void
check_output(boost::json::value const & j, bool has_source_location)
    {
    auto const & obj = j.as_object();
    if( has_source_location )
        {
        BOOST_TEST(obj.contains("throw_file"));
        BOOST_TEST(obj.contains("throw_line"));
        BOOST_TEST(obj.contains("throw_function"));
        }
#ifndef BOOST_NO_RTTI
    BOOST_TEST(obj.contains("dynamic_exception_type"));
#endif
    BOOST_TEST(obj.contains("std::exception::what"));
    BOOST_TEST_EQ(obj.at("std::exception::what").as_string(), "test_exception::what");

    BOOST_TEST(obj.contains("my_error1_"));
    BOOST_TEST_EQ(obj.at("my_error1_").as_int64(), 42);

    BOOST_TEST(obj.contains("my_error2_"));
    BOOST_TEST_EQ(obj.at("my_error2_").as_string(), "hello");

    BOOST_TEST(obj.contains("my_error3_"));
    auto const & mij = obj.at("my_error3_").as_object();
    BOOST_TEST_EQ(mij.at("code").as_int64(), 1);
    BOOST_TEST_EQ(mij.at("message").as_string(), "error one");

    BOOST_TEST(obj.contains("my_error4_"));
    auto const & vec = obj.at("my_error4_").as_array();
    BOOST_TEST_EQ(vec.size(), 3u);
    BOOST_TEST_EQ(vec[0].as_int64(), 1);
    BOOST_TEST_EQ(vec[1].as_int64(), 2);
    BOOST_TEST_EQ(vec[2].as_int64(), 3);
    }

int
main()
    {
        {
        std::cout << "Testing serialize_diagnostic_information_to:\n";
        boost::json::value j;
        try
            {
            test_exception e;
            e <<
                my_error1(42) <<
                my_error2("hello") <<
                my_error3({1, "error one"}) <<
                my_error4({1, 2, 3});
            BOOST_THROW_EXCEPTION(e);
            }
        catch( test_exception & e )
            {
            output_encoder enc{j};
            boost::serialize_diagnostic_information_to(e, enc);
            }
        std::cout << j << std::endl;
        check_output(j, true);
        }

        {
        std::cout << "\nTesting serialize_current_exception_diagnostic_information_to:\n";
        boost::json::value j;
        try
            {
            test_exception e;
            e <<
                my_error1(42) <<
                my_error2("hello") <<
                my_error3({1, "error one"}) <<
                my_error4({1, 2, 3});
            BOOST_THROW_EXCEPTION(e);
            }
        catch( ... )
            {
            output_encoder enc{j};
            boost::serialize_current_exception_diagnostic_information_to(enc);
            }
        std::cout << j << std::endl;
        check_output(j, true);
        }

    return boost::report_errors();
    }
