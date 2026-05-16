// Signals2 library
// test for move constructible only class as return type

// Copyright Cory Fields 2026

// Use, modification and
// distribution is subject to the Boost Software License, Version
// 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include <boost/signals2/signal.hpp>
#define BOOST_TEST_MODULE connection_test
#include <boost/test/included/unit_test.hpp>

struct moveonly_data
{
    moveonly_data(int data) : m_data(data){}
    moveonly_data(moveonly_data&&) = default;

    // The assignment operator should not be required here
    moveonly_data& operator=(moveonly_data&&) = default;

    moveonly_data(const moveonly_data&) = delete;
    moveonly_data& operator=(const moveonly_data&) = delete;
    int m_data;
};

moveonly_data moveonly_return_callback(int val)
{
    return {val+1};
}

BOOST_AUTO_TEST_CASE(test_main)
{
    boost::signals2::signal<moveonly_data(int)> sig;
    sig.connect(moveonly_return_callback);
    int data{3};
    auto ret = sig(data);
    BOOST_CHECK(ret->m_data == 4);
}
