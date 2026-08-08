//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_swap_ranges.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_swap_ranges_full()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   boost::container::vector<int> other(9, 0);
   for(int i = 0; i < 9; ++i)
      other[static_cast<std::size_t>(i)] = (i + 1) * 10;

   boost::container::vector<int>::iterator result =
      segmented_swap_ranges(sv.begin(), sv.end(), other.begin());

   BOOST_TEST(result == other.end());

   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(int i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(*it, (i + 1) * 10);

   for(int i = 0; i < 9; ++i)
      BOOST_TEST_EQ(other[static_cast<std::size_t>(i)], i + 1);
}

void test_swap_ranges_empty()
{
   test_detail::seg_vector<int> sv;
   boost::container::vector<int> other;
   boost::container::vector<int>::iterator result =
      segmented_swap_ranges(sv.begin(), sv.end(), other.begin());
   BOOST_TEST(result == other.begin());
}

void test_swap_ranges_single_segment()
{
   test_detail::seg_vector<int> sv;
   int a[] = {1, 2, 3};
   sv.add_segment_range(a, a + 3);

   int other[] = {10, 20, 30};
   segmented_swap_ranges(sv.begin(), sv.end(), other);

   test_detail::seg_vector<int>::iterator it = sv.begin();
   BOOST_TEST_EQ(*it, 10); ++it;
   BOOST_TEST_EQ(*it, 20); ++it;
   BOOST_TEST_EQ(*it, 30);

   BOOST_TEST_EQ(other[0], 1);
   BOOST_TEST_EQ(other[1], 2);
   BOOST_TEST_EQ(other[2], 3);
}

void test_swap_ranges_non_segmented()
{
   boost::container::vector<int> v1;
   v1.push_back(1); v1.push_back(2); v1.push_back(3);
   boost::container::vector<int> v2;
   v2.push_back(10); v2.push_back(20); v2.push_back(30);

   boost::container::vector<int>::iterator result =
      segmented_swap_ranges(v1.begin(), v1.end(), v2.begin());
   BOOST_TEST(result == v2.end());

   BOOST_TEST_EQ(v1[0], 10);
   BOOST_TEST_EQ(v1[1], 20);
   BOOST_TEST_EQ(v1[2], 30);
   BOOST_TEST_EQ(v2[0], 1);
   BOOST_TEST_EQ(v2[1], 2);
   BOOST_TEST_EQ(v2[2], 3);
}

void test_swap_ranges_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   boost::container::vector<int> other(9, 0);
   for(int i = 0; i < 9; ++i)
      other[static_cast<std::size_t>(i)] = (i + 1) * 10;

   boost::container::vector<int>::iterator result =
      segmented_swap_ranges(sv.begin(), test_detail::make_sentinel(sv.end()), other.begin());

   BOOST_TEST(result == other.end());

   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(int i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(*it, (i + 1) * 10);

   for(int i = 0; i < 9; ++i)
      BOOST_TEST_EQ(other[static_cast<std::size_t>(i)], i + 1);
}

void test_swap_ranges_sentinel_non_segmented()
{
   boost::container::vector<int> v1;
   v1.push_back(1); v1.push_back(2); v1.push_back(3);
   boost::container::vector<int> v2;
   v2.push_back(10); v2.push_back(20); v2.push_back(30);

   boost::container::vector<int>::iterator result =
      segmented_swap_ranges(v1.begin(), test_detail::make_sentinel(v1.end()), v2.begin());
   BOOST_TEST(result == v2.end());

   BOOST_TEST_EQ(v1[0], 10);
   BOOST_TEST_EQ(v1[1], 20);
   BOOST_TEST_EQ(v1[2], 30);
   BOOST_TEST_EQ(v2[0], 1);
   BOOST_TEST_EQ(v2[1], 2);
   BOOST_TEST_EQ(v2[2], 3);
}

void test_swap_ranges_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   boost::container::vector<int> other(9, 0);
   for(int i = 0; i < 9; ++i)
      other[static_cast<std::size_t>(i)] = (i + 1) * 10;

   boost::container::vector<int>::iterator result =
      segmented_swap_ranges(sv2.begin(), sv2.end(), other.begin());

   BOOST_TEST(result == other.end());

   test_detail::seg2_vector<int>::iterator it = sv2.begin();
   for(int i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(*it, (i + 1) * 10);

   for(int i = 0; i < 9; ++i)
      BOOST_TEST_EQ(other[static_cast<std::size_t>(i)], i + 1);
}

void test_swap_ranges_movable_seg()
{
   typedef test_detail::movable_int mi;
   test_detail::seg_vector<mi> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_from_ints(a1, a1 + 3);
   sv.add_segment_from_ints(a2, a2 + 2);
   sv.add_segment_from_ints(a3, a3 + 4);

   boost::container::vector<mi> other;
   for(int i = 0; i < 9; ++i)
      other.push_back(mi((i + 1) * 10));

   boost::container::vector<mi>::iterator result =
      segmented_swap_ranges(sv.begin(), sv.end(), other.begin());

   BOOST_TEST(result == other.end());

   test_detail::seg_vector<mi>::iterator it = sv.begin();
   for(int i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(it->value(), (i + 1) * 10);

   for(int i = 0; i < 9; ++i)
      BOOST_TEST_EQ(other[static_cast<std::size_t>(i)].value(), i + 1);
}

void test_swap_ranges_movable_seg2()
{
   typedef test_detail::movable_int mi;
   test_detail::seg2_vector<mi> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv2.add_flat_segment_from_ints(a1, a1 + 3);
   sv2.add_flat_segment_from_ints(a2, a2 + 2);
   sv2.add_flat_segment_from_ints(a3, a3 + 4);

   boost::container::vector<mi> other;
   for(int i = 0; i < 9; ++i)
      other.push_back(mi((i + 1) * 10));

   boost::container::vector<mi>::iterator result =
      segmented_swap_ranges(sv2.begin(), sv2.end(), other.begin());

   BOOST_TEST(result == other.end());

   test_detail::seg2_vector<mi>::iterator it = sv2.begin();
   for(int i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(it->value(), (i + 1) * 10);

   for(int i = 0; i < 9; ++i)
      BOOST_TEST_EQ(other[static_cast<std::size_t>(i)].value(), i + 1);
}

int main()
{
   test_swap_ranges_full();
   test_swap_ranges_empty();
   test_swap_ranges_single_segment();
   test_swap_ranges_non_segmented();
   test_swap_ranges_sentinel_segmented();
   test_swap_ranges_sentinel_non_segmented();
   test_swap_ranges_seg2();
   test_swap_ranges_movable_seg();
   test_swap_ranges_movable_seg2();
   return boost::report_errors();
}
