//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_copy_if.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

struct is_even
{
   bool operator()(int x) const { return x % 2 == 0; }
};

void test_copy_if_full_range()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   boost::container::vector<int> out(9, 0);
   boost::container::vector<int>::iterator result =
      segmented_copy_if(sv.begin(), sv.end(), out.begin(), is_even());

   std::size_t count = static_cast<std::size_t>(result - out.begin());
   BOOST_TEST_EQ(count, 4u);
   BOOST_TEST_EQ(out[0], 2);
   BOOST_TEST_EQ(out[1], 4);
   BOOST_TEST_EQ(out[2], 6);
   BOOST_TEST_EQ(out[3], 8);
}

void test_copy_if_none_match()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 3, 5};
   sv.add_segment_range(a1, a1 + 3);

   boost::container::vector<int> out(3, 0);
   boost::container::vector<int>::iterator result =
      segmented_copy_if(sv.begin(), sv.end(), out.begin(), is_even());

   BOOST_TEST(result == out.begin());
}

void test_copy_if_all_match()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {2, 4};
   int a2[] = {6, 8};
   sv.add_segment_range(a1, a1 + 2);
   sv.add_segment_range(a2, a2 + 2);

   boost::container::vector<int> out(4, 0);
   boost::container::vector<int>::iterator result =
      segmented_copy_if(sv.begin(), sv.end(), out.begin(), is_even());

   BOOST_TEST(result == out.end());
   BOOST_TEST_EQ(out[0], 2);
   BOOST_TEST_EQ(out[1], 4);
   BOOST_TEST_EQ(out[2], 6);
   BOOST_TEST_EQ(out[3], 8);
}

void test_copy_if_empty()
{
   test_detail::seg_vector<int> sv;
   boost::container::vector<int> out;
   boost::container::vector<int>::iterator result =
      segmented_copy_if(sv.begin(), sv.end(), out.begin(), is_even());
   BOOST_TEST(result == out.begin());
}

void test_copy_if_non_segmented()
{
   boost::container::vector<int> in;
   in.push_back(1); in.push_back(2); in.push_back(3);
   in.push_back(4); in.push_back(5);

   boost::container::vector<int> out(5, 0);
   boost::container::vector<int>::iterator result =
      segmented_copy_if(in.begin(), in.end(), out.begin(), is_even());

   std::size_t count = static_cast<std::size_t>(result - out.begin());
   BOOST_TEST_EQ(count, 2u);
   BOOST_TEST_EQ(out[0], 2);
   BOOST_TEST_EQ(out[1], 4);
}

void test_copy_if_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   boost::container::vector<int> out(9, 0);
   boost::container::vector<int>::iterator result =
      segmented_copy_if(sv.begin(), test_detail::make_sentinel(sv.end()), out.begin(), is_even());

   std::size_t count = static_cast<std::size_t>(result - out.begin());
   BOOST_TEST_EQ(count, 4u);
   BOOST_TEST_EQ(out[0], 2);
   BOOST_TEST_EQ(out[1], 4);
   BOOST_TEST_EQ(out[2], 6);
   BOOST_TEST_EQ(out[3], 8);
}

void test_copy_if_sentinel_non_segmented()
{
   boost::container::vector<int> in;
   in.push_back(1); in.push_back(2); in.push_back(3);
   in.push_back(4); in.push_back(5);

   boost::container::vector<int> out(5, 0);
   boost::container::vector<int>::iterator result =
      segmented_copy_if(in.begin(), test_detail::make_sentinel(in.end()), out.begin(), is_even());

   std::size_t count = static_cast<std::size_t>(result - out.begin());
   BOOST_TEST_EQ(count, 2u);
   BOOST_TEST_EQ(out[0], 2);
   BOOST_TEST_EQ(out[1], 4);
}

void test_copy_if_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   boost::container::vector<int> out(9, 0);
   boost::container::vector<int>::iterator result =
      segmented_copy_if(sv2.begin(), sv2.end(), out.begin(), is_even());

   std::size_t count = static_cast<std::size_t>(result - out.begin());
   BOOST_TEST_EQ(count, 4u);
   BOOST_TEST_EQ(out[0], 2);
   BOOST_TEST_EQ(out[1], 4);
   BOOST_TEST_EQ(out[2], 6);
   BOOST_TEST_EQ(out[3], 8);
}

void test_copy_if_segmented_output()
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
   out.add_segment(2, 0);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t result = segmented_copy_if(sv.begin(), sv.end(), out.begin(), is_even());

   std::size_t count = 0;
   iter_t it = out.begin();
   for(; it != result; ++it)
      ++count;
   BOOST_TEST_EQ(count, 4u);

   it = out.begin();
   BOOST_TEST_EQ(*it, 2); ++it;
   BOOST_TEST_EQ(*it, 4); ++it;
   BOOST_TEST_EQ(*it, 6); ++it;
   BOOST_TEST_EQ(*it, 8);
}

void test_copy_if_seg2_to_seg2()
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
      test_detail::seg_vector<int> s1; s1.add_segment(3, 0);
      test_detail::seg_vector<int> s2; s2.add_segment(2, 0);
      out.add_segment(s1);
      out.add_segment(s2);
   }

   typedef test_detail::seg2_vector<int>::iterator iter_t;
   iter_t result = segmented_copy_if(sv2.begin(), sv2.end(), out.begin(), is_even());

   std::size_t count = 0;
   iter_t it = out.begin();
   for(; it != result; ++it)
      ++count;
   BOOST_TEST_EQ(count, 4u);

   it = out.begin();
   BOOST_TEST_EQ(*it, 2); ++it;
   BOOST_TEST_EQ(*it, 4); ++it;
   BOOST_TEST_EQ(*it, 6); ++it;
   BOOST_TEST_EQ(*it, 8);
}

void test_copy_if_seg_to_seg_misaligned()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3, 4, 5};
   int a2[] = {6, 7, 8};
   sv.add_segment_range(a1, a1 + 5);
   sv.add_segment_range(a2, a2 + 3);

   test_detail::seg_vector<int> out;
   out.add_segment(2, 0);
   out.add_segment(3, 0);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t result = segmented_copy_if(sv.begin(), sv.end(), out.begin(), is_even());

   std::size_t count = 0;
   iter_t it = out.begin();
   for(; it != result; ++it)
      ++count;
   BOOST_TEST_EQ(count, 4u);

   it = out.begin();
   BOOST_TEST_EQ(*it, 2); ++it;
   BOOST_TEST_EQ(*it, 4); ++it;
   BOOST_TEST_EQ(*it, 6); ++it;
   BOOST_TEST_EQ(*it, 8);
}

int main()
{
   test_copy_if_full_range();
   test_copy_if_none_match();
   test_copy_if_all_match();
   test_copy_if_empty();
   test_copy_if_non_segmented();
   test_copy_if_sentinel_segmented();
   test_copy_if_sentinel_non_segmented();
   test_copy_if_seg2();
   test_copy_if_segmented_output();
   test_copy_if_seg2_to_seg2();
   test_copy_if_seg_to_seg_misaligned();
   return boost::report_errors();
}
