//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_reverse.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_reverse_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   segmented_reverse(sv.begin(), sv.end());

   int expected[] = {9, 8, 7, 6, 5, 4, 3, 2, 1};
   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(int i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(*it, expected[i]);
}

void test_reverse_single_element()
{
   test_detail::seg_vector<int> sv;
   int a[] = {42};
   sv.add_segment_range(a, a + 1);

   segmented_reverse(sv.begin(), sv.end());
   BOOST_TEST_EQ(*sv.begin(), 42);
}

void test_reverse_empty()
{
   test_detail::seg_vector<int> sv;
   segmented_reverse(sv.begin(), sv.end());
   BOOST_TEST_EQ(sv.total_size(), 0u);
}

void test_reverse_even_count()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2};
   int a2[] = {3, 4};
   sv.add_segment_range(a1, a1 + 2);
   sv.add_segment_range(a2, a2 + 2);

   segmented_reverse(sv.begin(), sv.end());

   int expected[] = {4, 3, 2, 1};
   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(int i = 0; i < 4; ++i, ++it)
      BOOST_TEST_EQ(*it, expected[i]);
}

void test_reverse_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1); v.push_back(2); v.push_back(3); v.push_back(4); v.push_back(5);

   segmented_reverse(v.begin(), v.end());

   BOOST_TEST_EQ(v[0], 5);
   BOOST_TEST_EQ(v[1], 4);
   BOOST_TEST_EQ(v[2], 3);
   BOOST_TEST_EQ(v[3], 2);
   BOOST_TEST_EQ(v[4], 1);
}

void test_reverse_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   segmented_reverse(sv2.begin(), sv2.end());

   int expected[] = {9, 8, 7, 6, 5, 4, 3, 2, 1};
   test_detail::seg2_vector<int>::iterator it = sv2.begin();
   for(int i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(*it, expected[i]);
}

void test_special_segment_conditions()
{
   {  //Test consuming both segments at the same time with no middle segment
      test_detail::seg2_vector<int> sv2;
      int a1[] = { 1, 2, 3 };
      int a2[] = { 4, 5, 6 };
      int a3[] = { 7, 8, 9 };
      int a4[] = { 10, 11, 12 };
      sv2.add_flat_segment_range(a1, a1 + 3);
      sv2.add_flat_segment_range(a2, a2 + 3);
      sv2.add_flat_segment_range(a3, a3 + 3);
      sv2.add_flat_segment_range(a4, a4 + 3);

      segmented_reverse(sv2.begin(), sv2.end());

      int expected[] = { 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };
      test_detail::seg2_vector<int>::iterator it = sv2.begin();
      for (int i = 0; i < 9; ++i, ++it)
         BOOST_TEST_EQ(*it, expected[i]);
   }

   {  //Test consuming both segments with a single in the middle
      test_detail::seg2_vector<int> sv2;
      int a1[] = { 1, 2, 3 };
      int a2[] = { 4, 5, 6 };
      int a3[] = { 7, 8, 9 };
      sv2.add_flat_segment_range(a1, a1 + 3);
      sv2.add_flat_segment_range(a2, a2 + 3);
      sv2.add_flat_segment_range(a3, a3 + 3);

      segmented_reverse(sv2.begin(), sv2.end());

      int expected[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1 };
      test_detail::seg2_vector<int>::iterator it = sv2.begin();
      for (int i = 0; i < 9; ++i, ++it)
         BOOST_TEST_EQ(*it, expected[i]);
   }
   {  //Asymetric comsumption of segments
      test_detail::seg2_vector<int> sv2;
      int a1[] = { 1, 2, 3, 4 };
      int a2[] = { 5, 6, 7, 8 };
      int a3[] = { 9, 10, 11 };
      int a4[] = { 12, 13 };
      int a5[] = { 14, 15,16 };
      sv2.add_flat_segment_range(a1, a1 + 4);
      sv2.add_flat_segment_range(a2, a2 + 4);
      sv2.add_flat_segment_range(a3, a3 + 3);
      sv2.add_flat_segment_range(a4, a4 + 2);
      sv2.add_flat_segment_range(a5, a5 + 3);

      segmented_reverse(sv2.begin(), sv2.end());

      int expected[] = { 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };
      test_detail::seg2_vector<int>::iterator it = sv2.begin();
      for (int i = 0; i < 9; ++i, ++it)
         BOOST_TEST_EQ(*it, expected[i]);
   }
}


void test_reverse_movable_seg()
{
   typedef test_detail::movable_int mi;
   test_detail::seg_vector<mi> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_from_ints(a1, a1 + 3);
   sv.add_segment_from_ints(a2, a2 + 2);
   sv.add_segment_from_ints(a3, a3 + 4);

   segmented_reverse(sv.begin(), sv.end());

   int expected[] = {9, 8, 7, 6, 5, 4, 3, 2, 1};
   test_detail::seg_vector<mi>::iterator it = sv.begin();
   for(int i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(it->value(), expected[i]);
}

void test_reverse_movable_seg2()
{
   typedef test_detail::movable_int mi;
   test_detail::seg2_vector<mi> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv2.add_flat_segment_from_ints(a1, a1 + 3);
   sv2.add_flat_segment_from_ints(a2, a2 + 2);
   sv2.add_flat_segment_from_ints(a3, a3 + 4);

   segmented_reverse(sv2.begin(), sv2.end());

   int expected[] = {9, 8, 7, 6, 5, 4, 3, 2, 1};
   test_detail::seg2_vector<mi>::iterator it = sv2.begin();
   for(int i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(it->value(), expected[i]);
}

int main()
{
   test_reverse_segmented();
   test_reverse_single_element();
   test_reverse_empty();
   test_reverse_even_count();
   test_reverse_non_segmented();
   test_reverse_seg2();
   test_special_segment_conditions();
   test_reverse_movable_seg();
   test_reverse_movable_seg2();
   return boost::report_errors();
}
