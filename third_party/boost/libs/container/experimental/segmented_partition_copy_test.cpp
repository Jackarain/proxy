//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_partition_copy.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>
#include <utility>

using namespace boost::container;

struct is_even
{
   bool operator()(int x) const { return x % 2 == 0; }
};

void test_partition_copy_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   int a3[] = {7};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);
   sv.add_segment_range(a3, a3 + 1);

   boost::container::vector<int> evens(7, 0);
   boost::container::vector<int> odds(7, 0);

   std::pair<boost::container::vector<int>::iterator, boost::container::vector<int>::iterator> r =
      segmented_partition_copy(sv.begin(), sv.end(), evens.begin(), odds.begin(), is_even());

   std::size_t ne = static_cast<std::size_t>(r.first  - evens.begin());
   std::size_t no = static_cast<std::size_t>(r.second - odds.begin());
   BOOST_TEST_EQ(ne, 3u);
   BOOST_TEST_EQ(no, 4u);

   BOOST_TEST_EQ(evens[0], 2);
   BOOST_TEST_EQ(evens[1], 4);
   BOOST_TEST_EQ(evens[2], 6);

   BOOST_TEST_EQ(odds[0], 1);
   BOOST_TEST_EQ(odds[1], 3);
   BOOST_TEST_EQ(odds[2], 5);
   BOOST_TEST_EQ(odds[3], 7);
}

void test_partition_copy_empty()
{
   test_detail::seg_vector<int> sv;
   boost::container::vector<int> evens;
   boost::container::vector<int> odds;
   std::pair<boost::container::vector<int>::iterator, boost::container::vector<int>::iterator> r =
      segmented_partition_copy(sv.begin(), sv.end(), evens.begin(), odds.begin(), is_even());
   BOOST_TEST(r.first  == evens.begin());
   BOOST_TEST(r.second == odds.begin());
}

void test_partition_copy_all_true()
{
   test_detail::seg_vector<int> sv;
   int a[] = {2, 4, 6};
   sv.add_segment_range(a, a + 3);

   boost::container::vector<int> evens(3, 0);
   boost::container::vector<int> odds(3, 0);
   std::pair<boost::container::vector<int>::iterator, boost::container::vector<int>::iterator> r =
      segmented_partition_copy(sv.begin(), sv.end(), evens.begin(), odds.begin(), is_even());

   BOOST_TEST_EQ(static_cast<std::size_t>(r.first  - evens.begin()), 3u);
   BOOST_TEST_EQ(static_cast<std::size_t>(r.second - odds.begin()),  0u);
}

void test_partition_copy_non_segmented()
{
   int src[] = {1, 2, 3, 4, 5};
   boost::container::vector<int> v(src, src + 5);
   boost::container::vector<int> evens(5, 0);
   boost::container::vector<int> odds(5, 0);

   std::pair<boost::container::vector<int>::iterator, boost::container::vector<int>::iterator> r =
      segmented_partition_copy(v.begin(), v.end(), evens.begin(), odds.begin(), is_even());

   std::size_t ne = static_cast<std::size_t>(r.first  - evens.begin());
   std::size_t no = static_cast<std::size_t>(r.second - odds.begin());
   BOOST_TEST_EQ(ne, 2u);
   BOOST_TEST_EQ(no, 3u);
   BOOST_TEST_EQ(evens[0], 2);
   BOOST_TEST_EQ(evens[1], 4);
   BOOST_TEST_EQ(odds[0], 1);
   BOOST_TEST_EQ(odds[1], 3);
   BOOST_TEST_EQ(odds[2], 5);
}

void test_partition_copy_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   int a3[] = {7};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);
   sv.add_segment_range(a3, a3 + 1);

   boost::container::vector<int> evens(7, 0);
   boost::container::vector<int> odds(7, 0);

   std::pair<boost::container::vector<int>::iterator, boost::container::vector<int>::iterator> r =
      segmented_partition_copy(sv.begin(), test_detail::make_sentinel(sv.end()), evens.begin(), odds.begin(), is_even());

   std::size_t ne = static_cast<std::size_t>(r.first  - evens.begin());
   std::size_t no = static_cast<std::size_t>(r.second - odds.begin());
   BOOST_TEST_EQ(ne, 3u);
   BOOST_TEST_EQ(no, 4u);

   BOOST_TEST_EQ(evens[0], 2);
   BOOST_TEST_EQ(evens[1], 4);
   BOOST_TEST_EQ(evens[2], 6);

   BOOST_TEST_EQ(odds[0], 1);
   BOOST_TEST_EQ(odds[1], 3);
   BOOST_TEST_EQ(odds[2], 5);
   BOOST_TEST_EQ(odds[3], 7);
}

void test_partition_copy_sentinel_non_segmented()
{
   int src[] = {1, 2, 3, 4, 5};
   boost::container::vector<int> v(src, src + 5);
   boost::container::vector<int> evens(5, 0);
   boost::container::vector<int> odds(5, 0);

   std::pair<boost::container::vector<int>::iterator, boost::container::vector<int>::iterator> r =
      segmented_partition_copy(v.begin(), test_detail::make_sentinel(v.end()), evens.begin(), odds.begin(), is_even());

   std::size_t ne = static_cast<std::size_t>(r.first  - evens.begin());
   std::size_t no = static_cast<std::size_t>(r.second - odds.begin());
   BOOST_TEST_EQ(ne, 2u);
   BOOST_TEST_EQ(no, 3u);
   BOOST_TEST_EQ(evens[0], 2);
   BOOST_TEST_EQ(evens[1], 4);
   BOOST_TEST_EQ(odds[0], 1);
   BOOST_TEST_EQ(odds[1], 3);
   BOOST_TEST_EQ(odds[2], 5);
}

void test_partition_copy_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 3);

   boost::container::vector<int> evens(6, 0);
   boost::container::vector<int> odds(6, 0);

   std::pair<boost::container::vector<int>::iterator, boost::container::vector<int>::iterator> r =
      segmented_partition_copy(sv2.begin(), sv2.end(), evens.begin(), odds.begin(), is_even());

   std::size_t ne = static_cast<std::size_t>(r.first  - evens.begin());
   std::size_t no = static_cast<std::size_t>(r.second - odds.begin());
   BOOST_TEST_EQ(ne, 3u);
   BOOST_TEST_EQ(no, 3u);

   BOOST_TEST_EQ(evens[0], 2);
   BOOST_TEST_EQ(evens[1], 4);
   BOOST_TEST_EQ(evens[2], 6);

   BOOST_TEST_EQ(odds[0], 1);
   BOOST_TEST_EQ(odds[1], 3);
   BOOST_TEST_EQ(odds[2], 5);
}

int main()
{
   test_partition_copy_segmented();
   test_partition_copy_empty();
   test_partition_copy_all_true();
   test_partition_copy_non_segmented();
   test_partition_copy_sentinel_segmented();
   test_partition_copy_sentinel_non_segmented();
   test_partition_copy_seg2();
   return boost::report_errors();
}
