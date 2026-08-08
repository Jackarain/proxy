//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_for_each.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

struct summer
{
   int* psum;
   explicit summer(int* p) : psum(p) {}
   void operator()(int x) { *psum += x; }
};

struct doubler
{
   void operator()(int& x) const { x *= 2; }
};

void test_for_each_sum()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);

   int sum = 0;
   segmented_for_each(sv.begin(), sv.end(), summer(&sum));
   BOOST_TEST_EQ(sum, 15);
}

void test_for_each_modify()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(3, 5);
   sv.add_segment(2, 10);

   segmented_for_each(sv.begin(), sv.end(), doubler());

   test_detail::seg_vector<int>::iterator it = sv.begin();
   BOOST_TEST_EQ(*it, 10); ++it;
   BOOST_TEST_EQ(*it, 10); ++it;
   BOOST_TEST_EQ(*it, 10); ++it;
   BOOST_TEST_EQ(*it, 20); ++it;
   BOOST_TEST_EQ(*it, 20);
}

void test_for_each_empty()
{
   test_detail::seg_vector<int> sv;
   int sum = 0;
   segmented_for_each(sv.begin(), sv.end(), summer(&sum));
   BOOST_TEST_EQ(sum, 0);
}

void test_for_each_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1);
   v.push_back(2);
   v.push_back(3);
   int sum = 0;
   segmented_for_each(v.begin(), v.end(), summer(&sum));
   BOOST_TEST_EQ(sum, 6);
}

void test_for_each_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);

   int sum = 0;
   segmented_for_each(sv.begin(), test_detail::make_sentinel(sv.end()), summer(&sum));
   BOOST_TEST_EQ(sum, 15);
}

void test_for_each_sentinel_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1);
   v.push_back(2);
   v.push_back(3);
   int sum = 0;
   segmented_for_each(v.begin(), test_detail::make_sentinel(v.end()), summer(&sum));
   BOOST_TEST_EQ(sum, 6);
}

void test_for_each_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   int sum = 0;
   segmented_for_each(sv2.begin(), sv2.end(), summer(&sum));
   BOOST_TEST_EQ(sum, 45);
}

int main()
{
   test_for_each_sum();
   test_for_each_modify();
   test_for_each_empty();
   test_for_each_non_segmented();
   test_for_each_sentinel_segmented();
   test_for_each_sentinel_non_segmented();
   test_for_each_seg2();
   return boost::report_errors();
}
