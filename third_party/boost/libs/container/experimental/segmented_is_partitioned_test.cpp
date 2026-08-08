//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_is_partitioned.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

struct less_than_5
{
   bool operator()(int x) const { return x < 5; }
};

void test_is_partitioned_true()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4};
   int a3[] = {5, 6, 7, 8};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 1);
   sv.add_segment_range(a3, a3 + 4);

   BOOST_TEST(segmented_is_partitioned(sv.begin(), sv.end(), less_than_5()));
}

void test_is_partitioned_false()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 7};
   int a2[] = {3, 5};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);

   BOOST_TEST(!segmented_is_partitioned(sv.begin(), sv.end(), less_than_5()));
}

void test_is_partitioned_all_true()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2};
   int a2[] = {3, 4};
   sv.add_segment_range(a1, a1 + 2);
   sv.add_segment_range(a2, a2 + 2);

   BOOST_TEST(segmented_is_partitioned(sv.begin(), sv.end(), less_than_5()));
}

void test_is_partitioned_all_false()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {5, 6};
   int a2[] = {7, 8};
   sv.add_segment_range(a1, a1 + 2);
   sv.add_segment_range(a2, a2 + 2);

   BOOST_TEST(segmented_is_partitioned(sv.begin(), sv.end(), less_than_5()));
}

void test_is_partitioned_empty()
{
   test_detail::seg_vector<int> sv;
   BOOST_TEST(segmented_is_partitioned(sv.begin(), sv.end(), less_than_5()));
}

void test_is_partitioned_boundary_across_segments()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {6, 5, 4};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   BOOST_TEST(!segmented_is_partitioned(sv.begin(), sv.end(), less_than_5()));
}

void test_is_partitioned_violation_in_second_segment()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2};
   int a2[] = {6, 3, 7};
   sv.add_segment_range(a1, a1 + 2);
   sv.add_segment_range(a2, a2 + 3);

   BOOST_TEST(!segmented_is_partitioned(sv.begin(), sv.end(), less_than_5()));
}

void test_is_partitioned_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1); v.push_back(2); v.push_back(3);
   v.push_back(5); v.push_back(6);
   BOOST_TEST(segmented_is_partitioned(v.begin(), v.end(), less_than_5()));

   boost::container::vector<int> v2;
   v2.push_back(1); v2.push_back(6); v2.push_back(3);
   BOOST_TEST(!segmented_is_partitioned(v2.begin(), v2.end(), less_than_5()));
}

void test_is_partitioned_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4};
   int a3[] = {5, 6, 7, 8};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 1);
   sv.add_segment_range(a3, a3 + 4);

   BOOST_TEST(segmented_is_partitioned(sv.begin(), test_detail::make_sentinel(sv.end()), less_than_5()));
}

void test_is_partitioned_sentinel_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1); v.push_back(2); v.push_back(3);
   v.push_back(5); v.push_back(6);
   BOOST_TEST(segmented_is_partitioned(v.begin(), test_detail::make_sentinel(v.end()), less_than_5()));

   boost::container::vector<int> v2;
   v2.push_back(1); v2.push_back(6); v2.push_back(3);
   BOOST_TEST(!segmented_is_partitioned(v2.begin(), test_detail::make_sentinel(v2.end()), less_than_5()));
}

void test_is_partitioned_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2};
   int a2[] = {3, 4};
   int a3[] = {5, 7, 8};
   sv2.add_flat_segment_range(a1, a1 + 2);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 3);

   BOOST_TEST(segmented_is_partitioned(sv2.begin(), sv2.end(), less_than_5()));
}

int main()
{
   test_is_partitioned_true();
   test_is_partitioned_false();
   test_is_partitioned_all_true();
   test_is_partitioned_all_false();
   test_is_partitioned_empty();
   test_is_partitioned_boundary_across_segments();
   test_is_partitioned_violation_in_second_segment();
   test_is_partitioned_non_segmented();
   test_is_partitioned_sentinel_segmented();
   test_is_partitioned_sentinel_non_segmented();
   test_is_partitioned_seg2();
   return boost::report_errors();
}
