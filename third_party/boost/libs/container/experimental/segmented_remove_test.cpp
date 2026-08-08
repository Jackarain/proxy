//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_remove.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_remove_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {2, 4, 2};
   int a3[] = {5, 2};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);
   sv.add_segment_range(a3, a3 + 2);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t new_end = segmented_remove(sv.begin(), sv.end(), 2);

   int expected[] = {1, 3, 4, 5};
   iter_t it = sv.begin();
   for(int i = 0; i < 4; ++i, ++it)
      BOOST_TEST_EQ(*it, expected[i]);
   BOOST_TEST(it == new_end);
}

void test_remove_no_match()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 3, 5};
   sv.add_segment_range(a1, a1 + 3);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t new_end = segmented_remove(sv.begin(), sv.end(), 2);
   BOOST_TEST(new_end == sv.end());
}

void test_remove_all_match()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(3, 2);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t new_end = segmented_remove(sv.begin(), sv.end(), 2);
   BOOST_TEST(new_end == sv.begin());
}

void test_remove_empty()
{
   test_detail::seg_vector<int> sv;
   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t new_end = segmented_remove(sv.begin(), sv.end(), 0);
   BOOST_TEST(new_end == sv.begin());
}

void test_remove_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1); v.push_back(2); v.push_back(3); v.push_back(2); v.push_back(4);

   boost::container::vector<int>::iterator new_end = segmented_remove(v.begin(), v.end(), 2);
   BOOST_TEST_EQ(new_end - v.begin(), 3);
   BOOST_TEST_EQ(v[0], 1);
   BOOST_TEST_EQ(v[1], 3);
   BOOST_TEST_EQ(v[2], 4);
}

void test_remove_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {2, 4, 2};
   int a3[] = {5, 2};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);
   sv.add_segment_range(a3, a3 + 2);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t new_end = segmented_remove(sv.begin(), test_detail::make_sentinel(sv.end()), 2);

   int expected[] = {1, 3, 4, 5};
   iter_t it = sv.begin();
   for(int i = 0; i < 4; ++i, ++it)
      BOOST_TEST_EQ(*it, expected[i]);
   BOOST_TEST(it == new_end);
}

void test_remove_sentinel_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1); v.push_back(2); v.push_back(3); v.push_back(2); v.push_back(4);

   boost::container::vector<int>::iterator new_end = segmented_remove(v.begin(), test_detail::make_sentinel(v.end()), 2);
   BOOST_TEST_EQ(new_end - v.begin(), 3);
   BOOST_TEST_EQ(v[0], 1);
   BOOST_TEST_EQ(v[1], 3);
   BOOST_TEST_EQ(v[2], 4);
}

void test_remove_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {2, 4, 2};
   int a3[] = {5, 2};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 3);
   sv2.add_flat_segment_range(a3, a3 + 2);

   typedef test_detail::seg2_vector<int>::iterator iter_t;
   iter_t new_end = segmented_remove(sv2.begin(), sv2.end(), 2);

   int expected[] = {1, 3, 4, 5};
   iter_t it = sv2.begin();
   for(int i = 0; i < 4; ++i, ++it)
      BOOST_TEST_EQ(*it, expected[i]);
   BOOST_TEST(it == new_end);
}

void test_remove_movable_seg()
{
   typedef test_detail::movable_int mi;
   test_detail::seg_vector<mi> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {2, 4, 2};
   int a3[] = {5, 2};
   sv.add_segment_from_ints(a1, a1 + 3);
   sv.add_segment_from_ints(a2, a2 + 3);
   sv.add_segment_from_ints(a3, a3 + 2);

   typedef test_detail::seg_vector<mi>::iterator iter_t;
   iter_t new_end = segmented_remove(sv.begin(), sv.end(), mi(2));

   int expected[] = {1, 3, 4, 5};
   iter_t it = sv.begin();
   for(int i = 0; i < 4; ++i, ++it)
      BOOST_TEST_EQ(it->value(), expected[i]);
   BOOST_TEST(it == new_end);
}

void test_remove_movable_seg2()
{
   typedef test_detail::movable_int mi;
   test_detail::seg2_vector<mi> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {2, 4, 2};
   int a3[] = {5, 2};
   sv2.add_flat_segment_from_ints(a1, a1 + 3);
   sv2.add_flat_segment_from_ints(a2, a2 + 3);
   sv2.add_flat_segment_from_ints(a3, a3 + 2);

   typedef test_detail::seg2_vector<mi>::iterator iter_t;
   iter_t new_end = segmented_remove(sv2.begin(), sv2.end(), mi(2));

   int expected[] = {1, 3, 4, 5};
   iter_t it = sv2.begin();
   for(int i = 0; i < 4; ++i, ++it)
      BOOST_TEST_EQ(it->value(), expected[i]);
   BOOST_TEST(it == new_end);
}

int main()
{
   test_remove_segmented();
   test_remove_no_match();
   test_remove_all_match();
   test_remove_empty();
   test_remove_non_segmented();
   test_remove_sentinel_segmented();
   test_remove_sentinel_non_segmented();
   test_remove_seg2();
   test_remove_movable_seg();
   test_remove_movable_seg2();
   return boost::report_errors();
}
