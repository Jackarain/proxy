//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_fill.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_fill_full_range()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(3, 0);
   sv.add_segment(4, 0);
   sv.add_segment(2, 0);

   segmented_fill(sv.begin(), sv.end(), 42);

   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(; it != sv.end(); ++it)
      BOOST_TEST_EQ(*it, 42);
}

void test_fill_empty_range()
{
   test_detail::seg_vector<int> sv;
   segmented_fill(sv.begin(), sv.end(), 42);
   BOOST_TEST_EQ(sv.total_size(), 0u);
}

void test_fill_single_segment()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(5, 0);

   segmented_fill(sv.begin(), sv.end(), 99);

   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(; it != sv.end(); ++it)
      BOOST_TEST_EQ(*it, 99);
}

void test_fill_partial_range()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6, 7};
   int a3[] = {8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 4);
   sv.add_segment_range(a3, a3 + 2);

   // Fill only the middle segment by constructing sub-range iterators
   typedef test_detail::seg_vector<int>::iterator iter_t;
   typedef segmented_iterator_traits<iter_t> traits;
   typename traits::segment_iterator seg_begin = traits::segment(sv.begin());
   ++seg_begin; // second segment
   iter_t mid_begin = traits::compose(seg_begin, traits::begin(seg_begin));
   iter_t mid_end   = traits::compose(seg_begin, traits::end(seg_begin));

   segmented_fill(mid_begin, mid_end, 0);

   // Verify: first segment unchanged, second zeroed, third unchanged
   iter_t it = sv.begin();
   BOOST_TEST_EQ(*it, 1); ++it;
   BOOST_TEST_EQ(*it, 2); ++it;
   BOOST_TEST_EQ(*it, 3); ++it;
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST_EQ(*it, 8); ++it;
   BOOST_TEST_EQ(*it, 9);
}

void test_fill_non_segmented()
{
   boost::container::vector<int> v(5, 0);
   segmented_fill(v.begin(), v.end(), 7);
   for(std::size_t i = 0; i < v.size(); ++i)
      BOOST_TEST_EQ(v[i], 7);
}

void test_fill_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(3, 0);
   sv.add_segment(4, 0);
   sv.add_segment(2, 0);

   segmented_fill(sv.begin(), test_detail::make_sentinel(sv.end()), 42);

   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(; it != sv.end(); ++it)
      BOOST_TEST_EQ(*it, 42);
}

void test_fill_sentinel_non_segmented()
{
   boost::container::vector<int> v(5, 0);
   segmented_fill(v.begin(), test_detail::make_sentinel(v.end()), 7);
   for(std::size_t i = 0; i < v.size(); ++i)
      BOOST_TEST_EQ(v[i], 7);
}

void test_fill_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {0, 0, 0};
   int a2[] = {0, 0, 0};
   int a3[] = {0, 0, 0};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 3);
   sv2.add_flat_segment_range(a3, a3 + 3);

   segmented_fill(sv2.begin(), sv2.end(), 42);

   test_detail::seg2_vector<int>::iterator it = sv2.begin();
   for(; it != sv2.end(); ++it)
      BOOST_TEST_EQ(*it, 42);
}

int main()
{
   test_fill_full_range();
   test_fill_empty_range();
   test_fill_single_segment();
   test_fill_partial_range();
   test_fill_non_segmented();
   test_fill_sentinel_segmented();
   test_fill_sentinel_non_segmented();
   test_fill_seg2();
   return boost::report_errors();
}
