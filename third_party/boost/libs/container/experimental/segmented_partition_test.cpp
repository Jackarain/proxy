//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_partition.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

struct is_even
{
   bool operator()(int v) const { return v % 2 == 0; }
   bool operator()(const test_detail::movable_int& v) const { return v.value() % 2 == 0; }
};

void test_partition_basic()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6, 7};
   int a3[] = {8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 4);
   sv.add_segment_range(a3, a3 + 2);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t mid = segmented_partition(sv.begin(), sv.end(), is_even());

   for(iter_t it = sv.begin(); it != mid; ++it)
      BOOST_TEST(*it % 2 == 0);
   for(iter_t it = mid; it != sv.end(); ++it)
      BOOST_TEST(*it % 2 != 0);

   int even_count = 0;
   for(iter_t it = sv.begin(); it != mid; ++it)
      ++even_count;
   BOOST_TEST_EQ(even_count, 4);
}

void test_partition_empty()
{
   test_detail::seg_vector<int> sv;
   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t mid = segmented_partition(sv.begin(), sv.end(), is_even());
   BOOST_TEST(mid == sv.end());
}

void test_partition_all_true()
{
   test_detail::seg_vector<int> sv;
   int a[] = {2, 4, 6};
   sv.add_segment_range(a, a + 3);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t mid = segmented_partition(sv.begin(), sv.end(), is_even());
   BOOST_TEST(mid == sv.end());
}

void test_partition_all_false()
{
   test_detail::seg_vector<int> sv;
   int a[] = {1, 3, 5};
   sv.add_segment_range(a, a + 3);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t mid = segmented_partition(sv.begin(), sv.end(), is_even());
   BOOST_TEST(mid == sv.begin());
}

void test_partition_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(5); v.push_back(2); v.push_back(7); v.push_back(4); v.push_back(1);

   boost::container::vector<int>::iterator mid = segmented_partition(v.begin(), v.end(), is_even());

   for(boost::container::vector<int>::iterator it = v.begin(); it != mid; ++it)
      BOOST_TEST(*it % 2 == 0);
   for(boost::container::vector<int>::iterator it = mid; it != v.end(); ++it)
      BOOST_TEST(*it % 2 != 0);

   int even_count = 0;
   for(boost::container::vector<int>::iterator it = v.begin(); it != mid; ++it)
      ++even_count;
   BOOST_TEST_EQ(even_count, 2);
}

void test_partition_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6, 7};
   int a3[] = {8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 4);
   sv.add_segment_range(a3, a3 + 2);

   typedef test_detail::seg_vector<int>::iterator iter_t;
   iter_t mid = segmented_partition(sv.begin(), test_detail::make_sentinel(sv.end()), is_even());

   for(iter_t it = sv.begin(); it != mid; ++it)
      BOOST_TEST(*it % 2 == 0);
   for(iter_t it = mid; it != sv.end(); ++it)
      BOOST_TEST(*it % 2 != 0);

   int even_count = 0;
   for(iter_t it = sv.begin(); it != mid; ++it)
      ++even_count;
   BOOST_TEST_EQ(even_count, 4);
}

void test_partition_sentinel_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(5); v.push_back(2); v.push_back(7); v.push_back(4); v.push_back(1);

   boost::container::vector<int>::iterator mid = segmented_partition(v.begin(), test_detail::make_sentinel(v.end()), is_even());

   for(boost::container::vector<int>::iterator it = v.begin(); it != mid; ++it)
      BOOST_TEST(*it % 2 == 0);
   for(boost::container::vector<int>::iterator it = mid; it != v.end(); ++it)
      BOOST_TEST(*it % 2 != 0);

   int even_count = 0;
   for(boost::container::vector<int>::iterator it = v.begin(); it != mid; ++it)
      ++even_count;
   BOOST_TEST_EQ(even_count, 2);
}

void test_partition_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 3);

   typedef test_detail::seg2_vector<int>::iterator iter_t;
   iter_t mid = segmented_partition(sv2.begin(), sv2.end(), is_even());

   for(iter_t it = sv2.begin(); it != mid; ++it)
      BOOST_TEST(*it % 2 == 0);
   for(iter_t it = mid; it != sv2.end(); ++it)
      BOOST_TEST(*it % 2 != 0);

   int even_count = 0;
   for(iter_t it = sv2.begin(); it != mid; ++it)
      ++even_count;
   BOOST_TEST_EQ(even_count, 3);
}

void test_partition_movable_seg()
{
   typedef test_detail::movable_int mi;
   test_detail::seg_vector<mi> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6, 7};
   int a3[] = {8, 9};
   sv.add_segment_from_ints(a1, a1 + 3);
   sv.add_segment_from_ints(a2, a2 + 4);
   sv.add_segment_from_ints(a3, a3 + 2);

   typedef test_detail::seg_vector<mi>::iterator iter_t;
   iter_t mid = segmented_partition(sv.begin(), sv.end(), is_even());

   for(iter_t it = sv.begin(); it != mid; ++it)
      BOOST_TEST(it->value() % 2 == 0);
   for(iter_t it = mid; it != sv.end(); ++it)
      BOOST_TEST(it->value() % 2 != 0);

   int even_count = 0;
   for(iter_t it = sv.begin(); it != mid; ++it)
      ++even_count;
   BOOST_TEST_EQ(even_count, 4);
}

void test_partition_movable_seg2()
{
   typedef test_detail::movable_int mi;
   test_detail::seg2_vector<mi> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv2.add_flat_segment_from_ints(a1, a1 + 3);
   sv2.add_flat_segment_from_ints(a2, a2 + 3);

   typedef test_detail::seg2_vector<mi>::iterator iter_t;
   iter_t mid = segmented_partition(sv2.begin(), sv2.end(), is_even());

   for(iter_t it = sv2.begin(); it != mid; ++it)
      BOOST_TEST(it->value() % 2 == 0);
   for(iter_t it = mid; it != sv2.end(); ++it)
      BOOST_TEST(it->value() % 2 != 0);

   int even_count = 0;
   for(iter_t it = sv2.begin(); it != mid; ++it)
      ++even_count;
   BOOST_TEST_EQ(even_count, 3);
}

int main()
{
   test_partition_basic();
   test_partition_empty();
   test_partition_all_true();
   test_partition_all_false();
   test_partition_non_segmented();
   test_partition_sentinel_segmented();
   test_partition_sentinel_non_segmented();
   test_partition_seg2();
   test_partition_movable_seg();
   test_partition_movable_seg2();
   return boost::report_errors();
}
