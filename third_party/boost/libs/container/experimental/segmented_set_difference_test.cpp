//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_set_difference.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_set_difference_basic()
{
   int a[] = {1, 3, 5, 7};
   int b[] = {2, 3, 6, 7, 8};
   boost::container::vector<int> out(5, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_difference(a, a + 4, b, b + 5, out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 2u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 5);
}

void test_set_difference_empty_second()
{
   int a[] = {1, 2, 3};
   int* empty = a;
   boost::container::vector<int> out(3, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_difference(a, a + 3, empty, empty, out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 3u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 2);
   BOOST_TEST_EQ(out[2], 3);
}

void test_set_difference_empty_first()
{
   int b[] = {1, 2, 3};
   int* empty = b;
   boost::container::vector<int> out(3, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_difference(empty, empty, b, b + 3, out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 0u);
}

void test_set_difference_identical()
{
   int a[] = {1, 2, 3};
   int b[] = {1, 2, 3};
   boost::container::vector<int> out(3, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_difference(a, a + 3, b, b + 3, out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 0u);
}

struct greater_int
{
   bool operator()(int a, int b) const { return a > b; }
};

void test_set_difference_with_comp()
{
   int a[] = {7, 5, 3, 1};
   int b[] = {8, 7, 6, 3, 2};
   boost::container::vector<int> out(5, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_difference(a, a + 4, b, b + 5, out.begin(), greater_int());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 2u);
   BOOST_TEST_EQ(out[0], 5);
   BOOST_TEST_EQ(out[1], 1);
}

void test_set_difference_segmented_input()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 3};
   int a2[] = {5, 7};
   sv.add_segment_range(a1, a1 + 2);
   sv.add_segment_range(a2, a2 + 2);

   int b[] = {3, 7};
   boost::container::vector<int> out(4, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_difference(sv.begin(), sv.end(), b, b + 2, out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 2u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 5);
}

void test_set_difference_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 3};
   int a2[] = {5, 7};
   sv.add_segment_range(a1, a1 + 2);
   sv.add_segment_range(a2, a2 + 2);

   int b[] = {3, 7};
   boost::container::vector<int> out(4, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_difference(sv.begin(), test_detail::make_sentinel(sv.end()),
                               b, test_detail::make_sentinel(b + 2), out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 2u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 5);
}

void test_set_difference_sentinel_non_segmented()
{
   int a[] = {1, 3, 5, 7};
   int b[] = {2, 3, 6, 7, 8};
   boost::container::vector<int> out(5, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_difference(a, test_detail::make_sentinel(a + 4),
                               b, test_detail::make_sentinel(b + 5), out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 2u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 5);
}

void test_set_difference_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5, 6};
   int a3[] = {7, 8, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 3);
   sv2.add_flat_segment_range(a3, a3 + 3);

   int b[] = {2, 5, 8};
   boost::container::vector<int> out(9, 0);

   boost::container::vector<int>::iterator end_it =
      segmented_set_difference(sv2.begin(), sv2.end(), b, b + 3, out.begin());

   std::size_t n = static_cast<std::size_t>(end_it - out.begin());
   BOOST_TEST_EQ(n, 6u);
   BOOST_TEST_EQ(out[0], 1);
   BOOST_TEST_EQ(out[1], 3);
   BOOST_TEST_EQ(out[2], 4);
   BOOST_TEST_EQ(out[3], 6);
   BOOST_TEST_EQ(out[4], 7);
   BOOST_TEST_EQ(out[5], 9);
}

int main()
{
   test_set_difference_basic();
   test_set_difference_empty_second();
   test_set_difference_empty_first();
   test_set_difference_identical();
   test_set_difference_with_comp();
   test_set_difference_segmented_input();
   test_set_difference_sentinel_segmented();
   test_set_difference_sentinel_non_segmented();
   test_set_difference_seg2();
   return boost::report_errors();
}
