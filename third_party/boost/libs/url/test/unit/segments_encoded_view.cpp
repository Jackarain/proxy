//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

// Test that header file is self-contained.
#include <boost/url/segments_encoded_view.hpp>

#include <boost/url/parse.hpp>
#include <boost/url/parse_path.hpp>
#include <boost/url/url_view.hpp>
#include <boost/core/detail/static_assert.hpp>
#include <boost/core/ignore_unused.hpp>

#include "test_suite.hpp"

#include <sstream>
#include <string>

#ifdef assert
#undef assert
#endif
#define assert BOOST_TEST

namespace boost {
namespace urls {

BOOST_CORE_STATIC_ASSERT(
    std::is_default_constructible<
        segments_encoded_view>::value);

BOOST_CORE_STATIC_ASSERT(
    std::is_copy_constructible<
        segments_encoded_view>::value);

BOOST_CORE_STATIC_ASSERT(
    std::is_copy_assignable<
        segments_encoded_view>::value);

BOOST_CORE_STATIC_ASSERT(
    std::is_default_constructible<
        segments_encoded_view::iterator>::value);

struct segments_const_encoded_view_test
{
    void
    testSpecialMembers()
    {
        // segments_encoded_view()
        {
            segments_encoded_view ps;
            BOOST_TEST(ps.empty());
            BOOST_TEST(! ps.is_absolute());
            BOOST_TEST_EQ(ps.buffer(), "");
            BOOST_TEST_EQ(ps.size(), 0);
        }

        // segments_encoded_view(segments_encoded_view)
        {
            segments_encoded_view ps0 =
                parse_path("/path/to/file.txt").value();
            segments_encoded_view ps1(ps0);
            BOOST_TEST_EQ(
                ps0.buffer().data(),
                ps1.buffer().data());
        }

        // segments_encoded_view(core::string_view)
        {
            try
            {
                core::string_view s = "/path/to/file.txt";
                segments_encoded_view ps(s);
                BOOST_TEST_PASS();
                BOOST_TEST_EQ(
                    ps.buffer().data(), s.data());
                BOOST_TEST_EQ(ps.buffer(), s);
            }
            catch(std::exception const&)
            {
                BOOST_TEST_FAIL();
            }

            // reserved character
            BOOST_TEST_THROWS(segments_encoded_view("?"), system::system_error);

            // invalid percent-escape
            BOOST_TEST_THROWS(segments_encoded_view("%"), system::system_error);
            BOOST_TEST_THROWS(segments_encoded_view("%F"), system::system_error);
            BOOST_TEST_THROWS(segments_encoded_view("%FX"), system::system_error);
            BOOST_TEST_THROWS(segments_encoded_view("%%"), system::system_error);
            BOOST_TEST_THROWS(segments_encoded_view("FA%"), system::system_error);
        }

        // segments_encoded_view(iterator, iterator)
        {
            segments_encoded_view ps = parse_path("/a/b/c").value();
            auto first = std::next(ps.begin());
            auto last  = ps.end();
            segments_encoded_view sub(first, last);

            BOOST_TEST_EQ(sub.size(), 2u);
            BOOST_TEST(sub.is_absolute());
            BOOST_TEST_EQ(sub.buffer(), "/b/c");

            auto it = sub.begin();
            BOOST_TEST_EQ(*it++, "b");
            BOOST_TEST_EQ(*it++, "c");
            BOOST_TEST(it == sub.end());
        }

        // relative source: mid slices become absolute
        {
            segments_encoded_view ps = parse_path("a/b/c").value();

            segments_encoded_view head(ps.begin(), std::next(ps.begin()));
            BOOST_TEST(!head.is_absolute());
            BOOST_TEST_EQ(head.buffer(), "a");

            segments_encoded_view tail(std::next(ps.begin()), ps.end());
            BOOST_TEST(tail.is_absolute());
            BOOST_TEST_EQ(tail.buffer(), "/b/c");

            std::string rebuilt(
                head.buffer().data(),
                head.buffer().size());
            rebuilt.append(
                tail.buffer().data(),
                tail.buffer().size());
            BOOST_TEST_EQ(rebuilt, ps.buffer());
            BOOST_TEST_EQ(
                head.buffer().decoded_size() +
                    tail.buffer().decoded_size(),
                ps.buffer().decoded_size());
        }

        // operator=(segments_encoded_view)
        {
            segments_encoded_view ps0("/path/to/file.txt");
            segments_encoded_view ps1("/index.htm");
            ps0 = ps1;
            BOOST_TEST_EQ(
                ps0.buffer().data(),
                ps1.buffer().data());
        }

        // operator segments_view()
        {
            segments_encoded_view ps0 =
                parse_path( "/path/to/file.txt" ).value();
            segments_view ps1(ps0);
            BOOST_TEST_EQ(
                ps0.buffer().data(),
                ps1.buffer().data());
        }

        // ostream
        {
            segments_encoded_view ps = parse_path(
                "/path/to/file.txt").value();
            std::stringstream ss;
            ss << ps;
            BOOST_TEST_EQ(ss.str(),
                "/path/to/file.txt");
        }
    }

    void
    testJavadocs()
    {
        // {class}
        {
    url_view u( "/path/to/file.txt" );

    segments_encoded_view ps = u.encoded_segments();

    assert( ps.buffer().data() == u.buffer().data() );

    ignore_unused(ps);
        }

        // operator segments_view()
        {
        segments_view ps = parse_path( "/path/to/file.txt" ).value();

        ignore_unused(ps);
        }
    }

    void
    run()
    {
        testSpecialMembers();
        testJavadocs();
    }
};

TEST_SUITE(
    segments_const_encoded_view_test,
    "boost.url.segments_encoded_view");

} // urls
} // boost
