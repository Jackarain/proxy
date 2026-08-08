//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_set_symmetric_difference.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_set_symmetric_difference_basic()
{
   int a[] = {1, 3, 5, 7};
   int b[] = {2, 3, 6, 7, 8};
   boost::container::vector<int> out(9, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_symmetric_difference(a, a + 4, b, b + 5, out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 5u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 2);
   BOOST_TEST_EQ(out[2], 5);
   BOOST_TEST_EQ(out[3], 6);
   BOOST_TEST_EQ(out[4], 8);
}

void test_set_symmetric_difference_empty()
{
   int a[] = {1, 2, 3};
   int* empty = a;
   boost::container::vector<int> out(3, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_symmetric_difference(a, a + 3, empty, empty, out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 3u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 2);
   BOOST_TEST_EQ(out[2], 3);
}

void test_set_symmetric_difference_identical()
{
   int a[] = {1, 2, 3};
   int b[] = {1, 2, 3};
   boost::container::vector<int> out(3, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_symmetric_difference(a, a + 3, b, b + 3, out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 0u);
}

void test_set_symmetric_difference_disjoint()
{
   int a[] = {1, 3, 5};
   int b[] = {2, 4, 6};
   boost::container::vector<int> out(6, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_symmetric_difference(a, a + 3, b, b + 3, out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 6u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 2);
   BOOST_TEST_EQ(out[2], 3);
   BOOST_TEST_EQ(out[3], 4);
   BOOST_TEST_EQ(out[4], 5);
   BOOST_TEST_EQ(out[5], 6);
}

struct greater_int
{
   bool operator()(int a, int b) const { return a > b; }
};

void test_set_symmetric_difference_with_comp()
{
   int a[] = {7, 5, 3, 1};
   int b[] = {8, 7, 6, 3, 2};
   boost::container::vector<int> out(9, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_symmetric_difference(a, a + 4, b, b + 5, out.begin(), greater_int());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 5u);
   BOOST_TEST_EQ(out[0], 8);
   BOOST_TEST_EQ(out[1], 6);
   BOOST_TEST_EQ(out[2], 5);
   BOOST_TEST_EQ(out[3], 2);
   BOOST_TEST_EQ(out[4], 1);
}

void test_set_symmetric_difference_segmented_input()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 3};
   int a2[] = {5, 7};
   sv.add_segment_range(a1, a1 + 2);
   sv.add_segment_range(a2, a2 + 2);

   int b[] = {2, 3, 5, 9};
   boost::container::vector<int> out(8, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_symmetric_difference(sv.begin(), sv.end(), b, b + 4, out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 4u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 2);
   BOOST_TEST_EQ(out[2], 7);
   BOOST_TEST_EQ(out[3], 9);
}

void test_set_symmetric_difference_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 3};
   int a2[] = {5, 7};
   sv.add_segment_range(a1, a1 + 2);
   sv.add_segment_range(a2, a2 + 2);

   int b[] = {2, 3, 5, 9};
   boost::container::vector<int> out(8, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_symmetric_difference(sv.begin(), test_detail::make_sentinel(sv.end()),
                                         b, test_detail::make_sentinel(b + 4), out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 4u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 2);
   BOOST_TEST_EQ(out[2], 7);
   BOOST_TEST_EQ(out[3], 9);
}

void test_set_symmetric_difference_sentinel_non_segmented()
{
   int a[] = {1, 3, 5, 7};
   int b[] = {2, 3, 6, 7, 8};
   boost::container::vector<int> out(9, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_symmetric_difference(a, test_detail::make_sentinel(a + 4),
                                         b, test_detail::make_sentinel(b + 5), out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 5u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 2);
   BOOST_TEST_EQ(out[2], 5);
   BOOST_TEST_EQ(out[3], 6);
   BOOST_TEST_EQ(out[4], 8);
}

void test_set_symmetric_difference_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 3, 5};
   int a2[] = {7, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);

   int b[] = {2, 3, 7, 10};
   boost::container::vector<int> out(9, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_symmetric_difference(sv2.begin(), sv2.end(), b, b + 4, out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 5u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 2);
   BOOST_TEST_EQ(out[2], 5);
   BOOST_TEST_EQ(out[3], 9);
   BOOST_TEST_EQ(out[4], 10);
}

int main()
{
   test_set_symmetric_difference_basic();
   test_set_symmetric_difference_empty();
   test_set_symmetric_difference_identical();
   test_set_symmetric_difference_disjoint();
   test_set_symmetric_difference_with_comp();
   test_set_symmetric_difference_segmented_input();
   test_set_symmetric_difference_sentinel_segmented();
   test_set_symmetric_difference_sentinel_non_segmented();
   test_set_symmetric_difference_seg2();
   return boost::report_errors();
}
