// Copyright David Abrahams 2003. Use, modification and distribution is
// subject to the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <deque>
#include <iterator>
#include <cstddef> // std::ptrdiff_t
#include <boost/noncopyable.hpp>
#include <boost/iterator/is_lvalue_iterator.hpp>

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

template <class T>
struct proxy_iterator
{
    typedef T value_type;
    typedef std::output_iterator_tag iterator_category;
    typedef std::ptrdiff_t difference_type;
    typedef T* pointer;
    typedef T& reference;

    struct proxy
    {
        operator value_type&() const;
        proxy& operator=(value_type) const;
    };

    proxy operator*() const;
};

template <class T>
struct lvalue_iterator
{
    typedef T value_type;
    typedef T& reference;
    typedef T difference_type;
    typedef std::input_iterator_tag iterator_category;
    typedef T* pointer;

    T& operator*() const;
    lvalue_iterator& operator++();
    lvalue_iterator operator++(int);
};

template <class T>
struct constant_lvalue_iterator
{
    typedef T value_type;
    typedef T const& reference;
    typedef T difference_type;
    typedef std::input_iterator_tag iterator_category;
    typedef T const* pointer;

    T const& operator*() const;
    constant_lvalue_iterator& operator++();
    constant_lvalue_iterator operator++(int);
};

int main()
{
    static_assert(boost::is_lvalue_iterator<v*>::value,
                  "boost::is_lvalue_iterator<v*>::value is expected to be true.");
    static_assert(boost::is_lvalue_iterator<v const*>::value,
                  "boost::is_lvalue_iterator<v const*>::value is expected to be true.");
    static_assert(boost::is_lvalue_iterator<std::deque<v>::iterator>::value,
                  "boost::is_lvalue_iterator<std::deque<v>::iterator>::value.");
    static_assert(boost::is_lvalue_iterator<std::deque<v>::const_iterator>::value,
                  "boost::is_lvalue_iterator<std::deque<v>::const_iterator>::value is expected to be true.");
    static_assert(!boost::is_lvalue_iterator<std::back_insert_iterator<std::deque<v>>>::value,
                  "boost::is_lvalue_iterator<std::back_insert_iterator<std::deque<v>>>::value is expected to be false.");
    static_assert(!boost::is_lvalue_iterator<std::ostream_iterator<v>>::value,
                  "boost::is_lvalue_iterator<std::ostream_iterator<v>>::value is expected to be false.");
    static_assert(!boost::is_lvalue_iterator<proxy_iterator<v>>::value,
                  "boost::is_lvalue_iterator<proxy_iterator<v>>::value is expected to be false.");
    static_assert(!boost::is_lvalue_iterator<proxy_iterator<int>>::value,
                  "boost::is_lvalue_iterator<proxy_iterator<int>>::value is expected to be false.");
    static_assert(!boost::is_lvalue_iterator<value_iterator>::value,
                  "boost::is_lvalue_iterator<value_iterator>::value is expected to be false.");
    // Make sure inaccessible copy constructor doesn't prevent
    // reference binding
    static_assert(boost::is_lvalue_iterator<noncopyable_iterator>::value,
                  "boost::is_lvalue_iterator<noncopyable_iterator>::value is expected to be true.");

    static_assert(boost::is_lvalue_iterator<lvalue_iterator<v>>::value,
                  "boost::is_lvalue_iterator<lvalue_iterator<v>>::value is expected to be true.");
    static_assert(boost::is_lvalue_iterator<lvalue_iterator<int>>::value,
                  "boost::is_lvalue_iterator<lvalue_iterator<int>>::value is expected to be true.");
    static_assert(boost::is_lvalue_iterator<lvalue_iterator<char*>>::value,
                  "boost::is_lvalue_iterator<lvalue_iterator<char*>>::value is expected to be true.");
    static_assert(boost::is_lvalue_iterator<lvalue_iterator<float>>::value,
                  "boost::is_lvalue_iterator<lvalue_iterator<float>>::value is expected to be true.");


    static_assert(boost::is_lvalue_iterator<constant_lvalue_iterator<v>>::value,
                  "boost::is_lvalue_iterator<constant_lvalue_iterator<v>>::value is expected to be true.");
    static_assert(boost::is_lvalue_iterator<constant_lvalue_iterator<int>>::value,
                  "boost::is_lvalue_iterator<constant_lvalue_iterator<int>>::value is expected to be true.");
    static_assert(boost::is_lvalue_iterator<constant_lvalue_iterator<char*>>::value,
                  "boost::is_lvalue_iterator<constant_lvalue_iterator<char*>>::value is expected to be true.");
    static_assert(boost::is_lvalue_iterator<constant_lvalue_iterator<float>>::value,
                  "boost::is_lvalue_iterator<constant_lvalue_iterator<float>>::value is expected to be true.");



    static_assert(boost::is_non_const_lvalue_iterator<v*>::value,
                  "boost::is_non_const_lvalue_iterator<v*>::value is expected to be true.");
    static_assert(!boost::is_non_const_lvalue_iterator<v const*>::value,
                  "boost::is_non_const_lvalue_iterator<v const*>::value is expected to be false.");
    static_assert(boost::is_non_const_lvalue_iterator<std::deque<v>::iterator>::value,
                  "boost::is_non_const_lvalue_iterator<std::deque<v>::iterator>::value is expected to be true.");
    static_assert(!boost::is_non_const_lvalue_iterator<std::deque<v>::const_iterator>::value,
                  "boost::is_non_const_lvalue_iterator<std::deque<v>::const_iterator>::value is expected to be false.");
    static_assert(!boost::is_non_const_lvalue_iterator<std::back_insert_iterator<std::deque<v>>>::value,
                  "boost::is_non_const_lvalue_iterator<std::back_insert_iterator<std::deque<v>>>::value is expected to be false.");
    static_assert(!boost::is_non_const_lvalue_iterator<std::ostream_iterator<v>>::value,
                  "boost::is_non_const_lvalue_iterator<std::ostream_iterator<v>>::value is expected to be false.");
    static_assert(!boost::is_non_const_lvalue_iterator<proxy_iterator<v>>::value,
                  "boost::is_non_const_lvalue_iterator<proxy_iterator<v>>::value is expected to be false.");
    static_assert(!boost::is_non_const_lvalue_iterator<proxy_iterator<int>>::value,
                  "boost::is_non_const_lvalue_iterator<proxy_iterator<int>>::value is expected to be false.");
    static_assert(!boost::is_non_const_lvalue_iterator<value_iterator>::value,
                  "boost::is_non_const_lvalue_iterator<value_iterator>::value is expected to be false.");
    static_assert(!boost::is_non_const_lvalue_iterator<noncopyable_iterator>::value,
                  "boost::is_non_const_lvalue_iterator<noncopyable_iterator>::value is expected to be false.");

    static_assert(boost::is_non_const_lvalue_iterator<lvalue_iterator<v>>::value,
                  "boost::is_non_const_lvalue_iterator<lvalue_iterator<v>>::value is expected to be true.");
    static_assert(boost::is_non_const_lvalue_iterator<lvalue_iterator<int>>::value,
                  "boost::is_non_const_lvalue_iterator<lvalue_iterator<int>>::value is expected to be true.");
    static_assert(boost::is_non_const_lvalue_iterator<lvalue_iterator<char*>>::value,
                  "boost::is_non_const_lvalue_iterator<lvalue_iterator<char*>>::value is expected to be true.");
    static_assert(boost::is_non_const_lvalue_iterator<lvalue_iterator<float>>::value,
                  "boost::is_non_const_lvalue_iterator<lvalue_iterator<float>>::value is expected to be true.");

    static_assert(!boost::is_non_const_lvalue_iterator<constant_lvalue_iterator<v>>::value,
                  "boost::is_non_const_lvalue_iterator<constant_lvalue_iterator<v>>::value is expected to be false.");
    static_assert(!boost::is_non_const_lvalue_iterator<constant_lvalue_iterator<int>>::value,
                  "boost::is_non_const_lvalue_iterator<constant_lvalue_iterator<int>>::value is expected to be false.");
    static_assert(!boost::is_non_const_lvalue_iterator<constant_lvalue_iterator<char*>>::value,
                  "boost::is_non_const_lvalue_iterator<constant_lvalue_iterator<char*>>::value is expected to be false.");
    static_assert(!boost::is_non_const_lvalue_iterator<constant_lvalue_iterator<float>>::value,
                  "boost::is_non_const_lvalue_iterator<constant_lvalue_iterator<float>>::value is expected to be false.");

    return 0;
}
