//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_generate.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

struct counter
{
   int n;
   counter() : n(0) {}
   int operator()() { return ++n; }
};

void test_generate_full_range()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(3, 0);
   sv.add_segment(2, 0);
   sv.add_segment(4, 0);

   segmented_generate(sv.begin(), sv.end(), counter());

   // Counter state must be preserved across segments: 1,2,3 | 4,5 | 6,7,8,9
   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(int expected = 1; it != sv.end(); ++it, ++expected)
      BOOST_TEST_EQ(*it, expected);
}

void test_generate_empty()
{
   test_detail::seg_vector<int> sv;
   segmented_generate(sv.begin(), sv.end(), counter());
   BOOST_TEST_EQ(sv.total_size(), 0u);
}

void test_generate_single_segment()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(5, 0);

   segmented_generate(sv.begin(), sv.end(), counter());

   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(int expected = 1; it != sv.end(); ++it, ++expected)
      BOOST_TEST_EQ(*it, expected);
}

void test_generate_non_segmented()
{
   boost::container::vector<int> v(5, 0);
   segmented_generate(v.begin(), v.end(), counter());
   for(int i = 0; i < 5; ++i)
      BOOST_TEST_EQ(v[static_cast<std::size_t>(i)], i + 1);
}

void test_generate_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(3, 0);
   sv.add_segment(2, 0);
   sv.add_segment(4, 0);

   segmented_generate(sv.begin(), test_detail::make_sentinel(sv.end()), counter());

   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(int expected = 1; it != sv.end(); ++it, ++expected)
      BOOST_TEST_EQ(*it, expected);
}

void test_generate_sentinel_non_segmented()
{
   boost::container::vector<int> v(5, 0);
   segmented_generate(v.begin(), test_detail::make_sentinel(v.end()), counter());
   for(int i = 0; i < 5; ++i)
      BOOST_TEST_EQ(v[static_cast<std::size_t>(i)], i + 1);
}

void test_generate_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int z1[] = {0, 0, 0};
   int z2[] = {0, 0, 0};
   int z3[] = {0, 0, 0};
   sv2.add_flat_segment_range(z1, z1 + 3);
   sv2.add_flat_segment_range(z2, z2 + 3);
   sv2.add_flat_segment_range(z3, z3 + 3);

   segmented_generate(sv2.begin(), sv2.end(), counter());

   test_detail::seg2_vector<int>::iterator it = sv2.begin();
   for(int expected = 1; it != sv2.end(); ++it, ++expected)
      BOOST_TEST_EQ(*it, expected);
}

int main()
{
   test_generate_full_range();
   test_generate_empty();
   test_generate_single_segment();
   test_generate_non_segmented();
   test_generate_sentinel_segmented();
   test_generate_sentinel_non_segmented();
   test_generate_seg2();
   return boost::report_errors();
}
