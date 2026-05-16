//
// Copyright (c) 2022 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

// Test that header file is self-contained.
#include <boost/url/grammar/recycled.hpp>

#include "test_suite.hpp"
#include <string>

namespace boost {
namespace urls {
namespace grammar {

struct recycled_test
{
    void
    run()
    {
        // basic
        {
            recycled_ptr<std::string> sp;
            sp->reserve(1000);
            BOOST_TEST(sp->capacity() >= 1000);
        }
        {
            recycled_ptr<std::string> sp;
            BOOST_TEST(sp->capacity() >= 1000);
        }

        // shared
        {
            recycled_ptr<std::string> sp;
            auto sp2 = sp;
            sp->reserve(1000);
            BOOST_TEST(sp->capacity() >= 1000);
            BOOST_TEST(sp2->capacity() >= 1000);
        }
        {
            recycled_ptr<std::string> sp;
            recycled_ptr<std::string> sp2(sp);
            BOOST_TEST(sp->capacity() >= 1000);
            BOOST_TEST(sp2->capacity() >= 1000);
        }

        // get() returns nullptr after release
        {
            recycled_ptr<std::string> sp;
            sp->reserve(100);
            sp.release();
            BOOST_TEST(sp.get() == nullptr);
        }

#if defined(__clang__) && defined(__has_warning)
# if __has_warning("-Wself-assign-overloaded") || __has_warning("-Wself-move")
#  pragma clang diagnostic push
#  if __has_warning("-Wself-assign-overloaded")
#   pragma clang diagnostic ignored "-Wself-assign-overloaded"
#  endif
#  if __has_warning("-Wself-move")
#   pragma clang diagnostic ignored "-Wself-move"
#  endif
# endif
#elif defined(__GNUC__) && !defined(__clang__)
# if __GNUC__ >= 13
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wself-move"
# endif
#endif

        // self-assignment preserves state
        {
            recycled_ptr<std::string> sp;
            sp->reserve(100);
            auto cap = sp->capacity();
            sp = sp;
            BOOST_TEST(sp.get() != nullptr);
            BOOST_TEST_EQ(sp->capacity(), cap);
        }

#if defined(__clang__) && defined(__has_warning)
# if __has_warning("-Wself-assign-overloaded") || __has_warning("-Wself-move")
#  pragma clang diagnostic pop
# endif
#elif defined(__GNUC__) && !defined(__clang__)
# if __GNUC__ >= 13
#  pragma GCC diagnostic pop
# endif
#endif

        // coverage
        {
            implementation_defined::recycled_add_impl(1);
            implementation_defined::recycled_remove_impl(1);
        }
    }
};

TEST_SUITE(
    recycled_test,
    "boost.url.grammar.recycled");

} // grammar
} // urls
} // boost
