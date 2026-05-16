//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2025. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/detail/is_constructible.hpp>
#include <boost/container/vector.hpp>

struct Point {
    int x, y;
    Point();
    Point(int);
    Point(int, int);
    Point(int, int, int);
    Point(int, int, int, int);
    Point(int, int, int, int, int, int, int, int);
};

struct Point2
{
    Point2(int*);
};

int main()
{
    // 0 args
   BOOST_CONTAINER_STATIC_ASSERT((true ==boost::container::is_constructible<Point>::value));
   BOOST_CONTAINER_STATIC_ASSERT((true ==boost::container::is_constructible<boost::container::vector<int> >::value));
   BOOST_CONTAINER_STATIC_ASSERT((false ==boost::container::is_constructible<Point2>::value));

    // 1 args
   BOOST_CONTAINER_STATIC_ASSERT((true ==boost::container::is_constructible<Point>::value));
   BOOST_CONTAINER_STATIC_ASSERT((true ==boost::container::is_constructible<boost::container::vector<int>, std::size_t>::value));
   BOOST_CONTAINER_STATIC_ASSERT((false ==boost::container::is_constructible<boost::container::vector<int>, int*>::value));
   BOOST_CONTAINER_STATIC_ASSERT((false ==boost::container::is_constructible<Point, int*>::value));
   BOOST_CONTAINER_STATIC_ASSERT((false ==boost::container::is_constructible<Point2, int>::value));

    // 2 args
   BOOST_CONTAINER_STATIC_ASSERT((true ==boost::container::is_constructible<Point, int, int>::value));
   BOOST_CONTAINER_STATIC_ASSERT((true ==boost::container::is_constructible<boost::container::vector<int>, size_t, int>::value));
   BOOST_CONTAINER_STATIC_ASSERT((false ==boost::container::is_constructible<Point, int*, int*>::value));

    // 3 args
   BOOST_CONTAINER_STATIC_ASSERT((true ==boost::container::is_constructible<Point, int, int, int>::value));
   BOOST_CONTAINER_STATIC_ASSERT((true ==boost::container::is_constructible<boost::container::vector<int>, int*, int*, boost::container::vector<int>::allocator_type>::value));
   BOOST_CONTAINER_STATIC_ASSERT((false ==boost::container::is_constructible<Point, int, int, int*>::value));

    // 4 args
   BOOST_CONTAINER_STATIC_ASSERT((true ==boost::container::is_constructible<Point, int, int, int, int>::value));
   BOOST_CONTAINER_STATIC_ASSERT((false ==boost::container::is_constructible<Point, int, int, int, int*>::value));

    // 8 args
   BOOST_CONTAINER_STATIC_ASSERT((true ==boost::container::is_constructible<Point, int, int, int, int, int, int, int, int>::value));
   BOOST_CONTAINER_STATIC_ASSERT((false ==boost::container::is_constructible<Point, int, int, int, int*, int, int, int, int>::value));

   return 0;
}

