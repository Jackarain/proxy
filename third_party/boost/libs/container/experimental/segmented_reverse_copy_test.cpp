//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_reverse_copy.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_reverse_copy_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 3);

   boost::container::vector<int> out(8, 0);
   boost::container::vector<int>::iterator r = segmented_reverse_copy(sv.begin(), sv.end(), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 8u);

   int expected[] = {8, 7, 6, 5, 4, 3, 2, 1};
   for(std::size_t i = 0; i < 8; ++i)
      BOOST_TEST_EQ(out[i], expected[i]);
}

void test_reverse_copy_empty()
{
   test_detail::seg_vector<int> sv;
   boost::container::vector<int> out;
   boost::container::vector<int>::iterator r = segmented_reverse_copy(sv.begin(), sv.end(), out.begin());
   BOOST_TEST(r == out.begin());
}

void test_reverse_copy_single_segment()
{
   test_detail::seg_vector<int> sv;
   int a[] = {10, 20, 30};
   sv.add_segment_range(a, a + 3);

   boost::container::vector<int> out(3, 0);
   boost::container::vector<int>::iterator r = segmented_reverse_copy(sv.begin(), sv.end(), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 3u);
   BOOST_TEST_EQ(out[0], 30);
   BOOST_TEST_EQ(out[1], 20);
   BOOST_TEST_EQ(out[2], 10);
}

void test_reverse_copy_single_element()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(1, 42);

   boost::container::vector<int> out(1, 0);
   segmented_reverse_copy(sv.begin(), sv.end(), out.begin());
   BOOST_TEST_EQ(out[0], 42);
}

void test_reverse_copy_non_segmented()
{
   int src[] = {1, 2, 3, 4, 5};
   boost::container::vector<int> v(src, src + 5);
   boost::container::vector<int> out(5, 0);
   boost::container::vector<int>::iterator r = segmented_reverse_copy(v.begin(), v.end(), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 5u);
   BOOST_TEST_EQ(out[0], 5);
   BOOST_TEST_EQ(out[1], 4);
   BOOST_TEST_EQ(out[2], 3);
   BOOST_TEST_EQ(out[3], 2);
   BOOST_TEST_EQ(out[4], 1);
}

void test_reverse_copy_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   boost::container::vector<int> out(9, 0);
   boost::container::vector<int>::iterator r = segmented_reverse_copy(sv2.begin(), sv2.end(), out.begin());

   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 9u);

   int expected[] = {9, 8, 7, 6, 5, 4, 3, 2, 1};
   for(std::size_t i = 0; i < 9; ++i)
      BOOST_TEST_EQ(out[i], expected[i]);
}

int main()
{
   test_reverse_copy_segmented();
   test_reverse_copy_empty();
   test_reverse_copy_single_segment();
   test_reverse_copy_single_element();
   test_reverse_copy_non_segmented();
   test_reverse_copy_seg2();
   return boost::report_errors();
}
