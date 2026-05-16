//
// Copyright (c) 2022 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

// Test that header file is self-contained.
#include <boost/url/decode_view.hpp>

#include <boost/core/ignore_unused.hpp>
#include <sstream>
#include "test_suite.hpp"

namespace boost {
namespace urls {

struct decode_view_test
{
    core::string_view str = "a%20uri+test";
    core::string_view dec_str = "a uri+test";
    core::string_view no_plus_dec_str = "a uri test";
    const std::size_t dn = 10;
    encoding_opts no_plus_opt;

    decode_view_test()
    {
        no_plus_opt.space_as_plus = true;
    }

    void
    testDecodeView()
    {
        // decode_view()
        {
            decode_view s;
            BOOST_TEST_EQ(s, "");
            BOOST_TEST_EQ(s.size(), 0u);
        }

        // decode_view(char const*)
        {
            decode_view s(str.data());
            BOOST_TEST_EQ(s, dec_str);
            BOOST_TEST_EQ(s.size(), dn);
        }

        // decode_view(char const*, bool space_as_plus)
        {
            decode_view
                s(str.data(), no_plus_opt);
            BOOST_TEST_EQ(s, no_plus_dec_str);
            BOOST_TEST_EQ(s.size(), dn);
        }

        // decode_view(core::string_view)
        {
            decode_view s(str);
            BOOST_TEST_EQ(s, dec_str);
            BOOST_TEST_EQ(s.size(), dn);
        }

        // decode_view(core::string_view, bool space_as_plus)
        {
            decode_view s(str, no_plus_opt);
            BOOST_TEST_EQ(s, no_plus_dec_str);
            BOOST_TEST_EQ(s.size(), dn);
        }

#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
        // decode_view(core::string_view)
        {
            std::string_view std_str = str;
            decode_view s(std_str);
            BOOST_TEST_EQ(s, dec_str);
            BOOST_TEST_EQ(s.size(), dn);
        }

        // decode_view(core::string_view, bool space_as_plus)
        {
            std::string_view std_str = str;
            decode_view s(std_str, no_plus_opt);
            BOOST_TEST_EQ(s, no_plus_dec_str);
            BOOST_TEST_EQ(s.size(), dn);
        }
#endif

        // decode_view(core::string_view)
        {
            std::string ss(str);
            decode_view s(ss);
            BOOST_TEST_EQ(s, dec_str);
            BOOST_TEST_EQ(s.size(), dn);
        }

        // decode_view(core::string_view, bool space_as_plus)
        {
            std::string ss(str);
            decode_view s(ss, no_plus_opt);
            BOOST_TEST_EQ(s, no_plus_dec_str);
            BOOST_TEST_EQ(s.size(), dn);
        }
    }

    void
    testIter()
    {
        // begin()
        {
            decode_view s(str);
            BOOST_TEST_EQ(*s.begin(), s.front());
            BOOST_TEST_NE(s.begin(),
                decode_view::iterator{});
        }

        // end()
        {
            decode_view s(str);
            auto l = s.end();
            --l;
            BOOST_TEST_EQ(*l, s.back());
            BOOST_TEST_NE(l,
                decode_view::iterator{});
        }
    }

    void
    testAccessors()
    {
        // front()
        {
            decode_view s(str);
            BOOST_TEST_EQ(s.front(), 'a');
        }

        // back()
        {
            decode_view s(str);
            BOOST_TEST_EQ(s.back(), 't');
        }
    }

    void
    testObservers()
    {
        // size()
        {
            decode_view s(str);
            BOOST_TEST_EQ(s.size(), dn);
        }

        // empty()
        {
            decode_view s;
            BOOST_TEST(s.empty());

            decode_view s2(str);
            BOOST_TEST_NOT(s2.empty());
        }
    }

    void
    testModifiers()
    {
        // remove_prefix()
        {
            decode_view s(str, no_plus_opt);
            s.remove_prefix(2);
            BOOST_TEST_EQ(s, "uri test");
        }

        // remove_prefix() with n == size() (issue #973)
        {
            decode_view s(str, no_plus_opt);
            s.remove_prefix(s.size());
            BOOST_TEST(s.empty());
            BOOST_TEST_EQ(s.size(), 0u);
        }

        // remove_prefix() with n == 0
        {
            decode_view s(str, no_plus_opt);
            s.remove_prefix(0);
            BOOST_TEST_EQ(s, "a uri test");
        }

        // remove_suffix()
        {
            decode_view s(str);
            s.remove_suffix(5);
            BOOST_TEST_EQ(s, "a uri");
        }

        // remove_suffix() with n == size() (issue #973)
        {
            decode_view s(str, no_plus_opt);
            s.remove_suffix(s.size());
            BOOST_TEST(s.empty());
            BOOST_TEST_EQ(s.size(), 0u);
        }

        // remove_suffix() with n == 0
        {
            decode_view s(str, no_plus_opt);
            s.remove_suffix(0);
            BOOST_TEST_EQ(s, "a uri test");
        }
    }

    void
    testOperations()
    {
        // starts_with()
        {
            decode_view s(str);
            BOOST_TEST(s.starts_with("a uri"));
            BOOST_TEST_NOT(s.starts_with("a uri test b"));
            BOOST_TEST(s.starts_with('a'));
            BOOST_TEST_NOT(s.starts_with("a url"));
        }

        // ends_with()
        {
            decode_view s(str, no_plus_opt);
            BOOST_TEST(s.ends_with("uri test"));
            BOOST_TEST_NOT(s.ends_with("b a uri test"));
            BOOST_TEST(s.ends_with('t'));
            BOOST_TEST_NOT(s.ends_with("url test"));
        }

        // ends_with() empty string regression
        {
            BOOST_TEST(decode_view("anything").ends_with(""));
            BOOST_TEST(decode_view("").ends_with(""));
        }

        // find()
        {
            decode_view s(str);
            auto it = s.find('t');
            BOOST_TEST(it != s.end());
            BOOST_TEST_EQ(*it.base(), 't');
        }

        // find()
        {
            decode_view s;
            auto it = s.find('t');
            BOOST_TEST(it == s.end());
        }

        // rfind()
        {
            decode_view s(str);
            auto it = s.rfind('t');
            BOOST_TEST(it != s.end());
            BOOST_TEST_EQ(*it.base(), 't');
            BOOST_TEST_EQ(*s.rfind('i'), 'i');
            it = s.rfind('x');
            BOOST_TEST(it == s.end());
            it = s.rfind('a');
            BOOST_TEST(it == s.begin());
        }

        // rfind()
        {
            decode_view s;
            auto it = s.rfind('t');
            BOOST_TEST(it == s.end());
        }
    }

    void
    testCompare()
    {
        // compare()
        {
            decode_view s(str);
            BOOST_TEST_EQ(s.compare(dec_str), 0);
            BOOST_TEST_EQ(s.compare("a a"), 1);
            BOOST_TEST_EQ(s.compare("a z"), -1);
            std::string bs = "z";
            BOOST_TEST_EQ(s.compare(bs), -1);
        }

        // operators
        {
            decode_view s(str);

            // decode_view
            {
                decode_view s0(str);
                decode_view s1("a%20tri+test");
                decode_view s2("a%20vri+test");
                BOOST_TEST(s == s0);
                BOOST_TEST_NOT(s == s1);
                BOOST_TEST(s != s2);
                BOOST_TEST_NOT(s != s0);
                BOOST_TEST(s < s2);
                BOOST_TEST_NOT(s < s0);
                BOOST_TEST(s <= s2);
                BOOST_TEST(s <= s0);
                BOOST_TEST(s > s1);
                BOOST_TEST_NOT(s > s0);
                BOOST_TEST(s >= s1);
                BOOST_TEST(s >= s0);
            }

            // core::string_view
            {
                core::string_view str0(dec_str);
                core::string_view str1("a tri test");
                core::string_view str2("a vri test");
                BOOST_TEST(s == str0);
                BOOST_TEST_NOT(s == str1);
                BOOST_TEST(s != str2);
                BOOST_TEST_NOT(s != str0);
                BOOST_TEST(s < str2);
                BOOST_TEST_NOT(s < str0);
                BOOST_TEST(s <= str2);
                BOOST_TEST(s <= str0);
                BOOST_TEST(s > str1);
                BOOST_TEST_NOT(s > str0);
                BOOST_TEST(s >= str1);
                BOOST_TEST(s >= str0);
            }

            // string
            {
                std::string bstr0(dec_str);
                std::string bstr1("a tri test");
                std::string bstr2("a vri test");
                BOOST_TEST(s == bstr0);
                BOOST_TEST_NOT(s == bstr1);
                BOOST_TEST(s != bstr2);
                BOOST_TEST_NOT(s != bstr0);
                BOOST_TEST(s < bstr2);
                BOOST_TEST_NOT(s < bstr0);
                BOOST_TEST(s <= bstr2);
                BOOST_TEST(s <= bstr0);
                BOOST_TEST(s > bstr1);
                BOOST_TEST_NOT(s > bstr0);
                BOOST_TEST(s >= bstr1);
                BOOST_TEST(s >= bstr0);
            }


            // string literals
            {
                BOOST_TEST(s == "a uri+test");
                BOOST_TEST_NOT(s == "a tri test");
                BOOST_TEST(s != "a vri test");
                BOOST_TEST_NOT(s != "a uri+test");
                BOOST_TEST(s < "a vri test");
                BOOST_TEST_NOT(s < "a uri test");
                BOOST_TEST(s <= "a vri test");
                BOOST_TEST(s <= "a uri+test");
                BOOST_TEST(s > "a tri test");
                BOOST_TEST_NOT(s > "a uri+test");
                BOOST_TEST(s >= "a tri test");
                BOOST_TEST(s >= "a uri test");
            }

        }
    }

    void
    testStream()
    {
        // operator<<
        {
            std::stringstream ss;
            decode_view  s(str);
            ss << s;
            BOOST_TEST_EQ(ss.str(), dec_str);
        }
    }

    void
    testPR127Cases()
    {
        {
            std::stringstream ss;
            urls::decode_view ds("test+string");
            // no warning about object slicing
            ss << ds;
        }
    }

    void
    testBorrowedRange()
    {
#ifdef BOOST_URL_HAS_CONCEPTS
        // decode_view is a borrowed range
        BOOST_CORE_STATIC_ASSERT(
            std::ranges::borrowed_range<decode_view>);

        // iterators remain valid after the view is destroyed
        decode_view::iterator it;
        {
            decode_view dv("hello%20world");
            it = dv.begin();
        }
        // iterator is still valid (points to external buffer)
        BOOST_TEST_EQ(*it, 'h');
#endif
    }

    void
    testJavadocs()
    {
        // decode_view()
        {
            decode_view ds;

            boost::ignore_unused(ds);
        }

        // decode_view(pct_string_view, encoding_opts)
        {
            decode_view ds( "Program%20Files" );

            boost::ignore_unused(ds);
        }

        // empty()
        {
            BOOST_TEST( decode_view( "" ).empty() );
        }

        // size()
        {
            BOOST_TEST_EQ( decode_view( "Program%20Files" ).size(), 13u );
        }

        // front()
        {
            BOOST_TEST_EQ( decode_view( "Program%20Files" ).front(), 'P' );
        }

        // back()
        {
            BOOST_TEST_EQ( decode_view( "Program%20Files" ).back(), 's' );
        }

        // starts_with(core::string_view)
        {
            BOOST_TEST( decode_view( "Program%20Files" ).starts_with("Program") );
        }

        // ends_with(core::string_view)
        {
            BOOST_TEST( decode_view( "Program%20Files" ).ends_with("Files") );
        }

        // starts_with(char)
        {
            BOOST_TEST( decode_view( "Program%20Files" ).starts_with('P') );
        }

        // ends_with(char)
        {
            BOOST_TEST( decode_view( "Program%20Files" ).ends_with('s') );
        }

        // remove_prefix(size_type)
        {
            decode_view d( "Program%20Files" );
            d.remove_prefix( 8 );
            BOOST_TEST_EQ( d, "Files" );
        }

        // remove_suffix(size_type)
        {
            decode_view d( "Program%20Files" );
            d.remove_suffix( 6 );
            BOOST_TEST_EQ( d, "Program" );
        }
    }

    void
    run()
    {
        testDecodeView();
        testIter();
        testAccessors();
        testObservers();
        testModifiers();
        testOperations();
        testCompare();
        testStream();
        testPR127Cases();
        testBorrowedRange();
        testJavadocs();
    }
};

TEST_SUITE(
    decode_view_test,
    "boost.url.decode_view");

} // urls
} // boost
