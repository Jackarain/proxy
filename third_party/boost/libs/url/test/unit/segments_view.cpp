//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

// Test that header file is self-contained.
#include <boost/url/segments_view.hpp>

#include <boost/url/parse.hpp>
#include <boost/url/parse_path.hpp>
#include <boost/url/url.hpp>
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
        segments_view>::value);

BOOST_CORE_STATIC_ASSERT(
    std::is_copy_constructible<
        segments_view>::value);

BOOST_CORE_STATIC_ASSERT(
    std::is_copy_assignable<
        segments_view>::value);

BOOST_CORE_STATIC_ASSERT(
    std::is_default_constructible<
        segments_view::iterator>::value);

struct segments_view_test
{
    void
    testSpecialMembers()
    {
        // segments_view()
        {
            segments_view ps;
            BOOST_TEST(ps.empty());
            BOOST_TEST(! ps.is_absolute());
            BOOST_TEST_EQ(ps.buffer(), "");
            BOOST_TEST_EQ(ps.size(), 0);
        }

        // segments_view(segments_view)
        {
            segments_view ps0 =
                parse_path("/path/to/file.txt").value();
            segments_view ps1(ps0);
            BOOST_TEST_EQ(
                ps0.buffer().data(),
                ps1.buffer().data());
        }

        // segments_view(core::string_view)
        {
            try
            {
                core::string_view s = "/path/to/file.txt";
                segments_view ps(s);
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
            BOOST_TEST_THROWS(segments_view("?"), system::system_error);

            // invalid percent-escape
            BOOST_TEST_THROWS(segments_view("%"), system::system_error);
            BOOST_TEST_THROWS(segments_view("%F"), system::system_error);
            BOOST_TEST_THROWS(segments_view("%FX"), system::system_error);
            BOOST_TEST_THROWS(segments_view("%%"), system::system_error);
            BOOST_TEST_THROWS(segments_view("FA%"), system::system_error);
        }

        // operator=(segments_view)
        {
            segments_view ps0("/path/to/file.txt");
            segments_view ps1("/index.htm");
            ps0 = ps1;
            BOOST_TEST_EQ(
                ps0.buffer().data(),
                ps1.buffer().data());
        }

        // ostream
        {
            segments_view ps = parse_path(
                "/path/to/file.txt").value();
            std::stringstream ss;
            ss << ps;
            BOOST_TEST_EQ(ss.str(),
                "/path/to/file.txt");
        }
    }

    void
    testRangeCtor()
    {
        // full slice equals original (absolute path)
        {
            segments_view ps = parse_path("/a/b/c").value();
            segments_view sub(ps.begin(), ps.end());

            BOOST_TEST_EQ(sub.size(), 3u);
            BOOST_TEST(sub.is_absolute());
            BOOST_TEST_EQ(sub.buffer(), "/a/b/c");
            // alias same storage
            BOOST_TEST_EQ(sub.buffer().data(), ps.buffer().data());
        }

        // full slice equals original (relative path)
        {
            segments_view ps = parse_path("a/b/c").value();
            segments_view sub(ps.begin(), ps.end());

            BOOST_TEST_EQ(sub.size(), 3u);
            BOOST_TEST(!sub.is_absolute());
            BOOST_TEST_EQ(sub.buffer(), "a/b/c");
            // alias same storage
            BOOST_TEST_EQ(sub.buffer().data(), ps.buffer().data());
        }

        // drop first segment: start at index 1 (retain separator)
        {
            segments_view ps = parse_path("/a/b/c").value();
            auto first = std::next(ps.begin());
            auto last = ps.end();
            segments_view sub(first, last);

            BOOST_TEST_EQ(sub.size(), 2u);
            BOOST_TEST(sub.is_absolute());
            BOOST_TEST_EQ(sub.buffer(), "/b/c");

            auto it = sub.begin();
            BOOST_TEST_EQ(*it++, "b");
            BOOST_TEST_EQ(*it++, "c");
            BOOST_TEST(it == sub.end());
        }

        // take prefix without the last segment:
        // [begin, prev(end)) -> "/a/b"
        {
            segments_view ps = parse_path("/a/b/c").value();
            auto first = ps.begin();
            auto last  = std::prev(ps.end());
            segments_view sub(first, last);

            BOOST_TEST_EQ(sub.size(), 2u);
            BOOST_TEST(sub.is_absolute());
            BOOST_TEST_EQ(sub.buffer(), "/a/b");

            auto it = sub.begin();
            BOOST_TEST_EQ(*it++, "a");
            BOOST_TEST_EQ(*it++, "b");
            BOOST_TEST(it == sub.end());
        }

        // single segment in the middle:
        // ["b", past "b") -> "/b"
        {
            segments_view ps = parse_path("/a/b/c").value();
            auto b = std::next(ps.begin());
            auto e = std::next(b);
            segments_view sub(b, e);

            BOOST_TEST_EQ(sub.size(), 1u);
            BOOST_TEST(sub.is_absolute());
            BOOST_TEST_EQ(sub.buffer(), "/b");
            BOOST_TEST_EQ(*sub.begin(), "b");
        }

        // relative path:
        // subranges not starting at begin become absolute
        {
            segments_view ps = parse_path("a/b/c").value();

            // full slice
            segments_view sub1(ps.begin(), ps.end());
            BOOST_TEST_EQ(sub1.size(), 3u);
            BOOST_TEST(!sub1.is_absolute());
            BOOST_TEST_EQ(sub1.buffer(), "a/b/c");

            // prefix [begin, prev(end)) -> "a/b"
            segments_view sub2(ps.begin(), std::prev(ps.end()));
            BOOST_TEST_EQ(sub2.size(), 2u);
            BOOST_TEST(!sub2.is_absolute());
            BOOST_TEST_EQ(sub2.buffer(), "a/b");

            // middle one ["b", past "b") -> "/b"
            auto b = std::next(ps.begin());
            segments_view sub3(b, std::next(b));
            BOOST_TEST_EQ(sub3.size(), 1u);
            BOOST_TEST(sub3.is_absolute());
            BOOST_TEST_EQ(sub3.buffer(), "/b");

            // suffix [next(begin), end) -> "/b/c"
            segments_view sub4(std::next(ps.begin()), ps.end());
            BOOST_TEST_EQ(sub4.size(), 2u);
            BOOST_TEST(sub4.is_absolute());
            BOOST_TEST_EQ(sub4.buffer(), "/b/c");

            // concatenating adjoining subviews recreates original text
            segments_view first_seg(ps.begin(), std::next(ps.begin()));
            std::string reconstructed(
                first_seg.buffer().data(),
                first_seg.buffer().size());
            reconstructed.append(
                sub4.buffer().data(),
                sub4.buffer().size());
            BOOST_TEST_EQ(reconstructed, ps.buffer());
            BOOST_TEST_EQ(
                first_seg.buffer().decoded_size() +
                    sub4.buffer().decoded_size(),
                ps.buffer().decoded_size());
        }

        // empty subrange [it, it):
        // empty buffer, not absolute
        {
            segments_view ps = parse_path("/a/b").value();
            auto it = ps.begin();
            segments_view sub(it, it);

            BOOST_TEST_EQ(sub.size(), 0u);
            BOOST_TEST(!sub.is_absolute());
            BOOST_TEST_EQ(sub.buffer(), "");
            BOOST_TEST(sub.begin() == sub.end());
        }

        // empty subrange from relative source
        {
            segments_view ps = parse_path("a/b").value();
            auto it = ps.begin();
            segments_view sub(it, it);

            BOOST_TEST_EQ(sub.size(), 0u);
            BOOST_TEST(!sub.is_absolute());
            BOOST_TEST_EQ(sub.buffer(), "");
            BOOST_TEST(sub.begin() == sub.end());
        }

        // percent-encoding:
        // slice of first encoded segment only, absolute start
        {
            segments_view ps = parse_path("/a%2Fb/c").value();
            auto first = ps.begin();
            auto last  = std::next(ps.begin());
            segments_view sub(first, last);

            // encoded in buffer
            BOOST_TEST(sub.is_absolute());
            BOOST_TEST_EQ(sub.size(), 1u);
            BOOST_TEST_EQ(sub.buffer(), "/a%2Fb");

            // decoded on deref
            auto it = sub.begin();
            BOOST_TEST_EQ(*it++, "a/b");
            BOOST_TEST(it == sub.end());
        }

        // aliasing: url_view -> segments_view -> subrange
        {
            url_view u("/x/y");
            segments_view ps = u.segments();
            segments_view sub(ps.begin(), ps.end());

            BOOST_TEST_EQ(sub.buffer().data(), u.buffer().data());
            BOOST_TEST_EQ(sub.buffer(), "/x/y");
            BOOST_TEST(sub.is_absolute());
            BOOST_TEST_EQ(sub.size(), 2u);
        }

        // empty prefix of absolute path:
        // [begin, begin) -> empty
        {
            segments_view ps = parse_path("/a/b/c").value();
            segments_view sub(ps.begin(), ps.begin());
            BOOST_TEST_EQ(sub.size(), 0u);
            BOOST_TEST_EQ(sub.buffer(), "");
            BOOST_TEST(!sub.is_absolute());
        }

        // empty subrange in the middle (absolute):
        // ["b","b") -> "", not absolute
        {
            segments_view ps = parse_path("/a/b/c").value();
            auto b = std::next(ps.begin());
            segments_view sub(b, b);

            BOOST_TEST_EQ(sub.size(), 0u);
            BOOST_TEST_EQ(sub.buffer(), "");
            BOOST_TEST(!sub.is_absolute());
            BOOST_TEST(sub.begin() == sub.end());
        }

        // empty subrange in the middle (relative):
        // ["b","b") -> "", not absolute
        {
            segments_view ps = parse_path("a/b/c").value();
            auto b = std::next(ps.begin()); // "b"
            segments_view sub(b, b);

            BOOST_TEST_EQ(sub.size(), 0u);
            BOOST_TEST_EQ(sub.buffer(), "");
            BOOST_TEST(!sub.is_absolute());
            BOOST_TEST(sub.begin() == sub.end());
        }

        // empty subrange at end:
        // [end,end) -> "", not absolute
        {
            segments_view ps = parse_path("/a/b/c").value();
            auto e = ps.end();
            segments_view sub(e, e);

            BOOST_TEST_EQ(sub.size(), 0u);
            BOOST_TEST_EQ(sub.buffer(), "");
            BOOST_TEST(!sub.is_absolute());
        }

        // single-segment absolute path full slice: "/a"
        {
            segments_view ps = parse_path("/a").value();
            segments_view sub(ps.begin(), ps.end());

            BOOST_TEST_EQ(sub.size(), 1u);
            BOOST_TEST(sub.is_absolute());
            BOOST_TEST_EQ(sub.buffer(), "/a");

            auto it = sub.begin();
            BOOST_TEST_EQ(*it++, "a");
            BOOST_TEST(it == sub.end());
        }

        // middle slice using last.pos path:
        // ["b", prev(end)) on "/a/b/c" -> "/b"
        {
            segments_view ps = parse_path("/a/b/c").value();
            auto b = std::next(ps.begin());
            auto last = std::prev(ps.end());
            segments_view sub(b, last);

            BOOST_TEST_EQ(sub.size(), 1u);
            BOOST_TEST(sub.is_absolute());
            BOOST_TEST_EQ(sub.buffer(), "/b");
            BOOST_TEST_EQ(*sub.begin(), "b");
        }
    }

    void
    testJavadocs()
    {
        // {class}
        {
    url_view u( "/path/to/file.txt" );

    segments_view ps = u.segments();

    assert( ps.buffer().data() == u.buffer().data() );

    ignore_unused(ps);
        }
    }

    void
    run()
    {
        testSpecialMembers();
        testRangeCtor();
        testJavadocs();
    }
};

TEST_SUITE(
    segments_view_test,
    "boost.url.segments_view");

} // urls
} // boost
