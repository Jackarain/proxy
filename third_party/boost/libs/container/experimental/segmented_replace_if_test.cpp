//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_replace_if.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

struct is_negative
{
   bool operator()(int x) const { return x < 0; }
};

struct is_even
{
   bool operator()(int x) const { return x % 2 == 0; }
};

void test_replace_if_basic()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, -2, 3};
   int a2[] = {-4, 5};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);

   segmented_replace_if(sv.begin(), sv.end(), is_negative(), 0);

   test_detail::seg_vector<int>::iterator it = sv.begin();
   BOOST_TEST_EQ(*it, 1); ++it;
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST_EQ(*it, 3); ++it;
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST_EQ(*it, 5);
}

void test_replace_if_none()
{
   test_detail::seg_vector<int> sv;
   int a[] = {1, 3, 5};
   sv.add_segment_range(a, a + 3);

   segmented_replace_if(sv.begin(), sv.end(), is_negative(), 0);

   test_detail::seg_vector<int>::iterator it = sv.begin();
   BOOST_TEST_EQ(*it, 1); ++it;
   BOOST_TEST_EQ(*it, 3); ++it;
   BOOST_TEST_EQ(*it, 5);
}

void test_replace_if_all()
{
   test_detail::seg_vector<int> sv;
   int a[] = {-1, -2, -3};
   sv.add_segment_range(a, a + 3);

   segmented_replace_if(sv.begin(), sv.end(), is_negative(), 0);

   test_detail::seg_vector<int>::iterator it = sv.begin();
   for(; it != sv.end(); ++it)
      BOOST_TEST_EQ(*it, 0);
}

void test_replace_if_empty()
{
   test_detail::seg_vector<int> sv;
   segmented_replace_if(sv.begin(), sv.end(), is_negative(), 0);
   BOOST_TEST_EQ(sv.total_size(), 0u);
}

void test_replace_if_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1);
   v.push_back(-2);
   v.push_back(3);
   v.push_back(-4);

   segmented_replace_if(v.begin(), v.end(), is_negative(), 0);
   BOOST_TEST_EQ(v[0], 1);
   BOOST_TEST_EQ(v[1], 0);
   BOOST_TEST_EQ(v[2], 3);
   BOOST_TEST_EQ(v[3], 0);
}

void test_replace_if_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, -2, 3};
   int a2[] = {-4, 5};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);

   segmented_replace_if(sv.begin(), test_detail::make_sentinel(sv.end()), is_negative(), 0);

   test_detail::seg_vector<int>::iterator it = sv.begin();
   BOOST_TEST_EQ(*it, 1); ++it;
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST_EQ(*it, 3); ++it;
   BOOST_TEST_EQ(*it, 0); ++it;
   BOOST_TEST_EQ(*it, 5);
}

void test_replace_if_sentinel_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1);
   v.push_back(-2);
   v.push_back(3);
   v.push_back(-4);

   segmented_replace_if(v.begin(), test_detail::make_sentinel(v.end()), is_negative(), 0);
   BOOST_TEST_EQ(v[0], 1);
   BOOST_TEST_EQ(v[1], 0);
   BOOST_TEST_EQ(v[2], 3);
   BOOST_TEST_EQ(v[3], 0);
}

void test_replace_if_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   int a3[] = {7, 8};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 3);
   sv2.add_flat_segment_range(a3, a3 + 2);

   segmented_replace_if(sv2.begin(), sv2.end(), is_even(), 0);

   int expected[] = {1, 0, 3, 0, 5, 0, 7, 0};
   test_detail::seg2_vector<int>::iterator it = sv2.begin();
   for(int i = 0; i < 8; ++i, ++it)
      BOOST_TEST_EQ(*it, expected[i]);
}

int main()
{
   test_replace_if_basic();
   test_replace_if_none();
   test_replace_if_all();
   test_replace_if_empty();
   test_replace_if_non_segmented();
   test_replace_if_sentinel_segmented();
   test_replace_if_sentinel_non_segmented();
   test_replace_if_seg2();
   return boost::report_errors();
}
