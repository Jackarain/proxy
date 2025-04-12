/*
Copyright 2025 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/config.hpp>
#if !defined(BOOST_NO_CXX11_CONSTEXPR)
#include <boost/core/span.hpp>
#include <boost/core/lightweight_test.hpp>

constexpr const int array[4]{ 5, 10, 15, 20 };

void test_first()
{
    constexpr boost::span<const int> s =
        boost::span<const int>(array, 4).first(2);
    BOOST_TEST_EQ(s.data(), &array[0]);
    BOOST_TEST_EQ(s.size(), 2);
}

void test_last()
{
    constexpr boost::span<const int> s =
        boost::span<const int>(array, 4).last(2);
    BOOST_TEST_EQ(s.data(), &array[2]);
    BOOST_TEST_EQ(s.size(), 2);
}

void test_subspan()
{
    constexpr boost::span<const int> s =
        boost::span<const int>(array, 4).subspan(1, 2);
    BOOST_TEST_EQ(s.data(), &array[1]);
    BOOST_TEST_EQ(s.size(), 2);
}

void test_index()
{
    constexpr const int i = boost::span<const int>(array, 4)[1];
    BOOST_TEST_EQ(i, 10);
}

void test_front()
{
    constexpr const int i = boost::span<const int>(array, 4).front();
    BOOST_TEST_EQ(i, 5);
}

void test_back()
{
    constexpr const int i = boost::span<const int>(array, 4).back();
    BOOST_TEST_EQ(i, 20);
}

int main()
{
    test_first();
    test_last();
    test_subspan();
    test_index();
    test_front();
    test_back();
    return boost::report_errors();
}
#else
int main()
{
    return 0;
}
#endif
