//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

// Test that header file is self-contained.
#include <boost/url/decode.hpp>

#include <boost/url/error.hpp>
#include <boost/url/string_view.hpp>

#include "test_suite.hpp"

namespace boost {
namespace urls {

class decode_test
{
public:
    void
    testDecodedSize()
    {
        // validated percent-encoding
        {
            auto const r = decoded_size("Hello%20World");
            BOOST_TEST(r);
            if(r)
                BOOST_TEST_EQ(*r, 11);
        }

        // fully encoded input
        {
            auto const r = decoded_size("alpha%20beta");
            BOOST_TEST(r);
            if(r)
                BOOST_TEST_EQ(*r, 10);
        }

        // malformed escape
        {
            auto const r = decoded_size("bad%2");
            BOOST_TEST(r.error() == error::incomplete_encoding);
        }
    }

    void
    testDecodeBuffer()
    {
        // full buffer
        {
            core::string_view const encoded = "Program%20Files";
            char buf[32] = {};
            auto const r = decode(buf, sizeof(buf), encoded);
            BOOST_TEST(r);
            if(r)
            {
                BOOST_TEST_EQ(*r, 13);
                BOOST_TEST_EQ(core::string_view(buf, *r), "Program Files");
            }
        }

        // truncated buffer
        {
            char small[4] = {};
            auto const r = decode(small, sizeof(small), "Program%20Files");
            BOOST_TEST(r);
            if(r)
            {
                BOOST_TEST_EQ(*r, sizeof(small));
                BOOST_TEST_EQ(core::string_view(small, *r), "Prog");
            }
        }

        // plus handling
        {
            encoding_opts opt;
            opt.space_as_plus = true;
            char plus_buf[8] = {};
            auto const r = decode(plus_buf, sizeof(plus_buf), "a+b", opt);
            BOOST_TEST(r);
            if(r)
            {
                BOOST_TEST_EQ(*r, 3);
                BOOST_TEST_EQ(core::string_view(plus_buf, *r), "a b");
            }
        }

        // plain percent sequence
        {
            char checked[16] = {};
            auto const r = decode(checked, sizeof(checked), "ready%21");
            BOOST_TEST(r);
            if(r)
                BOOST_TEST_EQ(core::string_view(checked, *r), "ready!");
        }

        // incomplete escape
        {
            char checked[16] = {};
            auto const r = decode(checked, sizeof(checked), "oops%2");
            BOOST_TEST(r.error() == error::incomplete_encoding);
        }
    }

    void
    testDecodeTokens()
    {
        // default token
        {
            auto const r = decode("user%3Dboost");
            BOOST_TEST(r);
            if(r)
                BOOST_TEST_EQ(*r, "user=boost");
        }

        // token plus handling
        {
            encoding_opts opt;
            opt.space_as_plus = true;
            auto const r = decode("a+b", opt);
            BOOST_TEST(r);
            if(r)
                BOOST_TEST_EQ(*r, "a b");
        }

        // explicit std::string token
        {
            auto r = decode("plan%3Dgold");
            BOOST_TEST(r);
            if(r)
                BOOST_TEST_EQ(*r, "plan=gold");
        }

        // token error
        {
            auto r = decode("bad%X");
            BOOST_TEST(! r);
            if(! r)
                BOOST_TEST(r.error() == error::incomplete_encoding);
        }
    }

    void
    testDocExamples()
    {
        // docs decoded_size example
        {
            auto const r = decoded_size("My%20Stuff");
            BOOST_TEST(r);
            if(r)
                BOOST_TEST_EQ(*r, 8);
        }

        // docs buffer example
        {
            char buf[100];
            auto const r = decode(buf, sizeof(buf), "Program%20Files");
            BOOST_TEST(r);
            if(r)
            {
                BOOST_TEST_EQ(*r, 13);
                BOOST_TEST_EQ(core::string_view(buf, *r), "Program Files");
            }
        }

        // docs token example
        {
            auto const r = decode("My%20Stuff");
            BOOST_TEST(r);
            if(r)
                BOOST_TEST_EQ(*r, "My Stuff");
        }
    }

    void
    testDecodeNoexcept()
    {
        // decode() token overload must not be
        // noexcept (noexcept removal regression)
        {
            static_assert(
                !noexcept(decode("x")),
                "");
        }
    }

    void
    run()
    {
        testDecodedSize();
        testDecodeBuffer();
        testDecodeTokens();
        testDecodeNoexcept();
        testDocExamples();
    }
};

TEST_SUITE(decode_test, "boost.url.decode");

} // urls
} // boost
