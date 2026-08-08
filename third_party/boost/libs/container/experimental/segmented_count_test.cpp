//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_count.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_count_basic()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 1};
   int a2[] = {3, 1, 4};
   int a3[] = {1, 5};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);
   sv.add_segment_range(a3, a3 + 2);

   BOOST_TEST_EQ(segmented_count(sv.begin(), sv.end(), 1), 4);
   BOOST_TEST_EQ(segmented_count(sv.begin(), sv.end(), 3), 1);
   BOOST_TEST_EQ(segmented_count(sv.begin(), sv.end(), 99), 0);
}

void test_count_empty()
{
   test_detail::seg_vector<int> sv;
   BOOST_TEST_EQ(segmented_count(sv.begin(), sv.end(), 0), 0);
}

void test_count_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1);
   v.push_back(2);
   v.push_back(1);
   v.push_back(3);
   v.push_back(1);

   BOOST_TEST_EQ(segmented_count(v.begin(), v.end(), 1), 3);
}

void test_count_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 1};
   int a2[] = {3, 1, 4};
   int a3[] = {1, 5};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);
   sv.add_segment_range(a3, a3 + 2);

   BOOST_TEST_EQ(segmented_count(sv.begin(), test_detail::make_sentinel(sv.end()), 1), 4);
   BOOST_TEST_EQ(segmented_count(sv.begin(), test_detail::make_sentinel(sv.end()), 99), 0);
}

void test_count_sentinel_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1);
   v.push_back(2);
   v.push_back(1);
   v.push_back(3);
   v.push_back(1);

   BOOST_TEST_EQ(segmented_count(v.begin(), test_detail::make_sentinel(v.end()), 1), 3);
}

void test_count_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 1};
   int a2[] = {3, 1, 4};
   int a3[] = {1, 5};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 3);
   sv2.add_flat_segment_range(a3, a3 + 2);

   BOOST_TEST_EQ(segmented_count(sv2.begin(), sv2.end(), 1), 4);
   BOOST_TEST_EQ(segmented_count(sv2.begin(), sv2.end(), 3), 1);
   BOOST_TEST_EQ(segmented_count(sv2.begin(), sv2.end(), 99), 0);
}

int main()
{
   test_count_basic();
   test_count_empty();
   test_count_non_segmented();
   test_count_sentinel_segmented();
   test_count_sentinel_non_segmented();
   test_count_seg2();
   return boost::report_errors();
}
