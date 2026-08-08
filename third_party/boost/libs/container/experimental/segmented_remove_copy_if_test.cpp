//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_remove_copy_if.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

struct is_even
{
   bool operator()(int x) const { return x % 2 == 0; }
};

struct is_negative
{
   bool operator()(int x) const { return x < 0; }
};

void test_remove_copy_if_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   boost::container::vector<int> out(6, 0);
   boost::container::vector<int>::iterator r = segmented_remove_copy_if(sv.begin(), sv.end(), out.begin(), is_even());

   int expected[] = {1, 3, 5};
   std::size_t count = static_cast<std::size_t>(r - out.begin());
   BOOST_TEST_EQ(count, 3u);
   for(std::size_t i = 0; i < count; ++i)
      BOOST_TEST_EQ(out[i], expected[i]);
}

void test_remove_copy_if_empty()
{
   test_detail::seg_vector<int> sv;
   boost::container::vector<int> out;
   boost::container::vector<int>::iterator r = segmented_remove_copy_if(sv.begin(), sv.end(), out.begin(), is_even());
   BOOST_TEST(r == out.begin());
}

void test_remove_copy_if_none_removed()
{
   test_detail::seg_vector<int> sv;
   int a[] = {1, 3, 5};
   sv.add_segment_range(a, a + 3);

   boost::container::vector<int> out(3, 0);
   boost::container::vector<int>::iterator r = segmented_remove_copy_if(sv.begin(), sv.end(), out.begin(), is_even());
   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 3u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 3);
   BOOST_TEST_EQ(out[2], 5);
}

void test_remove_copy_if_all_removed()
{
   test_detail::seg_vector<int> sv;
   int a[] = {2, 4, 6};
   sv.add_segment_range(a, a + 3);

   boost::container::vector<int> out(3, 0);
   boost::container::vector<int>::iterator r = segmented_remove_copy_if(sv.begin(), sv.end(), out.begin(), is_even());
   BOOST_TEST_EQ(static_cast<std::size_t>(r - out.begin()), 0u);
}

void test_remove_copy_if_non_segmented()
{
   int src[] = {1, 2, 3, 4, 5};
   boost::container::vector<int> v(src, src + 5);
   boost::container::vector<int> out(5, 0);
   boost::container::vector<int>::iterator r = segmented_remove_copy_if(v.begin(), v.end(), out.begin(), is_even());

   std::size_t count = static_cast<std::size_t>(r - out.begin());
   BOOST_TEST_EQ(count, 3u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 3);
   BOOST_TEST_EQ(out[2], 5);
}

void test_remove_copy_if_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   boost::container::vector<int> out(6, 0);
   boost::container::vector<int>::iterator r = segmented_remove_copy_if(sv.begin(), test_detail::make_sentinel(sv.end()), out.begin(), is_even());

   int expected[] = {1, 3, 5};
   std::size_t count = static_cast<std::size_t>(r - out.begin());
   BOOST_TEST_EQ(count, 3u);
   for(std::size_t i = 0; i < count; ++i)
      BOOST_TEST_EQ(out[i], expected[i]);
}

void test_remove_copy_if_sentinel_non_segmented()
{
   int src[] = {1, 2, 3, 4, 5};
   boost::container::vector<int> v(src, src + 5);
   boost::container::vector<int> out(5, 0);
   boost::container::vector<int>::iterator r = segmented_remove_copy_if(v.begin(), test_detail::make_sentinel(v.end()), out.begin(), is_even());

   std::size_t count = static_cast<std::size_t>(r - out.begin());
   BOOST_TEST_EQ(count, 3u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 3);
   BOOST_TEST_EQ(out[2], 5);
}

void test_remove_copy_if_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   int a3[] = {7, 8};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 3);
   sv2.add_flat_segment_range(a3, a3 + 2);

   boost::container::vector<int> out(8, 0);
   boost::container::vector<int>::iterator r = segmented_remove_copy_if(sv2.begin(), sv2.end(), out.begin(), is_even());

   int expected[] = {1, 3, 5, 7};
   std::size_t count = static_cast<std::size_t>(r - out.begin());
   BOOST_TEST_EQ(count, 4u);
   for(std::size_t i = 0; i < count; ++i)
      BOOST_TEST_EQ(out[i], expected[i]);
}

int main()
{
   test_remove_copy_if_segmented();
   test_remove_copy_if_empty();
   test_remove_copy_if_none_removed();
   test_remove_copy_if_all_removed();
   test_remove_copy_if_non_segmented();
   test_remove_copy_if_sentinel_segmented();
   test_remove_copy_if_sentinel_non_segmented();
   test_remove_copy_if_seg2();
   return boost::report_errors();
}
