//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_fill_n.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_fill_n_full()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(3, 0);
   sv.add_segment(4, 0);
   sv.add_segment(2, 0);

   segmented_fill_n(sv.begin(), 9, 42);

   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(; it != sv.end(); ++it)
      BOOST_TEST_EQ(*it, 42);
}

void test_fill_n_partial()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6, 7};
   int a3[] = {8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 4);
   sv.add_segment_range(a3, a3 + 2);

   test_detail::seg_vector<int>::iterator result = segmented_fill_n(sv.begin(), 5, 0);

   test_detail::seg_vector<int>::iterator it = sv.begin();
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST(it == result);
   BOOST_TEST_EQ(*it, 6); ++it;
   BOOST_TEST_EQ(*it, 7); ++it;
   BOOST_TEST_EQ(*it, 8); ++it;
   BOOST_TEST_EQ(*it, 9);
}

void test_fill_n_zero()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(3, 99);

   test_detail::seg_vector<int>::iterator result = segmented_fill_n(sv.begin(), 0, 0);
   BOOST_TEST(result == sv.begin());

   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(; it != sv.end(); ++it)
      BOOST_TEST_EQ(*it, 99);
}

void test_fill_n_single_segment()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(5, 0);

   segmented_fill_n(sv.begin(), 3, 77);

   test_detail::seg_vector<int>::iterator it = sv.begin();
   BOOST_TEST_EQ(*it, 77); ++it;
   BOOST_TEST_EQ(*it, 77); ++it;
   BOOST_TEST_EQ(*it, 77); ++it;
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST_EQ(*it, 0);
}

void test_fill_n_non_segmented()
{
   boost::container::vector<int> v(5, 0);
   boost::container::vector<int>::iterator result = segmented_fill_n(v.begin(), 3, 7);

   BOOST_TEST(result == v.begin() + 3);
   BOOST_TEST_EQ(v[0], 7);
   BOOST_TEST_EQ(v[1], 7);
   BOOST_TEST_EQ(v[2], 7);
   BOOST_TEST_EQ(v[3], 0);
   BOOST_TEST_EQ(v[4], 0);
}

void test_fill_n_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {0, 0, 0};
   int a2[] = {0, 0, 0};
   int a3[] = {0, 0, 0};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 3);
   sv2.add_flat_segment_range(a3, a3 + 3);

   segmented_fill_n(sv2.begin(), 9, 42);

   test_detail::seg2_vector<int>::iterator it = sv2.begin();
   for(; it != sv2.end(); ++it)
      BOOST_TEST_EQ(*it, 42);
}

int main()
{
   test_fill_n_full();
   test_fill_n_partial();
   test_fill_n_zero();
   test_fill_n_single_segment();
   test_fill_n_non_segmented();
   test_fill_n_seg2();
   return boost::report_errors();
}
