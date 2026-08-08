//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_count_if.hpp>
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

void test_count_if_basic()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   BOOST_TEST_EQ(segmented_count_if(sv.begin(), sv.end(), is_even()), 3);
}

void test_count_if_no_match()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 3, 5};
   sv.add_segment_range(a1, a1 + 3);

   BOOST_TEST_EQ(segmented_count_if(sv.begin(), sv.end(), is_negative()), 0);
}

void test_count_if_all_match()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {2, 4, 6};
   sv.add_segment_range(a1, a1 + 3);

   BOOST_TEST_EQ(segmented_count_if(sv.begin(), sv.end(), is_even()), 3);
}

void test_count_if_empty()
{
   test_detail::seg_vector<int> sv;
   BOOST_TEST_EQ(segmented_count_if(sv.begin(), sv.end(), is_even()), 0);
}

void test_count_if_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1);
   v.push_back(2);
   v.push_back(1);
   v.push_back(3);
   v.push_back(1);

   BOOST_TEST_EQ(segmented_count_if(v.begin(), v.end(), is_even()), 1);
}

void test_count_if_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   BOOST_TEST_EQ(segmented_count_if(sv.begin(), test_detail::make_sentinel(sv.end()), is_even()), 3);
}

void test_count_if_sentinel_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1);
   v.push_back(2);
   v.push_back(1);
   v.push_back(3);
   v.push_back(1);

   BOOST_TEST_EQ(segmented_count_if(v.begin(), test_detail::make_sentinel(v.end()), is_even()), 1);
}

void test_count_if_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2};
   int a2[] = {3, 4};
   int a3[] = {5, 6};
   sv2.add_flat_segment_range(a1, a1 + 2);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 2);

   BOOST_TEST_EQ(segmented_count_if(sv2.begin(), sv2.end(), is_even()), 3);
}

int main()
{
   test_count_if_basic();
   test_count_if_no_match();
   test_count_if_all_match();
   test_count_if_empty();
   test_count_if_non_segmented();
   test_count_if_sentinel_segmented();
   test_count_if_sentinel_non_segmented();
   test_count_if_seg2();
   return boost::report_errors();
}
