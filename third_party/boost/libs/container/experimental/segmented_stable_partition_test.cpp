//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_stable_partition.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

struct is_even
{
   bool operator()(int v) const { return v % 2 == 0; }
   bool operator()(const test_detail::movable_int& v) const { return v.value() % 2 == 0; }
};

void test_stable_partition_basic()
{
   boost::container::vector<int> v;
   v.push_back(1); v.push_back(2); v.push_back(3);
   v.push_back(4); v.push_back(5); v.push_back(6);

   boost::container::vector<int>::iterator mid = segmented_stable_partition(v.begin(), v.end(), is_even());

   BOOST_TEST_EQ(v[0], 2);
   BOOST_TEST_EQ(v[1], 4);
   BOOST_TEST_EQ(v[2], 6);
   BOOST_TEST_EQ(v[3], 1);
   BOOST_TEST_EQ(v[4], 3);
   BOOST_TEST_EQ(v[5], 5);

   int dist = 0;
   for(boost::container::vector<int>::iterator it = v.begin(); it != mid; ++it)
      ++dist;
   BOOST_TEST_EQ(dist, 3);
}

void test_stable_partition_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {3, 2, 1};
   int a2[] = {6, 5, 4};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t mid = segmented_stable_partition(sv.begin(), sv.end(), is_even());

   iter_t it = sv.begin();
   BOOST_TEST_EQ(*it, 2); ++it;
   BOOST_TEST_EQ(*it, 6); ++it;
   BOOST_TEST_EQ(*it, 4); ++it;
   BOOST_TEST(it == mid);
   BOOST_TEST_EQ(*it, 3); ++it;
   BOOST_TEST_EQ(*it, 1); ++it;
   BOOST_TEST_EQ(*it, 5);
}

void test_stable_partition_empty()
{
   boost::container::vector<int> v;
   boost::container::vector<int>::iterator mid = segmented_stable_partition(v.begin(), v.end(), is_even());
   BOOST_TEST(mid == v.end());
}

void test_stable_partition_all_true()
{
   boost::container::vector<int> v;
   v.push_back(2); v.push_back(4); v.push_back(6);
   boost::container::vector<int>::iterator mid = segmented_stable_partition(v.begin(), v.end(), is_even());
   BOOST_TEST(mid == v.end());
   BOOST_TEST_EQ(v[0], 2);
   BOOST_TEST_EQ(v[1], 4);
   BOOST_TEST_EQ(v[2], 6);
}

void test_stable_partition_all_false()
{
   boost::container::vector<int> v;
   v.push_back(1); v.push_back(3); v.push_back(5);
   boost::container::vector<int>::iterator mid = segmented_stable_partition(v.begin(), v.end(), is_even());
   BOOST_TEST(mid == v.begin());
   BOOST_TEST_EQ(v[0], 1);
   BOOST_TEST_EQ(v[1], 3);
   BOOST_TEST_EQ(v[2], 5);
}

void test_stable_partition_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(5); v.push_back(2); v.push_back(7); v.push_back(4); v.push_back(1);

   boost::container::vector<int>::iterator mid = segmented_stable_partition(v.begin(), v.end(), is_even());

   BOOST_TEST_EQ(v[0], 2);
   BOOST_TEST_EQ(v[1], 4);
   BOOST_TEST_EQ(v[2], 5);
   BOOST_TEST_EQ(v[3], 7);
   BOOST_TEST_EQ(v[4], 1);

   int even_count = 0;
   for(boost::container::vector<int>::iterator it = v.begin(); it != mid; ++it)
      ++even_count;
   BOOST_TEST_EQ(even_count, 2);
}


void test_stable_partition_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 3);

   typedef test_detail::seg2_vector<int>::iterator iter_t;
   iter_t mid = segmented_stable_partition(sv2.begin(), sv2.end(), is_even());

   iter_t it = sv2.begin();
   BOOST_TEST_EQ(*it, 2); ++it;
   BOOST_TEST_EQ(*it, 4); ++it;
   BOOST_TEST_EQ(*it, 6); ++it;
   BOOST_TEST(it == mid);
   BOOST_TEST_EQ(*it, 1); ++it;
   BOOST_TEST_EQ(*it, 3); ++it;
   BOOST_TEST_EQ(*it, 5);
}

void test_stable_partition_movable_seg()
{
   typedef test_detail::movable_int mi;
   test_detail::seg_vector<mi> sv;
   int a1[] = {3, 2, 1};
   int a2[] = {6, 5, 4};
   sv.add_segment_from_ints(a1, a1 + 3);
   sv.add_segment_from_ints(a2, a2 + 3);

   typedef test_detail::seg_vector<mi>::iterator iter_t;
   iter_t mid = segmented_stable_partition(sv.begin(), sv.end(), is_even());

   iter_t it = sv.begin();
   BOOST_TEST_EQ(it->value(), 2); ++it;
   BOOST_TEST_EQ(it->value(), 6); ++it;
   BOOST_TEST_EQ(it->value(), 4); ++it;
   BOOST_TEST(it == mid);
   BOOST_TEST_EQ(it->value(), 3); ++it;
   BOOST_TEST_EQ(it->value(), 1); ++it;
   BOOST_TEST_EQ(it->value(), 5);
}

void test_stable_partition_movable_seg2()
{
   typedef test_detail::movable_int mi;
   test_detail::seg2_vector<mi> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv2.add_flat_segment_from_ints(a1, a1 + 3);
   sv2.add_flat_segment_from_ints(a2, a2 + 3);

   typedef test_detail::seg2_vector<mi>::iterator iter_t;
   iter_t mid = segmented_stable_partition(sv2.begin(), sv2.end(), is_even());

   iter_t it = sv2.begin();
   BOOST_TEST_EQ(it->value(), 2); ++it;
   BOOST_TEST_EQ(it->value(), 4); ++it;
   BOOST_TEST_EQ(it->value(), 6); ++it;
   BOOST_TEST(it == mid);
   BOOST_TEST_EQ(it->value(), 1); ++it;
   BOOST_TEST_EQ(it->value(), 3); ++it;
   BOOST_TEST_EQ(it->value(), 5);
}

int main()
{
   test_stable_partition_basic();
   test_stable_partition_segmented();
   test_stable_partition_empty();
   test_stable_partition_all_true();
   test_stable_partition_all_false();
   test_stable_partition_non_segmented();
   test_stable_partition_seg2();
   test_stable_partition_movable_seg();
   test_stable_partition_movable_seg2();
   return boost::report_errors();
}
