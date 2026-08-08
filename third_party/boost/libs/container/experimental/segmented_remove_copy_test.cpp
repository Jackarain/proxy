//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_remove_copy.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_remove_copy_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {2, 4, 2};
   int a3[] = {5, 2};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);
   sv.add_segment_range(a3, a3 + 2);

   boost::container::vector<int> out(8, 0);
   boost::container::vector<int>::iterator r = segmented_remove_copy(sv.begin(), sv.end(), out.begin(), 2);

   int expected[] = {1, 3, 4, 5};
   std::size_t count = static_cast<std::size_t>(r - out.begin());
   BOOST_TEST_EQ(count, 4u);
   for(std::size_t i = 0; i < count; ++i)
      BOOST_TEST_EQ(out[i], expected[i]);
}

void test_remove_copy_empty()
{
   test_detail::seg_vector<int> sv;
   boost::container::vector<int> out;
   boost::container::vector<int>::iterator r = segmented_remove_copy(sv.begin(), sv.end(), out.begin(), 0);
   BOOST_TEST(r == out.begin());
}

void test_remove_copy_none_removed()
{
   test_detail::seg_vector<int> sv;
   int a[] = {1, 3, 5};
   sv.add_segment_range(a, a + 3);

   boost::container::vector<int> out(3, 0);
   boost::container::vector<int>::iterator r = segmented_remove_copy(sv.begin(), sv.end(), out.begin(), 2);
   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 3u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 3);
   BOOST_TEST_EQ(out[2], 5);
}

void test_remove_copy_non_segmented()
{
   int src[] = {1, 2, 3, 2, 4};
   boost::container::vector<int> v(src, src + 5);
   boost::container::vector<int> out(5, 0);
   boost::container::vector<int>::iterator r = segmented_remove_copy(v.begin(), v.end(), out.begin(), 2);

   std::size_t count = static_cast<std::size_t>(r - out.begin());
   BOOST_TEST_EQ(count, 3u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 3);
   BOOST_TEST_EQ(out[2], 4);
}

void test_remove_copy_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {2, 4, 2};
   int a3[] = {5, 2};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);
   sv.add_segment_range(a3, a3 + 2);

   boost::container::vector<int> out(8, 0);
   boost::container::vector<int>::iterator r = segmented_remove_copy(sv.begin(), test_detail::make_sentinel(sv.end()), out.begin(), 2);

   int expected[] = {1, 3, 4, 5};
   std::size_t count = static_cast<std::size_t>(r - out.begin());
   BOOST_TEST_EQ(count, 4u);
   for(std::size_t i = 0; i < count; ++i)
      BOOST_TEST_EQ(out[i], expected[i]);
}

void test_remove_copy_sentinel_non_segmented()
{
   int src[] = {1, 2, 3, 2, 4};
   boost::container::vector<int> v(src, src + 5);
   boost::container::vector<int> out(5, 0);
   boost::container::vector<int>::iterator r = segmented_remove_copy(v.begin(), test_detail::make_sentinel(v.end()), out.begin(), 2);

   std::size_t count = static_cast<std::size_t>(r - out.begin());
   BOOST_TEST_EQ(count, 3u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 3);
   BOOST_TEST_EQ(out[2], 4);
}

void test_remove_copy_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {2, 4, 2};
   int a3[] = {5, 2};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 3);
   sv2.add_flat_segment_range(a3, a3 + 2);

   boost::container::vector<int> out(8, 0);
   boost::container::vector<int>::iterator r = segmented_remove_copy(sv2.begin(), sv2.end(), out.begin(), 2);

   int expected[] = {1, 3, 4, 5};
   std::size_t count = static_cast<std::size_t>(r - out.begin());
   BOOST_TEST_EQ(count, 4u);
   for(std::size_t i = 0; i < count; ++i)
      BOOST_TEST_EQ(out[i], expected[i]);
}

int main()
{
   test_remove_copy_segmented();
   test_remove_copy_empty();
   test_remove_copy_none_removed();
   test_remove_copy_non_segmented();
   test_remove_copy_sentinel_segmented();
   test_remove_copy_sentinel_non_segmented();
   test_remove_copy_seg2();
   return boost::report_errors();
}
