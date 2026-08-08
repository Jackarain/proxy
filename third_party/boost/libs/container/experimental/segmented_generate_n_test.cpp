//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_generate_n.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

struct counter_gen
{
   int n;
   counter_gen() : n(0) {}
   int operator()() { return ++n; }
};

void test_generate_n_full()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(3, 0);
   sv.add_segment(4, 0);
   sv.add_segment(2, 0);

   segmented_generate_n(sv.begin(), 9, counter_gen());

   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(int i = 1; i <= 9; ++i, ++it)
      BOOST_TEST_EQ(*it, i);
}

void test_generate_n_partial()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(3, 0);
   sv.add_segment(4, 0);
   sv.add_segment(2, 0);

   test_detail::seg_vector<int>::iterator result =
      segmented_generate_n(sv.begin(), 5, counter_gen());

   test_detail::seg_vector<int>::iterator it = sv.begin();
   BOOST_TEST_EQ(*it, 1); ++it;
   BOOST_TEST_EQ(*it, 2); ++it;
   BOOST_TEST_EQ(*it, 3); ++it;
   BOOST_TEST_EQ(*it, 4); ++it;
   BOOST_TEST_EQ(*it, 5); ++it;
   BOOST_TEST(it == result);
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST_EQ(*it, 0);
}

void test_generate_n_zero()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(3, 99);

   test_detail::seg_vector<int>::iterator result =
      segmented_generate_n(sv.begin(), 0, counter_gen());
   BOOST_TEST(result == sv.begin());

   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(; it != sv.end(); ++it)
      BOOST_TEST_EQ(*it, 99);
}

void test_generate_n_state_preserved()
{
   test_detail::seg_vector<int> sv;
   sv.add_segment(2, 0);
   sv.add_segment(3, 0);

   segmented_generate_n(sv.begin(), 5, counter_gen());

   test_detail::seg_vector<int>::iterator it = sv.begin();
   BOOST_TEST_EQ(*it, 1); ++it;
   BOOST_TEST_EQ(*it, 2); ++it;
   BOOST_TEST_EQ(*it, 3); ++it;
   BOOST_TEST_EQ(*it, 4); ++it;
   BOOST_TEST_EQ(*it, 5);
}

void test_generate_n_non_segmented()
{
   boost::container::vector<int> v(5, 0);
   boost::container::vector<int>::iterator result =
      segmented_generate_n(v.begin(), 3, counter_gen());

   BOOST_TEST(result == v.begin() + 3);
   BOOST_TEST_EQ(v[0], 1);
   BOOST_TEST_EQ(v[1], 2);
   BOOST_TEST_EQ(v[2], 3);
   BOOST_TEST_EQ(v[3], 0);
   BOOST_TEST_EQ(v[4], 0);
}

void test_generate_n_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int z1[] = {0, 0, 0};
   int z2[] = {0, 0, 0};
   int z3[] = {0, 0, 0};
   sv2.add_flat_segment_range(z1, z1 + 3);
   sv2.add_flat_segment_range(z2, z2 + 3);
   sv2.add_flat_segment_range(z3, z3 + 3);

   segmented_generate_n(sv2.begin(), 9, counter_gen());

   test_detail::seg2_vector<int>::iterator it = sv2.begin();
   for(int i = 1; i <= 9; ++i, ++it)
      BOOST_TEST_EQ(*it, i);
}

int main()
{
   test_generate_n_full();
   test_generate_n_partial();
   test_generate_n_zero();
   test_generate_n_state_preserved();
   test_generate_n_non_segmented();
   test_generate_n_seg2();
   return boost::report_errors();
}
