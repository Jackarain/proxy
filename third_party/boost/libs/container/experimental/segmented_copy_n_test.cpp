//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_copy_n.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_copy_n_full()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   boost::container::vector<int> out(9, 0);
   boost::container::vector<int>::iterator result = segmented_copy_n(sv.begin(), 9, out.begin());

   BOOST_TEST(result == out.end());
   for(int i = 0; i < 9; ++i)
      BOOST_TEST_EQ(out[static_cast<std::size_t>(i)], i + 1);
}

void test_copy_n_partial()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   boost::container::vector<int> out(5, 0);
   boost::container::vector<int>::iterator result = segmented_copy_n(sv.begin(), 5, out.begin());

   BOOST_TEST(result == out.end());
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 2);
   BOOST_TEST_EQ(out[2], 3);
   BOOST_TEST_EQ(out[3], 4);
   BOOST_TEST_EQ(out[4], 5);
}

void test_copy_n_zero()
{
   test_detail::seg_vector<int> sv;
   int a[] = {1, 2, 3};
   sv.add_segment_range(a, a + 3);

   boost::container::vector<int> out(3, 99);
   boost::container::vector<int>::iterator result = segmented_copy_n(sv.begin(), 0, out.begin());

   BOOST_TEST(result == out.begin());
   BOOST_TEST_EQ(out[0], 99);
}

void test_copy_n_single_segment()
{
   test_detail::seg_vector<int> sv;
   int a[] = {10, 20, 30};
   sv.add_segment_range(a, a + 3);

   boost::container::vector<int> out(2, 0);
   boost::container::vector<int>::iterator result = segmented_copy_n(sv.begin(), 2, out.begin());

   BOOST_TEST(result == out.end());
   BOOST_TEST_EQ(out[0], 10);
   BOOST_TEST_EQ(out[1], 20);
}

void test_copy_n_non_segmented()
{
   boost::container::vector<int> in;
   in.push_back(1); in.push_back(2); in.push_back(3);

   boost::container::vector<int> out(2, 0);
   boost::container::vector<int>::iterator result = segmented_copy_n(in.begin(), 2, out.begin());

   BOOST_TEST(result == out.end());
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 2);
}

void test_copy_n_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   boost::container::vector<int> out(9, 0);
   boost::container::vector<int>::iterator result = segmented_copy_n(sv2.begin(), 9, out.begin());

   BOOST_TEST(result == out.end());
   for(int i = 0; i < 9; ++i)
      BOOST_TEST_EQ(out[static_cast<std::size_t>(i)], i + 1);
}

void test_copy_n_segmented_output()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   test_detail::seg_vector<int> out;
   out.add_segment(4, 0);
   out.add_segment(3, 0);
   out.add_segment(2, 0);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t result = segmented_copy_n(sv.begin(), 9, out.begin());

   BOOST_TEST(result == out.end());
   iter_t it = out.begin();
   for(int i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(*it, i + 1);
}

void test_copy_n_seg2_to_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   test_detail::seg2_vector<int> out;
   {
      test_detail::seg_vector<int> s1; s1.add_segment(5, 0);
      test_detail::seg_vector<int> s2; s2.add_segment(4, 0);
      out.add_segment(s1);
      out.add_segment(s2);
   }

   typedef test_detail::seg2_vector<int>::iterator iter_t;
   iter_t result = segmented_copy_n(sv2.begin(), 9, out.begin());

   BOOST_TEST(result == out.end());
   iter_t it = out.begin();
   for(int i = 0; i < 9; ++i, ++it)
      BOOST_TEST_EQ(*it, i + 1);
}

void test_copy_n_seg_to_seg_misaligned()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3, 4, 5};
   int a2[] = {6, 7, 8};
   sv.add_segment_range(a1, a1 + 5);
   sv.add_segment_range(a2, a2 + 3);

   test_detail::seg_vector<int> out;
   out.add_segment(2, 0);
   out.add_segment(3, 0);
   out.add_segment(3, 0);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t result = segmented_copy_n(sv.begin(), 8, out.begin());

   BOOST_TEST(result == out.end());
   iter_t it = out.begin();
   for(int i = 0; i < 8; ++i, ++it)
      BOOST_TEST_EQ(*it, i + 1);
}

void test_copy_n_partial_segmented_output()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   test_detail::seg_vector<int> out;
   out.add_segment(3, 0);
   out.add_segment(3, 0);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t result = segmented_copy_n(sv.begin(), 5, out.begin());

   std::size_t count = 0;
   iter_t it = out.begin();
   for(; it != result; ++it)
      ++count;
   BOOST_TEST_EQ(count, 5u);

   it = out.begin();
   for(int i = 0; i < 5; ++i, ++it)
      BOOST_TEST_EQ(*it, i + 1);
}

int main()
{
   test_copy_n_full();
   test_copy_n_partial();
   test_copy_n_zero();
   test_copy_n_single_segment();
   test_copy_n_non_segmented();
   test_copy_n_seg2();
   test_copy_n_segmented_output();
   test_copy_n_seg2_to_seg2();
   test_copy_n_seg_to_seg_misaligned();
   test_copy_n_partial_segmented_output();
   return boost::report_errors();
}
