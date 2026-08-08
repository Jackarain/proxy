//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_search_n.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_search_n_found()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 2};
   int a2[] = {2, 3};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);

   test_detail::seg_vector<int>::iterator it =
      segmented_search_n(sv.begin(), sv.end(), 3, 2);
   BOOST_TEST(it != sv.end());
   BOOST_TEST_EQ(*it, 2);
}

void test_search_n_not_found()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 2};
   int a2[] = {3, 2};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);

   test_detail::seg_vector<int>::iterator it =
      segmented_search_n(sv.begin(), sv.end(), 3, 2);
   BOOST_TEST(it == sv.end());
}

void test_search_n_zero_count()
{
   test_detail::seg_vector<int> sv;
   int a[] = {1, 2, 3};
   sv.add_segment_range(a, a + 3);

   test_detail::seg_vector<int>::iterator it =
      segmented_search_n(sv.begin(), sv.end(), 0, 99);
   BOOST_TEST(it == sv.begin());
}

void test_search_n_non_segmented()
{
   int src[] = {1, 2, 2, 2, 3};
   boost::container::vector<int> v(src, src + 5);
   boost::container::vector<int>::iterator it = segmented_search_n(v.begin(), v.end(), 3, 2);
   BOOST_TEST(it != v.end());
   BOOST_TEST_EQ(*it, 2);
   BOOST_TEST_EQ(static_cast<std::size_t>(it - v.begin()), 1u);
}

void test_search_n_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 2};
   int a2[] = {2, 3};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);

   test_detail::seg_vector<int>::iterator it =
      segmented_search_n(sv.begin(), test_detail::make_sentinel(sv.end()), 3, 2);
   BOOST_TEST(it != sv.end());
   BOOST_TEST_EQ(*it, 2);
}

void test_search_n_sentinel_non_segmented()
{
   int src[] = {1, 2, 2, 2, 3};
   boost::container::vector<int> v(src, src + 5);
   boost::container::vector<int>::iterator it =
      segmented_search_n(v.begin(), test_detail::make_sentinel(v.end()), 3, 2);
   BOOST_TEST(it != v.end());
   BOOST_TEST_EQ(*it, 2);
}

void test_search_n_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 2};
   int a2[] = {2, 2};
   int a3[] = {3, 4};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 2);

   test_detail::seg2_vector<int>::iterator it =
      segmented_search_n(sv2.begin(), sv2.end(), 4, 2);
   BOOST_TEST(it != sv2.end());
   BOOST_TEST_EQ(*it, 2);
}

void test_search_n_every_position()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {10, 20, 30};
   int a2[] = {40, 50};
   int a3[] = {60, 70, 80, 90};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   int vals[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
   const int N = 9;
   typedef test_detail::seg_vector<int>::iterator iter_t;

   iter_t expected = sv.begin();
   for(int i = 0; i < N; ++i, ++expected) {
      iter_t it = segmented_search_n(sv.begin(), sv.end(), 1, vals[i]);
      BOOST_TEST(it != sv.end());
      BOOST_TEST_EQ(*it, vals[i]);
      BOOST_TEST(it == expected);
   }

   BOOST_TEST(segmented_search_n(sv.begin(), sv.end(), 1, 999) == sv.end());
}

void test_search_n_every_position_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {10, 20, 30};
   int a2[] = {40, 50};
   int a3[] = {60, 70, 80, 90};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   int vals[] = {10, 20, 30, 40, 50, 60, 70, 80, 90};
   const int N = 9;
   typedef test_detail::seg2_vector<int>::iterator iter_t;

   iter_t expected = sv2.begin();
   for(int i = 0; i < N; ++i, ++expected) {
      iter_t it = segmented_search_n(sv2.begin(), sv2.end(), 1, vals[i]);
      BOOST_TEST(it != sv2.end());
      BOOST_TEST_EQ(*it, vals[i]);
      BOOST_TEST(it == expected);
   }

   BOOST_TEST(segmented_search_n(sv2.begin(), sv2.end(), 1, 999) == sv2.end());
}

int main()
{
   test_search_n_found();
   test_search_n_not_found();
   test_search_n_zero_count();
   test_search_n_non_segmented();
   test_search_n_sentinel_segmented();
   test_search_n_sentinel_non_segmented();
   test_search_n_seg2();
   test_search_n_every_position();
   test_search_n_every_position_seg2();
   return boost::report_errors();
}
