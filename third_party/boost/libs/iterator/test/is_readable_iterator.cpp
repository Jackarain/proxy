// Copyright David Abrahams 2003. Use, modification and distribution is
// subject to the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <deque>
#include <iterator>
#include <cstddef> // std::ptrdiff_t
#include <boost/noncopyable.hpp>
#include <boost/iterator/is_readable_iterator.hpp>

struct v
{
    v();
    ~v();
};


struct value_iterator
{
    typedef std::input_iterator_tag iterator_category;
    typedef v value_type;
    typedef std::ptrdiff_t difference_type;
    typedef v* pointer;
    typedef v& reference;

    v operator*() const;
};

struct noncopyable_iterator
{
    typedef std::forward_iterator_tag iterator_category;
    typedef boost::noncopyable value_type;
    typedef std::ptrdiff_t difference_type;
    typedef boost::noncopyable* pointer;
    typedef boost::noncopyable& reference;

    boost::noncopyable const& operator*() const;
};

struct proxy_iterator
{
    typedef std::output_iterator_tag iterator_category;
    typedef v value_type;
    typedef std::ptrdiff_t difference_type;
    typedef v* pointer;
    typedef v& reference;

    struct proxy
    {
        operator v&();
        proxy& operator=(v) const;
    };

    proxy operator*() const;
};

struct proxy_iterator2
{
    typedef std::output_iterator_tag iterator_category;
    typedef v value_type;
    typedef std::ptrdiff_t difference_type;
    typedef v* pointer;
    typedef v& reference;

    struct proxy
    {
        proxy& operator=(v) const;
    };

    proxy operator*() const;
};


int main()
{
    static_assert(boost::is_readable_iterator<v*>::value,
                  "boost::is_readable_iterator<v*>::value is expected to be true.");
    static_assert(boost::is_readable_iterator<v const*>::value,
                  "boost::is_readable_iterator<v const*>::value is expected to be true.");
    static_assert(boost::is_readable_iterator<std::deque<v>::iterator>::value,
                  "boost::is_readable_iterator<std::deque<v>::iterator>::value is expected to be true.");
    static_assert(boost::is_readable_iterator<std::deque<v>::const_iterator>::value,
                  "boost::is_readable_iterator<std::deque<v>::const_iterator>::value is expected to be true.");
    static_assert(!boost::is_readable_iterator<std::back_insert_iterator<std::deque<v>>>::value,
                  "boost::is_readable_iterator<std::back_insert_iterator<std::deque<v>>>::value is expected to be false.");
    static_assert(!boost::is_readable_iterator<std::ostream_iterator<v>>::value,
                  "boost::is_readable_iterator<std::ostream_iterator<v>>::value is expected to be false.");
    static_assert(boost::is_readable_iterator<proxy_iterator>::value,
                  "boost::is_readable_iterator<proxy_iterator>::value is expected to be true.");
    static_assert(!boost::is_readable_iterator<proxy_iterator2>::value,
                  "boost::is_readable_iterator<proxy_iterator2>::value is expected to be false.");
    static_assert(boost::is_readable_iterator<value_iterator>::value,
                  "boost::is_readable_iterator<value_iterator>::value is expected to be true.");

    // Make sure inaccessible copy constructor doesn't prevent
    // readability
    static_assert(boost::is_readable_iterator<noncopyable_iterator>::value,
                  "boost::is_readable_iterator<noncopyable_iterator>::value is expected to be true.");

    return 0;
}
