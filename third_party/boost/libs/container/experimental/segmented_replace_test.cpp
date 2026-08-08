//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_replace.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_replace_basic()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 1};
   int a2[] = {3, 1, 4};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   segmented_replace(sv.begin(), sv.end(), 1, 99);

   test_detail::seg_vector<int>::iterator it = sv.begin();
   BOOST_TEST_EQ(*it, 99); ++it;
   BOOST_TEST_EQ(*it, 2);  ++it;
   BOOST_TEST_EQ(*it, 99); ++it;
   BOOST_TEST_EQ(*it, 3);  ++it;
   BOOST_TEST_EQ(*it, 99); ++it;
   BOOST_TEST_EQ(*it, 4);
}

void test_replace_none()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(3, 5);

   segmented_replace(sv.begin(), sv.end(), 1, 99);

   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(; it != sv.end(); ++it)
      BOOST_TEST_EQ(*it, 5);
}

void test_replace_empty()
{
   test_detail::seg_vector<int> sv;
   segmented_replace(sv.begin(), sv.end(), 1, 2);
   BOOST_TEST_EQ(sv.total_size(), 0u);
}

void test_replace_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1);
   v.push_back(2);
   v.push_back(1);
   v.push_back(3);

   segmented_replace(v.begin(), v.end(), 1, 0);
   BOOST_TEST_EQ(v[0], 0);
   BOOST_TEST_EQ(v[1], 2);
   BOOST_TEST_EQ(v[2], 0);
   BOOST_TEST_EQ(v[3], 3);
}

void test_replace_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 1};
   int a2[] = {3, 1, 4};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   segmented_replace(sv.begin(), test_detail::make_sentinel(sv.end()), 1, 99);

   test_detail::seg_vector<int>::iterator it = sv.begin();
   BOOST_TEST_EQ(*it, 99); ++it;
   BOOST_TEST_EQ(*it, 2);  ++it;
   BOOST_TEST_EQ(*it, 99); ++it;
   BOOST_TEST_EQ(*it, 3);  ++it;
   BOOST_TEST_EQ(*it, 99); ++it;
   BOOST_TEST_EQ(*it, 4);
}

void test_replace_sentinel_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1);
   v.push_back(2);
   v.push_back(1);
   v.push_back(3);

   segmented_replace(v.begin(), test_detail::make_sentinel(v.end()), 1, 0);
   BOOST_TEST_EQ(v[0], 0);
   BOOST_TEST_EQ(v[1], 2);
   BOOST_TEST_EQ(v[2], 0);
   BOOST_TEST_EQ(v[3], 3);
}

void test_replace_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {2, 4, 2};
   int a3[] = {5, 2};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 3);
   sv2.add_flat_segment_range(a3, a3 + 2);

   segmented_replace(sv2.begin(), sv2.end(), 2, 99);

   int expected[] = {1, 99, 3, 99, 4, 99, 5, 99};
   test_detail::seg2_vector<int>::iterator it = sv2.begin();
   for(int i = 0; i < 8; ++i, ++it)
      BOOST_TEST_EQ(*it, expected[i]);
}

int main()
{
   test_replace_basic();
   test_replace_none();
   test_replace_empty();
   test_replace_non_segmented();
   test_replace_sentinel_segmented();
   test_replace_sentinel_non_segmented();
   test_replace_seg2();
   return boost::report_errors();
}
