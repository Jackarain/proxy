//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2025-2026. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/experimental/segmented_equal.hpp>
#include <boost/core/lightweight_test.hpp>
#include "segmented_test_helper.hpp"
#include <boost/container/vector.hpp>

using namespace boost::container;

void test_equal_matching()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   int ref[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
   BOOST_TEST(segmented_equal(sv.begin(), sv.end(), ref));
}

void test_equal_mismatch()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);

   int ref[] = {1, 2, 3, 4, 99};
   BOOST_TEST(!segmented_equal(sv.begin(), sv.end(), ref));
}

void test_equal_mismatch_first_segment()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);

   int ref[] = {1, 99, 3, 4, 5};
   BOOST_TEST(!segmented_equal(sv.begin(), sv.end(), ref));
}

void test_equal_empty()
{
   test_detail::seg_vector<int> sv;
   int dummy = 0;
   BOOST_TEST(segmented_equal(sv.begin(), sv.end(), &dummy));
}

void test_equal_single_segment()
{
   test_detail::seg_vector<int> sv;
   int a[] = {10, 20, 30};
   sv.add_segment_range(a, a + 3);

   int ref[] = {10, 20, 30};
   BOOST_TEST(segmented_equal(sv.begin(), sv.end(), ref));
}

void test_equal_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1); v.push_back(2); v.push_back(3);

   int ref_match[] = {1, 2, 3};
   BOOST_TEST(segmented_equal(v.begin(), v.end(), ref_match));

   int ref_fail[] = {1, 2, 99};
   BOOST_TEST(!segmented_equal(v.begin(), v.end(), ref_fail));
}

void test_equal_sentinel_segmented()
{
   test_detail::seg_vector<int> sv;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv.add_segment_range(a1, a1 + 3);
   sv.add_segment_range(a2, a2 + 2);
   sv.add_segment_range(a3, a3 + 4);

   int ref[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
   BOOST_TEST(segmented_equal(sv.begin(), test_detail::make_sentinel(sv.end()), ref));
}

void test_equal_sentinel_non_segmented()
{
   boost::container::vector<int> v;
   v.push_back(1); v.push_back(2); v.push_back(3);

   int ref_match[] = {1, 2, 3};
   BOOST_TEST(segmented_equal(v.begin(), test_detail::make_sentinel(v.end()), ref_match));

   int ref_fail[] = {1, 2, 99};
   BOOST_TEST(!segmented_equal(v.begin(), test_detail::make_sentinel(v.end()), ref_fail));
}

void test_equal_seg2()
{
   test_detail::seg2_vector<int> sv2;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv2.add_flat_segment_range(a1, a1 + 3);
   sv2.add_flat_segment_range(a2, a2 + 2);
   sv2.add_flat_segment_range(a3, a3 + 4);

   int ref[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
   BOOST_TEST(segmented_equal(sv2.begin(), sv2.end(), ref));

   int ref_bad[] = {1, 2, 3, 4, 5, 6, 7, 8, 0};
   BOOST_TEST(!segmented_equal(sv2.begin(), sv2.end(), ref_bad));
}

void test_equal_seg_to_seg()
{
   test_detail::seg_vector<int> sv1;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv1.add_segment_range(a1, a1 + 3);
   sv1.add_segment_range(a2, a2 + 2);
   sv1.add_segment_range(a3, a3 + 4);

   test_detail::seg_vector<int> sv2;
   int b1[] = {1, 2};
   int b2[] = {3, 4, 5, 6};
   int b3[] = {7, 8, 9};
   sv2.add_segment_range(b1, b1 + 2);
   sv2.add_segment_range(b2, b2 + 4);
   sv2.add_segment_range(b3, b3 + 3);

   BOOST_TEST(segmented_equal(sv1.begin(), sv1.end(), sv2.begin()));
}

void test_equal_seg_to_seg_mismatch()
{
   test_detail::seg_vector<int> sv1;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   sv1.add_segment_range(a1, a1 + 3);
   sv1.add_segment_range(a2, a2 + 2);

   test_detail::seg_vector<int> sv2;
   int b1[] = {1, 2};
   int b2[] = {3, 4, 99};
   sv2.add_segment_range(b1, b1 + 2);
   sv2.add_segment_range(b2, b2 + 3);

   BOOST_TEST(!segmented_equal(sv1.begin(), sv1.end(), sv2.begin()));
}

void test_equal_seg2_to_seg2()
{
   test_detail::seg2_vector<int> sv1;
   int a1[] = {1, 2, 3};
   int a2[] = {4, 5};
   int a3[] = {6, 7, 8, 9};
   sv1.add_flat_segment_range(a1, a1 + 3);
   sv1.add_flat_segment_range(a2, a2 + 2);
   sv1.add_flat_segment_range(a3, a3 + 4);

   test_detail::seg2_vector<int> sv2;
   int b1[] = {1, 2};
   int b2[] = {3, 4, 5, 6};
   int b3[] = {7, 8, 9};
   sv2.add_flat_segment_range(b1, b1 + 2);
   sv2.add_flat_segment_range(b2, b2 + 4);
   sv2.add_flat_segment_range(b3, b3 + 3);

   BOOST_TEST(segmented_equal(sv1.begin(), sv1.end(), sv2.begin()));

   test_detail::seg2_vector<int> sv3;
   int c1[] = {1, 2, 3, 4, 5, 6, 7, 8, 0};
   sv3.add_flat_segment_range(c1, c1 + 9);

   BOOST_TEST(!segmented_equal(sv1.begin(), sv1.end(), sv3.begin()));
}

void test_equal_seg_to_seg_misaligned()
{
   test_detail::seg_vector<int> sv1;
   int a1[] = {10};
   int a2[] = {20, 30};
   int a3[] = {40, 50, 60};
   sv1.add_segment_range(a1, a1 + 1);
   sv1.add_segment_range(a2, a2 + 2);
   sv1.add_segment_range(a3, a3 + 3);

   test_detail::seg_vector<int> sv2;
   int b1[] = {10, 20, 30, 40};
   int b2[] = {50, 60};
   sv2.add_segment_range(b1, b1 + 4);
   sv2.add_segment_range(b2, b2 + 2);

   BOOST_TEST(segmented_equal(sv1.begin(), sv1.end(), sv2.begin()));
}

int main()
{
   test_equal_matching();
   test_equal_mismatch();
   test_equal_mismatch_first_segment();
   test_equal_empty();
   test_equal_single_segment();
   test_equal_non_segmented();
   test_equal_sentinel_segmented();
   test_equal_sentinel_non_segmented();
   test_equal_seg2();
   test_equal_seg_to_seg();
   test_equal_seg_to_seg_mismatch();
   test_equal_seg2_to_seg2();
   test_equal_seg_to_seg_misaligned();
   return boost::report_errors();
}
