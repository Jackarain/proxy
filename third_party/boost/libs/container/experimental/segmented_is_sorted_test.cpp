//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_is_sorted.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_is_sorted_sorted_range()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   int a3[] = {7, 8};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);
   sv.add_segment_range(a3, a3 + 2);

   BOOST_TEST(segmented_is_sorted(sv.begin(), sv.end()));
}

void test_is_sorted_unsorted_within_segment()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 6, 5};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   BOOST_TEST(!segmented_is_sorted(sv.begin(), sv.end()));
}

void test_is_sorted_unsorted_at_boundary()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 5};
   int a2[] = {3, 6, 7};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   BOOST_TEST(!segmented_is_sorted(sv.begin(), sv.end()));
}

void test_is_sorted_empty()
{
   test_detail::seg_vector<int> sv;
   BOOST_TEST(segmented_is_sorted(sv.begin(), sv.end()));
}

void test_is_sorted_single()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(1, 42);
   BOOST_TEST(segmented_is_sorted(sv.begin(), sv.end()));
}

struct greater_comp
{
   bool operator()(int a, int b) const { return a > b; }
};

void test_is_sorted_with_comp()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {9, 7, 5};
   int a2[] = {3, 1};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);

   BOOST_TEST(segmented_is_sorted(sv.begin(), sv.end(), greater_comp()));
}

void test_is_sorted_non_segmented()
{
   {
      int data[] = {1, 2, 3, 4, 5};
      boost::container::vector<int> v(data, data + 5);
      BOOST_TEST(segmented_is_sorted(v.begin(), v.end()));
   }
   {
      int data[] = {1, 3, 2, 4, 5};
      boost::container::vector<int> v(data, data + 5);
      BOOST_TEST(!segmented_is_sorted(v.begin(), v.end()));
   }
}

void test_is_sorted_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   int a3[] = {7, 8};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);
   sv.add_segment_range(a3, a3 + 2);

   BOOST_TEST(segmented_is_sorted(sv.begin(), test_detail::make_sentinel(sv.end())));
}

void test_is_sorted_sentinel_non_segmented()
{
   {
      int data[] = {1, 2, 3, 4, 5};
      boost::container::vector<int> v(data, data + 5);
      BOOST_TEST(segmented_is_sorted(v.begin(), test_detail::make_sentinel(v.end())));
   }
   {
      int data[] = {1, 3, 2, 4, 5};
      boost::container::vector<int> v(data, data + 5);
      BOOST_TEST(!segmented_is_sorted(v.begin(), test_detail::make_sentinel(v.end())));
   }
}

void test_is_sorted_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   int a3[] = {7, 8};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 3);
   sv2.add_flat_segment_range(a3, a3 + 2);

   BOOST_TEST(segmented_is_sorted(sv2.begin(), sv2.end()));
}

int main()
{
   test_is_sorted_sorted_range();
   test_is_sorted_unsorted_within_segment();
   test_is_sorted_unsorted_at_boundary();
   test_is_sorted_empty();
   test_is_sorted_single();
   test_is_sorted_with_comp();
   test_is_sorted_non_segmented();
   test_is_sorted_sentinel_segmented();
   test_is_sorted_sentinel_non_segmented();
   test_is_sorted_seg2();
   return boost::report_errors();
}
