//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_find.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_find_present_first_segment()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   test_detail::seg_vector<int>::iterator it = segmented_find(sv.begin(), sv.end(), 2);
   BOOST_TEST(it != sv.end());
   BOOST_TEST_EQ(*it, 2);
}

void test_find_present_second_segment()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   test_detail::seg_vector<int>::iterator it = segmented_find(sv.begin(), sv.end(), 5);
   BOOST_TEST(it != sv.end());
   BOOST_TEST_EQ(*it, 5);
}

void test_find_not_present()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(3, 1);
   sv.add_segment(2, 2);

   test_detail::seg_vector<int>::iterator it = segmented_find(sv.begin(), sv.end(), 99);
   BOOST_TEST(it == sv.end());
}

void test_find_empty()
{
   test_detail::seg_vector<int> sv;
   test_detail::seg_vector<int>::iterator it = segmented_find(sv.begin(), sv.end(), 1);
   BOOST_TEST(it == sv.end());
}

void test_find_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(10);
   v.push_back(20);
   v.push_back(30);

   boost::container::vector<int>::iterator it = segmented_find(v.begin(), v.end(), 20);
   BOOST_TEST(it != v.end());
   BOOST_TEST_EQ(*it, 20);

   it = segmented_find(v.begin(), v.end(), 99);
   BOOST_TEST(it == v.end());
}

void test_find_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   test_detail::seg_vector<int>::iterator it =
      segmented_find(sv.begin(), test_detail::make_sentinel(sv.end()), 5);
   BOOST_TEST(it != sv.end());
   BOOST_TEST_EQ(*it, 5);

   it = segmented_find(sv.begin(), test_detail::make_sentinel(sv.end()), 99);
   BOOST_TEST(it == sv.end());
}

void test_find_sentinel_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(10);
   v.push_back(20);
   v.push_back(30);

   boost::container::vector<int>::iterator it =
      segmented_find(v.begin(), test_detail::make_sentinel(v.end()), 20);
   BOOST_TEST(it != v.end());
   BOOST_TEST_EQ(*it, 20);

   it = segmented_find(v.begin(), test_detail::make_sentinel(v.end()), 99);
   BOOST_TEST(it == v.end());
}

void test_find_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   test_detail::seg2_vector<int>::iterator it = segmented_find(sv2.begin(), sv2.end(), 5);
   BOOST_TEST(it != sv2.end());
   BOOST_TEST_EQ(*it, 5);

   it = segmented_find(sv2.begin(), sv2.end(), 99);
   BOOST_TEST(it == sv2.end());
}

void test_find_every_position()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {10, 20, 30};
   int a2[] = {40, 50};
   int a3[] = {60, 70, 80, 90};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   int vals[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
   const int N = 9;
   typedef test_detail::seg_vector<int>::iterator iter_t;

   iter_t expected = sv.begin();
   for(int i = 0; i < N; ++i, ++expected) {
      iter_t it = segmented_find(sv.begin(), sv.end(), vals[i]);
      BOOST_TEST(it != sv.end());
      BOOST_TEST_EQ(*it, vals[i]);
      BOOST_TEST(it == expected);
   }
   BOOST_TEST(segmented_find(sv.begin(), sv.end(), 999) == sv.end());
}

void test_find_every_position_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {10, 20, 30};
   int a2[] = {40, 50};
   int a3[] = {60, 70, 80, 90};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   int vals[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
   const int N = 9;
   typedef test_detail::seg2_vector<int>::iterator iter_t;

   iter_t expected = sv2.begin();
   for(int i = 0; i < N; ++i, ++expected) {
      iter_t it = segmented_find(sv2.begin(), sv2.end(), vals[i]);
      BOOST_TEST(it != sv2.end());
      BOOST_TEST_EQ(*it, vals[i]);
      BOOST_TEST(it == expected);
   }
   BOOST_TEST(segmented_find(sv2.begin(), sv2.end(), 999) == sv2.end());
}

int main()
{
   test_find_present_first_segment();
   test_find_present_second_segment();
   test_find_not_present();
   test_find_empty();
   test_find_non_segmented();
   test_find_sentinel_segmented();
   test_find_sentinel_non_segmented();
   test_find_seg2();
   test_find_every_position();
   test_find_every_position_seg2();
   return boost::report_errors();
}
