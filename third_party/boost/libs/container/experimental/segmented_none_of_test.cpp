//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_none_of.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

struct is_negative
{
   bool operator()(int x) const { return x < 0; }
};

void test_none_of_true()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   BOOST_TEST(segmented_none_of(sv.begin(), sv.end(), is_negative()));
}

void test_none_of_false()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {-4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   BOOST_TEST(!segmented_none_of(sv.begin(), sv.end(), is_negative()));
}

void test_none_of_empty()
{
   test_detail::seg_vector<int> sv;
   BOOST_TEST(segmented_none_of(sv.begin(), sv.end(), is_negative()));
}

void test_none_of_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1);
   v.push_back(2);
   v.push_back(3);
   BOOST_TEST(segmented_none_of(v.begin(), v.end(), is_negative()));

   v.clear();
   v.push_back(1);
   v.push_back(-2);
   v.push_back(3);
   BOOST_TEST(!segmented_none_of(v.begin(), v.end(), is_negative()));
}

void test_none_of_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 3);

   BOOST_TEST(segmented_none_of(sv.begin(), test_detail::make_sentinel(sv.end()), is_negative()));
}

void test_none_of_sentinel_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1);
   v.push_back(-2);
   v.push_back(3);

   BOOST_TEST(!segmented_none_of(v.begin(), test_detail::make_sentinel(v.end()), is_negative()));
}

void test_none_of_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {-4, 5, 6};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 3);

   BOOST_TEST(!segmented_none_of(sv2.begin(), sv2.end(), is_negative()));
}

int main()
{
   test_none_of_true();
   test_none_of_false();
   test_none_of_empty();
   test_none_of_non_segmented();
   test_none_of_sentinel_segmented();
   test_none_of_sentinel_non_segmented();
   test_none_of_seg2();
   return boost::report_errors();
}
